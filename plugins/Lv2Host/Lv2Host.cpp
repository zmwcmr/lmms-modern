/*
 * Lv2Host.cpp - child process that hosts an LV2 plugin instance
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

/*
 * Second step of P6.4: the child instantiates the plugin with lilv, reports
 * the port metadata, connects the audio and control ports to the shared
 * segment and runs the plugin. Atom events, state and the UI follow in later
 * steps. The special URI "--echo" keeps the transport echo used by the
 * scaffold test.
 */

#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <climits>
#include <cstring>
#include <exception>
#include <string>

#include <atomic>
#include <condition_variable>
#include <chrono>
#include <cmath>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "LmmsTypes.h"
#include "Lv2HostProtocol.h"
#include "RemotePluginBase.h"
#include "SharedMemory.h"
#include "lmmsconfig.h"

#ifdef LMMS_HAVE_LV2
#include <lilv/lilv.h>
#include <lv2/atom/atom.h>
#include <lv2/buf-size/buf-size.h>
#include <lv2/core/lv2.h>
#include <lv2/data-access/data-access.h>
#include <lv2/instance-access/instance-access.h>
#include <lv2/log/log.h>
#include <lv2/midi/midi.h>
#include <lv2/options/options.h>
#include <lv2/state/state.h>
#include <lv2/parameters/parameters.h>
#include <lv2/ui/ui.h>
#include <lv2/urid/urid.h>
#include <lv2/worker/worker.h>
#endif

#ifdef LMMS_BUILD_LINUX
#include <pthread.h>
#include <sched.h>
#include <sys/prctl.h>
#include <unistd.h>
#endif

#if defined(LMMS_BUILD_LINUX) && defined(LMMS_HAVE_SUIL)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <suil/suil.h>
#endif

#ifndef SYNC_WITH_SHM_FIFO
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace
{

using namespace lmms;

#ifdef LMMS_BUILD_LINUX
/*! Kills the child when its parent disappears without sending IdQuit. */
class ParentWatcher
{
public:
	ParentWatcher()
	{
		m_thread = std::thread([this]
		{
			std::unique_lock lock{m_mutex};
			while (!m_cv.wait_for(lock, std::chrono::milliseconds(500),
				[this] { return m_stop; }))
			{
				if (::getppid() == 1)
				{
					::kill(::getpid(), SIGHUP);
					break;
				}
			}
		});
	}

	~ParentWatcher()
	{
		{
			const std::lock_guard lock{m_mutex};
			m_stop = true;
		}
		m_cv.notify_all();
		m_thread.join();
	}

private:
	bool m_stop = false;
	std::mutex m_mutex;
	std::condition_variable m_cv;
	std::thread m_thread;
};
#endif // LMMS_BUILD_LINUX

#ifdef LMMS_HAVE_LV2

#if defined(LMMS_BUILD_LINUX) && defined(LMMS_HAVE_SUIL)
namespace
{
LilvInstance* g_extDataInstance = nullptr;

// Plugin UIs sometimes touch stale X resources or atoms. The default Xlib
// handler terminates the whole host process, which would take the audio down
// with the editor. Log the error and keep the DSP alive instead.
int lv2HostXErrorHandler(Display*, XErrorEvent* event)
{
	static std::atomic<int> logged{0};
	if (logged.fetch_add(1) < 20)
	{
		std::fprintf(stderr,
			"lv2 host: ignoring X error %d (request %d, resource %lu)\n",
			event->error_code, event->request_code,
			static_cast<unsigned long>(event->resourceid));
	}
	return 0;
}
}
#endif

#if defined(LMMS_BUILD_LINUX) && defined(LMMS_HAVE_SUIL)
/*! Asset root some UIs (Zyn Fusion) resolve relative to the working dir. */
std::string findUiDataRoot(const std::string& bundleDir)
{
	if (bundleDir.empty())
	{
		return std::string();
	}
	std::error_code error;
	auto directory = std::filesystem::canonical(bundleDir, error);
	if (error)
	{
		directory = bundleDir;
	}
	for (int depth = 0; depth < 4; ++depth)
	{
		if (std::filesystem::exists(directory / "qml" / "MainWindow.qml"))
		{
			return directory.string();
		}
		if (!directory.has_parent_path())
		{
			break;
		}
		directory = directory.parent_path();
	}
	return std::string();
}

/*! Restores the working directory when the UI creation scope ends. */
class CurrentDirGuard
{
public:
	explicit CurrentDirGuard(const std::string& directory)
	{
		char buffer[PATH_MAX] = {};
		if (::getcwd(buffer, sizeof(buffer)) == nullptr)
		{
			return;
		}
		m_previous = buffer;
		if (!directory.empty() && ::chdir(directory.c_str()) == 0)
		{
			m_changed = true;
		}
	}

	~CurrentDirGuard()
	{
		if (m_changed && ::chdir(m_previous.c_str()) != 0)
		{
			std::fprintf(stderr, "could not restore the working directory\n");
		}
	}

	CurrentDirGuard(const CurrentDirGuard&) = delete;
	CurrentDirGuard& operator=(const CurrentDirGuard&) = delete;

private:
	std::string m_previous;
	bool m_changed = false;
};
#endif

#if defined(LMMS_BUILD_LINUX) && defined(LMMS_HAVE_SUIL)
const void* extDataAccess(const char* uri)
{
	return g_extDataInstance != nullptr
		? lilv_instance_get_extension_data(g_extDataInstance, uri)
		: nullptr;
}
#endif

int logPrintf(LV2_Log_Handle, LV2_URID, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	const int result = std::vfprintf(stderr, fmt, args);
	va_end(args);
	std::fputc('\n', stderr);
	return result;
}

int logVPrintf(LV2_Log_Handle, LV2_URID, const char* fmt, va_list args)
{
	const int result = std::vfprintf(stderr, fmt, args);
	std::fputc('\n', stderr);
	return result;
}

/*! Small URID map; the child only needs stable IDs for the features it
 *  passes to the plugin.
 */
class UridMap
{
public:
	UridMap()
	{
		m_map.handle = this;
		m_map.map = [](LV2_URID_Map_Handle handle, const char* uri) -> LV2_URID {
			return static_cast<UridMap*>(handle)->map(uri);
		};
		m_unmap.handle = this;
		m_unmap.unmap = [](LV2_URID_Unmap_Handle handle, LV2_URID urid) -> const char* {
			return static_cast<UridMap*>(handle)->unmap(urid);
		};
	}

	LV2_URID map(const char* uri)
	{
		if (uri == nullptr)
		{
			return 0;
		}
		std::lock_guard<std::mutex> lock(m_mutex);
		const auto it = m_byUri.find(uri);
		if (it != m_byUri.end())
		{
			return it->second;
		}
		const auto id = static_cast<LV2_URID>(m_uris.size() + 1);
		m_uris.emplace_back(uri);
		m_byUri.emplace(uri, id);
		return id;
	}

	const char* unmap(LV2_URID urid) const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (urid == 0 || urid > m_uris.size())
		{
			return nullptr;
		}
		// deque keeps the string addresses stable when another thread maps a
		// new URI after the lock is released.
		return m_uris[urid - 1].c_str();
	}

	LV2_URID_Map* mapFeature() { return &m_map; }
	LV2_URID_Unmap* unmapFeature() { return &m_unmap; }

private:
	mutable std::mutex m_mutex;
	std::unordered_map<std::string, LV2_URID> m_byUri;
	std::deque<std::string> m_uris;
	LV2_URID_Map m_map{};
	LV2_URID_Unmap m_unmap{};
};

/*! Minimal worker: requests scheduled during run() are executed right after
 *  the block and their responses are delivered before the next run().
 */
class Worker
{
public:
	Worker()
	{
		m_schedule.handle = this;
		m_schedule.schedule_work = [](LV2_Worker_Schedule_Handle handle,
			uint32_t size, const void* data) -> LV2_Worker_Status {
			static_cast<Worker*>(handle)->schedule(size, data);
			return LV2_WORKER_SUCCESS;
		};
	}

	LV2_Worker_Schedule* scheduleFeature() { return &m_schedule; }

	void setInterface(const LV2_Worker_Interface* iface, LV2_Handle handle)
	{
		m_iface = iface;
		m_handle = handle;
	}

	void schedule(uint32_t size, const void* data)
	{
		const auto* bytes = static_cast<const uint8_t*>(data);
		m_requests.emplace_back(bytes, bytes + size);
	}

	void run()
	{
		// Deliver the responses produced by the previous block.
		if (m_iface != nullptr && m_iface->work_response != nullptr)
		{
			for (const auto& response : m_responses)
			{
				m_iface->work_response(m_handle, static_cast<uint32_t>(response.size()),
					response.data());
			}
		}
		m_responses.clear();

		if (m_iface == nullptr || m_iface->work == nullptr)
		{
			m_requests.clear();
			return;
		}

		std::deque<std::vector<uint8_t>> requests;
		requests.swap(m_requests);
		for (const auto& request : requests)
		{
			m_iface->work(m_handle,
				[](LV2_Worker_Respond_Handle handle, uint32_t size,
					const void* data) -> LV2_Worker_Status {
					static_cast<Worker*>(handle)->respond(size, data);
					return LV2_WORKER_SUCCESS;
				},
				this, static_cast<uint32_t>(request.size()), request.data());
		}

		// Responses are ready before the next run(), deliver them there.
	}

private:
	void respond(uint32_t size, const void* data)
	{
		const auto* bytes = static_cast<const uint8_t*>(data);
		m_responses.emplace_back(bytes, bytes + size);
	}

	LV2_Worker_Schedule m_schedule{};
	const LV2_Worker_Interface* m_iface = nullptr;
	LV2_Handle m_handle = nullptr;
	std::deque<std::vector<uint8_t>> m_requests;
	std::deque<std::vector<uint8_t>> m_responses;
};

#endif // LMMS_HAVE_LV2




/*! Transport plus, when a real plugin URI is given, the LV2 instance. */
class Lv2HostProcess : public RemotePluginBase
{
public:
#ifdef SYNC_WITH_SHM_FIFO
	Lv2HostProcess(shmFifo* in, shmFifo* out, std::string pluginUri) :
		RemotePluginBase(in, out),
		m_pluginUri(std::move(pluginUri))
	{
	}
#else
	explicit Lv2HostProcess(std::string pluginUri) :
		RemotePluginBase(),
		m_pluginUri(std::move(pluginUri))
	{
	}

	void setSocket(int socket)
	{
		m_socket = socket;
	}
#endif

	~Lv2HostProcess() override;

	int run();
	bool processMessage(const message& m) override;

private:
	bool instantiate();
	void connectPorts();
	void sendPortInfo();
	void processBlock();
	void echoBlock();
	void buildMidiInput();
	void sendMidiOutput();
	void sendFailed(const std::string& reason);

	bool saveState(const std::string& directory);
	bool restoreState(const std::string& directory);
	bool loadPreset(const std::string& presetUri);
	void sendControlValues();
	void syncControlValuesFromAudio();

	void startAudioThread();
	void stopAudioThread();
	void audioThreadMain();
	void queueAudioBlock();
	void sendToHost(const message& m);
	std::string ensureStateDir();
	static char* makePathCb(LV2_State_Make_Path_Handle handle, const char* path);
	static char* mapPathCb(LV2_State_Map_Path_Handle handle, const char* path);
	static void freePathCb(LV2_State_Free_Path_Handle handle, char* path);

	bool showUi(float scale);
	void hideUi();
	void destroyUi();
	bool showUiNow(float scale);
	void hideUiNow();
	void destroyUiNow();
	void idleUi();
	void processUiEvents();
	void startUiThread();
	void stopUiThread();
	void uiThreadMain();
	bool waitForMessage(int timeoutMs);
#if defined(LMMS_BUILD_LINUX) && defined(LMMS_HAVE_SUIL)
	static void writeToUiPort(void* handle, uint32_t port, uint32_t size,
		uint32_t protocol, const void* buffer);
	static uint32_t portIndexBySymbol(void* handle, const char* symbol);
	static void touchUiPort(void* handle, uint32_t port, bool grabbed);
#endif

	SharedMemory<float[]> m_audio;
	f_cnt_t m_frames = 0;
	sample_rate_t m_sampleRate = 0;
	std::string m_pluginUri;

	// Audio runs on its own thread; the main thread only pumps messages and
	// the plugin UI. This mirrors the VST2 child and keeps a heavy editor
	// idle from delaying the next audio block.
	std::thread m_audioThread;
	std::mutex m_stateMutex;
	std::mutex m_instanceMutex;
	std::mutex m_sendMutex;
	std::condition_variable m_audioCv;
	bool m_audioRequested = false;
	bool m_audioStop = false;
	std::vector<lv2host::MidiEventData> m_audioMidi;
	std::vector<float> m_audioControlValues;

	// The LV2 UI and its X11 event loop run on their own thread. The main
	// thread must stay free to read audio-start messages and wake the audio
	// thread; otherwise a slow editor still delays the next block.
	enum class UiRequest { Idle, Show, Hide, Destroy, Quit };
	struct UiRequestItem
	{
		UiRequest type;
		float scale = 1.0f;
	};
	std::thread m_uiThread;
	std::mutex m_uiMutex;
	std::condition_variable m_uiCv;
	std::deque<UiRequestItem> m_uiRequests;

#ifdef LMMS_HAVE_LV2
	LilvWorld* m_world = nullptr;
	const LilvPlugin* m_plugin = nullptr;
	LilvInstance* m_instance = nullptr;
	UridMap m_urids;
	Worker m_worker;

	LV2_Feature m_mapFeature{};
	LV2_Feature m_unmapFeature{};
	LV2_Feature m_optionsFeature{};
	LV2_Feature m_boundedFeature{};
	LV2_Feature m_logFeature{};
	LV2_Feature m_workerFeature{};
	LV2_Options_Option m_options[6]{};
	LV2_Log_Log m_log{};
	LV2_Worker_Schedule* m_schedule = nullptr;
	float m_optionSampleRate = 0.0f;
	int32_t m_optionMinBlock = 0;
	int32_t m_optionMaxBlock = 0;
	int32_t m_optionSequenceSize = 1 << 15;

	std::vector<float> m_controlValues;
	std::vector<float> m_portScratch;
	std::vector<std::vector<uint8_t>> m_atomBuffers;
	std::vector<float> m_audioScratch;
	std::vector<lv2host::MidiEventData> m_pendingMidi;
	int m_midiInPort = -1;
	int m_midiOutPort = -1;
	int m_latencyPort = -1;
	float m_latencyValue = 0.0f;
	int m_reportedLatency = -1;
	bool m_uiVisible = false;

	// State features: files created/read by the plugin live here.
	std::string m_stateDirPath;
	LV2_Feature m_stateMakePathFeature{};
	LV2_Feature m_stateMapPathFeature{};
	LV2_Feature m_stateFreePathFeature{};
	LV2_State_Make_Path m_stateMakePath{};
	LV2_State_Map_Path m_stateMapPath{};
	LV2_State_Free_Path m_stateFreePath{};

#if defined(LMMS_BUILD_LINUX) && defined(LMMS_HAVE_SUIL)
	SuilHost* m_uiHost = nullptr;
	SuilInstance* m_uiInstance = nullptr;
	Display* m_uiDisplay = nullptr;
	unsigned long m_uiWindow = 0;
	unsigned long m_uiWmDelete = 0;
	LV2_Feature m_uiParentFeature{};
	LV2_Feature m_uiInstanceFeature{};
	LV2_Feature m_uiDataFeature{};
	LV2_Feature m_uiResizeFeature{};
	LV2_Feature m_uiOptionsFeature{};
	LV2_Feature m_uiLogFeature{};
	LV2UI_Resize m_uiResize{};
	LV2_Extension_Data_Feature m_uiExtData{};
	LV2_Options_Option m_uiOptions[3]{};
	float m_uiScale = 1.0f;
	float m_uiSampleRate = 0.0f;
	//! Size requested by the UI while suil_instance_new() is running
	int m_pendingUiWidth = 0;
	int m_pendingUiHeight = 0;
	//! Asset root the UI resolves relative paths against (Zyn Fusion)
	std::string m_uiDataRoot;
	//! Last control values pushed to the UI
	std::vector<float> m_lastUiControl;
#endif
#endif // LMMS_HAVE_LV2
};




Lv2HostProcess::~Lv2HostProcess()
{
	stopUiThread();
	stopAudioThread();
	destroyUiNow();
#ifdef LMMS_HAVE_LV2
	if (m_instance != nullptr)
	{
		lilv_instance_deactivate(m_instance);
		lilv_instance_free(m_instance);
		m_instance = nullptr;
	}
	if (m_world != nullptr)
	{
		lilv_world_free(m_world);
		m_world = nullptr;
	}
	m_plugin = nullptr;
#endif
}




int Lv2HostProcess::run()
{
	startUiThread();
	for (;;)
	{
		if (!waitForMessage(30))
		{
			continue;
		}

		const auto m = receiveMessage();
		if (m.id == IdQuit || m.id == IdUndefined)
		{
			break;
		}
		processMessage(m);
	}

	stopUiThread();
	stopAudioThread();
	return 0;
}




bool Lv2HostProcess::waitForMessage(int timeoutMs)
{
#ifdef SYNC_WITH_SHM_FIFO
	// The shared memory transport has no non-blocking check; block on the
	// next message and pump the UI between blocks.
	(void)timeoutMs;
	return true;
#else
	if (m_socket == -1)
	{
		return false;
	}
	struct pollfd pollin = { m_socket, POLLIN, 0 };
	return ::poll(&pollin, 1, timeoutMs) > 0;
#endif
}




bool Lv2HostProcess::processMessage(const message& m)
{
	switch (m.id)
	{
		case IdBufferSizeInformation:
			m_frames = static_cast<f_cnt_t>(m.getInt());
			return true;

		case IdSampleRateInformation:
			m_sampleRate = static_cast<sample_rate_t>(m.getInt());
			return true;

		case IdChangeSharedMemoryKey:
			try
			{
				m_audio.attach(m.getString(0));
			}
			catch (const std::exception& error)
			{
				sendFailed(error.what());
				return false;
			}
			if (m_pluginUri != "--echo" && !instantiate())
			{
				return false;
			}
			startAudioThread();
			sendToHost(message(lv2host::IdReady));
			return true;

		case lv2host::IdControlValue:
		{
			const int index = m.getInt(0);
			const float value = m.getFloat(1);
#ifdef LMMS_HAVE_LV2
			if (index >= 0 && static_cast<std::size_t>(index) < m_controlValues.size())
			{
				std::lock_guard<std::mutex> lock(m_stateMutex);
				m_controlValues[index] = value;
			}
#else
			(void)index;
			(void)value;
#endif
			return true;
		}

		case lv2host::IdMidiEvents:
			m_pendingMidi = lv2host::decodeMidiEvents(m.getString(0));
			return true;

		case lv2host::IdSaveState:
		{
			const bool ok = saveState(m.getString(0));
			// Values first, so the host has them when the ack arrives.
			sendControlValues();
			sendToHost(message(lv2host::IdStateSaved).addInt(ok ? 1 : 0));
			return true;
		}

		case lv2host::IdRestoreState:
		{
			const bool ok = restoreState(m.getString(0));
			sendControlValues();
			sendToHost(message(lv2host::IdStateRestored).addInt(ok ? 1 : 0));
			return true;
		}

		case lv2host::IdLoadPreset:
		{
			const bool ok = loadPreset(m.getString(0));
			sendControlValues();
			sendToHost(message(lv2host::IdPresetLoaded).addInt(ok ? 1 : 0));
			return true;
		}

		case lv2host::IdShowUI:
			showUi(m.getFloat(0));
			return true;

		case lv2host::IdHideUI:
			hideUi();
			return true;

		case lv2host::IdDestroyUI:
			destroyUi();
			return true;

		case IdStartProcessing:
			queueAudioBlock();
			return true;

		default:
			return false;
	}
}




void Lv2HostProcess::sendToHost(const message& m)
{
	std::lock_guard<std::mutex> lock(m_sendMutex);
	sendMessage(m);
}




void Lv2HostProcess::startAudioThread()
{
	if (m_audioThread.joinable())
	{
		return;
	}
	m_audioStop = false;
	m_audioThread = std::thread(&Lv2HostProcess::audioThreadMain, this);
}




void Lv2HostProcess::stopAudioThread()
{
	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		m_audioStop = true;
	}
	m_audioCv.notify_all();
	if (m_audioThread.joinable())
	{
		m_audioThread.join();
	}
}




void Lv2HostProcess::queueAudioBlock()
{
	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		m_audioMidi.swap(m_pendingMidi);
		m_audioControlValues = m_controlValues;
		m_audioRequested = true;
	}
	m_audioCv.notify_one();
}




void Lv2HostProcess::audioThreadMain()
{
#ifdef LMMS_BUILD_LINUX
	struct sched_param param = {};
	param.sched_priority = (sched_get_priority_max(SCHED_FIFO)
		+ sched_get_priority_min(SCHED_FIFO)) / 2;
	pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
#endif

	for (;;)
	{
		{
			std::unique_lock<std::mutex> lock(m_stateMutex);
			m_audioCv.wait(lock, [this] {
				return m_audioStop || m_audioRequested;
			});
			if (m_audioStop)
			{
				return;
			}
			m_audioRequested = false;
		}

		{
			std::lock_guard<std::mutex> lock(m_instanceMutex);
			processBlock();
		}
		sendToHost(message(IdProcessingDone));
	}
}




void Lv2HostProcess::syncControlValuesFromAudio()
{
	std::lock_guard<std::mutex> lock(m_stateMutex);
	m_controlValues = m_audioControlValues;
}




void Lv2HostProcess::processBlock()
{
	if (!m_audio || m_frames == 0)
	{
		return;
	}

	if (m_pluginUri == "--echo")
	{
		echoBlock();
		return;
	}

#ifdef LMMS_HAVE_LV2
	if (m_instance == nullptr)
	{
		std::memset(m_audio.get(),
			0, lv2host::AudioSegment::floatCount(static_cast<std::size_t>(m_frames))
				* sizeof(float));
		return;
	}

	buildMidiInput();
	m_worker.run();

	// Plugins with fewer than two output ports leave the remaining planes
	// untouched; start from silence so stale samples are never played.
	const auto frames = static_cast<std::size_t>(m_frames);
	const auto outputOffset = lv2host::AudioSegment::outputOffset(frames);
	std::memset(m_audio.get() + outputOffset, 0,
		(lv2host::AudioSegment::floatCount(frames) - outputOffset) * sizeof(float));

	lilv_instance_run(m_instance, static_cast<uint32_t>(m_frames));
	sendMidiOutput();

	if (m_latencyPort >= 0)
	{
		const int latency = static_cast<int>(std::lround(m_latencyValue));
		if (latency != m_reportedLatency)
		{
			m_reportedLatency = latency;
			sendToHost(message(lv2host::IdLatency).addInt(latency));
		}
	}
#endif // LMMS_HAVE_LV2
}




void Lv2HostProcess::echoBlock()
{
	const auto frames = static_cast<std::size_t>(m_frames);
	const float* input = m_audio.get() + lv2host::AudioSegment::inputOffset();
	float* output = m_audio.get() + lv2host::AudioSegment::outputOffset(frames);
	std::memcpy(output, input,
		lv2host::AudioSegment::channels * frames * sizeof(float));
}




void Lv2HostProcess::buildMidiInput()
{
	if (m_midiInPort < 0)
	{
		m_audioMidi.clear();
		return;
	}

	auto& buffer = m_atomBuffers[m_midiInPort];
	auto* sequence = reinterpret_cast<LV2_Atom_Sequence*>(buffer.data());
	sequence->atom.type = m_urids.map(LV2_ATOM__Sequence);
	sequence->atom.size = sizeof(LV2_Atom_Sequence_Body);
	sequence->body.unit = 0;
	sequence->body.pad = 0;

	uint8_t* out = buffer.data() + sizeof(LV2_Atom) + sizeof(LV2_Atom_Sequence_Body);
	const uint8_t* const end = buffer.data() + buffer.size();
	const LV2_URID midiType = m_urids.map(LV2_MIDI__MidiEvent);

	for (const auto& event : m_audioMidi)
	{
		const std::size_t total = sizeof(LV2_Atom_Event) + event.data.size();
		if (out + total > end)
		{
			break;
		}
		auto* atomEvent = reinterpret_cast<LV2_Atom_Event*>(out);
		atomEvent->time.frames = static_cast<int64_t>(event.frame);
		atomEvent->body.size = static_cast<uint32_t>(event.data.size());
		atomEvent->body.type = midiType;
		std::memcpy(out + sizeof(LV2_Atom_Event),
			event.data.data(), event.data.size());
		out += total;
		sequence->atom.size += static_cast<uint32_t>(total);
	}
	m_audioMidi.clear();
}




void Lv2HostProcess::sendMidiOutput()
{
	if (m_midiOutPort < 0)
	{
		return;
	}

	auto& buffer = m_atomBuffers[m_midiOutPort];
	auto* sequence = reinterpret_cast<LV2_Atom_Sequence*>(buffer.data());
	const LV2_URID sequenceType = m_urids.map(LV2_ATOM__Sequence);

	std::vector<lv2host::MidiEventData> events;
	if (sequence->atom.type == sequenceType
		&& sequence->atom.size > sizeof(LV2_Atom_Sequence_Body))
	{
		const uint8_t* cursor =
			buffer.data() + sizeof(LV2_Atom) + sizeof(LV2_Atom_Sequence_Body);
		const uint8_t* const end = cursor
			+ (sequence->atom.size - sizeof(LV2_Atom_Sequence_Body));
		while (cursor + sizeof(LV2_Atom_Event) <= end)
		{
			const auto* atomEvent = reinterpret_cast<const LV2_Atom_Event*>(cursor);
			const std::size_t total = sizeof(LV2_Atom_Event) + atomEvent->body.size;
			if (cursor + total > end)
			{
				break;
			}
			events.push_back(lv2host::MidiEventData{
				static_cast<uint32_t>(atomEvent->time.frames),
				std::vector<uint8_t>(cursor + sizeof(LV2_Atom_Event), cursor + total)});
			cursor += total;
		}
	}
	if (!events.empty())
	{
		sendToHost(message(lv2host::IdMidiOut)
			.addString(lv2host::encodeMidiEvents(events)));
	}

	sequence->atom.type = sequenceType;
	sequence->atom.size = sizeof(LV2_Atom_Sequence_Body);
	sequence->body.unit = 0;
	sequence->body.pad = 0;
}




std::string Lv2HostProcess::ensureStateDir()
{
	if (m_stateDirPath.empty())
	{
		std::error_code error;
		const auto base = std::filesystem::temp_directory_path(error);
		if (!error)
		{
			const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
			for (int attempt = 0; attempt < 100 && m_stateDirPath.empty(); ++attempt)
			{
				const auto candidate = base / ("lmms-lv2host-state-"
					+ std::to_string(nonce) + "-" + std::to_string(attempt));
				if (std::filesystem::create_directory(candidate, error))
				{
					m_stateDirPath = candidate.string();
				}
				else if (error)
				{
					break;
				}
			}
		}
	}
	return m_stateDirPath;
}




char* Lv2HostProcess::makePathCb(LV2_State_Make_Path_Handle handle, const char* path)
{
	auto* self = static_cast<Lv2HostProcess*>(handle);
	if (self == nullptr || path == nullptr || *path == '\0')
	{
		return nullptr;
	}
	const std::string root = self->ensureStateDir();
	if (root.empty())
	{
		return nullptr;
	}
	const std::filesystem::path full = std::filesystem::path(root) / path;
	std::error_code error;
	std::filesystem::create_directories(full.parent_path(), error);
	return ::strdup(full.string().c_str());
}




char* Lv2HostProcess::mapPathCb(LV2_State_Map_Path_Handle handle, const char* path)
{
	auto* self = static_cast<Lv2HostProcess*>(handle);
	if (self == nullptr || path == nullptr || *path == '\0')
	{
		return nullptr;
	}
	const std::filesystem::path input(path);
	if (input.is_absolute())
	{
		return ::strdup(path);
	}
	const std::string root = self->m_stateDirPath.empty()
		? self->ensureStateDir() : self->m_stateDirPath;
	if (root.empty())
	{
		return nullptr;
	}
	return ::strdup((std::filesystem::path(root) / input).string().c_str());
}




void Lv2HostProcess::freePathCb(LV2_State_Free_Path_Handle, char* path)
{
	std::free(path);
}




void Lv2HostProcess::sendControlValues()
{
	std::vector<float> values;
	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		values = m_controlValues;
	}
	sendToHost(message(lv2host::IdControlValues).addString(
		lv2host::encodeControlValues(values)));
}




bool Lv2HostProcess::saveState(const std::string& directory)
{
	std::lock_guard<std::mutex> instanceLock(m_instanceMutex);
#ifdef LMMS_HAVE_LV2
	if (m_instance == nullptr || m_plugin == nullptr || directory.empty())
	{
		return false;
	}
	const std::string scratch = ensureStateDir();
	// State saving reads the control ports connected to the instance. The
	// audio thread copies the host values at block start, but a parameter
	// change may not have reached a block yet; publish the current host
	// values before snapshotting.
	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		m_audioControlValues = m_controlValues;
	}
	LilvState* state = lilv_state_new_from_instance(m_plugin, m_instance,
		m_urids.mapFeature(), scratch.c_str(), directory.c_str(),
		directory.c_str(), directory.c_str(), nullptr, nullptr, 0, nullptr);
	if (state == nullptr)
	{
		return false;
	}
	const int result = lilv_state_save(m_world, m_urids.mapFeature(),
		m_urids.unmapFeature(), state, nullptr, directory.c_str(), "state.ttl");
	lilv_state_free(state);
	return result == 0;
#else
	(void)directory;
	return false;
#endif
}




bool Lv2HostProcess::restoreState(const std::string& directory)
{
	std::lock_guard<std::mutex> instanceLock(m_instanceMutex);
#ifdef LMMS_HAVE_LV2
	if (m_instance == nullptr || directory.empty())
	{
		return false;
	}
	const std::string path =
		(std::filesystem::path(directory) / "state.ttl").string();
	LilvState* state = lilv_state_new_from_file(m_world,
		m_urids.mapFeature(), nullptr, path.c_str());
	if (state == nullptr)
	{
		return false;
	}

	m_stateDirPath = directory;
	const LV2_Feature* features[] = {
		&m_stateMakePathFeature,
		&m_stateMapPathFeature,
		&m_stateFreePathFeature,
		nullptr
	};
	lilv_state_restore(state, m_instance, nullptr, nullptr, 0, features);
	lilv_state_free(state);
	syncControlValuesFromAudio();
	return true;
#else
	(void)directory;
	return false;
#endif
}




bool Lv2HostProcess::loadPreset(const std::string& presetUri)
{
	std::lock_guard<std::mutex> instanceLock(m_instanceMutex);
#ifdef LMMS_HAVE_LV2
	if (m_instance == nullptr || presetUri.empty())
	{
		return false;
	}
	LilvNode* node = lilv_new_uri(m_world, presetUri.c_str());
	if (node == nullptr)
	{
		return false;
	}
	lilv_world_load_resource(m_world, node);
	LilvState* state = lilv_state_new_from_world(m_world,
		m_urids.mapFeature(), node);
	lilv_node_free(node);
	if (state == nullptr)
	{
		return false;
	}
	const LV2_Feature* features[] = {
		&m_stateMakePathFeature,
		&m_stateMapPathFeature,
		&m_stateFreePathFeature,
		nullptr
	};
	lilv_state_restore(state, m_instance, nullptr, nullptr, 0, features);
	lilv_state_free(state);
	syncControlValuesFromAudio();
	return true;
#else
	(void)presetUri;
	return false;
#endif
}




void Lv2HostProcess::sendFailed(const std::string& reason)
{
	std::fprintf(stderr, "lv2 host: %s\n", reason.c_str());
	sendToHost(message(lv2host::IdFailed).addString(reason));
}




void Lv2HostProcess::hideUi()
{
	if (m_uiThread.joinable()
		&& std::this_thread::get_id() != m_uiThread.get_id())
	{
		{
			std::lock_guard<std::mutex> lock(m_uiMutex);
			m_uiRequests.push_back({ UiRequest::Hide });
		}
		m_uiCv.notify_one();
		return;
	}
	hideUiNow();
}




void Lv2HostProcess::hideUiNow()
{
#if defined(LMMS_BUILD_LINUX) && defined(LMMS_HAVE_SUIL)
	if (m_uiWindow != 0 && m_uiDisplay != nullptr)
	{
		XUnmapWindow(m_uiDisplay, m_uiWindow);
		XFlush(m_uiDisplay);
	}
#endif
	m_uiVisible = false;
}




void Lv2HostProcess::destroyUi()
{
	if (m_uiThread.joinable()
		&& std::this_thread::get_id() != m_uiThread.get_id())
	{
		{
			std::lock_guard<std::mutex> lock(m_uiMutex);
			m_uiRequests.push_back({ UiRequest::Destroy });
		}
		m_uiCv.notify_one();
		return;
	}
	destroyUiNow();
}




void Lv2HostProcess::destroyUiNow()
{
#if defined(LMMS_BUILD_LINUX) && defined(LMMS_HAVE_SUIL)
	if (m_uiInstance != nullptr)
	{
		const auto* show = static_cast<const LV2UI_Show_Interface*>(
			suil_instance_extension_data(m_uiInstance, LV2_UI__showInterface));
		if (show != nullptr && show->hide != nullptr)
		{
			show->hide(suil_instance_get_handle(m_uiInstance));
		}
		suil_instance_free(m_uiInstance);
		m_uiInstance = nullptr;
		m_lastUiControl.clear();
	}
	if (m_uiHost != nullptr)
	{
		suil_host_free(m_uiHost);
		m_uiHost = nullptr;
	}
	if (m_uiWindow != 0 && m_uiDisplay != nullptr)
	{
		XDestroyWindow(m_uiDisplay, m_uiWindow);
		m_uiWindow = 0;
	}
	if (m_uiDisplay != nullptr)
	{
		XCloseDisplay(m_uiDisplay);
		m_uiDisplay = nullptr;
	}
	g_extDataInstance = nullptr;
#endif
	m_uiVisible = false;
}




void Lv2HostProcess::processUiEvents()
{
#if defined(LMMS_BUILD_LINUX) && defined(LMMS_HAVE_SUIL)
	if (m_uiDisplay == nullptr || m_uiWindow == 0)
	{
		return;
	}
	while (XPending(m_uiDisplay) > 0)
	{
		XEvent event;
		XNextEvent(m_uiDisplay, &event);
		if (event.type == ClientMessage
			&& event.xclient.window == m_uiWindow
			&& static_cast<Atom>(event.xclient.data.l[0]) == m_uiWmDelete)
		{
			destroyUi();
			sendToHost(message(lv2host::IdUiClosed).addString("closed by user"));
			return;
		}
		if (event.type == DestroyNotify
			&& event.xdestroywindow.window == m_uiWindow)
		{
			m_uiWindow = 0;
		}
	}
#endif
}




void Lv2HostProcess::idleUi()
{
	processUiEvents();
#if defined(LMMS_BUILD_LINUX) && defined(LMMS_HAVE_SUIL) && defined(LMMS_HAVE_LV2)
	if (m_uiInstance == nullptr || !m_uiVisible)
	{
		return;
	}
	const CurrentDirGuard cwdGuard(m_uiDataRoot);

	// Keep the UI in sync with the values the DSP currently uses.
	std::vector<float> controlValues;
	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		controlValues = m_controlValues;
	}
	for (std::size_t i = 0; i < controlValues.size(); ++i)
	{
		const float value = controlValues[i];
		if (i >= m_lastUiControl.size() || m_lastUiControl[i] != value)
		{
			suil_instance_port_event(m_uiInstance, static_cast<uint32_t>(i),
				sizeof(float), 0, &value);
			if (i >= m_lastUiControl.size())
			{
				m_lastUiControl.resize(i + 1);
			}
			m_lastUiControl[i] = value;
		}
	}

	const auto* idle = static_cast<const LV2UI_Idle_Interface*>(
		suil_instance_extension_data(m_uiInstance, LV2_UI__idleInterface));
	if (idle != nullptr && idle->idle != nullptr)
	{
		// Zyn Fusion loads QML and helper libraries lazily from its asset
		// root, so the idle calls need the same working directory.
		idle->idle(suil_instance_get_handle(m_uiInstance));
	}
#endif
}




#if defined(LMMS_BUILD_LINUX) && defined(LMMS_HAVE_SUIL)
void Lv2HostProcess::writeToUiPort(void* handle, uint32_t port, uint32_t size,
	uint32_t protocol, const void* buffer)
{
	auto* self = static_cast<Lv2HostProcess*>(handle);
	if (buffer == nullptr || protocol != 0 || size != sizeof(float))
	{
		return;
	}
#ifdef LMMS_HAVE_LV2
	if (port < self->m_controlValues.size())
	{
		std::lock_guard<std::mutex> lock(self->m_stateMutex);
		self->m_controlValues[port] = *static_cast<const float*>(buffer);
	}
#endif
}




uint32_t Lv2HostProcess::portIndexBySymbol(void* handle, const char* symbol)
{
#ifdef LMMS_HAVE_LV2
	auto* self = static_cast<Lv2HostProcess*>(handle);
	if (self->m_plugin == nullptr || symbol == nullptr)
	{
		return LV2UI_INVALID_PORT_INDEX;
	}
	const uint32_t portCount = lilv_plugin_get_num_ports(self->m_plugin);
	for (uint32_t i = 0; i < portCount; ++i)
	{
		const LilvPort* port = lilv_plugin_get_port_by_index(self->m_plugin, i);
		const LilvNode* node = lilv_port_get_symbol(self->m_plugin, port);
		if (node != nullptr && std::strcmp(lilv_node_as_string(node), symbol) == 0)
		{
			return i;
		}
	}
#endif
	(void)handle;
	(void)symbol;
	return LV2UI_INVALID_PORT_INDEX;
}




void Lv2HostProcess::touchUiPort(void* handle, uint32_t port, bool grabbed)
{
	(void)handle;
	(void)port;
	(void)grabbed;
}
#endif




void Lv2HostProcess::startUiThread()
{
	if (m_uiThread.joinable())
	{
		return;
	}
	m_uiThread = std::thread(&Lv2HostProcess::uiThreadMain, this);
}




void Lv2HostProcess::stopUiThread()
{
	if (!m_uiThread.joinable())
	{
		return;
	}
	{
		std::lock_guard<std::mutex> lock(m_uiMutex);
		m_uiRequests.clear();
		m_uiRequests.push_back({ UiRequest::Quit });
	}
	m_uiCv.notify_all();
	m_uiThread.join();
}




void Lv2HostProcess::uiThreadMain()
{
	for (;;)
	{
		UiRequestItem request{ UiRequest::Idle };
		{
			std::unique_lock<std::mutex> lock(m_uiMutex);
			m_uiCv.wait_for(lock, std::chrono::milliseconds(30),
				[this] { return !m_uiRequests.empty(); });
			if (!m_uiRequests.empty())
			{
				request = m_uiRequests.front();
				m_uiRequests.pop_front();
			}
		}

		if (request.type == UiRequest::Quit)
		{
			destroyUiNow();
			return;
		}
		if (request.type == UiRequest::Show) { showUiNow(request.scale); }
		else if (request.type == UiRequest::Hide) { hideUiNow(); }
		else if (request.type == UiRequest::Destroy) { destroyUiNow(); }

		idleUi();
	}
}




bool Lv2HostProcess::showUi(float scale)
{
	if (m_uiThread.joinable()
		&& std::this_thread::get_id() != m_uiThread.get_id())
	{
		{
			std::lock_guard<std::mutex> lock(m_uiMutex);
			m_uiRequests.push_back({ UiRequest::Show, scale });
		}
		m_uiCv.notify_one();
		return true;
	}
	return showUiNow(scale);
}




bool Lv2HostProcess::showUiNow(float scale)
{
#if defined(LMMS_BUILD_LINUX) && defined(LMMS_HAVE_SUIL)
#ifdef LMMS_HAVE_LV2
	if (m_instance == nullptr || m_plugin == nullptr)
	{
		sendToHost(message(lv2host::IdUiClosed).addString("no plugin instance"));
		return false;
	}
	if (m_uiInstance != nullptr)
	{
		if (m_uiWindow != 0 && m_uiDisplay != nullptr)
		{
			if (!m_uiVisible) { XMapWindow(m_uiDisplay, m_uiWindow); }
			XRaiseWindow(m_uiDisplay, m_uiWindow);
			XFlush(m_uiDisplay);
			m_uiVisible = true;
		}
		return true;
	}

	if (m_uiDisplay == nullptr)
	{
		m_uiDisplay = XOpenDisplay(nullptr);
		if (m_uiDisplay == nullptr)
		{
			sendToHost(message(lv2host::IdUiClosed).addString("no X11 display"));
			return false;
		}
	}

	std::string title = "LV2 UI";
	{
		LilvNode* name = lilv_plugin_get_name(m_plugin);
		if (name != nullptr)
		{
			title = lilv_node_as_string(name);
			lilv_node_free(name);
		}
	}

	const int screen = DefaultScreen(m_uiDisplay);
	m_uiWindow = XCreateSimpleWindow(m_uiDisplay, RootWindow(m_uiDisplay, screen),
		100, 100, 900, 700, 1,
		BlackPixel(m_uiDisplay, screen), WhitePixel(m_uiDisplay, screen));
	if (m_uiWindow == 0)
	{
		sendToHost(message(lv2host::IdUiClosed).addString("could not create a window"));
		return false;
	}
	XStoreName(m_uiDisplay, m_uiWindow, title.c_str());
	m_uiWmDelete = XInternAtom(m_uiDisplay, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(m_uiDisplay, m_uiWindow, &m_uiWmDelete, 1);
	XSelectInput(m_uiDisplay, m_uiWindow, StructureNotifyMask);
	XMapWindow(m_uiDisplay, m_uiWindow);
	XFlush(m_uiDisplay);

	m_uiHost = suil_host_new(writeToUiPort, portIndexBySymbol, nullptr, nullptr);
	if (m_uiHost == nullptr)
	{
		destroyUi();
		sendToHost(message(lv2host::IdUiClosed).addString("could not create the UI host"));
		return false;
	}
	suil_host_set_touch_func(m_uiHost, touchUiPort);

	LilvNode* x11Type = lilv_new_uri(m_world, LV2_UI__X11UI);
	const LilvUI* selected = nullptr;
	const LilvNode* uiType = nullptr;
	LilvUIs* uis = lilv_plugin_get_uis(m_plugin);
	LILV_FOREACH(uis, it, uis)
	{
		const LilvUI* candidate = lilv_uis_get(uis, it);
		const LilvNode* candidateType = nullptr;
		if (lilv_ui_is_supported(candidate, suil_ui_supported, x11Type, &candidateType))
		{
			selected = candidate;
			uiType = candidateType;
			break;
		}
	}
	lilv_node_free(x11Type);
	if (selected == nullptr)
	{
		destroyUi();
		sendToHost(message(lv2host::IdUiClosed).addString("no X11 UI available"));
		return false;
	}

	m_uiExtData.data_access = extDataAccess;
	g_extDataInstance = m_instance;
	m_uiParentFeature = LV2_Feature{
		LV2_UI__parent,
		reinterpret_cast<void*>(static_cast<uintptr_t>(m_uiWindow))
	};
	m_uiInstanceFeature = LV2_Feature{
		LV2_INSTANCE_ACCESS_URI,
		const_cast<void*>(lilv_instance_get_handle(m_instance))
	};
	m_uiDataFeature = LV2_Feature{LV2_DATA_ACCESS_URI, &m_uiExtData};
	m_uiResize.handle = this;
	m_uiResize.ui_resize = [](LV2UI_Handle handle, int width, int height) -> int {
		auto* self = static_cast<Lv2HostProcess*>(handle);
		if (width <= 0 || height <= 0)
		{
			return 1;
		}
		self->m_pendingUiWidth = width;
		self->m_pendingUiHeight = height;
		if (self->m_uiDisplay != nullptr && self->m_uiWindow != 0)
		{
			XResizeWindow(self->m_uiDisplay, self->m_uiWindow,
				static_cast<unsigned>(width), static_cast<unsigned>(height));
			XFlush(self->m_uiDisplay);
		}
		return 0;
	};
	m_uiResizeFeature = LV2_Feature{LV2_UI__resize, &m_uiResize};

	const LV2_URID floatType = m_urids.map(LV2_ATOM__Float);
	m_uiScale = scale > 0.0f ? scale : 1.0f;
	m_uiSampleRate = static_cast<float>(m_sampleRate);
	m_uiOptions[0] = LV2_Options_Option{
		LV2_OPTIONS_INSTANCE, 0,
		m_urids.map(LV2_UI__scaleFactor),
		sizeof(float), floatType, &m_uiScale
	};
	m_uiOptions[1] = LV2_Options_Option{
		LV2_OPTIONS_INSTANCE, 0,
		m_urids.map(LV2_PARAMETERS__sampleRate),
		sizeof(float), floatType, &m_uiSampleRate
	};
	m_uiOptions[2] = LV2_Options_Option{LV2_OPTIONS_INSTANCE, 0, 0, 0, 0, nullptr};
	m_uiOptionsFeature = LV2_Feature{LV2_OPTIONS__options, m_uiOptions};
	m_uiLogFeature = LV2_Feature{LV2_LOG__log, &m_log};

	const LV2_Feature* features[] = {
		&m_uiParentFeature,
		&m_uiInstanceFeature,
		&m_uiDataFeature,
		&m_uiResizeFeature,
		&m_uiOptionsFeature,
		&m_uiLogFeature,
		&m_mapFeature,
		&m_unmapFeature,
		nullptr
	};

	char* bundlePath = lilv_file_uri_parse(
		lilv_node_as_uri(lilv_ui_get_bundle_uri(selected)), nullptr);
	char* binaryPath = lilv_file_uri_parse(
		lilv_node_as_uri(lilv_ui_get_binary_uri(selected)), nullptr);

	// Zyn Fusion resolves qml/ and libzest.so relative to the working dir.
	m_uiDataRoot = findUiDataRoot(bundlePath != nullptr ? bundlePath : "");
	m_lastUiControl.clear();
	const CurrentDirGuard cwdGuard(m_uiDataRoot);

	m_uiInstance = suil_instance_new(m_uiHost, this, LV2_UI__X11UI,
		m_pluginUri.c_str(),
		lilv_node_as_uri(lilv_ui_get_uri(selected)),
		uiType != nullptr ? lilv_node_as_uri(uiType) : LV2_UI__X11UI,
		bundlePath, binaryPath, features);
	lilv_free(bundlePath);
	lilv_free(binaryPath);

	if (m_uiInstance == nullptr)
	{
		destroyUi();
		sendToHost(message(lv2host::IdUiClosed).addString("could not instantiate the UI"));
		return false;
	}

	// Some UIs (DrumGizmo) request their natural size from inside
	// suil_instance_new(), before the widget has that size. Prefer that
	// request and fall back to the widget size.
	const auto widget = reinterpret_cast<uintptr_t>(suil_instance_get_widget(m_uiInstance));
	int uiWidth = m_pendingUiWidth;
	int uiHeight = m_pendingUiHeight;
	XWindowAttributes attributes{};
	if ((uiWidth <= 1 || uiHeight <= 1)
		&& widget != 0
		&& XGetWindowAttributes(m_uiDisplay, static_cast<Window>(widget), &attributes)
		&& attributes.width > 1 && attributes.height > 1)
	{
		uiWidth = attributes.width;
		uiHeight = attributes.height;
	}
	if (uiWidth > 1 && uiHeight > 1)
	{
		XResizeWindow(m_uiDisplay, m_uiWindow,
			static_cast<unsigned>(uiWidth), static_cast<unsigned>(uiHeight));
		if (widget != 0)
		{
			// Make the plugin widget match its window; some UIs only repaint
			// at the size they asked for.
			XResizeWindow(m_uiDisplay, static_cast<Window>(widget),
				static_cast<unsigned>(uiWidth), static_cast<unsigned>(uiHeight));
		}
		XFlush(m_uiDisplay);
	}

	const auto* show = static_cast<const LV2UI_Show_Interface*>(
		suil_instance_extension_data(m_uiInstance, LV2_UI__showInterface));
	if (show != nullptr && show->show != nullptr)
	{
		show->show(suil_instance_get_handle(m_uiInstance));
	}
	if (m_uiWindow != 0 && m_uiDisplay != nullptr)
	{
		XRaiseWindow(m_uiDisplay, m_uiWindow);
		XFlush(m_uiDisplay);
	}

	std::fprintf(stderr, "lv2 host: UI created (window=%lu widget=%p size=%dx%d)\n",
		m_uiWindow, reinterpret_cast<void*>(widget), uiWidth, uiHeight);
	m_uiVisible = true;
	return true;
#else
	(void)scale;
	sendToHost(message(lv2host::IdUiClosed).addString("LV2 UI support missing"));
	return false;
#endif // LMMS_HAVE_LV2
#else
	(void)scale;
	sendToHost(message(lv2host::IdUiClosed).addString("LV2 UI is only supported on X11"));
	return false;
#endif
}




#ifdef LMMS_HAVE_LV2
bool Lv2HostProcess::instantiate()
{
	if (m_instance != nullptr)
	{
		return true;
	}
	if (m_frames == 0 || m_sampleRate == 0)
	{
		sendFailed("sample rate or block size missing");
		return false;
	}

	m_world = lilv_world_new();
	lilv_world_load_all(m_world);

	LilvNode* uri = lilv_new_uri(m_world, m_pluginUri.c_str());
	const LilvPlugins* plugins = lilv_world_get_all_plugins(m_world);
	const LilvPlugin* plugin = nullptr;
	// lilv_plugins_get_by_uri crashes on an empty plugin set in lilv 0.28,
	// so look the plugin up by iterating.
	LILV_FOREACH(plugins, it, plugins)
	{
		const LilvPlugin* candidate = lilv_plugins_get(plugins, it);
		if (lilv_node_equals(lilv_plugin_get_uri(candidate), uri))
		{
			plugin = candidate;
			break;
		}
	}
	lilv_node_free(uri);
	if (plugin == nullptr)
	{
		sendFailed("plugin '" + m_pluginUri + "' not found");
		return false;
	}
	m_plugin = plugin;

	const LV2_URID floatType = m_urids.map(LV2_ATOM__Float);
	const LV2_URID intType = m_urids.map(LV2_ATOM__Int);
	m_optionSampleRate = static_cast<float>(m_sampleRate);
	m_optionMinBlock = static_cast<int32_t>(m_frames);
	m_optionMaxBlock = static_cast<int32_t>(m_frames);
	m_options[0] = LV2_Options_Option{
		LV2_OPTIONS_INSTANCE, 0,
		m_urids.map(LV2_PARAMETERS__sampleRate),
		sizeof(float), floatType, &m_optionSampleRate
	};
	m_options[1] = LV2_Options_Option{
		LV2_OPTIONS_INSTANCE, 0,
		m_urids.map(LV2_BUF_SIZE__minBlockLength),
		sizeof(int32_t), intType, &m_optionMinBlock
	};
	m_options[2] = LV2_Options_Option{
		LV2_OPTIONS_INSTANCE, 0,
		m_urids.map(LV2_BUF_SIZE__maxBlockLength),
		sizeof(int32_t), intType, &m_optionMaxBlock
	};
	m_options[3] = LV2_Options_Option{
		LV2_OPTIONS_INSTANCE, 0,
		m_urids.map(LV2_BUF_SIZE__nominalBlockLength),
		sizeof(int32_t), intType, &m_optionMaxBlock
	};
	m_options[4] = LV2_Options_Option{
		LV2_OPTIONS_INSTANCE, 0,
		m_urids.map(LV2_BUF_SIZE__sequenceSize),
		sizeof(int32_t), intType, &m_optionSequenceSize
	};
	m_options[5] = LV2_Options_Option{LV2_OPTIONS_INSTANCE, 0, 0, 0, 0, nullptr};

	m_mapFeature = LV2_Feature{LV2_URID__map, m_urids.mapFeature()};
	m_unmapFeature = LV2_Feature{LV2_URID__unmap, m_urids.unmapFeature()};
	m_optionsFeature = LV2_Feature{LV2_OPTIONS__options, m_options};
	m_boundedFeature = LV2_Feature{LV2_BUF_SIZE__boundedBlockLength, nullptr};
	m_log.handle = this;
	m_log.printf = logPrintf;
	m_log.vprintf = logVPrintf;
	m_logFeature = LV2_Feature{LV2_LOG__log, &m_log};
	m_schedule = m_worker.scheduleFeature();
	m_workerFeature = LV2_Feature{LV2_WORKER__schedule, m_schedule};

	m_stateMakePath.handle = this;
	m_stateMakePath.path = makePathCb;
	m_stateMapPath.handle = this;
	m_stateMapPath.abstract_path = nullptr;
	m_stateMapPath.absolute_path = mapPathCb;
	m_stateFreePath.handle = this;
	m_stateFreePath.free_path = freePathCb;
	m_stateMakePathFeature = LV2_Feature{LV2_STATE__makePath, &m_stateMakePath};
	m_stateMapPathFeature = LV2_Feature{LV2_STATE__mapPath, &m_stateMapPath};
	m_stateFreePathFeature = LV2_Feature{LV2_STATE__freePath, &m_stateFreePath};
	ensureStateDir();

	const LV2_Feature* features[] = {
		&m_mapFeature,
		&m_unmapFeature,
		&m_optionsFeature,
		&m_boundedFeature,
		&m_logFeature,
		&m_workerFeature,
		&m_stateMakePathFeature,
		&m_stateMapPathFeature,
		&m_stateFreePathFeature,
		nullptr
	};

	m_instance = lilv_plugin_instantiate(plugin, m_sampleRate, features);
	if (m_instance == nullptr)
	{
		sendFailed("lilv_plugin_instantiate failed");
		return false;
	}

	m_worker.setInterface(
		static_cast<const LV2_Worker_Interface*>(
			lilv_instance_get_extension_data(m_instance, LV2_WORKER__interface)),
		lilv_instance_get_handle(m_instance));

	connectPorts();
	sendPortInfo();
	lilv_instance_activate(m_instance);
	return true;
}




void Lv2HostProcess::connectPorts()
{
	const uint32_t portCount = lilv_plugin_get_num_ports(m_plugin);
	LilvNode* audioClass = lilv_new_uri(m_world, LV2_CORE__AudioPort);
	LilvNode* controlClass = lilv_new_uri(m_world, LV2_CORE__ControlPort);
	LilvNode* atomClass = lilv_new_uri(m_world, LV2_ATOM__AtomPort);
	LilvNode* inputClass = lilv_new_uri(m_world, LV2_CORE__InputPort);
	LilvNode* outputClass = lilv_new_uri(m_world, LV2_CORE__OutputPort);

	const auto frames = static_cast<std::size_t>(m_frames);
	m_controlValues.assign(portCount, 0.0f);
	m_audioControlValues.assign(portCount, 0.0f);
	m_portScratch.assign(portCount, 0.0f);
	m_atomBuffers.resize(portCount);

	float* const inputPlanes = m_audio.get() + lv2host::AudioSegment::inputOffset();
	float* const outputPlanes = m_audio.get() + lv2host::AudioSegment::outputOffset(frames);
	int inputPlane = 0;
	int outputPlane = 0;
	std::size_t scratchPlane = 0;

	// Count the audio ports that need a private buffer.
	std::size_t scratchLength = 0;
	for (uint32_t i = 0; i < portCount; ++i)
	{
		const LilvPort* port = lilv_plugin_get_port_by_index(m_plugin, i);
		if (lilv_port_is_a(m_plugin, port, audioClass))
		{
			const bool isInput = lilv_port_is_a(m_plugin, port, inputClass);
			if ((isInput && inputPlane >= lv2host::AudioSegment::channels)
				|| (!isInput && outputPlane >= lv2host::AudioSegment::channels))
			{
				++scratchLength;
			}
			else if (isInput)
			{
				++inputPlane;
			}
			else
			{
				++outputPlane;
			}
		}
	}
	m_audioScratch.assign(scratchLength * frames, 0.0f);
	inputPlane = 0;
	outputPlane = 0;
	scratchPlane = 0;

	for (uint32_t i = 0; i < portCount; ++i)
	{
		const LilvPort* port = lilv_plugin_get_port_by_index(m_plugin, i);
		const bool isInput = lilv_port_is_a(m_plugin, port, inputClass);

		if (lilv_port_is_a(m_plugin, port, controlClass))
		{
			LilvNode* def = nullptr;
			LilvNode* min = nullptr;
			LilvNode* max = nullptr;
			lilv_port_get_range(m_plugin, port, &def, &min, &max);
			m_controlValues[i] = def != nullptr ? lilv_node_as_float(def) : 0.0f;
			m_audioControlValues[i] = m_controlValues[i];
			if (def != nullptr) { lilv_node_free(def); }
			if (min != nullptr) { lilv_node_free(min); }
			if (max != nullptr) { lilv_node_free(max); }
			lilv_instance_connect_port(m_instance, i, &m_audioControlValues[i]);
		}
		else if (lilv_port_is_a(m_plugin, port, audioClass))
		{
			if (isInput && inputPlane < lv2host::AudioSegment::channels)
			{
				lilv_instance_connect_port(m_instance, i,
					inputPlanes + static_cast<std::size_t>(inputPlane++) * frames);
			}
			else if (!isInput && outputPlane < lv2host::AudioSegment::channels)
			{
				lilv_instance_connect_port(m_instance, i,
					outputPlanes + static_cast<std::size_t>(outputPlane++) * frames);
			}
			else
			{
				lilv_instance_connect_port(m_instance, i,
					m_audioScratch.data() + scratchPlane++ * frames);
			}
		}
		else if (lilv_port_is_a(m_plugin, port, atomClass))
		{
			m_atomBuffers[i].assign(8192, 0);
			auto* sequence = reinterpret_cast<LV2_Atom_Sequence*>(m_atomBuffers[i].data());
			sequence->atom.type = m_urids.map(LV2_ATOM__Sequence);
			sequence->atom.size = sizeof(LV2_Atom_Sequence_Body);
			sequence->body.unit = 0;
			sequence->body.pad = 0;
			lilv_instance_connect_port(m_instance, i, m_atomBuffers[i].data());
			if (isInput && m_midiInPort < 0)
			{
				m_midiInPort = static_cast<int>(i);
			}
			else if (!isInput && m_midiOutPort < 0)
			{
				m_midiOutPort = static_cast<int>(i);
			}
		}
		else
		{
			lilv_instance_connect_port(m_instance, i, &m_portScratch[i]);
		}
	}

	// A designated latency output port reports the plugin latency.
	LilvNode* latency = lilv_new_uri(m_world, LV2_CORE__latency);
	const LilvPort* latencyPort =
		lilv_plugin_get_port_by_designation(m_plugin, outputClass, latency);
	lilv_node_free(latency);
	if (latencyPort != nullptr)
	{
		m_latencyPort = static_cast<int>(lilv_port_get_index(m_plugin, latencyPort));
		lilv_instance_connect_port(m_instance,
			static_cast<uint32_t>(m_latencyPort), &m_latencyValue);
	}

	lilv_node_free(audioClass);
	lilv_node_free(controlClass);
	lilv_node_free(atomClass);
	lilv_node_free(inputClass);
	lilv_node_free(outputClass);
}




void Lv2HostProcess::sendPortInfo()
{
	LilvNode* audioClass = lilv_new_uri(m_world, LV2_CORE__AudioPort);
	LilvNode* controlClass = lilv_new_uri(m_world, LV2_CORE__ControlPort);
	LilvNode* atomClass = lilv_new_uri(m_world, LV2_ATOM__AtomPort);
	LilvNode* inputClass = lilv_new_uri(m_world, LV2_CORE__InputPort);

	const uint32_t portCount = lilv_plugin_get_num_ports(m_plugin);
	for (uint32_t i = 0; i < portCount; ++i)
	{
		const LilvPort* port = lilv_plugin_get_port_by_index(m_plugin, i);
		const LilvNode* symbol = lilv_port_get_symbol(m_plugin, port);
		auto name = std::unique_ptr<LilvNode, void (*)(LilvNode*)>(
			lilv_port_get_name(m_plugin, port), lilv_node_free);

		std::string kind = "other";
		if (lilv_port_is_a(m_plugin, port, audioClass))
		{
			kind = "audio";
		}
		else if (lilv_port_is_a(m_plugin, port, controlClass))
		{
			kind = "control";
		}
		else if (lilv_port_is_a(m_plugin, port, atomClass))
		{
			kind = "atom";
		}

		float def = 0.0f;
		float min = 0.0f;
		float max = 0.0f;
		if (kind == "control")
		{
			LilvNode* defNode = nullptr;
			LilvNode* minNode = nullptr;
			LilvNode* maxNode = nullptr;
			lilv_port_get_range(m_plugin, port, &defNode, &minNode, &maxNode);
			if (defNode != nullptr) { def = lilv_node_as_float(defNode); }
			if (minNode != nullptr) { min = lilv_node_as_float(minNode); }
			if (maxNode != nullptr) { max = lilv_node_as_float(maxNode); }
			if (defNode != nullptr) { lilv_node_free(defNode); }
			if (minNode != nullptr) { lilv_node_free(minNode); }
			if (maxNode != nullptr) { lilv_node_free(maxNode); }
		}

		sendToHost(message(lv2host::IdPortInfo)
			.addInt(static_cast<int>(i))
			.addString(symbol != nullptr ? lilv_node_as_string(symbol) : "")
			.addString(name != nullptr ? lilv_node_as_string(name.get()) : "")
			.addString(kind)
			.addInt(lilv_port_is_a(m_plugin, port, inputClass) ? 1 : 0)
			.addFloat(def)
			.addFloat(min)
			.addFloat(max));
	}

	lilv_node_free(audioClass);
	lilv_node_free(controlClass);
	lilv_node_free(atomClass);
	lilv_node_free(inputClass);
}
#endif // LMMS_HAVE_LV2




#ifndef SYNC_WITH_SHM_FIFO
int connectToHost(const char* path)
{
	struct sockaddr_un address = {};
	address.sun_family = AF_LOCAL;
	std::strncpy(address.sun_path, path, sizeof(address.sun_path) - 1);

	const int fd = ::socket(PF_LOCAL, SOCK_STREAM, 0);
	if (fd == -1)
	{
		return -1;
	}
	if (::connect(fd, reinterpret_cast<struct sockaddr*>(&address),
			sizeof(address)) == -1)
	{
		::close(fd);
		return -1;
	}
	return fd;
}
#endif

} // namespace




int main(int argc, char** argv)
{
#if defined(LMMS_BUILD_LINUX) && defined(LMMS_HAVE_SUIL)
	// Many LV2 UIs render from a second thread; Xlib must be thread safe
	// before the first display connection is opened.
	XInitThreads();
	XSetErrorHandler(lv2HostXErrorHandler);
#endif

#ifdef LMMS_BUILD_LINUX
	// If LMMS is killed with SIGKILL, the kernel terminates this process as
	// well instead of leaving an orphan behind.
	prctl(PR_SET_PDEATHSIG, SIGTERM);
	if (getppid() == 1)
	{
		return 1;
	}
#endif

	// A dead host must not kill the child through SIGPIPE before it notices
	// the broken connection.
#ifndef LMMS_BUILD_WIN32
	std::signal(SIGPIPE, SIG_IGN);
#endif

#ifdef SYNC_WITH_SHM_FIFO
	if (argc < 4)
	{
		std::fprintf(stderr,
			"usage: %s <in-shm-key> <out-shm-key> <plugin-uri>\n", argv[0]);
		return 1;
	}
	Lv2HostProcess host{new shmFifo(argv[1]), new shmFifo(argv[2]), argv[3]};
#else
	if (argc < 3)
	{
		std::fprintf(stderr, "usage: %s <socket-path> <plugin-uri>\n", argv[0]);
		return 1;
	}
	const int fd = connectToHost(argv[1]);
	if (fd == -1)
	{
		std::fprintf(stderr, "could not connect to the LMMS LV2 host socket\n");
		return 1;
	}
	Lv2HostProcess host{argv[2]};
	host.setSocket(fd);
#endif

#ifdef LMMS_BUILD_LINUX
	const ParentWatcher parentWatcher;
#endif
	return host.run();
}
