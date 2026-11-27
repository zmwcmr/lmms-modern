/*
 * HostedEffect.cpp - generic Effect wrapper for a PluginHost
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

#include "HostedEffect.h"

#include <algorithm>

#include <QDomElement>

#include "AudioBuffer.h"

namespace lmms
{

namespace
{

void saveStateAttribute(QDomElement& element, const char* name,
	const QByteArray& value)
{
	if (!value.isEmpty())
	{
		element.setAttribute(QString::fromLatin1(name),
			QString::fromLatin1(value.toBase64()));
	}
}

QByteArray stateAttribute(const QDomElement& element, const char* name)
{
	return QByteArray::fromBase64(
		element.attribute(QString::fromLatin1(name)).toLatin1());
}

} // namespace




HostedEffectControls::HostedEffectControls(Effect* effect, PluginHost* host) :
	EffectControls(effect),
	m_host(host)
{
}




void HostedEffectControls::initializeModels()
{
	m_models.initialize(this, m_host);
}




void HostedEffectControls::saveSettings(
	QDomDocument& doc, QDomElement& element)
{
	m_models.saveSettings(doc, element);
	if (m_host != nullptr && m_host->isLoaded())
	{
		const auto state = m_host->saveState();
		saveStateAttribute(element, "hoststate", state.primary);
		saveStateAttribute(element, "hoststate2", state.secondary);
	}
}




void HostedEffectControls::loadSettings(const QDomElement& element)
{
	m_models.loadSettings(element);
	if (m_host != nullptr && m_host->isLoaded())
	{
		PluginStateBlob state;
		state.primary = stateAttribute(element, "hoststate");
		state.secondary = stateAttribute(element, "hoststate2");
		if (!state.primary.isEmpty() || !state.secondary.isEmpty())
		{
			m_host->loadState(state);
		}
		m_models.refreshFromHost();
	}
}




HostedEffect::HostedEffect(const Descriptor* descriptor, Model* parent,
	const Descriptor::SubPluginFeatures::Key* key,
	std::unique_ptr<PluginHost> host) :
	Effect(descriptor, parent, key),
	m_host(std::move(host)),
	m_controls(this, m_host.get())
{
}




void HostedEffect::initializeModels()
{
	m_controls.initializeModels();
}




Effect::ProcessStatus HostedEffect::processImpl(
	SampleFrame* buffer, const f_cnt_t frames)
{
	if (buffer == nullptr || frames <= 0 || m_host == nullptr
		|| !m_host->isLoaded())
	{
		return ProcessStatus::Continue;
	}

	const auto count = static_cast<std::size_t>(frames);
	m_inputLeft.resize(count);
	m_inputRight.resize(count);
	m_sidechainLeft.resize(count);
	m_sidechainRight.resize(count);
	m_outputLeft.resize(count);
	m_outputRight.resize(count);

	for (std::size_t frame = 0; frame < count; ++frame)
	{
		m_inputLeft[frame] = buffer[frame][0];
		m_inputRight[frame] = buffer[frame][1];
		m_sidechainLeft[frame] = 0.0f;
		m_sidechainRight[frame] = 0.0f;
		m_outputLeft[frame] = 0.0f;
		m_outputRight[frame] = 0.0f;
	}

	if (const auto* sidechain = sidechainBuffer();
		sidechain != nullptr && sidechain->frames() >= frames)
	{
		const auto group = sidechain->groupBuffers(0);
		if (group.channels() > 0)
		{
			std::copy_n(group.buffer(0).data(), count, m_sidechainLeft.begin());
		}
		if (group.channels() > 1)
		{
			std::copy_n(group.buffer(1).data(), count, m_sidechainRight.begin());
		}
		else
		{
			std::copy(m_sidechainLeft.begin(), m_sidechainLeft.end(),
				m_sidechainRight.begin());
		}
	}

	m_controls.parameterModels().queueChanges(frames);

	float* inputs[2] = { m_inputLeft.data(), m_inputRight.data() };
	float* sidechain[2] = { m_sidechainLeft.data(), m_sidechainRight.data() };
	float* outputs[2] = { m_outputLeft.data(), m_outputRight.data() };
	PluginBusBlock inputBuses[2] = {
		PluginBusBlock{ inputs, 2 },
		PluginBusBlock{ sidechain, 2 },
	};
	PluginProcessBlock block;
	block.inputs = inputs;
	block.outputs = outputs;
	block.inputChannels = 2;
	block.outputChannels = 2;
	block.frames = frames;
	if (m_host->inputBuses().size() > 1)
	{
		block.inputBuses = inputBuses;
		block.inputBusCount = 2;
	}

	if (!m_host->process(block))
	{
		return ProcessStatus::Continue;
	}

	const float dry = dryLevel();
	const float wet = wetLevel();
	for (std::size_t frame = 0; frame < count; ++frame)
	{
		buffer[frame][0] = dry * buffer[frame][0] + wet * m_outputLeft[frame];
		buffer[frame][1] = dry * buffer[frame][1] + wet * m_outputRight[frame];
	}
	return ProcessStatus::Continue;
}




f_cnt_t HostedEffect::latency() const
{
	return m_host != nullptr ? static_cast<f_cnt_t>(
		m_host->latencySamples()) : 0;
}

} // namespace lmms
