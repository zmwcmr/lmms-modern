/*
 * fake-vst3.cpp - minimal VST3 test module
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

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "travesty/audio_processor.h"
#include "travesty/bstream.h"
#include "travesty/component.h"
#include "travesty/edit_controller.h"
#include "travesty/factory.h"
#include "travesty/host.h"
#include "travesty/message.h"

namespace
{

constexpr double pi = 3.14159265358979323846;

const uint8_t kInstrumentId[16] = {
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
	0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x00, 0x01,
};
const uint8_t kEffectId[16] = {
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
	0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x00, 0x02,
};

bool matches(const v3_tuid a, const uint8_t* b)
{
	return std::memcmp(a, b, 16) == 0;
}

void setUtf16(v3_str_128 out, const char* text)
{
	const size_t length = std::strlen(text);
	for (size_t i = 0; i < length && i < 127; ++i)
	{
		out[i] = static_cast<int16_t>(text[i]);
	}
	out[length < 127 ? length : 127] = 0;
}

void setString(char* out, size_t size, const char* text)
{
	std::strncpy(out, text, size - 1);
	out[size - 1] = 0;
}

struct FakeState
{
	bool instrument = true;
	double gain = 0.5;
	double sampleRate = 48000.0;
	bool active = false;
	double phase = 0.0;
};

struct FakeProcessor : v3_audio_processor_cpp
{
	FakeState* state = nullptr;
	v3_audio_processor* exposedPointer = nullptr;

	explicit FakeProcessor(FakeState* s) : state(s)
	{
		exposedPointer = reinterpret_cast<v3_audio_processor*>(this);
		query_interface = queryInterface;
		ref = refStatic;
		unref = unrefStatic;
		proc.set_bus_arrangements = setBusArrangements;
		proc.get_bus_arrangement = getBusArrangement;
		proc.can_process_sample_size = canProcessSampleSize;
		proc.get_latency_samples = getLatencySamples;
		proc.setup_processing = setupProcessing;
		proc.set_processing = setProcessing;
		proc.process = process;
		proc.get_tail_samples = getTailSamples;
	}

private:
	static FakeProcessor* processor(void* self)
	{
		return *static_cast<FakeProcessor**>(self);
	}

	static v3_result V3_API queryInterface(void* self, const v3_tuid iid, void** iface)
	{
		if (v3_tuid_match(iid, v3_funknown_iid)
			|| v3_tuid_match(iid, v3_audio_processor_iid))
		{
			*iface = &processor(self)->exposedPointer;
			return V3_OK;
		}
		*iface = nullptr;
		return V3_NO_INTERFACE;
	}

	static uint32_t V3_API refStatic(void*) { return 1; }
	static uint32_t V3_API unrefStatic(void*) { return 0; }

	static v3_result V3_API setBusArrangements(
		void*, v3_speaker_arrangement*, int32_t, v3_speaker_arrangement*, int32_t)
	{
		return V3_OK;
	}

	static v3_result V3_API getBusArrangement(
		void*, int32_t, int32_t, v3_speaker_arrangement* arrangement)
	{
		if (arrangement != nullptr)
		{
			*arrangement = V3_SPEAKER_L | V3_SPEAKER_R;
		}
		return V3_OK;
	}

	static v3_result V3_API canProcessSampleSize(void*, int32_t size)
	{
		return size == V3_SAMPLE_32 ? V3_TRUE : V3_FALSE;
	}

	static uint32_t V3_API getLatencySamples(void*) { return 0; }

	static v3_result V3_API setupProcessing(void* self, v3_process_setup* setup)
	{
		if (setup == nullptr)
		{
			return V3_INVALID_ARG;
		}
		processor(self)->state->sampleRate = setup->sample_rate;
		return V3_OK;
	}

	static v3_result V3_API setProcessing(void* self, v3_bool active)
	{
		processor(self)->state->active = active != 0;
		return V3_OK;
	}

	static v3_result V3_API process(void* self, v3_process_data* data)
	{
		if (data == nullptr || data->nframes <= 0)
		{
			return V3_INVALID_ARG;
		}
		FakeState* state = processor(self)->state;
		if (data->num_output_buses <= 0 || data->outputs == nullptr)
		{
			return V3_OK;
		}
		auto& output = data->outputs[0];
		if (output.num_channels < 2 || output.channel_buffers_32 == nullptr)
		{
			return V3_OK;
		}

		if (state->instrument)
		{
			const double increment = 2.0 * pi * 440.0 / state->sampleRate;
			for (int32_t frame = 0; frame < data->nframes; ++frame)
			{
				const float sample = static_cast<float>(0.2 * std::sin(state->phase));
				output.channel_buffers_32[0][frame] = sample;
				output.channel_buffers_32[1][frame] = sample;
				state->phase += increment;
				if (state->phase > 2.0 * pi)
				{
					state->phase -= 2.0 * pi;
				}
			}
		}
		else
		{
			const float gain = static_cast<float>(state->gain);
			const bool hasInput = data->num_input_buses > 0 && data->inputs != nullptr
				&& data->inputs[0].num_channels >= 2
				&& data->inputs[0].channel_buffers_32 != nullptr;
			for (int32_t frame = 0; frame < data->nframes; ++frame)
			{
				const float left = hasInput ? data->inputs[0].channel_buffers_32[0][frame] : 0.0f;
				const float right = hasInput ? data->inputs[0].channel_buffers_32[1][frame] : 0.0f;
				output.channel_buffers_32[0][frame] = left * gain;
				output.channel_buffers_32[1][frame] = right * gain;
			}
		}
		return V3_OK;
	}

	static uint32_t V3_API getTailSamples(void*) { return 0; }
};

struct FakeController : v3_edit_controller_cpp
{
	FakeState* state = nullptr;
	v3_edit_controller* exposedPointer = nullptr;

	explicit FakeController(FakeState* s) : state(s)
	{
		exposedPointer = reinterpret_cast<v3_edit_controller*>(this);
		query_interface = queryInterface;
		ref = refStatic;
		unref = unrefStatic;
		base.initialize = initialize;
		base.terminate = terminate;
		ctrl.set_component_state = setState;
		ctrl.set_state = setState;
		ctrl.get_state = getState;
		ctrl.get_parameter_count = getParameterCount;
		ctrl.get_parameter_info = getParameterInfo;
		ctrl.get_parameter_string_for_value = getParameterString;
		ctrl.get_parameter_value_for_string = getParameterValue;
		ctrl.normalised_parameter_to_plain = normalisedToPlain;
		ctrl.plain_parameter_to_normalised = plainToNormalised;
		ctrl.get_parameter_normalised = getParameterNormalised;
		ctrl.set_parameter_normalised = setParameterNormalised;
		ctrl.set_component_handler = setComponentHandler;
		ctrl.create_view = createView;
	}

private:
	static FakeController* controller(void* self)
	{
		return *static_cast<FakeController**>(self);
	}

	static v3_result V3_API queryInterface(void* self, const v3_tuid iid, void** iface)
	{
		if (v3_tuid_match(iid, v3_funknown_iid)
			|| v3_tuid_match(iid, v3_edit_controller_iid))
		{
			*iface = &controller(self)->exposedPointer;
			return V3_OK;
		}
		*iface = nullptr;
		return V3_NO_INTERFACE;
	}

	static uint32_t V3_API refStatic(void*) { return 1; }
	static uint32_t V3_API unrefStatic(void*) { return 0; }
	static v3_result V3_API initialize(void*, v3_funknown**) { return V3_OK; }
	static v3_result V3_API terminate(void*) { return V3_OK; }

	static v3_result V3_API setState(void* self, v3_bstream** stream)
	{
		if (stream == nullptr || *stream == nullptr)
		{
			return V3_INVALID_ARG;
		}
		float gain = 0.0f;
		int32_t read = 0;
		if (v3_cpp_obj(stream)->read(stream, &gain, sizeof(gain), &read) != V3_OK
			|| read != sizeof(gain))
		{
			return V3_INVALID_ARG;
		}
		controller(self)->state->gain = std::clamp(static_cast<double>(gain), 0.0, 1.0);
		return V3_OK;
	}

	static v3_result V3_API getState(void* self, v3_bstream** stream)
	{
		if (stream == nullptr || *stream == nullptr)
		{
			return V3_INVALID_ARG;
		}
		const float gain = static_cast<float>(controller(self)->state->gain);
		int32_t written = 0;
		if (v3_cpp_obj(stream)->write(stream, const_cast<float*>(&gain),
				sizeof(gain), &written) != V3_OK
			|| written != sizeof(gain))
		{
			return V3_INVALID_ARG;
		}
		return V3_OK;
	}

	static int32_t V3_API getParameterCount(void*) { return 1; }

	static v3_result V3_API getParameterInfo(void*, int32_t index, v3_param_info* info)
	{
		if (index != 0 || info == nullptr)
		{
			return V3_INVALID_ARG;
		}
		std::memset(info, 0, sizeof(*info));
		info->param_id = 1;
		setUtf16(info->title, "Gain");
		setUtf16(info->short_title, "Gain");
		setUtf16(info->units, "%");
		info->step_count = 0;
		info->default_normalised_value = 0.5;
		info->flags = V3_PARAM_CAN_AUTOMATE;
		return V3_OK;
	}

	static v3_result V3_API getParameterString(
		void*, v3_param_id id, double normalised, v3_str_128 output)
	{
		if (id != 1)
		{
			return V3_INVALID_ARG;
		}
		char text[32] = {};
		std::snprintf(text, sizeof(text), "%d", static_cast<int>(normalised * 100.0));
		setUtf16(output, text);
		return V3_OK;
	}

	static v3_result V3_API getParameterValue(void*, v3_param_id, int16_t*, double*)
	{
		return V3_NOT_IMPLEMENTED;
	}

	static double V3_API normalisedToPlain(void*, v3_param_id, double normalised)
	{
		return normalised * 100.0;
	}

	static double V3_API plainToNormalised(void*, v3_param_id, double plain)
	{
		return std::clamp(plain / 100.0, 0.0, 1.0);
	}

	static double V3_API getParameterNormalised(void* self, v3_param_id id)
	{
		return id == 1 ? controller(self)->state->gain : 0.0;
	}

	static v3_result V3_API setParameterNormalised(
		void* self, v3_param_id id, double normalised)
	{
		if (id != 1)
		{
			return V3_INVALID_ARG;
		}
		controller(self)->state->gain = std::clamp(normalised, 0.0, 1.0);
		return V3_OK;
	}

	static v3_result V3_API setComponentHandler(void*, v3_component_handler**)
	{
		return V3_OK;
	}

	static v3_plugin_view** createView(void*, const char*)
	{
		return nullptr;
	}
};

struct FakeComponent : v3_component_cpp
{
	FakeState state;
	FakeProcessor processor;
	FakeController controller;
	void* externalPointer = nullptr;
	v3_component* componentPointer = nullptr;
	v3_audio_processor* processorPointer = nullptr;
	v3_edit_controller* controllerPointer = nullptr;

	explicit FakeComponent(bool instrument) :
		processor(&state),
		controller(&state)
	{
		state.instrument = instrument;
		componentPointer = reinterpret_cast<v3_component*>(this);
		processorPointer = reinterpret_cast<v3_audio_processor*>(&processor);
		controllerPointer = reinterpret_cast<v3_edit_controller*>(&controller);
		query_interface = queryInterface;
		ref = refStatic;
		unref = unrefStatic;
		base.initialize = initialize;
		base.terminate = terminate;
		comp.get_controller_class_id = getControllerClassId;
		comp.set_io_mode = setIoMode;
		comp.get_bus_count = getBusCount;
		comp.get_bus_info = getBusInfo;
		comp.get_routing_info = getRoutingInfo;
		comp.activate_bus = activateBus;
		comp.set_active = setActive;
		comp.set_state = setState;
		comp.get_state = getState;
	}

private:
	static FakeComponent* component(void* self)
	{
		return *static_cast<FakeComponent**>(self);
	}

	static v3_result V3_API queryInterface(void* self, const v3_tuid iid, void** iface)
	{
		auto* const object = component(self);
		if (v3_tuid_match(iid, v3_funknown_iid)
			|| v3_tuid_match(iid, v3_component_iid))
		{
			*iface = &object->componentPointer;
			return V3_OK;
		}
		if (v3_tuid_match(iid, v3_edit_controller_iid))
		{
			*iface = &object->controllerPointer;
			return V3_OK;
		}
		if (v3_tuid_match(iid, v3_audio_processor_iid))
		{
			*iface = &object->processorPointer;
			return V3_OK;
		}
		*iface = nullptr;
		return V3_NO_INTERFACE;
	}

	static uint32_t V3_API refStatic(void*) { return 1; }
	static uint32_t V3_API unrefStatic(void* self)
	{
		delete component(self);
		return 0;
	}

	static v3_result V3_API initialize(void*, v3_funknown**) { return V3_OK; }
	static v3_result V3_API terminate(void*) { return V3_OK; }

	static v3_result V3_API getControllerClassId(void*, v3_tuid classId)
	{
		std::memset(classId, 0, 16);
		return V3_OK;
	}

	static v3_result V3_API setIoMode(void*, int32_t) { return V3_OK; }

	static int32_t V3_API getBusCount(void* self, int32_t media, int32_t direction)
	{
		const auto* const object = component(self);
		if (media == V3_AUDIO)
		{
			if (object->state.instrument)
			{
				return direction == V3_OUTPUT ? 1 : 0;
			}
			return 1;
		}
		if (media == V3_EVENT)
		{
			return direction == V3_INPUT ? 1 : 0;
		}
		return 0;
	}

	static v3_result V3_API getBusInfo(void*, int32_t media, int32_t direction,
		int32_t index, v3_bus_info* info)
	{
		if (info == nullptr || index != 0)
		{
			return V3_INVALID_ARG;
		}
		std::memset(info, 0, sizeof(*info));
		info->media_type = media;
		info->direction = direction;
		info->bus_type = V3_MAIN;
		info->flags = V3_DEFAULT_ACTIVE;
		if (media == V3_AUDIO)
		{
			info->channel_count = 2;
		}
		else if (media == V3_EVENT)
		{
			info->channel_count = 16;
		}
		return V3_OK;
	}

	static v3_result V3_API getRoutingInfo(void*, v3_routing_info*, v3_routing_info*)
	{
		return V3_OK;
	}

	static v3_result V3_API activateBus(void*, int32_t, int32_t, int32_t, v3_bool)
	{
		return V3_OK;
	}

	static v3_result V3_API setActive(void* self, v3_bool active)
	{
		component(self)->state.active = active != 0;
		return V3_OK;
	}

	static v3_result V3_API setState(void* self, v3_bstream** stream)
	{
		if (stream == nullptr || *stream == nullptr)
		{
			return V3_INVALID_ARG;
		}
		float gain = 0.0f;
		int32_t read = 0;
		if (v3_cpp_obj(stream)->read(stream, &gain, sizeof(gain), &read) != V3_OK
			|| read != sizeof(gain))
		{
			return V3_INVALID_ARG;
		}
		component(self)->state.gain = std::clamp(static_cast<double>(gain), 0.0, 1.0);
		return V3_OK;
	}

	static v3_result V3_API getState(void* self, v3_bstream** stream)
	{
		if (stream == nullptr || *stream == nullptr)
		{
			return V3_INVALID_ARG;
		}
		const float gain = static_cast<float>(component(self)->state.gain);
		int32_t written = 0;
		if (v3_cpp_obj(stream)->write(stream, const_cast<float*>(&gain),
				sizeof(gain), &written) != V3_OK
			|| written != sizeof(gain))
		{
			return V3_INVALID_ARG;
		}
		return V3_OK;
	}
};

struct FakeFactory : v3_plugin_factory_cpp
{
	int refcount = 1;
	v3_plugin_factory* v1Pointer = nullptr;
	v3_plugin_factory_2* v2Pointer = nullptr;

	FakeFactory()
	{
		v1Pointer = reinterpret_cast<v3_plugin_factory*>(this);
		v2Pointer = reinterpret_cast<v3_plugin_factory_2*>(this);
		query_interface = queryInterface;
		ref = refStatic;
		unref = unrefStatic;
		v1.get_factory_info = getFactoryInfo;
		v1.num_classes = numClasses;
		v1.get_class_info = getClassInfo;
		v1.create_instance = createInstance;
		v2.get_class_info_2 = getClassInfo2;
		std::memset(&v3, 0, sizeof(v3));
	}

private:
	static FakeFactory* factory(void* self)
	{
		return *static_cast<FakeFactory**>(self);
	}

	static v3_result V3_API queryInterface(void* self, const v3_tuid iid, void** iface)
	{
		if (v3_tuid_match(iid, v3_funknown_iid)
			|| v3_tuid_match(iid, v3_plugin_factory_iid))
		{
			*iface = &factory(self)->v1Pointer;
			return V3_OK;
		}
		if (v3_tuid_match(iid, v3_plugin_factory_2_iid))
		{
			auto* const object = factory(self);
			++object->refcount;
			*iface = &object->v2Pointer;
			return V3_OK;
		}
		*iface = nullptr;
		return V3_NO_INTERFACE;
	}

	static uint32_t V3_API refStatic(void* self)
	{
		return static_cast<uint32_t>(++factory(self)->refcount);
	}

	static uint32_t V3_API unrefStatic(void* self)
	{
		auto* const object = factory(self);
		if (--object->refcount == 0)
		{
			delete object;
			return 0;
		}
		return static_cast<uint32_t>(object->refcount);
	}

	static v3_result V3_API getFactoryInfo(void*, v3_factory_info* info)
	{
		if (info == nullptr)
		{
			return V3_INVALID_ARG;
		}
		std::memset(info, 0, sizeof(*info));
		setString(info->vendor, sizeof(info->vendor), "LMMS Tests");
		setString(info->url, sizeof(info->url), "https://lmms.io");
		setString(info->email, sizeof(info->email), "tests@lmms.io");
		return V3_OK;
	}

	static int32_t V3_API numClasses(void*) { return 2; }

	static v3_result V3_API getClassInfo(void*, int32_t index, v3_class_info* info)
	{
		if (info == nullptr || index < 0 || index > 1)
		{
			return V3_INVALID_ARG;
		}
		std::memset(info, 0, sizeof(*info));
		std::memcpy(info->class_id, index == 0 ? kInstrumentId : kEffectId, 16);
		info->cardinality = 0x7FFFFFFF;
		setString(info->category, sizeof(info->category), "Audio Module Class");
		setString(info->name, sizeof(info->name),
			index == 0 ? "Fake VST3 Instrument" : "Fake VST3 Effect");
		return V3_OK;
	}

	static v3_result V3_API getClassInfo2(void*, int32_t index, v3_class_info_2* info)
	{
		if (info == nullptr || index < 0 || index > 1)
		{
			return V3_INVALID_ARG;
		}
		std::memset(info, 0, sizeof(*info));
		std::memcpy(info->class_id, index == 0 ? kInstrumentId : kEffectId, 16);
		info->cardinality = 0x7FFFFFFF;
		setString(info->category, sizeof(info->category), "Audio Module Class");
		setString(info->name, sizeof(info->name),
			index == 0 ? "Fake VST3 Instrument" : "Fake VST3 Effect");
		setString(info->sub_categories, sizeof(info->sub_categories),
			index == 0 ? "Instrument|Synth" : "Fx");
		setString(info->vendor, sizeof(info->vendor), "LMMS Tests");
		setString(info->version, sizeof(info->version), "1.0.0");
		setString(info->sdk_version, sizeof(info->sdk_version), "VST 3.7.0");
		return V3_OK;
	}

	static v3_result V3_API createInstance(void*, const v3_tuid classId,
		const v3_tuid iid, void** instance)
	{
		if (instance == nullptr)
		{
			return V3_INVALID_ARG;
		}
		*instance = nullptr;
		const bool instrument = matches(classId, kInstrumentId);
		if (!instrument && !matches(classId, kEffectId))
		{
			return V3_INVALID_ARG;
		}
		if (!v3_tuid_match(iid, v3_component_iid))
		{
			return V3_NO_INTERFACE;
		}
		auto* const component = new FakeComponent(instrument);
		component->externalPointer = component;
		*instance = &component->externalPointer;
		return V3_OK;
	}
};

} // namespace

extern "C" __attribute__((visibility("default"))) bool ModuleEntry(void*)
{
	return true;
}

extern "C" __attribute__((visibility("default"))) bool ModuleExit()
{
	return true;
}

extern "C" __attribute__((visibility("default"))) v3_plugin_factory** GetPluginFactory()
{
	static FakeFactory* factory = new FakeFactory();
	static v3_plugin_factory* pointer = reinterpret_cast<v3_plugin_factory*>(factory);
	return &pointer;
}
