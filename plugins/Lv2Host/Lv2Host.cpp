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
#include <cstring>
#include <exception>
#include <string>

#include <condition_variable>
#include <chrono>
#include <cmath>
#include <deque>
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
#include <lv2/log/log.h>
#include <lv2/midi/midi.h>
#include <lv2/options/options.h>
#include <lv2/parameters/parameters.h>
#include <lv2/urid/urid.h>
#include <lv2/worker/worker.h>
#endif

#ifdef LMMS_BUILD_LINUX
#include <sys/prctl.h>
#include <unistd.h>
#endif

#ifndef SYNC_WITH_SHM_FIFO
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

int logPrintf(LV2_Log_Handle, LV2_URID, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	const int result = std::vfprintf(stderr, fmt, args);
	va_end(args);
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
		if (urid == 0 || urid > m_uris.size())
		{
			return nullptr;
		}
		return m_uris[urid - 1].c_str();
	}

	LV2_URID_Map* mapFeature() { return &m_map; }
	LV2_URID_Unmap* unmapFeature() { return &m_unmap; }

private:
	std::unordered_map<std::string, LV2_URID> m_byUri;
	std::vector<std::string> m_uris;
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

	SharedMemory<float[]> m_audio;
	f_cnt_t m_frames = 0;
	sample_rate_t m_sampleRate = 0;
	std::string m_pluginUri;

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
	LV2_Options_Option m_options[4]{};
	LV2_Log_Log m_log{};
	LV2_Worker_Schedule* m_schedule = nullptr;
	float m_optionSampleRate = 0.0f;
	int32_t m_optionMinBlock = 0;
	int32_t m_optionMaxBlock = 0;

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
#endif
};




Lv2HostProcess::~Lv2HostProcess()
{
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
	for (;;)
	{
		const auto m = receiveMessage();
		if (m.id == IdQuit || m.id == IdUndefined)
		{
			break;
		}
		processMessage(m);
	}
	return 0;
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
			sendMessage(message(lv2host::IdReady));
			return true;

		case lv2host::IdControlValue:
		{
			const int index = m.getInt(0);
			const float value = m.getFloat(1);
#ifdef LMMS_HAVE_LV2
			if (index >= 0 && static_cast<std::size_t>(index) < m_controlValues.size())
			{
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

		case IdStartProcessing:
			processBlock();
			sendMessage(message(IdProcessingDone));
			return true;

		default:
			return false;
	}
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
			sendMessage(message(lv2host::IdLatency).addInt(latency));
		}
	}
#endif
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
		m_pendingMidi.clear();
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

	for (const auto& event : m_pendingMidi)
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
	m_pendingMidi.clear();
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
		sendMessage(message(lv2host::IdMidiOut)
			.addString(lv2host::encodeMidiEvents(events)));
	}

	sequence->atom.type = sequenceType;
	sequence->atom.size = sizeof(LV2_Atom_Sequence_Body);
	sequence->body.unit = 0;
	sequence->body.pad = 0;
}




void Lv2HostProcess::sendFailed(const std::string& reason)
{
	std::fprintf(stderr, "lv2 host: %s\n", reason.c_str());
	sendMessage(message(lv2host::IdFailed).addString(reason));
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
	m_options[3] = LV2_Options_Option{LV2_OPTIONS_INSTANCE, 0, 0, 0, 0, nullptr};

	m_mapFeature = LV2_Feature{LV2_URID__map, m_urids.mapFeature()};
	m_unmapFeature = LV2_Feature{LV2_URID__unmap, m_urids.unmapFeature()};
	m_optionsFeature = LV2_Feature{LV2_OPTIONS__options, m_options};
	m_boundedFeature = LV2_Feature{LV2_BUF_SIZE__boundedBlockLength, nullptr};
	m_log = LV2_Log_Log{this, logPrintf};
	m_logFeature = LV2_Feature{LV2_LOG__log, &m_log};
	m_schedule = m_worker.scheduleFeature();
	m_workerFeature = LV2_Feature{LV2_WORKER__schedule, m_schedule};

	const LV2_Feature* features[] = {
		&m_mapFeature,
		&m_unmapFeature,
		&m_optionsFeature,
		&m_boundedFeature,
		&m_logFeature,
		&m_workerFeature,
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
			if (def != nullptr) { lilv_node_free(def); }
			if (min != nullptr) { lilv_node_free(min); }
			if (max != nullptr) { lilv_node_free(max); }
			lilv_instance_connect_port(m_instance, i, &m_controlValues[i]);
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

		sendMessage(message(lv2host::IdPortInfo)
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
