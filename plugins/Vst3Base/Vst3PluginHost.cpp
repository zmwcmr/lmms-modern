/*
 * Vst3PluginHost.cpp - PluginHost adapter for VST3
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

#include "Vst3PluginHost.h"

#include <algorithm>

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

} // namespace




Vst3PluginHost::Vst3PluginHost() = default;




Vst3PluginHost::Vst3PluginHost(Vst3HostBase& host) :
	m_host(&host)
{
}




void Vst3PluginHost::adopt(Vst3HostBase* host)
{
	m_host = host;
	m_parameters.clear();
	if (m_host != nullptr)
	{
		refreshParameters();
	}
}




PluginFormat Vst3PluginHost::format() const
{
	return PluginFormat::Vst3;
}




PluginHostCapabilities Vst3PluginHost::capabilities() const
{
	PluginHostCapabilities capabilities;
	// The VST3 editor is already implemented in Vst3HostBase, but the
	// format-independent editor backend belongs to the editor-host stage.
	capabilities.hasEditor = false;
	capabilities.editorMustShareProcess = false;
	capabilities.supportsPrograms = m_host != nullptr
		&& m_host->programCount() > 0;
	capabilities.supportsUnits = m_host != nullptr
		&& !m_host->units().empty();
	capabilities.sampleAccurateParameters = true;
	capabilities.supportsMidiOut = false;
	capabilities.multipleBuses = false;
	capabilities.parameterKind = PluginParameterKind::NormalisedId;
	capabilities.stateKind = PluginStateKind::ComponentController;
	return capabilities;
}




bool Vst3PluginHost::load(const PluginLoadRequest& request, QString* error)
{
	unload();

	if (m_host == nullptr)
	{
		setError(error, QStringLiteral("No VST3 host is attached"));
		return false;
	}
	if (request.path.isEmpty() || request.classId.isEmpty())
	{
		setError(error, QStringLiteral(
			"VST3 needs a bundle path and a class ID"));
		return false;
	}
	if (request.sampleRate <= 0.0 || request.maxBlockSize <= 0)
	{
		setError(error, QStringLiteral(
			"VST3 needs a positive sample rate and block size"));
		return false;
	}

	if (!m_host->load(request.path, request.classId, request.sampleRate,
		request.maxBlockSize, error))
	{
		return false;
	}

	refreshParameters();
	return true;
}




void Vst3PluginHost::unload()
{
	if (m_host != nullptr)
	{
		m_host->unload();
	}
	m_parameters.clear();
}




bool Vst3PluginHost::isLoaded() const
{
	return m_host != nullptr && m_host->isLoaded();
}




void Vst3PluginHost::refreshParameters()
{
	m_parameters.clear();
	if (m_host == nullptr)
	{
		return;
	}
	for (const auto& parameter : m_host->parameters())
	{
		PluginParameterInfo info;
		info.id = parameter.id;
		info.name = parameter.name;
		info.unit = parameter.unit;
		info.min = 0.0;
		info.max = 1.0;
		info.defaultValue = parameter.defaultNormalised;
		info.stepCount = parameter.stepCount;
		info.automatable = parameter.automatable;
		info.readOnly = parameter.readOnly;
		info.hidden = parameter.hidden;
		m_parameters.push_back(info);
	}
}




bool Vst3PluginHost::setParameter(uint32_t id, double value, int sampleOffset)
{
	if (m_host == nullptr || !m_host->isLoaded())
	{
		return false;
	}

	const double normalised = std::clamp(value, 0.0, 1.0);
	if (sampleOffset > 0)
	{
		m_host->queueParameter(id, normalised, sampleOffset);
		return true;
	}
	return m_host->setParameterNormalised(id, normalised);
}




double Vst3PluginHost::parameterValue(uint32_t id) const
{
	return m_host != nullptr && m_host->isLoaded()
		? m_host->parameterNormalised(id) : 0.0;
}




void Vst3PluginHost::sendMidi(const PluginMidiEvent& event)
{
	if (m_host == nullptr || !m_host->isLoaded())
	{
		return;
	}

	Vst3HostBase::MidiEvent midi;
	switch (event.type)
	{
		case PluginMidiEvent::Type::NoteOn:
			midi.type = Vst3HostBase::MidiType::NoteOn; break;
		case PluginMidiEvent::Type::NoteOff:
			midi.type = Vst3HostBase::MidiType::NoteOff; break;
		case PluginMidiEvent::Type::ControlChange:
			midi.type = Vst3HostBase::MidiType::ControlChange; break;
		case PluginMidiEvent::Type::PitchBend:
			midi.type = Vst3HostBase::MidiType::PitchBend; break;
		case PluginMidiEvent::Type::ChannelPressure:
			midi.type = Vst3HostBase::MidiType::ChannelPressure; break;
		case PluginMidiEvent::Type::PolyPressure:
			midi.type = Vst3HostBase::MidiType::PolyPressure; break;
		case PluginMidiEvent::Type::ProgramChange:
			midi.type = Vst3HostBase::MidiType::ProgramChange; break;
		case PluginMidiEvent::Type::SysEx:
			// Vst3HostBase has no SysEx event type yet.
			return;
	}

	midi.sampleOffset = event.sampleOffset;
	midi.channel = event.channel;
	midi.data1 = event.data1;
	midi.data2 = event.data2;
	midi.value = event.value;
	m_host->queueMidi(midi);
}




bool Vst3PluginHost::process(const PluginProcessBlock& block)
{
	if (m_host == nullptr || !m_host->isLoaded() || block.frames <= 0
		|| block.outputChannels <= 0 || block.outputs == nullptr
		|| block.outputs[0] == nullptr)
	{
		return false;
	}

	const float* leftIn = block.inputs && block.inputChannels > 0
		? block.inputs[0] : nullptr;
	const float* rightIn = block.inputs && block.inputChannels > 1
		? block.inputs[1] : leftIn;
	float* leftOut = block.outputs[0];
	float* rightOut = block.outputChannels > 1 && block.outputs[1]
		? block.outputs[1] : leftOut;

	m_host->process(leftIn, rightIn, leftOut, rightOut, block.frames);
	return true;
}




int Vst3PluginHost::latencySamples() const
{
	return m_host != nullptr ? m_host->latencySamples() : 0;
}




void Vst3PluginHost::setTempo(double bpm)
{
	if (m_host != nullptr)
	{
		m_host->setTempo(bpm);
	}
}




void Vst3PluginHost::setPlaying(bool playing)
{
	if (m_host != nullptr)
	{
		m_host->setPlaying(playing);
	}
}




int Vst3PluginHost::programCount() const
{
	return m_host != nullptr ? m_host->programCount() : 0;
}




int Vst3PluginHost::currentProgram() const
{
	return m_host != nullptr ? m_host->currentProgram() : -1;
}




QString Vst3PluginHost::programName(int index) const
{
	return m_host != nullptr ? m_host->programName(index) : QString();
}




bool Vst3PluginHost::setProgram(int index)
{
	return m_host != nullptr && m_host->setProgram(index);
}




PluginStateBlob Vst3PluginHost::saveState(QString* error)
{
	PluginStateBlob blob;
	if (m_host == nullptr || !m_host->isLoaded())
	{
		setError(error, QStringLiteral("No VST3 plugin is loaded"));
		return blob;
	}

	blob.primary = m_host->componentState();
	blob.secondary = m_host->controllerState();
	if (blob.primary.isEmpty() && blob.secondary.isEmpty())
	{
		setError(error, QStringLiteral("The VST3 plugin returned no state"));
	}
	return blob;
}




bool Vst3PluginHost::loadState(const PluginStateBlob& state, QString* error)
{
	if (m_host == nullptr || !m_host->isLoaded())
	{
		setError(error, QStringLiteral("No VST3 plugin is loaded"));
		return false;
	}
	if (state.primary.isEmpty() && state.secondary.isEmpty())
	{
		setError(error, QStringLiteral("The VST3 state blob is empty"));
		return false;
	}

	if (!m_host->setState(state.primary, state.secondary))
	{
		setError(error, QStringLiteral("The VST3 plugin rejected the state"));
		return false;
	}
	refreshParameters();
	return true;
}

} // namespace lmms
