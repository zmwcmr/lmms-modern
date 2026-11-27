/*
 * FakePluginHost.h - in-memory PluginHost used by the hosted-wrapper tests
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

#ifndef LMMS_FAKE_PLUGIN_HOST_H
#define LMMS_FAKE_PLUGIN_HOST_H

#include <map>
#include <vector>

#include "PluginHost.h"

namespace lmms
{

class FakePluginHost : public PluginHost
{
public:
	struct ParameterCall
	{
		uint32_t id;
		double value;
		int sampleOffset;
	};

	FakePluginHost()
	{
		m_parameters = {
			{ 7, QStringLiteral("Gain"), QString(), 0.0, 1.0, 0.5 },
			{ 9, QStringLiteral("Mix"), QString(), 0.0, 100.0, 25.0 },
		};
		m_values[7] = 0.5;
		m_values[9] = 25.0;
	}

	PluginFormat format() const override { return PluginFormat::Vst3; }

	PluginHostCapabilities capabilities() const override
	{
		PluginHostCapabilities capabilities;
		capabilities.supportsPrograms = true;
		capabilities.parameterKind = PluginParameterKind::FlatIndex;
		capabilities.stateKind = PluginStateKind::Chunk;
		return capabilities;
	}

	bool load(const PluginLoadRequest&, QString*) override
	{
		m_loaded = true;
		return true;
	}
	void unload() override { m_loaded = false; }
	bool isLoaded() const override { return m_loaded; }

	const std::vector<PluginParameterInfo>& parameters() const override
	{
		return m_parameters;
	}

	bool setParameter(uint32_t id, double value, int sampleOffset) override
	{
		m_values[id] = value;
		m_calls.push_back({ id, value, sampleOffset });
		return true;
	}

	double parameterValue(uint32_t id) const override
	{
		const auto it = m_values.find(id);
		return it != m_values.end() ? it->second : 0.0;
	}

	void sendMidi(const PluginMidiEvent& event) override
	{
		m_midi.push_back(event);
	}

	bool process(const PluginProcessBlock& block) override
	{
		if (!block.outputs)
		{
			return false;
		}
		for (int channel = 0; channel < block.outputChannels; ++channel)
		{
			if (!block.outputs[channel])
			{
				continue;
			}
			for (int frame = 0; frame < block.frames; ++frame)
			{
				block.outputs[channel][frame] = m_outputValue;
			}
		}
		return true;
	}

	int latencySamples() const override { return 42; }
	void setTempo(double bpm) override { m_tempo = bpm; }
	void setPlaying(bool playing) override { m_playing = playing; }

	PluginStateBlob saveState(QString*) override
	{
		PluginStateBlob state;
		state.primary = QByteArrayLiteral("fake-state");
		return state;
	}

	bool loadState(const PluginStateBlob& state, QString*) override
	{
		m_loadedState = state.primary;
		return true;
	}

	std::vector<PluginParameterInfo> m_parameters;
	std::map<uint32_t, double> m_values;
	std::vector<ParameterCall> m_calls;
	std::vector<PluginMidiEvent> m_midi;
	QByteArray m_loadedState;
	double m_outputValue = 0.5;
	double m_tempo = 0.0;
	bool m_playing = false;
	bool m_loaded = true;
};

} // namespace lmms

#endif // LMMS_FAKE_PLUGIN_HOST_H
