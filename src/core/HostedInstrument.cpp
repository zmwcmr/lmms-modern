/*
 * HostedInstrument.cpp - generic Instrument wrapper for a PluginHost
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

#include "HostedInstrument.h"

#include <algorithm>
#include <cstring>

#include <QDomElement>

#include "AudioEngine.h"
#include "Engine.h"
#include "InstrumentTrack.h"
#include "MidiEvent.h"
#include "Song.h"

namespace lmms
{

namespace
{

void setErrorAttribute(QDomElement& parent, const char* name,
	const QByteArray& value)
{
	if (!value.isEmpty())
	{
		parent.setAttribute(QString::fromLatin1(name),
			QString::fromLatin1(value.toBase64()));
	}
}

QByteArray getStateAttribute(const QDomElement& parent, const char* name)
{
	return QByteArray::fromBase64(
		parent.attribute(QString::fromLatin1(name)).toLatin1());
}

} // namespace




HostedInstrument::HostedInstrument(InstrumentTrack* instrumentTrack,
	const Descriptor* descriptor,
	const Descriptor::SubPluginFeatures::Key* key,
	std::unique_ptr<PluginHost> host,
	Flags flags) :
	Instrument(instrumentTrack, descriptor, key, flags),
	m_host(std::move(host))
{
}




HostedInstrument::~HostedInstrument() = default;




QString HostedInstrument::nodeName() const
{
	return QStringLiteral("hostedinstrument");
}




void HostedInstrument::initializeModels()
{
	m_models.initialize(this, m_host.get());
}




void HostedInstrument::play(SampleFrame* workingBuffer)
{
	if (workingBuffer == nullptr || m_host == nullptr || !m_host->isLoaded())
	{
		return;
	}

	const f_cnt_t frames = Engine::audioEngine()->framesPerPeriod();
	m_outputLeft.resize(static_cast<std::size_t>(frames));
	m_outputRight.resize(static_cast<std::size_t>(frames));

	if (auto* song = Engine::getSong())
	{
		m_host->setTempo(song->getTempo());
		m_host->setPlaying(song->isPlaying());
	}

	m_models.queueChanges(frames);

	float* outputs[2] = { m_outputLeft.data(), m_outputRight.data() };
	PluginProcessBlock block;
	block.outputs = outputs;
	block.outputChannels = 2;
	block.frames = frames;
	if (!m_host->process(block))
	{
		std::fill(m_outputLeft.begin(), m_outputLeft.end(), 0.0f);
		std::fill(m_outputRight.begin(), m_outputRight.end(), 0.0f);
	}

	for (f_cnt_t frame = 0; frame < frames; ++frame)
	{
		workingBuffer[frame][0] = m_outputLeft[static_cast<std::size_t>(frame)];
		workingBuffer[frame][1] = m_outputRight[static_cast<std::size_t>(frame)];
	}

	if (auto* track = instrumentTrack())
	{
		track->audioBusHandle()->setSourceLatency(latency());
	}
}




bool HostedInstrument::handleMidiEvent(const MidiEvent& event,
	const TimePos& time, f_cnt_t offset)
{
	Q_UNUSED(time)
	if (m_host == nullptr || !m_host->isLoaded())
	{
		return false;
	}

	PluginMidiEvent midi;
	midi.sampleOffset = offset;
	midi.channel = event.channel();

	switch (event.type())
	{
		case MidiNoteOn:
			midi.type = PluginMidiEvent::Type::NoteOn;
			midi.data1 = event.param(0);
			midi.data2 = event.param(1);
			midi.value = event.param(1) / 127.0f;
			break;
		case MidiNoteOff:
			midi.type = PluginMidiEvent::Type::NoteOff;
			midi.data1 = event.param(0);
			midi.data2 = event.param(1);
			midi.value = event.param(1) / 127.0f;
			break;
		case MidiControlChange:
			midi.type = PluginMidiEvent::Type::ControlChange;
			midi.data1 = event.param(0);
			midi.data2 = event.param(1);
			break;
		case MidiPitchBend:
			midi.type = PluginMidiEvent::Type::PitchBend;
			midi.data1 = event.param(0) & 0x7F;
			midi.data2 = (event.param(0) >> 7) & 0x7F;
			break;
		case MidiChannelPressure:
			midi.type = PluginMidiEvent::Type::ChannelPressure;
			midi.data1 = event.param(0);
			break;
		case MidiKeyPressure:
			midi.type = PluginMidiEvent::Type::PolyPressure;
			midi.data1 = event.param(0);
			midi.value = event.param(1) / 127.0f;
			break;
		case MidiProgramChange:
			midi.type = PluginMidiEvent::Type::ProgramChange;
			midi.data1 = event.param(0);
			break;
		case MidiSysEx:
			// The generic event has no length accessor for SysEx yet.
			return false;
		default:
			return false;
	}

	m_host->sendMidi(midi);
	return true;
}




void HostedInstrument::saveSettings(
	QDomDocument& doc, QDomElement& parent)
{
	m_models.saveSettings(doc, parent);
	saveHostState(doc, parent);
}




void HostedInstrument::loadSettings(const QDomElement& parent)
{
	m_models.loadSettings(parent);
	loadHostState(parent);
	if (m_host != nullptr)
	{
		m_models.refreshFromHost();
	}
}




void HostedInstrument::saveHostState(
	QDomDocument& doc, QDomElement& parent) const
{
	Q_UNUSED(doc)
	if (m_host == nullptr || !m_host->isLoaded())
	{
		return;
	}
	const auto state = m_host->saveState();
	setErrorAttribute(parent, "hoststate", state.primary);
	setErrorAttribute(parent, "hoststate2", state.secondary);
}




void HostedInstrument::loadHostState(const QDomElement& parent)
{
	if (m_host == nullptr || !m_host->isLoaded())
	{
		return;
	}
	PluginStateBlob state;
	state.primary = getStateAttribute(parent, "hoststate");
	state.secondary = getStateAttribute(parent, "hoststate2");
	if (!state.primary.isEmpty() || !state.secondary.isEmpty())
	{
		m_host->loadState(state);
	}
}




f_cnt_t HostedInstrument::latency() const
{
	return m_host != nullptr ? static_cast<f_cnt_t>(
		m_host->latencySamples()) : 0;
}

} // namespace lmms
