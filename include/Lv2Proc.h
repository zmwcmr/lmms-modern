/*
 * Lv2Proc.h - Lv2 processor class
 *
 * Copyright (c) 2019-2024 Johannes Lorenz <jlsf2013$users.sourceforge.net, $=@>
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 */

#ifndef LMMS_LV2_PROC_H
#define LMMS_LV2_PROC_H

#include "lmmsconfig.h"

#ifdef LMMS_HAVE_LV2

#include <lilv/lilv.h>
#include <lv2/state/state.h>
#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include <QTemporaryDir>

#include <ringbuffer/ringbuffer.h>

#include "LocklessRingBuffer.h"
#include "ModelGroup.h"
#include "LmmsSemaphore.h"
#include "Lv2Basics.h"
#include "Lv2Features.h"
#include "Lv2Options.h"
#include "Lv2Worker.h"
#include "Plugin.h"
#include "TimePos.h"


namespace lmms
{

class AutomatableModel;
class Lv2HostClient;
class MidiEvent;
class PluginIssue;
class SampleFrame;

// forward declare port structs/enums
namespace Lv2Ports
{
	struct Audio;
	struct Control;
	struct PortBase;
	struct AtomSeq;
}

/**
	Class representing one Lv2 processor, i.e. one Lv2 handle.
	For mono plugins, L/R channel routing is being used
	(this includes techniques like upmixing and downmixing).

	This class provides everything Lv2 effect and instrument have in common.
	It's not named Lv2Plugin, because
	* it does not inherit Instrument
	* the Plugin subclass Effect does not inherit this class

	This class would usually be a Model subclass. However, Qt doesn't allow
	this:
	* inheriting only from Model will cause diamond inheritance for QObject,
	  which will cause errors with Q_OBJECT
	* making this a direct subclass of Instrument resp. EffectControls would
	  require CRTP, which would make this class a template class, which would
	  conflict with Q_OBJECT

	The consequence is that this class can neither inherit QObject or Model, nor
	Instrument or EffectControls, which means in fact:
	* this class contains no signals or slots, but it offers stubs for slots
	  that shall be called by child classes
	* this class can not override virtuals of Instrument or EffectControls, so
	  it will offer functions that must be called by virtuals in its child class
*/
class LMMS_EXPORT Lv2Proc : public ModelGroup
{
	friend class Lv2ProcSuspender;
public:
	static Plugin::Type check(const LilvPlugin* plugin,
		std::vector<PluginIssue> &issues);

	/*
		ctor/dtor/reload
	*/
	Lv2Proc(Model *parent, const QString &uri);
	~Lv2Proc();
	void reload();
	void onSampleRateChanged();

	//! Save port values and, if supported, the plugin's state extension
	void saveSettings(QDomDocument& doc, QDomElement& that);
	//! Load port values and, if present, the plugin's state extension
	void loadSettings(const QDomElement& that);

	/*
		port access
	 */
	struct StereoPortRef
	{
		//! mono port or left port in case of stereo
		Lv2Ports::Audio* m_left = nullptr;
		//! unused, or right port in case of stereo
		Lv2Ports::Audio* m_right = nullptr;
	};

	StereoPortRef& inPorts() { return m_inPorts; }
	const StereoPortRef& inPorts() const { return m_inPorts; }
	StereoPortRef& outPorts() { return m_outPorts; }
	const StereoPortRef& outPorts() const { return m_outPorts; }
	template<class Functor>
	void foreach_port(const Functor& ftor)
	{
		for (std::unique_ptr<Lv2Ports::PortBase>& port : m_ports)
		{
			ftor(port.get());
		}
	}
	template<class Functor>
	void foreach_port(const Functor& ftor) const
	{
		for (const std::unique_ptr<Lv2Ports::PortBase>& port : m_ports)
		{
			ftor(port.get());
		}
	}

	//! Debug function to print ports to stdout
	void dumpPorts();

	/*
		factory presets
	*/
	struct Preset
	{
		QString label;
		QString uri;
	};

	//! Presets from the plugin bundle that apply to this plugin
	const std::vector<Preset>& presets() const { return m_presets; }
	//! Restore a factory preset; must be called from the GUI thread
	bool loadPreset(std::size_t index);

	//! One MIDI message emitted on the plugin's MIDI output port
	struct MidiOutEvent
	{
		uint32_t offset = 0;
		std::array<uint8_t, 3> data{};
		uint8_t size = 0;
	};

	//! MIDI events produced since the last clear; audio-thread only
	const std::vector<MidiOutEvent>& midiOutEvents() const
	{
		return m_midiOutEvents;
	}
	void clearMidiOutEvents() { m_midiOutEvents.clear(); }

	/*
		utils for the run thread
	*/

	//! @brief Copy values from the LMMS core (connected models, MIDI events, ...) into
	//! the respective ports
	void copyModelsFromCore();

	//! @brief Bring values from all ports to the LMMS core
	void copyModelsToCore();
	//! @brief Copy buffer passed by the core into our ports
	//! @param buf buffer of sample frames, each sample frame is something like
	//!   a `float[<number-of-procs> * <channels per proc>]` array.
	//! @param frames The length of @p buf
	void copyBuffersFromCore(const SampleFrame* buf, f_cnt_t frames);

	//! @brief Copy our ports into buffers passed by the core
	//! @param buf buffer of sample frames, each sample frame is something like
	//!   a `float[<number-of-procs> * <channels per proc>]` array.
	//! @param frames The length of @p buf
	void copyBuffersToCore(SampleFrame* buf, f_cnt_t frames) const;

	//! @brief Run the Lv2 plugin instance for @p frames frames
	//! @param frames The number of frames to run
	void run(f_cnt_t frames);

	void handleMidiInputEvent(const MidiEvent &event,
		const TimePos &time, f_cnt_t offset);

	/*
		misc
	 */
	AutomatableModel *modelAtPort(const QString &uri); // unused currently
	std::size_t controlCount() const { return ModelGroup::size(); }
	bool hasNoteInput() const;
	//! Processing latency reported by the plugin, in samples
	f_cnt_t latency() const { return m_latency.load(std::memory_order_relaxed); }

	//! Whether this processor runs in an isolated host child process
	bool isRemote() const { return m_remote != nullptr; }
	//! Child process handle in remote mode, nullptr otherwise
	Lv2HostClient* remoteClient() const { return m_remote.get(); }
	const LilvPlugin* getPlugin() const { return m_plugin; }
	bool hasGui() const { return m_hasGUI; }

	/*
		UI support
	*/
	LilvUIs* getUis() const { return lilv_plugin_get_uis(m_plugin); }
	const char* pluginUri() const { return lilv_node_as_uri(lilv_plugin_get_uri(m_plugin)); }
	std::size_t getIdOfPort(const char* symbol) const;
	std::size_t portNum() const { return m_ports.size(); }
	Lv2Ports::PortBase* port(std::size_t idx) { return m_ports[idx].get(); }
	const Lv2Ports::PortBase* port(std::size_t idx) const { return m_ports[idx].get(); }
	//! Called by the UI when the user moves a control
	void setControlFromUi(uint32_t portIndex, float value);
	//! Handle any event sent by the plugin UI (float control or atom event)
	bool writeUiEvent(uint32_t portIndex, uint32_t protocol,
		uint32_t size, const void* buffer);
	struct UiEvent
	{
		uint32_t index = 0;
		uint32_t protocol = 0;
		std::vector<char> data;
	};
	//! Start forwarding plugin -> UI events
	void connectToPluginEvents();
	//! Stop forwarding plugin -> UI events
	void setUiActive(bool active) { m_uiActive = active; }
	//! Take all queued plugin -> UI events
	std::vector<UiEvent> takeUiEvents();
	//! Let the view detach its native UI before the plugin instance dies
	void setViewDetacher(std::function<void()> detacher) { m_viewDetacher = std::move(detacher); }

	const LilvInstance* getInstanceForInstanceFeatureOnly() const { return m_instance; }
	const LV2_Feature* mapFeature() const { return m_features.feature(LV2_URID__map); }
	const LV2_Feature* unmapFeature() const { return m_features.feature(LV2_URID__unmap); }
	const LV2_Feature* optionsFeature() const { return m_features.feature(LV2_OPTIONS__options); }
	LV2_Extension_Data_Feature* extdataFeature() { return &m_features.m_extData; }

protected:
	//! To be called by `Plugin::loadFile` overrides
	void loadFile(const QString &file) { (void)file; }
	//! XML DOM Node name
	QString nodeName() const { return "lv2controls"; }

private:
	// lv2
	const LilvPlugin* m_plugin;
	LilvInstance* m_instance = nullptr;
	Lv2Features m_features;

	// options
	Lv2Options m_options;

	// worker
	std::optional<Lv2Worker> m_worker;
	Semaphore m_workLock; // this must be shared by different workers

	// full list of ports
	std::vector<std::unique_ptr<Lv2Ports::PortBase>> m_ports;
	// quick reference to specific, unique ports
	StereoPortRef m_inPorts, m_outPorts;
	Lv2Ports::AtomSeq *m_midiIn = nullptr, *m_midiOut = nullptr;

	// MIDI
	// many things here may be moved into the `Instrument` class
	constexpr const static std::size_t m_maxMidiInputEvents = 1024;
	//! spinlock for the MIDI ringbuffer (for MIDI events going to the plugin)
	std::atomic_flag m_ringLock = ATOMIC_FLAG_INIT;

	//! MIDI ringbuffer (for MIDI events going to the plugin)
	ringbuffer_t<struct MidiInputEvent> m_midiInputBuf;
	//! MIDI ringbuffer reader
	ringbuffer_reader_t<struct MidiInputEvent> m_midiInputReader;

	// other
	static int32_t defaultEvbufSize() { return 1 << 15; /* ardour uses this*/ }
	bool m_hasGUI = false;

	//! Per-instance directory for files created via state:makePath.
	//! It lives as long as the plugin instance so restored samples etc.
	//! remain available.
	std::unique_ptr<QTemporaryDir> m_stateDir;
	//! Serializes run() against state save/restore
	std::mutex m_stateMutex;
	LV2_State_Make_Path m_makePath;
	LV2_State_Free_Path m_freePath;
	LV2_State_Map_Path m_mapPath;

	//! Called by the view to detach its native UI before shutdown
	std::function<void()> m_viewDetacher;

	//! Output control port carrying the plugin latency, if any
	Lv2Ports::Control* m_latencyPort = nullptr;
	std::atomic<f_cnt_t> m_latency{0};

	//! Isolated host process (LMMS_LV2_REMOTE=1), null in-process
	std::unique_ptr<Lv2HostClient> m_remote;
	//! Last control values sent to the child; NaN forces the first send
	std::vector<float> m_lastRemoteControl;

	void startRemote();
	void runRemote(f_cnt_t frames);
	void copyModelsToRemote();
	void saveStateRemote(QDomDocument& doc, QDomElement& that);
	void loadStateRemote(const QDomElement& that);
	void applyRemoteControlValues();

	//! Plugin -> UI event queue (atom events, etc.)
	std::mutex m_uiEventMutex;
	std::deque<UiEvent> m_uiEvents;
	std::atomic<bool> m_uiActive{false};
	bool sendToUi(uint32_t portIndex, uint32_t type, uint32_t size, const void* body);

	//! Factory presets exposed by the plugin bundle
	std::vector<Preset> m_presets;
	void scanPresets();

	//! MIDI output collected in run(); consumed on the audio thread
	std::vector<MidiOutEvent> m_midiOutEvents;
	void collectMidiOutEvent(uint32_t offset, const uint8_t* data, uint32_t size);

	void saveState(QDomDocument& doc, QDomElement& that);
	void loadState(const QDomElement& that);

	static char* makePath(LV2_State_Make_Path_Handle handle, const char* path);
	static void freePath(LV2_State_Free_Path_Handle handle, char* path);
	static char* absolutePath(LV2_State_Map_Path_Handle handle, const char* path);
	static char* abstractPath(LV2_State_Map_Path_Handle handle, const char* path);

	//! models for the controls, sorted by port symbols
	//! @note These are not owned, but rather link to the models in
	//!   ControlPorts in `m_ports`
	std::map<std::string, AutomatableModel *> m_connectedModels;

	void initMOptions(); //!< initialize m_options
	void initPluginSpecificFeatures();

	//! Create ports and instance, connect ports, activate plugin
	void initPlugin();
	//! Deactivate instance
	void shutdownPlugin();
	//! load a file in the plugin, but don't do anything in LMMS
	void loadFileInternal(const QString &file);
	//! allocate m_ports, fill all with metadata, and assign meaning of ports
	void createPorts();
	//! fill m_ports[portNum] with metadata
	void createPort(std::size_t portNum);
	//! connect m_ports[portNum] with Lv2
	void connectPort(std::size_t num);

	void dumpPort(std::size_t num);

	static bool portIsSideChain(const LilvPlugin* plugin, const LilvPort *port);
	static bool portIsOptional(const LilvPlugin* plugin, const LilvPort *port);
	static AutoLilvNode uri(const char* uriStr);
};


} // namespace lmms

#endif // LMMS_HAVE_LV2

#endif // LMMS_LV2_PROC_H
