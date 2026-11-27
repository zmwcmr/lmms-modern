/*
 * Lv2PluginHost.cpp - PluginHost adapter for LV2
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

#include "Lv2PluginHost.h"

#include "lmmsconfig.h"

#ifdef LMMS_HAVE_LV2

#include <algorithm>
#include <cmath>
#include <deque>
#include <exception>
#include <limits>

#include <QDomDocument>
#include <QDomElement>
#include <QDebug>

#include "AudioEngine.h"
#include "ComboBoxModel.h"
#include "Engine.h"
#include "Lv2Ports.h"
#include "MidiEvent.h"

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




Lv2PluginHost::Lv2PluginHost(Model* parent) :
	m_parent(parent)
{
}




Lv2PluginHost::Lv2PluginHost(Lv2Proc* proc) :
	m_proc(proc)
{
	if (m_proc)
	{
		refreshParameters();
	}
}




Lv2PluginHost::~Lv2PluginHost() = default;




void Lv2PluginHost::adopt(Lv2Proc* proc)
{
	m_ownedProc.reset();
	m_proc = proc;
	refreshParameters();
}




PluginFormat Lv2PluginHost::format() const
{
	return PluginFormat::Lv2;
}




PluginHostCapabilities Lv2PluginHost::capabilities() const
{
	PluginHostCapabilities capabilities;
	// The LV2 editor still belongs to the GUI layer; it is not available
	// through the core adapter until the editor-host stage.
	capabilities.hasEditor = false;
	capabilities.editorMustShareProcess = true;
	capabilities.supportsPrograms = m_proc && !m_proc->presets().empty();
	capabilities.supportsUnits = false;
	capabilities.sampleAccurateParameters = false;
	capabilities.supportsMidiOut = true;
	capabilities.multipleBuses = false;
	capabilities.parameterKind = PluginParameterKind::PortIndex;
	capabilities.stateKind = PluginStateKind::Lv2Bundle;
	return capabilities;
}




bool Lv2PluginHost::load(const PluginLoadRequest& request, QString* error)
{
	unload();

	if (m_parent == nullptr)
	{
		setError(error, QStringLiteral(
			"The LV2 adapter needs a parent model"));
		return false;
	}
	if (request.pluginUri.isEmpty())
	{
		setError(error, QStringLiteral("No LV2 plugin URI given"));
		return false;
	}

	try
	{
		m_ownedProc = std::make_unique<Lv2Proc>(m_parent,
			request.pluginUri);
		m_proc = m_ownedProc.get();
	}
	catch (const std::exception& exception)
	{
		setError(error, QString::fromLocal8Bit(exception.what()));
		m_ownedProc.reset();
		m_proc = nullptr;
		return false;
	}

	refreshParameters();
	m_currentProgram = -1;
	return true;
}




void Lv2PluginHost::unload()
{
	m_ownedProc.reset();
	m_proc = nullptr;
	m_parameters.clear();
	m_controls.clear();
	m_currentProgram = -1;
}




bool Lv2PluginHost::isLoaded() const
{
	return m_proc != nullptr;
}




void Lv2PluginHost::refreshParameters()
{
	m_parameters.clear();
	m_controls.clear();
	if (!m_proc)
	{
		return;
	}

	sample_rate_t sampleRate = 44100;
	if (auto* engine = Engine::audioEngine())
	{
		sampleRate = engine->outputSampleRate();
	}

	std::size_t portIndex = 0;
	m_proc->foreach_port([this, sampleRate, &portIndex](
		Lv2Ports::PortBase* port) {
		auto* control = (port->m_type == Lv2Ports::Type::Control
			&& port->m_flow == Lv2Ports::Flow::Input)
			? Lv2Ports::dcast<Lv2Ports::Control>(port) : nullptr;

		if (control && control->m_connectedModel)
		{
			PluginParameterInfo info;
			info.id = static_cast<uint32_t>(portIndex);
			info.name = port->name();
			info.min = control->min(sampleRate);
			info.max = control->max(sampleRate);
			info.defaultValue = control->def();
			info.automatable = true;
			m_parameters.push_back(info);
		}

		// Keep the lookup aligned with the port index used as the ID.
		m_controls.push_back(control && control->m_connectedModel
			? control : nullptr);
		++portIndex;
	});
}




Lv2Ports::Control* Lv2PluginHost::controlAt(uint32_t id) const
{
	if (id >= m_controls.size())
	{
		return nullptr;
	}
	auto* control = m_controls[id];
	return control && control->m_connectedModel ? control : nullptr;
}




bool Lv2PluginHost::setParameter(uint32_t id, double value, int sampleOffset)
{
	Q_UNUSED(sampleOffset)
	auto* control = controlAt(id);
	if (!control)
	{
		return false;
	}

	auto* model = control->m_connectedModel.get();
	if (auto* combo = model->dynamicCast<ComboBoxModel>())
	{
		if (control->m_scalePointMap.empty())
		{
			return false;
		}
		// The port value is a scale-point value; the model stores an index.
		std::size_t best = 0;
		double bestDistance = std::numeric_limits<double>::max();
		for (std::size_t i = 0; i < control->m_scalePointMap.size(); ++i)
		{
			const double distance = std::abs(
				control->m_scalePointMap[i] - value);
			if (distance < bestDistance)
			{
				bestDistance = distance;
				best = i;
			}
		}
		combo->setValue(static_cast<int>(best));
		return true;
	}

	model->setValue(static_cast<float>(value));
	return true;
}




double Lv2PluginHost::parameterValue(uint32_t id) const
{
	auto* control = controlAt(id);
	if (!control)
	{
		return 0.0;
	}

	auto* model = control->m_connectedModel.get();
	if (auto* combo = model->dynamicCast<ComboBoxModel>())
	{
		if (control->m_scalePointMap.empty())
		{
			return 0.0;
		}
		const auto index = std::clamp<int>(combo->value(), 0,
			static_cast<int>(control->m_scalePointMap.size()) - 1);
		return control->m_scalePointMap[static_cast<std::size_t>(index)];
	}
	return model->value<float>();
}




void Lv2PluginHost::sendMidi(const PluginMidiEvent& event)
{
	if (!m_proc)
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
			// The LMMS MIDI queue stores a pointer to the payload. Keep a copy
			// alive until the next process() call.
			m_sysexStorage.push_back(event.sysex);
			m_proc->handleMidiInputEvent(
				MidiEvent(MidiSysEx, m_sysexStorage.back().constData(),
					static_cast<std::size_t>(m_sysexStorage.back().size())),
				TimePos(static_cast<tick_t>(0)), event.sampleOffset);
			return;
	}
	m_proc->handleMidiInputEvent(
		MidiEvent(type, static_cast<int8_t>(event.channel),
			static_cast<int16_t>(event.data1),
			static_cast<int16_t>(event.data2)),
		TimePos(static_cast<tick_t>(0)), event.sampleOffset);
}




bool Lv2PluginHost::process(const PluginProcessBlock& block)
{
	if (!m_proc || block.frames <= 0 || block.outputChannels <= 0
		|| block.outputs == nullptr)
	{
		return false;
	}

	if (auto* leftOut = m_proc->outPorts().m_left;
		leftOut && block.frames > static_cast<int>(leftOut->bufferSize()))
	{
		// The Lv2Proc buffers were allocated for the engine block size.
		return false;
	}

	copyInput(block);
	m_proc->copyModelsFromCore();
	m_proc->run(block.frames);
	m_proc->copyModelsToCore();
	copyOutput(block);
	m_sysexStorage.clear();
	return true;
}




void Lv2PluginHost::copyInput(const PluginProcessBlock& block)
{
	if (!m_proc || block.inputChannels <= 0 || block.inputs == nullptr)
	{
		return;
	}

	const auto frames = static_cast<std::size_t>(block.frames);
	auto& in = m_proc->inPorts();
	if (in.m_left == nullptr)
	{
		return;
	}

	if (block.inputs[0] != nullptr)
	{
		std::copy_n(block.inputs[0], frames, in.m_left->data());
	}
	else
	{
		std::fill_n(in.m_left->data(), frames, 0.0f);
	}

	if (in.m_right != nullptr)
	{
		if (block.inputChannels >= 2 && block.inputs[1] != nullptr)
		{
			std::copy_n(block.inputs[1], frames, in.m_right->data());
		}
		else if (block.inputs[0] != nullptr)
		{
			// Mono input into a stereo plugin: feed both channels.
			std::copy_n(block.inputs[0], frames, in.m_right->data());
		}
		else
		{
			std::fill_n(in.m_right->data(), frames, 0.0f);
		}
	}
	else if (block.inputChannels >= 2 && block.inputs[1] != nullptr)
	{
		// Stereo input into a mono plugin: average both channels.
		auto* data = in.m_left->data();
		for (std::size_t frame = 0; frame < frames; ++frame)
		{
			data[frame] = 0.5f * (block.inputs[0][frame]
				+ block.inputs[1][frame]);
		}
	}
}




void Lv2PluginHost::copyOutput(const PluginProcessBlock& block) const
{
	if (!m_proc || block.outputs == nullptr || block.outputChannels <= 0)
	{
		return;
	}

	auto& out = m_proc->outPorts();
	if (out.m_left == nullptr || block.outputs[0] == nullptr)
	{
		return;
	}

	const auto frames = static_cast<std::size_t>(block.frames);
	std::copy_n(out.m_left->data(), frames, block.outputs[0]);

	if (block.outputChannels >= 2 && block.outputs[1] != nullptr)
	{
		if (out.m_right != nullptr)
		{
			std::copy_n(out.m_right->data(), frames, block.outputs[1]);
		}
		else
		{
			// Mono plugin output into a stereo block: duplicate it.
			std::copy_n(out.m_left->data(), frames, block.outputs[1]);
		}
	}
}




int Lv2PluginHost::latencySamples() const
{
	return m_proc ? static_cast<int>(m_proc->latency()) : 0;
}




int Lv2PluginHost::programCount() const
{
	return m_proc ? static_cast<int>(m_proc->presets().size()) : 0;
}




QString Lv2PluginHost::programName(int index) const
{
	if (!m_proc || index < 0
		|| index >= static_cast<int>(m_proc->presets().size()))
	{
		return {};
	}
	return m_proc->presets()[static_cast<std::size_t>(index)].label;
}




bool Lv2PluginHost::setProgram(int index)
{
	if (!m_proc || index < 0
		|| index >= static_cast<int>(m_proc->presets().size()))
	{
		return false;
	}
	if (!m_proc->loadPreset(static_cast<std::size_t>(index)))
	{
		return false;
	}
	m_currentProgram = index;
	return true;
}




PluginStateBlob Lv2PluginHost::saveState(QString* error)
{
	PluginStateBlob blob;
	if (!m_proc)
	{
		setError(error, QStringLiteral("No LV2 plugin is loaded"));
		return blob;
	}

	QDomDocument document;
	QDomElement root = document.createElement(QStringLiteral("lv2plugin"));
	document.appendChild(root);
	m_proc->saveSettings(document, root);
	blob.primary = document.toByteArray();
	return blob;
}




bool Lv2PluginHost::loadState(const PluginStateBlob& state, QString* error)
{
	if (!m_proc)
	{
		setError(error, QStringLiteral("No LV2 plugin is loaded"));
		return false;
	}
	if (state.primary.isEmpty())
	{
		setError(error, QStringLiteral("The LV2 state blob is empty"));
		return false;
	}

	QDomDocument document;
	QString parseError;
	int line = 0;
	int column = 0;
	if (!document.setContent(state.primary, &parseError, &line, &column))
	{
		setError(error, QStringLiteral("Invalid LV2 state XML at %1:%2: %3")
			.arg(line).arg(column).arg(parseError));
		return false;
	}

	auto root = document.documentElement();
	if (root.isNull())
	{
		setError(error, QStringLiteral("The LV2 state XML has no root element"));
		return false;
	}
	m_proc->loadSettings(root);
	m_currentProgram = -1;
	return true;
}

} // namespace lmms

#endif // LMMS_HAVE_LV2
