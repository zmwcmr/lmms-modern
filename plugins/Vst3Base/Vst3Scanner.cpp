/*
 * Vst3Scanner.cpp - metadata extraction for VST3 bundles
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

#include "Vst3Scanner.h"

#include <cstring>
#include <utility>

#include <QString>

#include "Vst3Module.h"

#include "travesty/audio_processor.h"
#include "travesty/component.h"
#include "travesty/factory.h"
#include "travesty/host.h"

namespace fs = std::filesystem;

namespace
{

QString fixedUtf8(const char* text, size_t size)
{
	return QString::fromUtf8(text, static_cast<int>(strnlen(text, size)));
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
QString fixedUtf16(const int16_t* text, size_t size)
{
	size_t length = 0;
	while (length < size && text[length] != 0)
	{
		++length;
	}
	return QString::fromUtf16(reinterpret_cast<const char16_t*>(text), length);
}
#else
QString fixedUtf16(const int16_t* text, size_t size)
{
	size_t length = 0;
	while (length < size && text[length] != 0)
	{
		++length;
	}
	return QString::fromUtf16(reinterpret_cast<const ushort*>(text), static_cast<int>(length));
}
#endif

QString tuidToString(const v3_tuid id)
{
	QString result;
	result.reserve(32);
	for (int i = 0; i < 16; ++i)
	{
		result += QStringLiteral("%1").arg(id[i], 2, 16, QLatin1Char('0'));
	}
	return result;
}

/**
 * Minimal IHostApplication. Most plugins only query the host name while
 * scanning; createInstance is only needed for advanced host/extensions.
 */
struct ScannerHostApplication : v3_host_application_cpp
{
	ScannerHostApplication()
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

void releaseFactories(lmms::Vst3Module& module)
{
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
}

bool isAudioModuleClass(const QString& category)
{
	return category.trimmed() == QLatin1String("Audio Module Class");
}

bool containsInstrument(const QString& subCategories)
{
	return subCategories.contains(QLatin1String("Instrument"), Qt::CaseInsensitive);
}

void addBusInfo(v3_component** component, lmms::Vst3ClassInfo& info)
{
	auto* const componentMethods = v3_cpp_obj(component);

	int inputBuses = componentMethods->get_bus_count(component, V3_AUDIO, V3_INPUT);
	int outputBuses = componentMethods->get_bus_count(component, V3_AUDIO, V3_OUTPUT);
	info.inputBuses = inputBuses > 0 ? inputBuses : 0;
	info.outputBuses = outputBuses > 0 ? outputBuses : 0;

	bool haveMainInput = false;
	bool haveMainOutput = false;
	for (int bus = 0; bus < info.inputBuses; ++bus)
	{
		v3_bus_info busInfo = {};
		if (componentMethods->get_bus_info(component, V3_AUDIO, V3_INPUT, bus, &busInfo) != V3_OK)
		{
			continue;
		}
		info.inputChannels += busInfo.channel_count;
		if (busInfo.bus_type == V3_MAIN && !haveMainInput)
		{
			info.mainInputChannels = busInfo.channel_count;
			haveMainInput = true;
		}
	}
	for (int bus = 0; bus < info.outputBuses; ++bus)
	{
		v3_bus_info busInfo = {};
		if (componentMethods->get_bus_info(component, V3_AUDIO, V3_OUTPUT, bus, &busInfo) != V3_OK)
		{
			continue;
		}
		info.outputChannels += busInfo.channel_count;
		if (busInfo.bus_type == V3_MAIN && !haveMainOutput)
		{
			info.mainOutputChannels = busInfo.channel_count;
			haveMainOutput = true;
		}
	}

	// Fall back to the first bus for plugins which do not flag a main bus.
	if (!haveMainInput && info.inputBuses > 0)
	{
		v3_bus_info busInfo = {};
		if (componentMethods->get_bus_info(component, V3_AUDIO, V3_INPUT, 0, &busInfo) == V3_OK)
		{
			info.mainInputChannels = busInfo.channel_count;
		}
	}
	if (!haveMainOutput && info.outputBuses > 0)
	{
		v3_bus_info busInfo = {};
		if (componentMethods->get_bus_info(component, V3_AUDIO, V3_OUTPUT, 0, &busInfo) == V3_OK)
		{
			info.mainOutputChannels = busInfo.channel_count;
		}
	}

	// Event buses carry MIDI note and controller data.
	const int midiInputs = componentMethods->get_bus_count(component, V3_EVENT, V3_INPUT);
	const int midiOutputs = componentMethods->get_bus_count(component, V3_EVENT, V3_OUTPUT);
	info.midiInputs = midiInputs > 0 ? midiInputs : 0;
	info.midiOutputs = midiOutputs > 0 ? midiOutputs : 0;
}

} // namespace

namespace lmms
{

bool Vst3Scanner::scanBundle(
	const std::filesystem::path& bundlePath,
	Vst3BundleInfo& result,
	QString* error)
{
	result = {};
	result.path = bundlePath;

	Vst3Module module;
	QString loadError;
	if (!module.load(bundlePath, &loadError))
	{
		result.error = loadError;
		if (error != nullptr)
		{
			*error = loadError;
		}
		return false;
	}

	auto* const factory = module.factory();
	if (factory == nullptr)
	{
		result.error = QStringLiteral("No VST3 plugin factory");
		releaseFactories(module);
		if (error != nullptr)
		{
			*error = result.error;
		}
		return false;
	}

	ScannerHostApplication hostApplication;
	v3_funknown* hostPointer = &hostApplication;
	v3_funknown** hostContext = &hostPointer;

	if (module.factory3() != nullptr)
	{
		v3_cpp_obj(module.factory3())->set_host_context(module.factory3(), hostContext);
	}

	v3_factory_info factoryInfo = {};
	v3_cpp_obj(factory)->get_factory_info(factory, &factoryInfo);

	const int32_t numClasses = v3_cpp_obj(factory)->num_classes(factory);
	for (int32_t index = 0; index < numClasses; ++index)
	{
		Vst3ClassInfo info;
		v3_tuid classId = {};

		if (module.factory3() != nullptr)
		{
			v3_class_info_3 classInfo = {};
			if (v3_cpp_obj(module.factory3())->get_class_info_utf16(
					module.factory3(), index, &classInfo) != V3_OK)
			{
				continue;
			}
			std::memcpy(classId, classInfo.class_id, sizeof(classId));
			info.category = fixedUtf8(classInfo.category, sizeof(classInfo.category));
			info.subCategories = fixedUtf8(
				classInfo.sub_categories, sizeof(classInfo.sub_categories));
			info.name = fixedUtf16(classInfo.name,
				sizeof(classInfo.name) / sizeof(classInfo.name[0]));
			info.vendor = fixedUtf16(classInfo.vendor,
				sizeof(classInfo.vendor) / sizeof(classInfo.vendor[0]));
			info.version = fixedUtf16(classInfo.version,
				sizeof(classInfo.version) / sizeof(classInfo.version[0]));
			info.sdkVersion = fixedUtf16(classInfo.sdk_version,
				sizeof(classInfo.sdk_version) / sizeof(classInfo.sdk_version[0]));
		}
		else if (module.factory2() != nullptr)
		{
			v3_class_info_2 classInfo = {};
			if (v3_cpp_obj(module.factory2())->get_class_info_2(
					module.factory2(), index, &classInfo) != V3_OK)
			{
				continue;
			}
			std::memcpy(classId, classInfo.class_id, sizeof(classId));
			info.category = fixedUtf8(classInfo.category, sizeof(classInfo.category));
			info.subCategories = fixedUtf8(
				classInfo.sub_categories, sizeof(classInfo.sub_categories));
			info.name = fixedUtf8(classInfo.name, sizeof(classInfo.name));
			info.vendor = fixedUtf8(classInfo.vendor, sizeof(classInfo.vendor));
			info.version = fixedUtf8(classInfo.version, sizeof(classInfo.version));
			info.sdkVersion = fixedUtf8(classInfo.sdk_version, sizeof(classInfo.sdk_version));
		}
		else
		{
			v3_class_info classInfo = {};
			if (v3_cpp_obj(factory)->get_class_info(factory, index, &classInfo) != V3_OK)
			{
				continue;
			}
			std::memcpy(classId, classInfo.class_id, sizeof(classId));
			info.category = fixedUtf8(classInfo.category, sizeof(classInfo.category));
			info.name = fixedUtf8(classInfo.name, sizeof(classInfo.name));
			info.vendor = fixedUtf8(factoryInfo.vendor, sizeof(factoryInfo.vendor));
		}

		if (!isAudioModuleClass(info.category))
		{
			continue;
		}

		info.classId = tuidToString(classId);
		info.isInstrument = containsInstrument(info.subCategories);

		void* instance = nullptr;
		if (v3_cpp_obj(factory)->create_instance(
				factory, classId, v3_component_iid, &instance) != V3_OK
			|| instance == nullptr)
		{
			info.unsupportedReason = QStringLiteral("Unable to create component");
			result.classes.push_back(std::move(info));
			continue;
		}

		auto component = static_cast<v3_component**>(instance);
		if (v3_cpp_obj_initialize(component, hostContext) != V3_OK)
		{
			info.unsupportedReason = QStringLiteral("Component initialization failed");
			v3_cpp_obj_unref(component);
			result.classes.push_back(std::move(info));
			continue;
		}

		addBusInfo(component, info);

		v3_audio_processor** processor = nullptr;
		if (v3_cpp_obj_query_interface(
				component, v3_audio_processor_iid, &processor) != V3_OK)
		{
			processor = nullptr;
		}
		if (processor != nullptr)
		{
			info.supports32Bit = v3_cpp_obj(processor)->can_process_sample_size(
				processor, V3_SAMPLE_32) == V3_OK;
			v3_cpp_obj_unref(processor);
		}

		if (!info.supports32Bit)
		{
			info.unsupportedReason = QStringLiteral("Plugin does not support 32-bit audio");
		}
		else if (info.mainOutputChannels <= 0)
		{
			info.unsupportedReason = QStringLiteral("Plugin has no main audio output");
		}
		else
		{
			info.supported = true;
			if (info.mainOutputChannels > 2)
			{
				info.warning = QStringLiteral(
					"Plugin has more than two output channels; only the first stereo pair is used");
			}
			if (info.mainInputChannels > 2)
			{
				info.warning += info.warning.isEmpty() ? QString() : QStringLiteral(" ");
				info.warning += QStringLiteral(
					"Plugin has more than two input channels; only the first stereo pair is used");
			}
		}

		v3_cpp_obj_terminate(component);
		v3_cpp_obj_unref(component);
		result.classes.push_back(std::move(info));
	}

	releaseFactories(module);
	module.unload();
	return true;
}

} // namespace lmms
