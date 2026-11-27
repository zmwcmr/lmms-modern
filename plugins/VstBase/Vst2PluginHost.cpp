/*
 * Vst2PluginHost.cpp - PluginHost adapter for VST2
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

#include "Vst2PluginHost.h"

#include <algorithm>
#include <exception>

#include <QDomDocument>
#include <QDomElement>

#include "LocaleHelper.h"
#include "ParameterCurve.h"
#include "MidiEvent.h"
#include "VstPlugin.h"

namespace lmms
{

namespace
{

void setError(QString* error, const QString& message)
{
	if (error)
	{
		*error = message;
	}
}

double dumpValue(const QMap<QString, QString>& dump, int index)
{
	const QString entry = dump.value(QStringLiteral("param")
		+ QString::number(index));
	if (entry.isEmpty())
	{
		return 0.0;
	}
	return LocaleHelper::toFloat(entry.section(':', 2, -1));
}

} // namespace




Vst2PluginHost::Vst2PluginHost() = default;
Vst2PluginHost::~Vst2PluginHost() = default;




PluginFormat Vst2PluginHost::format() const
{
	return PluginFormat::Vst2;
}




PluginHostCapabilities Vst2PluginHost::capabilities() const
{
	PluginHostCapabilities capabilities;
	// The Vestige editor stays in the VST2 child for now; the generic editor
	// backend is a later stage.
	capabilities.hasEditor = false;
	capabilities.editorMustShareProcess = false;
	capabilities.supportsPrograms = !m_programNames.isEmpty();
	capabilities.supportsUnits = false;
	capabilities.sampleAccurateParameters = true;
	capabilities.supportsMidiOut = false;
	capabilities.multipleBuses = false;
	capabilities.parameterKind = PluginParameterKind::FlatIndex;
	capabilities.stateKind = PluginStateKind::Chunk;
	return capabilities;
}




bool Vst2PluginHost::load(const PluginLoadRequest& request, QString* error)
{
	unload();

	if (request.path.isEmpty())
	{
		setError(error, QStringLiteral("No VST2 plugin path given"));
		return false;
	}

	try
	{
		m_plugin = m_factory
			? m_factory(request.path)
			: std::make_unique<VstPlugin>(request.path);
	}
	catch (const std::exception& exception)
	{
		setError(error, QString::fromLocal8Bit(exception.what()));
		m_plugin.reset();
		return false;
	}

	if (!m_plugin || m_plugin->failed())
	{
		setError(error, QStringLiteral("Could not load the VST2 plugin '%1'")
			.arg(request.path));
		m_plugin.reset();
		return false;
	}

	m_plugin->loadParameterLabels();
	m_plugin->loadProgramNames();
	const QStringList names = m_plugin->allProgramNames().split('|',
		Qt::SkipEmptyParts);
	m_programNames = names;
	rebuildParameters();
	return true;
}




void Vst2PluginHost::unload()
{
	m_plugin.reset();
	m_parameters.clear();
	m_values.clear();
	m_programNames.clear();
	m_inputFrames.clear();
	m_outputFrames.clear();
}




bool Vst2PluginHost::isLoaded() const
{
	return m_plugin && !m_plugin->failed();
}




std::vector<PluginBusInfo> Vst2PluginHost::inputBuses() const
{
	if (!isLoaded() || m_plugin->inputCount() <= 0)
	{
		return {};
	}
	return { PluginBusInfo{ QStringLiteral("Main"),
		std::min(m_plugin->inputCount(), 2), true, PluginBusRole::Main,
		false } };
}




std::vector<PluginBusInfo> Vst2PluginHost::outputBuses() const
{
	if (!isLoaded() || m_plugin->outputCount() <= 0)
	{
		return {};
	}
	return { PluginBusInfo{ QStringLiteral("Main"),
		std::min(m_plugin->outputCount(), 2), false, PluginBusRole::Main,
		false } };
}




void Vst2PluginHost::rebuildParameters()
{
	m_parameters.clear();
	m_values.clear();
	if (!m_plugin)
	{
		return;
	}

	const auto& dump = m_plugin->parameterDump();
	const auto& labels = m_plugin->allParameterLabels();
	const int count = dump.size();
	for (int index = 0; index < count; ++index)
	{
		PluginParameterInfo info;
		info.id = static_cast<uint32_t>(index);
		info.name = index < static_cast<int>(labels.size())
			&& !labels[static_cast<std::size_t>(index)].isEmpty()
			? labels[static_cast<std::size_t>(index)]
			: QStringLiteral("param%1").arg(index);
		info.min = 0.0;
		info.max = 1.0;
		info.defaultValue = dumpValue(dump, index);
		info.automatable = true;
		m_parameters.push_back(info);
		m_values.push_back(info.defaultValue);
	}
}




void Vst2PluginHost::refreshValues()
{
	if (!m_plugin)
	{
		return;
	}
	const auto& dump = m_plugin->parameterDump();
	for (std::size_t index = 0; index < m_values.size(); ++index)
	{
		m_values[index] = dumpValue(dump, static_cast<int>(index));
	}
}




bool Vst2PluginHost::setParameter(uint32_t id, double value, int sampleOffset)
{
	if (!isLoaded() || id >= m_values.size())
	{
		return false;
	}

	if (sampleOffset > 0)
	{
		const ParameterEvent event{
			sampleOffset,
			static_cast<float>(value)
		};
		m_plugin->setParamEvents(static_cast<int>(id), { event });
	}
	else
	{
		m_plugin->setParam(static_cast<int>(id), static_cast<float>(value));
	}
	m_values[id] = value;
	return true;
}




double Vst2PluginHost::parameterValue(uint32_t id) const
{
	return id < m_values.size() ? m_values[id] : 0.0;
}




void Vst2PluginHost::sendMidi(const PluginMidiEvent& event)
{
	if (!isLoaded())
	{
		return;
	}

	MidiEventTypes type = MidiNoteOn;
	switch (event.type)
	{
		case PluginMidiEvent::Type::NoteOn: type = MidiNoteOn; break;
		case PluginMidiEvent::Type::NoteOff: type = MidiNoteOff; break;
		case PluginMidiEvent::Type::ControlChange:
			type = MidiControlChange; break;
		case PluginMidiEvent::Type::PitchBend: type = MidiPitchBend; break;
		case PluginMidiEvent::Type::ChannelPressure:
			type = MidiChannelPressure; break;
		case PluginMidiEvent::Type::PolyPressure:
			type = MidiKeyPressure; break;
		case PluginMidiEvent::Type::ProgramChange:
			type = MidiProgramChange; break;
		case PluginMidiEvent::Type::SysEx:
			// The remote VST2 protocol only carries three-byte events.
			return;
	}

	m_plugin->processMidiEvent(
		MidiEvent(type, static_cast<int8_t>(event.channel),
			static_cast<int16_t>(event.data1),
			static_cast<int16_t>(event.data2)),
		event.sampleOffset);
}




void Vst2PluginHost::ensureBuffers(int frames)
{
	const auto size = static_cast<std::size_t>(frames);
	if (m_inputFrames.size() < size)
	{
		m_inputFrames.resize(size);
	}
	if (m_outputFrames.size() < size)
	{
		m_outputFrames.resize(size);
	}
}




bool Vst2PluginHost::process(const PluginProcessBlock& block)
{
	if (!isLoaded() || block.frames <= 0 || block.outputChannels <= 0
		|| block.outputs == nullptr || block.outputs[0] == nullptr)
	{
		return false;
	}

	ensureBuffers(block.frames);
	const bool hasInput = block.inputs != nullptr && block.inputChannels > 0
		&& block.inputs[0] != nullptr;
	if (hasInput)
	{
		for (int frame = 0; frame < block.frames; ++frame)
		{
			m_inputFrames[static_cast<std::size_t>(frame)][0] =
				block.inputs[0][frame];
			m_inputFrames[static_cast<std::size_t>(frame)][1] =
				block.inputChannels > 1 && block.inputs[1] != nullptr
				? block.inputs[1][frame] : block.inputs[0][frame];
		}
	}

	if (!m_plugin->process(hasInput ? m_inputFrames.data() : nullptr,
		m_outputFrames.data()))
	{
		return false;
	}

	for (int frame = 0; frame < block.frames; ++frame)
	{
		const auto& source = m_outputFrames[static_cast<std::size_t>(frame)];
		block.outputs[0][frame] = source[0];
		if (block.outputChannels > 1 && block.outputs[1] != nullptr)
		{
			block.outputs[1][frame] = source[1];
		}
	}
	return true;
}




int Vst2PluginHost::latencySamples() const
{
	return isLoaded() ? static_cast<int>(m_plugin->latency()) : 0;
}




void Vst2PluginHost::setTempo(double bpm)
{
	if (isLoaded())
	{
		m_plugin->setTempo(static_cast<bpm_t>(bpm));
	}
}




void Vst2PluginHost::sendParameterEvents(uint32_t id,
	const std::vector<ParameterEvent>& events)
{
	if (!isLoaded() || events.empty())
	{
		return;
	}
	m_plugin->setParamEvents(static_cast<int>(id), events);
	noteParameterValue(id, events.back().value);
}




void Vst2PluginHost::refreshParameterValues()
{
	refreshValues();
}




void Vst2PluginHost::noteParameterValue(uint32_t id, double value)
{
	if (id < m_values.size())
	{
		m_values[id] = value;
	}
}




int Vst2PluginHost::programCount() const
{
	return static_cast<int>(m_programNames.size());
}




int Vst2PluginHost::currentProgram() const
{
	return isLoaded() ? m_plugin->currentProgram() : -1;
}




QString Vst2PluginHost::programName(int index) const
{
	if (index < 0 || index >= m_programNames.size())
	{
		return {};
	}
	return m_programNames[index];
}




bool Vst2PluginHost::setProgram(int index)
{
	if (!isLoaded() || index < 0 || index >= m_programNames.size())
	{
		return false;
	}
	m_plugin->setProgram(index);
	refreshValues();
	return true;
}




PluginStateBlob Vst2PluginHost::saveState(QString* error)
{
	PluginStateBlob blob;
	if (!isLoaded())
	{
		setError(error, QStringLiteral("No VST2 plugin is loaded"));
		return blob;
	}

	QDomDocument document;
	QDomElement root = document.createElement(QStringLiteral("vstplugin"));
	document.appendChild(root);
	m_plugin->saveSettings(document, root);
	blob.primary = document.toByteArray();
	return blob;
}




bool Vst2PluginHost::loadState(const PluginStateBlob& state, QString* error)
{
	if (!isLoaded())
	{
		setError(error, QStringLiteral("No VST2 plugin is loaded"));
		return false;
	}
	if (state.primary.isEmpty())
	{
		setError(error, QStringLiteral("The VST2 state blob is empty"));
		return false;
	}

	QDomDocument document;
	QString parseError;
	int line = 0;
	int column = 0;
	if (!document.setContent(state.primary, &parseError, &line, &column))
	{
		setError(error, QStringLiteral("Invalid VST2 state XML at %1:%2: %3")
			.arg(line).arg(column).arg(parseError));
		return false;
	}

	m_plugin->loadSettings(document.documentElement());
	refreshValues();
	return true;
}

} // namespace lmms
