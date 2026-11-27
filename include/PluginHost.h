/*
 * PluginHost.h - format-independent interface for hosted plugins
 *
 * Copyright (c) 2026 LMMS contributors
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

#ifndef LMMS_PLUGIN_HOST_H
#define LMMS_PLUGIN_HOST_H

#include <cstdint>
#include <vector>

#include <QByteArray>
#include <QString>

class QWidget;

namespace lmms
{

enum class PluginFormat
{
	Vst2,
	Vst3,
	Lv2,
};

/*! How parameter IDs are interpreted by an adapter. */
enum class PluginParameterKind
{
	//! VST2: the flat program parameter index.
	FlatIndex,
	//! VST3: the edit controller's parameter ID, normalised to 0..1.
	NormalisedId,
	//! LV2: the index of the control port in plugin order.
	PortIndex,
};

enum class PluginStateKind
{
	//! VST2: one opaque chunk, or a parameter dump when unsupported.
	Chunk,
	//! VST3: component and controller streams.
	ComponentController,
	//! LV2: the state extension plus control-port values.
	Lv2Bundle,
};

/*! Static properties of a loaded plugin.
 *
 *  The flags describe what the adapter can do, not what the format could do
 *  in theory. An adapter reports `false` while the feature is still being
 *  migrated, so callers never have to guess from the format.
 */
struct PluginHostCapabilities
{
	bool hasEditor = false;
	//! The editor needs the plugin's address space (LV2 instance-access).
	bool editorMustShareProcess = false;
	bool supportsPrograms = false;
	bool supportsUnits = false;
	bool sampleAccurateParameters = false;
	bool supportsMidiOut = false;
	bool multipleBuses = false;
	PluginParameterKind parameterKind = PluginParameterKind::FlatIndex;
	PluginStateKind stateKind = PluginStateKind::Chunk;
};

/*! One automatable parameter, in plugin units.
 *
 *  `id` is opaque to the caller and only meaningful for the adapter that
 *  produced it; `PluginHostCapabilities::parameterKind` documents its range.
 */
struct PluginParameterInfo
{
	uint32_t id = 0;
	QString name;
	QString unit;
	double min = 0.0;
	double max = 1.0;
	double defaultValue = 0.0;
	int stepCount = 0;
	bool automatable = true;
	bool readOnly = false;
	bool hidden = false;
};

struct PluginLoadRequest
{
	//! Plugin binary or bundle path (VST2/VST3), empty for LV2.
	QString path;
	//! VST3 class ID, empty for other formats.
	QString classId;
	//! LV2 plugin URI, empty for other formats.
	QString pluginUri;
	//! Format-specific unique ID (VST2), when known.
	QString uniqueId;
	double sampleRate = 0.0;
	int maxBlockSize = 0;
	bool isInstrument = false;
};

/*! A non-owning planar audio block.
 *
 *  `inputs` and `outputs` point to arrays of channel pointers; each channel
 *  holds `frames` floats. Instruments may pass no inputs. The adapter never
 *  keeps the pointers after process() returns.
 */
struct PluginProcessBlock
{
	float* const* inputs = nullptr;
	float* const* outputs = nullptr;
	int inputChannels = 0;
	int outputChannels = 0;
	int frames = 0;
};

struct PluginMidiEvent
{
	enum class Type
	{
		NoteOn,
		NoteOff,
		ControlChange,
		PitchBend,
		ChannelPressure,
		PolyPressure,
		ProgramChange,
		SysEx,
	};

	Type type = Type::NoteOn;
	int sampleOffset = 0;
	int channel = 0;
	int data1 = 0;
	int data2 = 0;
	float value = 0.0f;
	QByteArray sysex;
};

/*! Format-neutral state blob.
 *
 *  `primary` is required; `secondary` is used by formats with two independent
 *  state streams (VST3 component/controller). Adapters define the encoding.
 */
struct PluginStateBlob
{
	QByteArray primary;
	QByteArray secondary;
};

struct PluginEditorHandle
{
	enum class Kind
	{
		Unavailable,
		InProcessWidget,
		X11Window,
		Remote,
	};

	Kind kind = Kind::Unavailable;
	QWidget* widget = nullptr;
	quintptr windowId = 0;
	QString error;
};

/*! Interface shared by the VST2, VST3 and LV2 adapters.
 *
 *  Threading: load(), unload(), parameters(), program access, state access and
 *  createEditor() are GUI-thread operations. process(), setParameter(),
 *  parameterValue() and sendMidi() are called from the audio thread; an
 *  adapter must either be lock-free there or document its own locking.
 */
class PluginHost
{
public:
	virtual ~PluginHost() = default;

	virtual PluginFormat format() const = 0;
	virtual PluginHostCapabilities capabilities() const = 0;

	virtual bool load(const PluginLoadRequest& request,
		QString* error = nullptr) = 0;
	virtual void unload() = 0;
	virtual bool isLoaded() const = 0;

	//! Parameters in plugin order; the returned vector lives until unload().
	virtual const std::vector<PluginParameterInfo>& parameters() const = 0;
	//! @param sampleOffset hint for sample-accurate formats, ignored otherwise
	virtual bool setParameter(uint32_t id, double value,
		int sampleOffset = 0) = 0;
	virtual double parameterValue(uint32_t id) const = 0;

	virtual void sendMidi(const PluginMidiEvent& event) = 0;
	virtual bool process(const PluginProcessBlock& block) = 0;

	virtual int latencySamples() const = 0;

	//! Optional transport information; adapters that do not use it ignore it.
	virtual void setTempo(double) {}
	virtual void setPlaying(bool) {}

	virtual int programCount() const { return 0; }
	virtual int currentProgram() const { return -1; }
	virtual QString programName(int) const { return {}; }
	virtual bool setProgram(int) { return false; }

	virtual PluginStateBlob saveState(QString* error = nullptr) = 0;
	virtual bool loadState(const PluginStateBlob& state,
		QString* error = nullptr) = 0;

	//! Editor creation is unified in a later stage; adapters may return
	//! `PluginEditorHandle::Kind::Unavailable` until then.
	virtual PluginEditorHandle createEditor(quintptr parentWindow = 0,
		QString* error = nullptr)
	{
		Q_UNUSED(parentWindow)
		PluginEditorHandle handle;
		handle.error = QStringLiteral(
			"Editor hosting is not implemented for this plugin");
		if (error)
		{
			*error = handle.error;
		}
		return handle;
	}
};

} // namespace lmms

#endif // LMMS_PLUGIN_HOST_H
