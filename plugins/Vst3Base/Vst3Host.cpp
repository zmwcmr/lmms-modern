/*
 * Vst3Host.cpp - in-process VST3 host
 *
 * Copyright (c) 2026 LMMS developers
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

#include "Vst3Host.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <functional>
#include <map>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "Vst3Module.h"

#include "travesty/audio_processor.h"
#include "travesty/bstream.h"
#include "travesty/component.h"
#include "travesty/edit_controller.h"
#include "travesty/events.h"
#include "travesty/factory.h"
#include "travesty/host.h"
#include "travesty/message.h"
#include "travesty/unit.h"
#include <QSocketNotifier>
#include <QTimer>

#include "travesty/view.h"

namespace
{

QString fromUtf8(const char* text, size_t size)
{
	return QString::fromUtf8(text, static_cast<int>(strnlen(text, size)));
}

QString fromUtf16(const int16_t* text, size_t size)
{
	size_t length = 0;
	while (length < size && text[length] != 0)
	{
		++length;
	}
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	return QString::fromUtf16(reinterpret_cast<const char16_t*>(text), length);
#else
	return QString::fromUtf16(reinterpret_cast<const ushort*>(text), static_cast<int>(length));
#endif
}

QString fromStr128(const v3_str_128 text)
{
	return fromUtf16(text, 128);
}

bool containsInstrument(const QString& subCategories)
{
	return subCategories.contains(QLatin1String("Instrument"), Qt::CaseInsensitive);
}

bool parseTuid(const QString& text, v3_tuid tuid)
{
	QString hex;
	for (const auto character : text)
	{
		if (character.isLetterOrNumber())
		{
			hex += character;
		}
	}
	if (hex.size() != 32)
	{
		return false;
	}
	for (int i = 0; i < 16; ++i)
	{
		bool ok = false;
		const auto byte = hex.mid(i * 2, 2).toUInt(&ok, 16);
		if (!ok)
		{
			return false;
		}
		tuid[i] = static_cast<uint8_t>(byte);
	}
	return true;
}

struct HostApplication : v3_host_application_cpp
{
	HostApplication()
	{
		query_interface = queryInterface;
		ref = refStatic;
		unref = unrefStatic;
		app.get_name = getName;
		app.create_instance = createInstance;
	}

private:
	static v3_result V3_API queryInterface(void* self, const v3_tuid iid, void** iface)
	{
		if (v3_tuid_match(iid, v3_funknown_iid)
			|| v3_tuid_match(iid, v3_host_application_iid))
		{
			*iface = self;
			return V3_OK;
		}
		*iface = nullptr;
		return V3_NO_INTERFACE;
	}

	static uint32_t V3_API refStatic(void*) { return 1; }
	static uint32_t V3_API unrefStatic(void*) { return 0; }

	static v3_result V3_API getName(void*, v3_str_128 name)
	{
		static const char16_t hostName[] = u"LMMS";
		for (size_t i = 0; i < sizeof(hostName) / sizeof(hostName[0]); ++i)
		{
			name[i] = static_cast<int16_t>(hostName[i]);
		}
		return V3_OK;
	}

	static v3_result V3_API createInstance(void*, v3_tuid, v3_tuid, void** obj)
	{
		if (obj != nullptr)
		{
			*obj = nullptr;
		}
		return V3_NOT_IMPLEMENTED;
	}
};

struct ComponentHandler : v3_component_handler_cpp
{
	std::function<void(uint32_t)> beginEdit;
	std::function<void(uint32_t, double)> performEdit;
	std::function<void(uint32_t)> endEdit;
	std::function<void(int32_t)> restartComponent;

	ComponentHandler()
	{
		query_interface = queryInterface;
		ref = refStatic;
		unref = unrefStatic;
		comp.begin_edit = beginEditCallback;
		comp.perform_edit = performEditCallback;
		comp.end_edit = endEditCallback;
		comp.restart_component = restartComponentCallback;
	}

private:
	static v3_result V3_API queryInterface(void* self, const v3_tuid iid, void** iface)
	{
		if (v3_tuid_match(iid, v3_funknown_iid)
			|| v3_tuid_match(iid, v3_component_handler_iid))
		{
			*iface = self;
			return V3_OK;
		}
		*iface = nullptr;
		return V3_NO_INTERFACE;
	}

	static uint32_t V3_API refStatic(void*) { return 1; }
	static uint32_t V3_API unrefStatic(void*) { return 0; }

	static ComponentHandler* handler(void* self)
	{
		return *static_cast<ComponentHandler**>(self);
	}

	static v3_result V3_API beginEditCallback(void* self, v3_param_id id)
	{
		if (handler(self)->beginEdit)
		{
			handler(self)->beginEdit(id);
		}
		return V3_OK;
	}

	static v3_result V3_API performEditCallback(
		void* self, v3_param_id id, double value)
	{
		if (handler(self)->performEdit)
		{
			handler(self)->performEdit(id, value);
		}
		return V3_OK;
	}

	static v3_result V3_API endEditCallback(void* self, v3_param_id id)
	{
		if (handler(self)->endEdit)
		{
			handler(self)->endEdit(id);
		}
		return V3_OK;
	}

	static v3_result V3_API restartComponentCallback(void* self, int32_t flags)
	{
		if (handler(self)->restartComponent)
		{
			handler(self)->restartComponent(flags);
		}
		return V3_OK;
	}
};

struct RunLoop : v3_run_loop_cpp
{
	RunLoop()
	{
		query_interface = queryInterface;
		ref = refStatic;
		unref = unrefStatic;
		loop.register_event_handler = registerEventHandler;
		loop.unregister_event_handler = unregisterEventHandler;
		loop.register_timer = registerTimer;
		loop.unregister_timer = unregisterTimer;
	}

	~RunLoop()
	{
		for (auto& registration : m_eventHandlers)
		{
			delete registration.notifier;
		}
		for (auto& [_, timer] : m_timers)
		{
			timer->stop();
			delete timer;
		}
	}

private:
	static RunLoop* runLoop(void* self)
	{
		return *static_cast<RunLoop**>(self);
	}

	static v3_result V3_API queryInterface(void* self, const v3_tuid iid, void** iface)
	{
		if (v3_tuid_match(iid, v3_funknown_iid)
			|| v3_tuid_match(iid, v3_run_loop_iid))
		{
			*iface = self;
			return V3_OK;
		}
		*iface = nullptr;
		return V3_NO_INTERFACE;
	}

	static uint32_t V3_API refStatic(void*) { return 1; }
	static uint32_t V3_API unrefStatic(void*) { return 0; }

	static v3_result V3_API registerEventHandler(
		void* self, v3_event_handler** handler, int fd)
	{
		if (handler == nullptr || fd < 0)
		{
			return V3_INVALID_ARG;
		}
		auto* const loop = runLoop(self);
		// The same event handler is registered once per file descriptor.
		// Drop only a previous registration of this handler/fd pair, not
		// the other descriptors handled by the same object.
		for (auto it = loop->m_eventHandlers.begin();
			it != loop->m_eventHandlers.end();)
		{
			if (it->handler == handler && it->fd == fd)
			{
				delete it->notifier;
				it = loop->m_eventHandlers.erase(it);
			}
			else
			{
				++it;
			}
		}
		auto* notifier = new QSocketNotifier(fd, QSocketNotifier::Read);
		QObject::connect(notifier, &QSocketNotifier::activated, notifier,
			[handler, fd] {
				v3_cpp_obj(handler)->on_fd_is_set(handler, fd);
			});
		loop->m_eventHandlers.push_back({handler, fd, notifier});
		return V3_OK;
	}

	static v3_result V3_API unregisterEventHandler(
		void* self, v3_event_handler** handler)
	{
		runLoop(self)->removeEventHandler(handler);
		return V3_OK;
	}

	static v3_result V3_API registerTimer(
		void* self, v3_timer_handler** handler, uint64_t milliseconds)
	{
		if (handler == nullptr)
		{
			return V3_INVALID_ARG;
		}
		auto* const loop = runLoop(self);
		loop->removeTimer(handler);
		auto* timer = new QTimer();
		timer->setInterval(static_cast<int>(std::max<uint64_t>(1, milliseconds)));
		QObject::connect(timer, &QTimer::timeout, timer, [handler] {
			v3_cpp_obj(handler)->on_timer(handler);
		});
		timer->start();
		loop->m_timers[handler] = timer;
		return V3_OK;
	}

	static v3_result V3_API unregisterTimer(
		void* self, v3_timer_handler** handler)
	{
		runLoop(self)->removeTimer(handler);
		return V3_OK;
	}

	void removeEventHandler(v3_event_handler** handler)
	{
		for (auto it = m_eventHandlers.begin(); it != m_eventHandlers.end();)
		{
			if (it->handler == handler)
			{
				delete it->notifier;
				it = m_eventHandlers.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	void removeTimer(v3_timer_handler** handler)
	{
		const auto it = m_timers.find(handler);
		if (it == m_timers.end())
		{
			return;
		}
		it->second->stop();
		delete it->second;
		m_timers.erase(it);
	}

	struct EventRegistration
	{
		v3_event_handler** handler;
		int fd;
		QSocketNotifier* notifier;
	};
	std::vector<EventRegistration> m_eventHandlers;
	std::map<v3_timer_handler**, QTimer*> m_timers;
};

struct EditorFrame : v3_plugin_frame_cpp
{
	std::function<void(int, int)> resizeView;
	RunLoop runLoop;
	RunLoop* runLoopPointer = &runLoop;

	EditorFrame()
	{
		query_interface = queryInterface;
		ref = refStatic;
		unref = unrefStatic;
		frame.resize_view = resizeViewCallback;
	}

private:
	static EditorFrame* editorFrame(void* self)
	{
		return *static_cast<EditorFrame**>(self);
	}

	static v3_result V3_API resizeViewCallback(
		void* self, v3_plugin_view**, v3_view_rect* rect)
	{
		if (rect != nullptr && editorFrame(self)->resizeView)
		{
			editorFrame(self)->resizeView(rect->right - rect->left,
				rect->bottom - rect->top);
		}
		return V3_OK;
	}

	static v3_result V3_API queryInterface(void* self, const v3_tuid iid, void** iface)
	{
		auto* const frame = *static_cast<EditorFrame**>(self);
		if (v3_tuid_match(iid, v3_funknown_iid)
			|| v3_tuid_match(iid, v3_plugin_frame_iid))
		{
			*iface = self;
			return V3_OK;
		}
		if (v3_tuid_match(iid, v3_run_loop_iid))
		{
			*iface = &frame->runLoopPointer;
			return V3_OK;
		}
		*iface = nullptr;
		return V3_NO_INTERFACE;
	}

	static uint32_t V3_API refStatic(void*) { return 1; }
	static uint32_t V3_API unrefStatic(void*) { return 0; }
};

struct EventList : v3_event_list_cpp
{
	std::vector<v3_event> events;
	std::vector<std::unique_ptr<std::vector<uint8_t>>> data;

	EventList()
	{
		query_interface = queryInterface;
		ref = refStatic;
		unref = unrefStatic;
		list.get_event_count = getEventCount;
		list.get_event = getEvent;
		list.add_event = addEvent;
	}

	void clear()
	{
		events.clear();
		data.clear();
	}

	void addMidiData(int32_t offset, const uint8_t* bytes, uint32_t size)
	{
		data.emplace_back(std::make_unique<std::vector<uint8_t>>(bytes, bytes + size));
		v3_event event = {};
		event.sample_offset = offset;
		event.flags = V3_EVENT_IS_LIVE;
		event.type = V3_EVENT_DATA;
		event.data.size = size;
		event.data.type = 0;
		event.data.bytes = data.back()->data();
		events.push_back(event);
	}

	void addEventCopy(const v3_event& source)
	{
		v3_event event = source;
		if (source.type == V3_EVENT_DATA && source.data.bytes != nullptr
			&& source.data.size > 0)
		{
			data.emplace_back(std::make_unique<std::vector<uint8_t>>(
				source.data.bytes, source.data.bytes + source.data.size));
			event.data.bytes = data.back()->data();
		}
		events.push_back(event);
	}

private:
	static uint32_t V3_API getEventCount(void* self)
	{
		return static_cast<uint32_t>(eventList(self)->events.size());
	}

	static v3_result V3_API getEvent(void* self, int32_t index, v3_event* event)
	{
		const auto* currentList = eventList(self);
		if (index < 0 || event == nullptr
			|| static_cast<size_t>(index) >= currentList->events.size())
		{
			return V3_INVALID_ARG;
		}
		*event = currentList->events[static_cast<size_t>(index)];
		return V3_OK;
	}

	static v3_result V3_API addEvent(void* self, v3_event* event)
	{
		if (event == nullptr)
		{
			return V3_INVALID_ARG;
		}
		eventList(self)->addEventCopy(*event);
		return V3_OK;
	}

	static EventList* eventList(void* self)
	{
		return *static_cast<EventList**>(self);
	}

	static v3_result V3_API queryInterface(void* self, const v3_tuid iid, void** iface)
	{
		if (v3_tuid_match(iid, v3_funknown_iid)
			|| v3_tuid_match(iid, v3_event_list_iid))
		{
			*iface = self;
			return V3_OK;
		}
		*iface = nullptr;
		return V3_NO_INTERFACE;
	}

	static uint32_t V3_API refStatic(void*) { return 1; }
	static uint32_t V3_API unrefStatic(void*) { return 0; }
};

struct ParamValueQueue : v3_param_value_queue_cpp
{
	v3_param_id id = 0;
	std::vector<std::pair<int32_t, double>> points;

	ParamValueQueue()
	{
		query_interface = queryInterface;
		ref = refStatic;
		unref = unrefStatic;
		queue.get_param_id = getParamId;
		queue.get_point_count = getPointCount;
		queue.get_point = getPoint;
		queue.add_point = addPoint;
	}

private:
	static v3_param_id V3_API getParamId(void* self)
	{
		return valueQueue(self)->id;
	}

	static int32_t V3_API getPointCount(void* self)
	{
		return static_cast<int32_t>(valueQueue(self)->points.size());
	}

	static v3_result V3_API getPoint(
		void* self, int32_t index, int32_t* sampleOffset, double* value)
	{
		auto* const currentQueue = valueQueue(self);
		if (index < 0 || sampleOffset == nullptr || value == nullptr
			|| static_cast<size_t>(index) >= currentQueue->points.size())
		{
			return V3_INVALID_ARG;
		}
		*sampleOffset = currentQueue->points[static_cast<size_t>(index)].first;
		*value = currentQueue->points[static_cast<size_t>(index)].second;
		return V3_OK;
	}

	static v3_result V3_API addPoint(
		void* self, int32_t sampleOffset, double value, int32_t* index)
	{
		auto* const currentQueue = valueQueue(self);
		currentQueue->points.emplace_back(sampleOffset, value);
		if (index != nullptr)
		{
			*index = static_cast<int32_t>(currentQueue->points.size() - 1);
		}
		return V3_OK;
	}

	static ParamValueQueue* valueQueue(void* self)
	{
		return *static_cast<ParamValueQueue**>(self);
	}

	static v3_result V3_API queryInterface(void* self, const v3_tuid iid, void** iface)
	{
		if (v3_tuid_match(iid, v3_funknown_iid)
			|| v3_tuid_match(iid, v3_param_value_queue_iid))
		{
			*iface = self;
			return V3_OK;
		}
		*iface = nullptr;
		return V3_NO_INTERFACE;
	}

	static uint32_t V3_API refStatic(void*) { return 1; }
	static uint32_t V3_API unrefStatic(void*) { return 0; }
};

struct ParamChanges : v3_param_changes_cpp
{
	std::vector<std::unique_ptr<ParamValueQueue>> queues;
	// Stable object pointers exposed to the plugin as v3_param_value_queue**.
	std::vector<ParamValueQueue*> exposedQueues;

	ParamChanges()
	{
		query_interface = queryInterface;
		ref = refStatic;
		unref = unrefStatic;
		changes.get_param_count = getParamCount;
		changes.get_param_data = getParamData;
		changes.add_param_data = addParamData;
	}

	void clear()
	{
		queues.clear();
		exposedQueues.clear();
	}

	void addPoint(v3_param_id id, int32_t sampleOffset, double value)
	{
		ParamValueQueue* target = nullptr;
		for (auto* queue : exposedQueues)
		{
			if (queue->id == id)
			{
				target = queue;
				break;
			}
		}
		if (target == nullptr)
		{
			auto queue = std::make_unique<ParamValueQueue>();
			queue->id = id;
			target = queue.get();
			exposedQueues.push_back(target);
			queues.push_back(std::move(queue));
		}
		target->points.emplace_back(sampleOffset, value);
	}

private:
	static int32_t V3_API getParamCount(void* self)
	{
		return static_cast<int32_t>(paramChanges(self)->exposedQueues.size());
	}

	static v3_param_value_queue** getParamData(void* self, int32_t index)
	{
		auto* const currentChanges = paramChanges(self);
		if (index < 0
			|| static_cast<size_t>(index) >= currentChanges->exposedQueues.size())
		{
			return nullptr;
		}
		return reinterpret_cast<v3_param_value_queue**>(
			&currentChanges->exposedQueues[static_cast<size_t>(index)]);
	}

	static v3_param_value_queue** addParamData(
		void* self, const v3_param_id* id, int32_t* index)
	{
		if (id == nullptr)
		{
			return nullptr;
		}
		auto* const currentChanges = paramChanges(self);
		for (size_t i = 0; i < currentChanges->exposedQueues.size(); ++i)
		{
			if (currentChanges->exposedQueues[i]->id == *id)
			{
				if (index != nullptr)
				{
					*index = static_cast<int32_t>(i);
				}
				return reinterpret_cast<v3_param_value_queue**>(
					&currentChanges->exposedQueues[i]);
			}
		}
		auto queue = std::make_unique<ParamValueQueue>();
		queue->id = *id;
		currentChanges->exposedQueues.push_back(queue.get());
		currentChanges->queues.push_back(std::move(queue));
		if (index != nullptr)
		{
			*index = static_cast<int32_t>(currentChanges->exposedQueues.size() - 1);
		}
		return reinterpret_cast<v3_param_value_queue**>(
			&currentChanges->exposedQueues.back());
	}

	static ParamChanges* paramChanges(void* self)
	{
		return *static_cast<ParamChanges**>(self);
	}

	static v3_result V3_API queryInterface(void* self, const v3_tuid iid, void** iface)
	{
		if (v3_tuid_match(iid, v3_funknown_iid)
			|| v3_tuid_match(iid, v3_param_changes_iid))
		{
			*iface = self;
			return V3_OK;
		}
		*iface = nullptr;
		return V3_NO_INTERFACE;
	}

	static uint32_t V3_API refStatic(void*) { return 1; }
	static uint32_t V3_API unrefStatic(void*) { return 0; }
};

struct MemoryStream : v3_bstream_cpp
{
	std::vector<uint8_t> data;
	size_t position = 0;

	MemoryStream()
	{
		query_interface = queryInterface;
		ref = refStatic;
		unref = unrefStatic;
		stream.read = read;
		stream.write = write;
		stream.seek = seek;
		stream.tell = tell;
	}

private:
	static v3_result V3_API read(
		void* self, void* buffer, int32_t numBytes, int32_t* bytesRead)
	{
		if (buffer == nullptr || numBytes < 0)
		{
			return V3_INVALID_ARG;
		}
		auto* const memory = memoryStream(self);
		const auto available = memory->data.size() - memory->position;
		const auto count = std::min<size_t>(
			static_cast<size_t>(numBytes), available);
		if (count > 0)
		{
			std::memcpy(buffer, memory->data.data() + memory->position, count);
			memory->position += count;
		}
		if (bytesRead != nullptr)
		{
			*bytesRead = static_cast<int32_t>(count);
		}
		return V3_OK;
	}

	static v3_result V3_API write(
		void* self, void* buffer, int32_t numBytes, int32_t* bytesWritten)
	{
		if (buffer == nullptr || numBytes < 0)
		{
			return V3_INVALID_ARG;
		}
		auto* const memory = memoryStream(self);
		const auto count = static_cast<size_t>(numBytes);
		if (memory->position + count > memory->data.size())
		{
			memory->data.resize(memory->position + count);
		}
		std::memcpy(memory->data.data() + memory->position, buffer, count);
		memory->position += count;
		if (bytesWritten != nullptr)
		{
			*bytesWritten = numBytes;
		}
		return V3_OK;
	}

	static v3_result V3_API seek(
		void* self, int64_t pos, int32_t seekMode, int64_t* result)
	{
		auto* const memory = memoryStream(self);
		int64_t target = pos;
		if (seekMode == V3_SEEK_CUR)
		{
			target += static_cast<int64_t>(memory->position);
		}
		else if (seekMode == V3_SEEK_END)
		{
			target += static_cast<int64_t>(memory->data.size());
		}
		if (target < 0)
		{
			return V3_INVALID_ARG;
		}
		if (static_cast<size_t>(target) > memory->data.size())
		{
			memory->data.resize(static_cast<size_t>(target));
		}
		memory->position = static_cast<size_t>(target);
		if (result != nullptr)
		{
			*result = target;
		}
		return V3_OK;
	}

	static v3_result V3_API tell(void* self, int64_t* pos)
	{
		if (pos == nullptr)
		{
			return V3_INVALID_ARG;
		}
		*pos = static_cast<int64_t>(memoryStream(self)->position);
		return V3_OK;
	}

	static MemoryStream* memoryStream(void* self)
	{
		return *static_cast<MemoryStream**>(self);
	}

	static v3_result V3_API queryInterface(void* self, const v3_tuid iid, void** iface)
	{
		if (v3_tuid_match(iid, v3_funknown_iid)
			|| v3_tuid_match(iid, v3_bstream_iid))
		{
			*iface = self;
			return V3_OK;
		}
		*iface = nullptr;
		return V3_NO_INTERFACE;
	}

	static uint32_t V3_API refStatic(void*) { return 1; }
	static uint32_t V3_API unrefStatic(void*) { return 0; }
};

} // namespace

namespace lmms
{

struct Vst3Host::Impl
{
	Vst3Module module;
	// Travesty represents every interface as a pointer to an object pointer.
	// Keep the object pointers in stable variables so the plugin can use
	// v3_cpp_obj() on the address it receives.
	HostApplication hostApplication;
	HostApplication* hostPointer = nullptr;
	v3_funknown** hostContext = nullptr;

	ComponentHandler componentHandler;
	ComponentHandler* handlerPointer = nullptr;

	v3_component** component = nullptr;
	v3_edit_controller** controller = nullptr;
	v3_audio_processor** processor = nullptr;
	v3_connection_point** connectionComponent = nullptr;
	v3_connection_point** connectionController = nullptr;

	bool terminateComponent = false;
	bool terminateController = false;
	bool active = false;

	std::recursive_mutex hostMutex;

	bool isInstrument = false;
	QString pluginName;
	QString pluginVendor;
	QString pluginVersion;
	QString classId;
	std::function<void(uint32_t, double)> parameterChangeCallback;

	double sampleRate = 44100.0;
	int maxBlockSize = 512;
	int allocatedFrames = 0;
	double tempo = 120.0;
	bool playing = false;
	int64_t projectTime = 0;

	std::vector<Parameter> parameters;
	std::map<uint32_t, double> parameterValues;
	std::vector<QString> programNames;
	uint32_t programParameterId = 0;
	int programStepCount = 0;
	bool hasProgramParameter = false;

	std::vector<Unit> units;
	int selectedUnit = -1;
	v3_unit_information** unitInformation = nullptr;

	std::vector<int> inputBusChannels;
	std::vector<int> outputBusChannels;
	std::vector<Vst3HostBase::BusInfo> inputBusInfos;
	std::vector<Vst3HostBase::BusInfo> outputBusInfos;
	int mainInputBus = -1;
	int mainOutputBus = -1;

	std::vector<v3_audio_bus_buffers> inputBusBuffers;
	std::vector<v3_audio_bus_buffers> outputBusBuffers;
	std::vector<std::vector<float*>> inputChannelPointers;
	std::vector<std::vector<float*>> outputChannelPointers;
	std::vector<std::vector<float>> inputChannelData;
	std::vector<std::vector<float>> outputChannelData;

	EventList inputEvents;
	EventList outputEvents;
	ParamChanges inputParams;
	ParamChanges outputParams;

	EditorFrame editorFrame;
	EditorFrame* editorFramePointer = nullptr;
	v3_plugin_view** editorView = nullptr;
	std::function<void(int, int)> editorResizeCallback;
	std::vector<MidiEvent> pendingMidi;

	struct QueuedParameter
	{
		uint32_t id = 0;
		double value = 0.0;
		int sampleOffset = 0;
	};
	std::vector<QueuedParameter> queuedParameters;

	~Impl() { unload(); }

	bool load(const QString& bundlePath,
		const QString& classId,
		double newSampleRate,
		int newMaxBlockSize,
		QString* error);
	void unload();

	void queryClassInfo(const v3_tuid classId);
	void setupParameters();
	void setupPrograms();
	void setupBuses();
	bool setupProcessing();
	void ensureBufferSize(int frames);
	void fillInputBuffer(int bus, int channel, const float* source, int frames);
	void clearOutputBuffers(int frames);
	void copyOutput(int frames, float* leftOut, float* rightOut);

};

namespace
{

QString fromPath(const std::filesystem::path& path)
{
#ifdef Q_OS_WIN
	return QString::fromStdWString(path.wstring());
#else
	return QString::fromStdString(path.string());
#endif
}

std::filesystem::path toPath(const QString& path)
{
#ifdef Q_OS_WIN
	return std::filesystem::path(path.toStdWString());
#else
	return std::filesystem::path(path.toStdString());
#endif
}

void setError(QString* error, const QString& message)
{
	if (error != nullptr)
	{
		*error = message;
	}
}

} // namespace

bool Vst3Host::Impl::load(const QString& bundlePath,
	const QString& classId,
	double newSampleRate,
	int newMaxBlockSize,
	QString* error)
{
	unload();

	v3_tuid tuid = {};
	if (!parseTuid(classId, tuid))
	{
		setError(error, QStringLiteral("Invalid VST3 class ID: %1").arg(classId));
		return false;
	}

	Impl::classId = classId;
	QString loadError;
	if (!module.load(toPath(bundlePath), &loadError))
	{
		setError(error, loadError);
		return false;
	}

	hostPointer = &hostApplication;
	hostContext = reinterpret_cast<v3_funknown**>(&hostPointer);
	if (module.factory3() != nullptr)
	{
		v3_cpp_obj(module.factory3())->set_host_context(module.factory3(), hostContext);
	}

	queryClassInfo(tuid);

	void* instance = nullptr;
	if (v3_cpp_obj(module.factory())->create_instance(
			module.factory(), tuid, v3_component_iid, &instance) != V3_OK
		|| instance == nullptr)
	{
		setError(error, QStringLiteral("Unable to create VST3 component"));
		unload();
		return false;
	}
	component = static_cast<v3_component**>(instance);
	if (v3_cpp_obj_initialize(component, hostContext) != V3_OK)
	{
		setError(error, QStringLiteral("VST3 component initialization failed"));
		unload();
		return false;
	}
	terminateComponent = true;

	if (v3_cpp_obj_query_interface(component, v3_edit_controller_iid, &controller) != V3_OK)
	{
		controller = nullptr;
	}
	if (controller == nullptr)
	{
		v3_tuid controllerId = {};
		if (v3_cpp_obj(component)->get_controller_class_id(component, controllerId) == V3_OK)
		{
			instance = nullptr;
			if (v3_cpp_obj(module.factory())->create_instance(module.factory(),
					controllerId, v3_edit_controller_iid, &instance) == V3_OK
				&& instance != nullptr)
			{
				controller = static_cast<v3_edit_controller**>(instance);
				if (v3_cpp_obj_initialize(controller, hostContext) != V3_OK)
				{
					setError(error, QStringLiteral("VST3 controller initialization failed"));
					unload();
					return false;
				}
				terminateController = true;
			}
		}
	}

	if (v3_cpp_obj_query_interface(component, v3_audio_processor_iid, &processor) != V3_OK)
	{
		processor = nullptr;
	}
	if (processor == nullptr)
	{
		setError(error, QStringLiteral("VST3 plugin has no audio processor"));
		unload();
		return false;
	}

	if (v3_cpp_obj_query_interface(component, v3_connection_point_iid,
			&connectionComponent) != V3_OK)
	{
		connectionComponent = nullptr;
	}
	if (controller != nullptr
		&& v3_cpp_obj_query_interface(controller, v3_connection_point_iid,
			&connectionController) != V3_OK)
	{
		connectionController = nullptr;
	}
	if (connectionComponent != nullptr && connectionController != nullptr)
	{
		v3_cpp_obj(connectionComponent)->connect(connectionComponent, connectionController);
		v3_cpp_obj(connectionController)->connect(connectionController, connectionComponent);
	}

	componentHandler.beginEdit = [](uint32_t) {};
	componentHandler.performEdit = [this](uint32_t id, double value) {
		parameterValues[id] = value;
		if (parameterChangeCallback)
		{
			parameterChangeCallback(id, value);
		}
	};
	componentHandler.endEdit = [](uint32_t) {};
	componentHandler.restartComponent = [](int32_t) {};
	editorFrame.resizeView = [this](int width, int height) {
		if (editorResizeCallback)
		{
			editorResizeCallback(width, height);
		}
	};
	handlerPointer = &componentHandler;
	if (controller != nullptr)
	{
		v3_cpp_obj(controller)->set_component_handler(
			controller, reinterpret_cast<v3_component_handler**>(&handlerPointer));
	}

	sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
	maxBlockSize = newMaxBlockSize > 0 ? newMaxBlockSize : 512;

	setupParameters();
	setupPrograms();
	setupBuses();
	if (!setupProcessing())
	{
		setError(error, QStringLiteral("VST3 processing setup failed"));
		unload();
		return false;
	}
	return true;
}

void Vst3Host::Impl::unload()
{
	if (editorView != nullptr)
	{
		v3_cpp_obj(editorView)->set_frame(editorView, nullptr);
		v3_cpp_obj(editorView)->removed(editorView);
		v3_cpp_obj_unref(editorView);
		editorView = nullptr;
	}
	if (processor != nullptr && component != nullptr && active)
	{
		v3_cpp_obj(processor)->set_processing(processor, false);
		v3_cpp_obj(component)->set_active(component, false);
	}
	active = false;

	if (connectionComponent != nullptr && connectionController != nullptr)
	{
		v3_cpp_obj(connectionComponent)->disconnect(connectionComponent, connectionController);
		v3_cpp_obj(connectionController)->disconnect(connectionController, connectionComponent);
	}
	if (connectionController != nullptr)
	{
		v3_cpp_obj_unref(connectionController);
		connectionController = nullptr;
	}
	if (connectionComponent != nullptr)
	{
		v3_cpp_obj_unref(connectionComponent);
		connectionComponent = nullptr;
	}
	if (processor != nullptr)
	{
		v3_cpp_obj_unref(processor);
		processor = nullptr;
	}
	if (controller != nullptr)
	{
		if (terminateController)
		{
			v3_cpp_obj_terminate(controller);
			terminateController = false;
		}
		v3_cpp_obj_unref(controller);
		controller = nullptr;
	}
	if (component != nullptr)
	{
		if (terminateComponent)
		{
			v3_cpp_obj_terminate(component);
			terminateComponent = false;
		}
		v3_cpp_obj_unref(component);
		component = nullptr;
	}

	if (unitInformation != nullptr)
	{
		v3_cpp_obj_unref(unitInformation);
		unitInformation = nullptr;
	}
	if (module.factory3() != nullptr)
	{
		v3_cpp_obj_unref(module.factory3());
	}
	if (module.factory2() != nullptr)
	{
		v3_cpp_obj_unref(module.factory2());
	}
	if (module.factory() != nullptr)
	{
		v3_cpp_obj_unref(module.factory());
	}
	module.unload();

	parameters.clear();
	parameterValues.clear();
	programNames.clear();
	programParameterId = 0;
	programStepCount = 0;
	inputBusChannels.clear();
	outputBusChannels.clear();
	inputBusInfos.clear();
	outputBusInfos.clear();
	inputBusBuffers.clear();
	outputBusBuffers.clear();
	inputChannelPointers.clear();
	outputChannelPointers.clear();
	inputChannelData.clear();
	outputChannelData.clear();
	pendingMidi.clear();
	queuedParameters.clear();
	inputEvents.clear();
	outputEvents.clear();
	inputParams.clear();
	outputParams.clear();
	mainInputBus = -1;
	mainOutputBus = -1;
	isInstrument = false;
	pluginName.clear();
	pluginVendor.clear();
	pluginVersion.clear();
	classId.clear();
	projectTime = 0;
	allocatedFrames = 0;
}

void Vst3Host::Impl::queryClassInfo(const v3_tuid classId)
{
	isInstrument = false;
	pluginName.clear();
	pluginVendor.clear();
	pluginVersion.clear();

	v3_factory_info factoryInfo = {};
	v3_cpp_obj(module.factory())->get_factory_info(module.factory(), &factoryInfo);
	const int32_t classCount = v3_cpp_obj(module.factory())->num_classes(module.factory());
	for (int32_t index = 0; index < classCount; ++index)
	{
		v3_tuid currentId = {};
		QString name;
		QString vendor;
		QString version;
		QString subCategories;

		if (module.factory3() != nullptr)
		{
			v3_class_info_3 info = {};
			if (v3_cpp_obj(module.factory3())->get_class_info_utf16(
					module.factory3(), index, &info) != V3_OK)
			{
				continue;
			}
			std::memcpy(currentId, info.class_id, sizeof(currentId));
			name = fromUtf16(info.name, 64);
			vendor = fromUtf16(info.vendor, 64);
			version = fromUtf16(info.version, 64);
			subCategories = fromUtf8(info.sub_categories, sizeof(info.sub_categories));
		}
		else if (module.factory2() != nullptr)
		{
			v3_class_info_2 info = {};
			if (v3_cpp_obj(module.factory2())->get_class_info_2(
					module.factory2(), index, &info) != V3_OK)
			{
				continue;
			}
			std::memcpy(currentId, info.class_id, sizeof(currentId));
			name = fromUtf8(info.name, sizeof(info.name));
			vendor = fromUtf8(info.vendor, sizeof(info.vendor));
			version = fromUtf8(info.version, sizeof(info.version));
			subCategories = fromUtf8(info.sub_categories, sizeof(info.sub_categories));
		}
		else
		{
			v3_class_info info = {};
			if (v3_cpp_obj(module.factory())->get_class_info(
					module.factory(), index, &info) != V3_OK)
			{
				continue;
			}
			std::memcpy(currentId, info.class_id, sizeof(currentId));
			name = fromUtf8(info.name, sizeof(info.name));
			vendor = fromUtf8(factoryInfo.vendor, sizeof(factoryInfo.vendor));
			subCategories = QString();
		}

		if (std::memcmp(currentId, classId, sizeof(v3_tuid)) == 0)
		{
			pluginName = name;
			pluginVendor = vendor;
			pluginVersion = version;
			isInstrument = containsInstrument(subCategories);
			return;
		}
	}
}

void Vst3Host::Impl::setupParameters()
{
	parameters.clear();
	parameterValues.clear();
	programNames.clear();
	programParameterId = 0;
	programStepCount = 0;
	if (controller == nullptr)
	{
		return;
	}

	const int parameterCount = v3_cpp_obj(controller)->get_parameter_count(controller);
	for (int index = 0; index < parameterCount; ++index)
	{
		v3_param_info info = {};
		if (v3_cpp_obj(controller)->get_parameter_info(
				controller, index, &info) != V3_OK)
		{
			continue;
		}

		Parameter parameter;
		parameter.id = info.param_id;
		parameter.name = fromStr128(info.title);
		if (parameter.name.isEmpty())
		{
			parameter.name = fromStr128(info.short_title);
		}
		if (parameter.name.isEmpty())
		{
			parameter.name = QStringLiteral("Parameter %1").arg(index + 1);
		}
		parameter.unit = fromStr128(info.units);
		parameter.defaultNormalised = info.default_normalised_value;
		parameter.stepCount = info.step_count;
		parameter.flags = info.flags;
		parameter.automatable = (info.flags & V3_PARAM_CAN_AUTOMATE) != 0;
		parameter.readOnly = (info.flags & V3_PARAM_READ_ONLY) != 0;
		parameter.hidden = (info.flags & V3_PARAM_IS_HIDDEN) != 0;

		double value = v3_cpp_obj(controller)->get_parameter_normalised(
			controller, parameter.id);
		if (!std::isfinite(value))
		{
			value = parameter.defaultNormalised;
		}
		value = std::clamp(value, 0.0, 1.0);
		parameterValues[parameter.id] = value;
		parameters.push_back(std::move(parameter));
	}
}

void Vst3Host::Impl::setupPrograms()
{
	programNames.clear();
	programParameterId = 0;
	programStepCount = 0;
	hasProgramParameter = false;
	units.clear();
	selectedUnit = -1;

	for (const auto& parameter : parameters)
	{
		if ((parameter.flags & V3_PARAM_PROGRAM_CHANGE) != 0)
		{
			programParameterId = parameter.id;
			programStepCount = std::max(0, parameter.stepCount);
			hasProgramParameter = true;
			break;
		}
	}

	if (unitInformation == nullptr)
	{
		if (controller != nullptr
			&& v3_cpp_obj_query_interface(controller, v3_unit_information_iid,
				&unitInformation) != V3_OK)
		{
			unitInformation = nullptr;
		}
		if (unitInformation == nullptr && component != nullptr
			&& v3_cpp_obj_query_interface(component, v3_unit_information_iid,
				&unitInformation) != V3_OK)
		{
			unitInformation = nullptr;
		}
	}

	if (unitInformation != nullptr)
	{
		const int unitCount = v3_cpp_obj(unitInformation)->get_unit_count(
			unitInformation);
		for (int unit = 0; unit < unitCount; ++unit)
		{
			v3_unit_info info = {};
			if (v3_cpp_obj(unitInformation)->get_unit_info(
					unitInformation, unit, &info) != V3_OK)
			{
				continue;
			}
			Unit entry;
			entry.id = info.id;
			entry.parentId = info.parent_unit_id;
			entry.name = fromStr128(info.name);
			if (entry.name.isEmpty())
			{
				entry.name = QStringLiteral("Unit %1").arg(unit + 1);
			}
			units.push_back(std::move(entry));
		}
		selectedUnit = v3_cpp_obj(unitInformation)->get_selected_unit(
			unitInformation);

		const int listCount = v3_cpp_obj(unitInformation)->get_program_list_count(
			unitInformation);
		for (int list = 0; list < listCount; ++list)
		{
			v3_program_list_info info = {};
			if (v3_cpp_obj(unitInformation)->get_program_list_info(
					unitInformation, list, &info) != V3_OK)
			{
				continue;
			}
			for (int program = 0; program < info.programCount; ++program)
			{
				v3_str_128 name = {};
				QString programName;
				if (v3_cpp_obj(unitInformation)->get_program_name(
						unitInformation, info.id, program, name) == V3_OK)
				{
					programName = fromStr128(name);
				}
				if (programName.isEmpty())
				{
					programName = QStringLiteral("Program %1").arg(program + 1);
				}
				programNames.push_back(programName);
			}
		}
	}

	if (programNames.empty() && programStepCount > 0)
	{
		for (int program = 0; program <= programStepCount; ++program)
		{
			programNames.push_back(QStringLiteral("Program %1").arg(program + 1));
		}
	}
}

void Vst3Host::Impl::setupBuses()
{
	inputBusChannels.clear();
	outputBusChannels.clear();
	inputBusInfos.clear();
	outputBusInfos.clear();
	mainInputBus = -1;
	mainOutputBus = -1;

	auto* const componentMethods = v3_cpp_obj(component);
	int inputBusCount = componentMethods->get_bus_count(component, V3_AUDIO, V3_INPUT);
	int outputBusCount = componentMethods->get_bus_count(component, V3_AUDIO, V3_OUTPUT);
	inputBusCount = std::max(0, inputBusCount);
	outputBusCount = std::max(0, outputBusCount);

	for (int bus = 0; bus < inputBusCount; ++bus)
	{
		v3_bus_info info = {};
		if (componentMethods->get_bus_info(component, V3_AUDIO, V3_INPUT, bus, &info) != V3_OK)
		{
			inputBusChannels.push_back(0);
			continue;
		}
		inputBusChannels.push_back(std::max(0, info.channel_count));
		Vst3HostBase::BusInfo busInfo;
		busInfo.channels = std::max(0, info.channel_count);
		busInfo.isInput = true;
		busInfo.isMain = info.bus_type == V3_MAIN;
		busInfo.active = (info.flags & V3_DEFAULT_ACTIVE) != 0;
		busInfo.name = info.bus_name[0] != 0
			? QString::fromUtf16(reinterpret_cast<const ushort*>(info.bus_name))
			: QStringLiteral("Input %1").arg(bus + 1);
		inputBusInfos.push_back(busInfo);
		if (mainInputBus < 0 && info.bus_type == V3_MAIN)
		{
			mainInputBus = bus;
		}
	}
	for (int bus = 0; bus < outputBusCount; ++bus)
	{
		v3_bus_info info = {};
		if (componentMethods->get_bus_info(component, V3_AUDIO, V3_OUTPUT, bus, &info) != V3_OK)
		{
			outputBusChannels.push_back(0);
			continue;
		}
		outputBusChannels.push_back(std::max(0, info.channel_count));
		Vst3HostBase::BusInfo busInfo;
		busInfo.channels = std::max(0, info.channel_count);
		busInfo.isInput = false;
		busInfo.isMain = info.bus_type == V3_MAIN;
		busInfo.active = (info.flags & V3_DEFAULT_ACTIVE) != 0;
		busInfo.name = info.bus_name[0] != 0
			? QString::fromUtf16(reinterpret_cast<const ushort*>(info.bus_name))
			: QStringLiteral("Output %1").arg(bus + 1);
		outputBusInfos.push_back(busInfo);
		if (mainOutputBus < 0 && info.bus_type == V3_MAIN)
		{
			mainOutputBus = bus;
		}
	}

	if (mainInputBus < 0 && !inputBusChannels.empty())
	{
		mainInputBus = 0;
	}
	if (mainOutputBus < 0 && !outputBusChannels.empty())
	{
		mainOutputBus = 0;
	}

	// Ask for a stereo arrangement on the main buses and silence the rest.
	if (processor != nullptr)
	{
		std::vector<v3_speaker_arrangement> inputArrangements(
			inputBusChannels.size(), 0);
		std::vector<v3_speaker_arrangement> outputArrangements(
			outputBusChannels.size(), 0);
		for (size_t bus = 0; bus < inputBusChannels.size(); ++bus)
		{
			if (inputBusChannels[bus] >= 2)
				inputArrangements[bus] = V3_SPEAKER_L | V3_SPEAKER_R;
			else if (inputBusChannels[bus] == 1)
				inputArrangements[bus] = V3_SPEAKER_M;
		}
		for (size_t bus = 0; bus < outputBusChannels.size(); ++bus)
		{
			if (outputBusChannels[bus] >= 2)
				outputArrangements[bus] = V3_SPEAKER_L | V3_SPEAKER_R;
			else if (outputBusChannels[bus] == 1)
				outputArrangements[bus] = V3_SPEAKER_M;
		}
		v3_cpp_obj(processor)->set_bus_arrangements(processor,
			inputArrangements.data(), static_cast<int32_t>(inputArrangements.size()),
			outputArrangements.data(), static_cast<int32_t>(outputArrangements.size()));
	}

	// Re-query after the arrangement change: plugins may adjust channel
	// counts or bus activation.
	for (size_t bus = 0; bus < inputBusChannels.size(); ++bus)
	{
		v3_bus_info info = {};
		if (v3_cpp_obj(component)->get_bus_info(component, V3_AUDIO, V3_INPUT,
				static_cast<int32_t>(bus), &info) == V3_OK)
		{
			inputBusChannels[bus] = std::max(0, info.channel_count);
			if (bus < inputBusInfos.size())
			{
				inputBusInfos[bus].channels = std::max(0, info.channel_count);
			}
		}
	}
	for (size_t bus = 0; bus < outputBusChannels.size(); ++bus)
	{
		v3_bus_info info = {};
		if (v3_cpp_obj(component)->get_bus_info(component, V3_AUDIO, V3_OUTPUT,
				static_cast<int32_t>(bus), &info) == V3_OK)
		{
			outputBusChannels[bus] = std::max(0, info.channel_count);
			if (bus < outputBusInfos.size())
			{
				outputBusInfos[bus].channels = std::max(0, info.channel_count);
			}
		}
	}

	for (size_t bus = 0; bus < inputBusChannels.size(); ++bus)
	{
		v3_cpp_obj(component)->activate_bus(component, V3_AUDIO, V3_INPUT,
			static_cast<int32_t>(bus), true);
	}
	for (size_t bus = 0; bus < outputBusChannels.size(); ++bus)
	{
		v3_cpp_obj(component)->activate_bus(component, V3_AUDIO, V3_OUTPUT,
			static_cast<int32_t>(bus), true);
	}

	ensureBufferSize(maxBlockSize);
}

bool Vst3Host::Impl::setupProcessing()
{
	v3_process_setup setup = {};
	setup.process_mode = V3_REALTIME;
	setup.symbolic_sample_size = V3_SAMPLE_32;
	setup.max_block_size = maxBlockSize;
	setup.sample_rate = sampleRate;
	if (v3_cpp_obj(processor)->setup_processing(processor, &setup) != V3_OK)
	{
		return false;
	}
	// Some plugins return kResultFalse here even though they are usable.
	v3_cpp_obj(component)->set_active(component, true);
	v3_cpp_obj(processor)->set_processing(processor, true);
	active = true;
	return true;
}

void Vst3Host::Impl::ensureBufferSize(int frames)
{
	if (frames <= 0)
	{
		return;
	}
	if (frames > maxBlockSize)
	{
		maxBlockSize = frames;
		allocatedFrames = 0;
		v3_process_setup setup = {};
		setup.process_mode = V3_REALTIME;
		setup.symbolic_sample_size = V3_SAMPLE_32;
		setup.max_block_size = maxBlockSize;
		setup.sample_rate = sampleRate;
		if (processor != nullptr)
		{
			v3_cpp_obj(processor)->setup_processing(processor, &setup);
		}
	}
	if (allocatedFrames >= frames)
	{
		return;
	}

	inputChannelData.resize(inputBusChannels.size());
	inputChannelPointers.resize(inputBusChannels.size());
	outputChannelData.resize(outputBusChannels.size());
	outputChannelPointers.resize(outputBusChannels.size());

	auto allocate = [frames](const std::vector<int>& channels,
		std::vector<std::vector<float>>& data,
		std::vector<std::vector<float*>>& pointers,
		std::vector<v3_audio_bus_buffers>& buffers) {
		data.resize(channels.size());
		pointers.resize(channels.size());
		buffers.resize(channels.size());
		for (size_t bus = 0; bus < channels.size(); ++bus)
		{
			const auto channelCount = static_cast<size_t>(std::max(0, channels[bus]));
			data[bus].assign(channelCount * static_cast<size_t>(frames), 0.0f);
			pointers[bus].resize(channelCount);
			for (size_t channel = 0; channel < channelCount; ++channel)
			{
				pointers[bus][channel] = data[bus].data()
					+ channel * static_cast<size_t>(frames);
			}
			buffers[bus].num_channels = static_cast<int32_t>(channelCount);
			buffers[bus].channel_silence_bitset = 0;
			buffers[bus].channel_buffers_32 = pointers[bus].empty()
				? nullptr
				: pointers[bus].data();
		}
	};

	allocate(inputBusChannels, inputChannelData, inputChannelPointers, inputBusBuffers);
	allocate(outputBusChannels, outputChannelData, outputChannelPointers, outputBusBuffers);
	allocatedFrames = maxBlockSize;
}

void Vst3Host::Impl::fillInputBuffer(
	int bus, int channel, const float* source, int frames)
{
	if (bus < 0 || static_cast<size_t>(bus) >= inputChannelData.size()
		|| channel < 0 || frames <= 0)
	{
		return;
	}
	const auto stride = static_cast<size_t>(allocatedFrames);
	auto& busData = inputChannelData[static_cast<size_t>(bus)];
	const auto channelCount = stride > 0 ? busData.size() / stride : 0;
	if (static_cast<size_t>(channel) >= channelCount)
	{
		return;
	}
	auto* const target = busData.data() + static_cast<size_t>(channel) * stride;
	if (source != nullptr)
	{
		std::memcpy(target, source, sizeof(float) * static_cast<size_t>(frames));
	}
	else
	{
		std::memset(target, 0, sizeof(float) * static_cast<size_t>(frames));
	}
}

void Vst3Host::Impl::clearOutputBuffers(int frames)
{
	const auto stride = static_cast<size_t>(allocatedFrames);
	for (auto& bus : outputChannelData)
	{
		const auto channelCount = stride > 0 ? bus.size() / stride : 0;
		const auto count = static_cast<size_t>(frames) * channelCount;
		if (count > 0)
		{
			std::memset(bus.data(), 0, sizeof(float) * count);
		}
	}
}

void Vst3Host::Impl::copyOutput(int frames, float* leftOut, float* rightOut)
{
	if (leftOut == nullptr || rightOut == nullptr || frames <= 0)
	{
		return;
	}
	if (mainOutputBus < 0
		|| static_cast<size_t>(mainOutputBus) >= outputChannelData.size()
		|| outputChannelData[static_cast<size_t>(mainOutputBus)].empty())
	{
		std::memset(leftOut, 0, sizeof(float) * static_cast<size_t>(frames));
		std::memset(rightOut, 0, sizeof(float) * static_cast<size_t>(frames));
		return;
	}

	const auto& bus = outputChannelData[static_cast<size_t>(mainOutputBus)];
	const auto stride = static_cast<size_t>(allocatedFrames);
	const auto* left = bus.data();
	const auto* right = bus.size() >= 2 * stride ? bus.data() + stride : left;
	std::memcpy(leftOut, left, sizeof(float) * static_cast<size_t>(frames));
	std::memcpy(rightOut, right, sizeof(float) * static_cast<size_t>(frames));
}

Vst3Host::Vst3Host() :
	m_impl(std::make_unique<Impl>())
{
}

Vst3Host::~Vst3Host() = default;

bool Vst3Host::load(const QString& bundlePath,
	const QString& classId,
	double sampleRate,
	int maxBlockSize,
	QString* error)
{
	return m_impl->load(bundlePath, classId, sampleRate, maxBlockSize, error);
}

void Vst3Host::unload()
{
	m_impl->unload();
}

bool Vst3Host::isLoaded() const
{
	return m_impl->component != nullptr;
}

bool Vst3Host::isInstrument() const
{
	return m_impl->isInstrument;
}

QString Vst3Host::name() const
{
	return m_impl->pluginName;
}

QString Vst3Host::vendor() const
{
	return m_impl->pluginVendor;
}

QString Vst3Host::version() const
{
	return m_impl->pluginVersion;
}

QString Vst3Host::bundlePath() const
{
	return fromPath(m_impl->module.bundlePath());
}

QString Vst3Host::classId() const
{
	return m_impl->classId;
}

void Vst3Host::setParameterChangeCallback(
	std::function<void(uint32_t, double)> callback)
{
	m_impl->parameterChangeCallback = std::move(callback);
}

const std::vector<Vst3Host::Parameter>& Vst3Host::parameters() const
{
	return m_impl->parameters;
}




std::vector<Vst3Host::BusInfo> Vst3Host::inputBuses() const
{
	return m_impl->inputBusInfos;
}




std::vector<Vst3Host::BusInfo> Vst3Host::outputBuses() const
{
	return m_impl->outputBusInfos;
}

bool Vst3Host::setParameterNormalised(uint32_t id, double value)
{
	std::lock_guard<std::recursive_mutex> lock(m_impl->hostMutex);
	if (m_impl->controller == nullptr)
	{
		return false;
	}
	if (m_impl->parameterValues.find(id) == m_impl->parameterValues.end())
	{
		return false;
	}
	value = std::clamp(value, 0.0, 1.0);
	if (v3_cpp_obj(m_impl->controller)->set_parameter_normalised(
			m_impl->controller, id, value) != V3_OK)
	{
		return false;
	}
	m_impl->parameterValues[id] = value;
	return true;
}

double Vst3Host::parameterNormalised(uint32_t id) const
{
	const auto it = m_impl->parameterValues.find(id);
	return it != m_impl->parameterValues.end() ? it->second : 0.0;
}

int Vst3Host::programCount() const
{
	return static_cast<int>(m_impl->programNames.size());
}

QString Vst3Host::programName(int index) const
{
	if (index < 0 || static_cast<size_t>(index) >= m_impl->programNames.size())
	{
		return {};
	}
	return m_impl->programNames[static_cast<size_t>(index)];
}

int Vst3Host::currentProgram() const
{
	if (!m_impl->hasProgramParameter || m_impl->programStepCount <= 0)
	{
		return -1;
	}
	const auto it = m_impl->parameterValues.find(m_impl->programParameterId);
	if (it == m_impl->parameterValues.end())
	{
		return -1;
	}
	return std::clamp(static_cast<int>(std::lround(it->second
		* static_cast<double>(m_impl->programStepCount))),
		0, std::max(0, programCount() - 1));
}

bool Vst3Host::setProgram(int index)
{
	if (index < 0 || index >= programCount() || !m_impl->hasProgramParameter)
	{
		return false;
	}
	const double normalised = m_impl->programStepCount > 0
		? static_cast<double>(index) / static_cast<double>(m_impl->programStepCount)
		: 0.0;
	return setParameterNormalised(m_impl->programParameterId, normalised);
}

const std::vector<Vst3Host::Unit>& Vst3Host::units() const
{
	return m_impl->units;
}

int Vst3Host::currentUnit() const
{
	return m_impl->selectedUnit;
}

bool Vst3Host::selectUnit(int unitId)
{
	std::lock_guard<std::recursive_mutex> lock(m_impl->hostMutex);
	if (m_impl->unitInformation == nullptr)
	{
		return false;
	}
	if (v3_cpp_obj(m_impl->unitInformation)->select_unit(
			m_impl->unitInformation, unitId) != V3_OK)
	{
		return false;
	}
	m_impl->selectedUnit = unitId;
	return true;
}

void Vst3Host::queueParameter(uint32_t id, double value, int sampleOffset)
{
	std::lock_guard<std::recursive_mutex> lock(m_impl->hostMutex);
	if (m_impl->parameterValues.find(id) == m_impl->parameterValues.end())
	{
		return;
	}
	value = std::clamp(value, 0.0, 1.0);
	m_impl->parameterValues[id] = value;
	m_impl->queuedParameters.push_back({id, value, sampleOffset});
}

void Vst3Host::queueMidi(const MidiEvent& event)
{
	m_impl->pendingMidi.push_back(event);
}

void Vst3Host::clearMidi()
{
	m_impl->pendingMidi.clear();
}

void Vst3Host::setTempo(double bpm)
{
	m_impl->tempo = bpm > 0.0 ? bpm : 120.0;
}

void Vst3Host::setPlaying(bool playing)
{
	m_impl->playing = playing;
}

void Vst3Host::process(const float* leftIn,
	const float* rightIn,
	float* leftOut,
	float* rightOut,
	int frames)
{
	std::lock_guard<std::recursive_mutex> lock(m_impl->hostMutex);
	if (frames <= 0)
	{
		return;
	}
	if (leftOut != nullptr)
	{
		std::memset(leftOut, 0, sizeof(float) * static_cast<size_t>(frames));
	}
	if (rightOut != nullptr)
	{
		std::memset(rightOut, 0, sizeof(float) * static_cast<size_t>(frames));
	}
	if (!m_impl->active || m_impl->processor == nullptr)
	{
		return;
	}

	m_impl->ensureBufferSize(frames);
	m_impl->inputEvents.clear();
	m_impl->outputEvents.clear();
	m_impl->inputParams.clear();
	m_impl->outputParams.clear();

	std::stable_sort(m_impl->queuedParameters.begin(), m_impl->queuedParameters.end(),
		[](const auto& lhs, const auto& rhs) {
			return lhs.sampleOffset < rhs.sampleOffset;
		});
	for (const auto& parameter : m_impl->queuedParameters)
	{
		m_impl->inputParams.addPoint(parameter.id,
			std::clamp(parameter.sampleOffset, 0, frames - 1), parameter.value);
	}

	std::stable_sort(m_impl->pendingMidi.begin(), m_impl->pendingMidi.end(),
		[](const auto& lhs, const auto& rhs) {
			return lhs.sampleOffset < rhs.sampleOffset;
		});

	for (const auto& midi : m_impl->pendingMidi)
	{
		const auto offset = std::clamp(midi.sampleOffset, 0, frames - 1);
		const auto channel = static_cast<int16_t>(std::clamp(midi.channel, 0, 15));
		switch (midi.type)
		{
		case MidiType::NoteOn:
		{
			v3_event event = {};
			event.sample_offset = offset;
			event.flags = V3_EVENT_IS_LIVE;
			event.type = V3_EVENT_NOTE_ON;
			event.note_on.channel = channel;
			event.note_on.pitch = static_cast<int16_t>(std::clamp(midi.data1, 0, 127));
			event.note_on.velocity = std::clamp(midi.value, 0.0f, 1.0f);
			event.note_on.length = -1;
			event.note_on.note_id = -1;
			m_impl->inputEvents.events.push_back(event);
			break;
		}
		case MidiType::NoteOff:
		{
			v3_event event = {};
			event.sample_offset = offset;
			event.flags = V3_EVENT_IS_LIVE;
			event.type = V3_EVENT_NOTE_OFF;
			event.note_off.channel = channel;
			event.note_off.pitch = static_cast<int16_t>(std::clamp(midi.data1, 0, 127));
			event.note_off.velocity = std::clamp(midi.value, 0.0f, 1.0f);
			event.note_off.note_id = -1;
			m_impl->inputEvents.events.push_back(event);
			break;
		}
		case MidiType::ControlChange:
		{
			const uint8_t bytes[3] = {
				static_cast<uint8_t>(0xB0 | channel),
				static_cast<uint8_t>(std::clamp(midi.data1, 0, 127)),
				static_cast<uint8_t>(std::clamp(midi.data2, 0, 127)),
			};
			m_impl->inputEvents.addMidiData(offset, bytes, sizeof(bytes));
			break;
		}
		case MidiType::PitchBend:
		{
			const uint8_t bytes[3] = {
				static_cast<uint8_t>(0xE0 | channel),
				static_cast<uint8_t>(std::clamp(midi.data1, 0, 127)),
				static_cast<uint8_t>(std::clamp(midi.data2, 0, 127)),
			};
			m_impl->inputEvents.addMidiData(offset, bytes, sizeof(bytes));
			break;
		}
		case MidiType::ChannelPressure:
		{
			const uint8_t bytes[2] = {
				static_cast<uint8_t>(0xD0 | channel),
				static_cast<uint8_t>(std::clamp(midi.data1, 0, 127)),
			};
			m_impl->inputEvents.addMidiData(offset, bytes, sizeof(bytes));
			break;
		}
		case MidiType::PolyPressure:
		{
			v3_event event = {};
			event.sample_offset = offset;
			event.flags = V3_EVENT_IS_LIVE;
			event.type = V3_EVENT_POLY_PRESSURE;
			event.poly_pressure.channel = channel;
			event.poly_pressure.pitch = static_cast<int16_t>(
				std::clamp(midi.data1, 0, 127));
			event.poly_pressure.pressure = std::clamp(midi.value, 0.0f, 1.0f);
			event.poly_pressure.note_id = -1;
			m_impl->inputEvents.events.push_back(event);
			break;
		}
		case MidiType::ProgramChange:
		{
			const uint8_t bytes[2] = {
				static_cast<uint8_t>(0xC0 | channel),
				static_cast<uint8_t>(std::clamp(midi.data1, 0, 127)),
			};
			m_impl->inputEvents.addMidiData(offset, bytes, sizeof(bytes));
			break;
		}
		}
	}

	const auto inputStride = static_cast<size_t>(m_impl->allocatedFrames);
	for (size_t bus = 0; bus < m_impl->inputChannelData.size(); ++bus)
	{
		const auto channelCount = inputStride > 0
			? m_impl->inputChannelData[bus].size() / inputStride
			: 0;
		for (size_t channel = 0; channel < channelCount; ++channel)
		{
			auto* const target = m_impl->inputChannelData[bus].data()
				+ channel * inputStride;
			std::memset(target, 0, sizeof(float) * static_cast<size_t>(frames));
		}
	}
	if (m_impl->mainInputBus >= 0)
	{
		m_impl->fillInputBuffer(m_impl->mainInputBus, 0, leftIn, frames);
		m_impl->fillInputBuffer(m_impl->mainInputBus, 1,
			rightIn != nullptr ? rightIn : leftIn, frames);
	}
	m_impl->clearOutputBuffers(frames);

	v3_process_data processData = {};
	processData.process_mode = V3_REALTIME;
	processData.symbolic_sample_size = V3_SAMPLE_32;
	processData.nframes = frames;
	processData.num_input_buses = static_cast<int32_t>(m_impl->inputBusBuffers.size());
	processData.num_output_buses = static_cast<int32_t>(m_impl->outputBusBuffers.size());
	processData.inputs = m_impl->inputBusBuffers.empty()
		? nullptr
		: m_impl->inputBusBuffers.data();
	processData.outputs = m_impl->outputBusBuffers.empty()
		? nullptr
		: m_impl->outputBusBuffers.data();

	ParamChanges* inputParameters = &m_impl->inputParams;
	ParamChanges* outputParameters = &m_impl->outputParams;
	EventList* inputEvents = &m_impl->inputEvents;
	EventList* outputEvents = &m_impl->outputEvents;
	processData.input_params =
		reinterpret_cast<v3_param_changes**>(&inputParameters);
	processData.output_params =
		reinterpret_cast<v3_param_changes**>(&outputParameters);
	processData.input_events =
		reinterpret_cast<v3_event_list**>(&inputEvents);
	processData.output_events =
		reinterpret_cast<v3_event_list**>(&outputEvents);

	v3_process_context context = {};
	context.state = V3_PROCESS_CTX_PROJECT_TIME_VALID
		| V3_PROCESS_CTX_CONT_TIME_VALID
		| V3_PROCESS_CTX_TEMPO_VALID
		| V3_PROCESS_CTX_TIME_SIG_VALID;
	if (m_impl->playing)
	{
		context.state |= V3_PROCESS_CTX_PLAYING;
	}
	context.sample_rate = m_impl->sampleRate;
	context.project_time_in_samples = m_impl->projectTime;
	context.continuous_time_in_samples = m_impl->projectTime;
	context.bpm = m_impl->tempo;
	context.time_sig_numerator = 4;
	context.time_sig_denom = 4;
	processData.ctx = &context;

	v3_cpp_obj(m_impl->processor)->process(m_impl->processor, &processData);

	m_impl->copyOutput(frames, leftOut, rightOut);
	m_impl->projectTime += frames;
	m_impl->pendingMidi.clear();
	m_impl->queuedParameters.clear();
}

int Vst3Host::latencySamples() const
{
	if (m_impl->processor == nullptr)
	{
		return 0;
	}
	return static_cast<int>(v3_cpp_obj(m_impl->processor)->get_latency_samples(
		m_impl->processor));
}

bool Vst3Host::hasEditor() const
{
	return m_impl->controller != nullptr;
}

bool Vst3Host::openEditor(void* parentWindow, QString* error)
{
	if (m_impl->editorView != nullptr)
	{
		return true;
	}
	if (m_impl->controller == nullptr)
	{
		setError(error, QStringLiteral("Plugin has no edit controller"));
		return false;
	}

	auto* const view = v3_cpp_obj(m_impl->controller)->create_view(
		m_impl->controller, "editor");
	if (view == nullptr)
	{
		setError(error, QStringLiteral("Plugin did not create an editor view"));
		return false;
	}
	// V3_TRUE and V3_OK are both zero in Travesty.
	if (v3_cpp_obj(view)->is_platform_type_supported(
			view, V3_VIEW_PLATFORM_TYPE_X11) != V3_OK)
	{
		setError(error, QStringLiteral("Plugin does not support X11 embedding"));
		v3_cpp_obj_unref(view);
		return false;
	}

	m_impl->editorFramePointer = &m_impl->editorFrame;
	v3_cpp_obj(view)->set_frame(view,
		reinterpret_cast<v3_plugin_frame**>(&m_impl->editorFramePointer));
	if (v3_cpp_obj(view)->attached(
			view, parentWindow, V3_VIEW_PLATFORM_TYPE_X11) != V3_OK)
	{
		setError(error, QStringLiteral("Plugin failed to attach its editor"));
		v3_cpp_obj(view)->set_frame(view, nullptr);
		v3_cpp_obj_unref(view);
		return false;
	}

	m_impl->editorView = view;
	return true;
}

void Vst3Host::closeEditor()
{
	if (m_impl->editorView == nullptr)
	{
		return;
	}
	v3_cpp_obj(m_impl->editorView)->set_frame(m_impl->editorView, nullptr);
	v3_cpp_obj(m_impl->editorView)->removed(m_impl->editorView);
	v3_cpp_obj_unref(m_impl->editorView);
	m_impl->editorView = nullptr;
}

bool Vst3Host::isEditorOpen() const
{
	return m_impl->editorView != nullptr;
}

bool Vst3Host::editorSize(int& width, int& height) const
{
	if (m_impl->editorView == nullptr)
	{
		return false;
	}
	v3_view_rect rect = {};
	if (v3_cpp_obj(m_impl->editorView)->get_size(
			m_impl->editorView, &rect) != V3_OK)
	{
		return false;
	}
	width = std::max(1, rect.right - rect.left);
	height = std::max(1, rect.bottom - rect.top);
	return true;
}

bool Vst3Host::canResizeEditor() const
{
	if (m_impl->editorView == nullptr)
	{
		return false;
	}
	return v3_cpp_obj(m_impl->editorView)->can_resize(
		m_impl->editorView) == V3_TRUE;
}

void Vst3Host::resizeEditor(int width, int height)
{
	if (m_impl->editorView == nullptr || width <= 0 || height <= 0)
	{
		return;
	}
	v3_view_rect rect = {0, 0, width, height};
	v3_cpp_obj(m_impl->editorView)->on_size(m_impl->editorView, &rect);
}

void Vst3Host::setEditorContentScale(float scale)
{
	if (m_impl->editorView == nullptr || scale <= 0.0f)
	{
		return;
	}
	v3_plugin_view_content_scale** contentScale = nullptr;
	if (v3_cpp_obj_query_interface(m_impl->editorView,
			v3_plugin_view_content_scale_iid, &contentScale) == V3_OK
		&& contentScale != nullptr)
	{
		v3_cpp_obj(contentScale)->set_content_scale_factor(contentScale, scale);
		v3_cpp_obj_unref(contentScale);
	}
}

void Vst3Host::focusEditor(bool focused)
{
	if (m_impl->editorView == nullptr)
	{
		return;
	}
	v3_cpp_obj(m_impl->editorView)->on_focus(
		m_impl->editorView, focused ? V3_TRUE : V3_FALSE);
}

void Vst3Host::setEditorResizeCallback(std::function<void(int, int)> callback)
{
	m_impl->editorResizeCallback = std::move(callback);
}

QByteArray Vst3Host::componentState() const
{
	std::lock_guard<std::recursive_mutex> lock(m_impl->hostMutex);
	if (m_impl->component == nullptr)
	{
		return {};
	}
	MemoryStream stream;
	MemoryStream* streamObject = &stream;
	if (v3_cpp_obj(m_impl->component)->get_state(m_impl->component,
			reinterpret_cast<v3_bstream**>(&streamObject)) != V3_OK)
	{
		return {};
	}
	return QByteArray(reinterpret_cast<const char*>(stream.data.data()),
		static_cast<int>(stream.data.size()));
}

QByteArray Vst3Host::controllerState() const
{
	std::lock_guard<std::recursive_mutex> lock(m_impl->hostMutex);
	if (m_impl->controller == nullptr)
	{
		return {};
	}
	MemoryStream stream;
	MemoryStream* streamObject = &stream;
	if (v3_cpp_obj(m_impl->controller)->get_state(m_impl->controller,
			reinterpret_cast<v3_bstream**>(&streamObject)) != V3_OK)
	{
		return {};
	}
	return QByteArray(reinterpret_cast<const char*>(stream.data.data()),
		static_cast<int>(stream.data.size()));
}

bool Vst3Host::setState(
	const QByteArray& componentState, const QByteArray& controllerState)
{
	std::lock_guard<std::recursive_mutex> lock(m_impl->hostMutex);
	if (m_impl->component == nullptr)
	{
		return false;
	}

	MemoryStream componentStream;
	componentStream.data.assign(componentState.begin(), componentState.end());
	MemoryStream* componentObject = &componentStream;
	v3_bstream** componentStreamPtr =
		reinterpret_cast<v3_bstream**>(&componentObject);
	if (v3_cpp_obj(m_impl->component)->set_state(
			m_impl->component, componentStreamPtr) != V3_OK)
	{
		return false;
	}

	if (m_impl->controller != nullptr && m_impl->terminateController)
	{
		componentStream.position = 0;
		v3_cpp_obj(m_impl->controller)->set_component_state(
			m_impl->controller, componentStreamPtr);
	}

	if (m_impl->controller != nullptr && !controllerState.isEmpty())
	{
		MemoryStream controllerStream;
		controllerStream.data.assign(controllerState.begin(), controllerState.end());
		MemoryStream* controllerObject = &controllerStream;
		v3_bstream** controllerStreamPtr =
			reinterpret_cast<v3_bstream**>(&controllerObject);
		if (v3_cpp_obj(m_impl->controller)->set_state(
				m_impl->controller, controllerStreamPtr) != V3_OK)
		{
			return false;
		}
	}

	m_impl->setupParameters();
	return true;
}

} // namespace lmms
