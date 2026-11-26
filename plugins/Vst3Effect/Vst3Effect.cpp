/*
 * Vst3Effect.cpp - VST3 effect host
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

#include "Vst3Effect.h"

#include <algorithm>

#include <QDomElement>
#include <QStringList>

#include "AudioEngine.h"
#include "Engine.h"
#include "Song.h"
#include "Vst3Host.h"
#include "Vst3SubPluginFeatures.h"

#include "embed.h"
#include "plugin_export.h"

namespace lmms
{

extern "C"
{

Plugin::Descriptor PLUGIN_EXPORT vst3effect_plugin_descriptor =
{
	LMMS_STRINGIFY(PLUGIN_NAME),
	"VST3",
	QT_TRANSLATE_NOOP("PluginBrowser", "Native VST3 effect host"),
	"LMMS developers",
	0x0100,
	Plugin::Type::Effect,
	new PixmapLoader("lmms-plugin-logo"),
	nullptr,
	new Vst3SubPluginFeatures(Plugin::Type::Effect),
};

}

Vst3Effect::Vst3Effect(Model* parent, const Descriptor::SubPluginFeatures::Key* key) :
	Effect(&vst3effect_plugin_descriptor, parent, key),
	m_key(key != nullptr ? *key : EffectKey()),
	m_controls(this)
{
	const bool loaded = openPlugin();
	setDisplayName(m_key.displayName());
	if (!loaded)
	{
		setDontRun(true);
		collectErrorForUI(tr("The VST3 effect %1 could not be loaded.")
			.arg(m_key.attributes.value(QStringLiteral("file"))));
	}
}

Vst3Effect::~Vst3Effect() = default;

bool Vst3Effect::openPlugin()
{
	const auto file = m_key.attributes.value(QStringLiteral("file"));
	const auto classId = m_key.attributes.value(QStringLiteral("class"));
	if (file.isEmpty() || classId.isEmpty())
	{
		return false;
	}

	double sampleRate = 44100.0;
	int blockSize = 4096;
	if (auto* engine = Engine::audioEngine())
	{
		sampleRate = engine->outputSampleRate();
		blockSize = std::max<int>(engine->framesPerPeriod(), 4096);
	}

	m_host = std::make_unique<Vst3Host>();
	QString error;
	if (!m_host->load(file, classId, sampleRate, blockSize, &error))
	{
		m_host.reset();
		m_controls.initializeParameters();
		return false;
	}
	m_controls.initializeParameters();

	QStringList parameterNames;
	for (const auto& parameter : m_host->parameters())
	{
		if (!parameter.hidden)
		{
			parameterNames.push_back(parameter.name);
		}
	}
	m_controls.setPluginInfo(m_host->name(), m_host->vendor(), parameterNames);
	return true;
}

Effect::ProcessStatus Vst3Effect::processImpl(SampleFrame* buf, const f_cnt_t frames)
{
	if (m_host == nullptr)
	{
		return ProcessStatus::ContinueIfNotQuiet;
	}
	if (!m_mutex.tryLock())
	{
		return ProcessStatus::ContinueIfNotQuiet;
	}

	const auto frameCount = static_cast<int>(frames);
	if (static_cast<int>(m_inputLeft.size()) < frameCount)
	{
		m_inputLeft.resize(frameCount);
		m_inputRight.resize(frameCount);
		m_outputLeft.resize(frameCount);
		m_outputRight.resize(frameCount);
	}

	for (int frame = 0; frame < frameCount; ++frame)
	{
		m_inputLeft[frame] = buf[frame][0];
		m_inputRight[frame] = buf[frame][1];
	}

	m_controls.queueParameterChanges(frameCount);
	m_host->setTempo(Engine::getSong() != nullptr ? Engine::getSong()->getTempo() : 120.0);
	m_host->setPlaying(Engine::getSong() != nullptr && Engine::getSong()->isPlaying());
	m_host->process(m_inputLeft.data(), m_inputRight.data(),
		m_outputLeft.data(), m_outputRight.data(), frameCount);

	const float wet = wetLevel();
	const float dry = dryLevel();
	for (int frame = 0; frame < frameCount; ++frame)
	{
		buf[frame][0] = wet * m_outputLeft[frame] + dry * buf[frame][0];
		buf[frame][1] = wet * m_outputRight[frame] + dry * buf[frame][1];
	}

	m_mutex.unlock();
	return ProcessStatus::ContinueIfNotQuiet;
}

void Vst3Effect::saveSettings(QDomDocument& doc, QDomElement& parent)
{
	Effect::saveSettings(doc, parent);
	parent.setAttribute(QStringLiteral("file"),
		m_key.attributes.value(QStringLiteral("file")));
	parent.setAttribute(QStringLiteral("class"),
		m_key.attributes.value(QStringLiteral("class")));

	if (m_host != nullptr)
	{
		const auto component = m_host->componentState();
		const auto controller = m_host->controllerState();
		if (!component.isEmpty())
		{
			parent.setAttribute(QStringLiteral("componentstate"),
				QString::fromLatin1(component.toBase64()));
		}
		if (!controller.isEmpty())
		{
			parent.setAttribute(QStringLiteral("controllerstate"),
				QString::fromLatin1(controller.toBase64()));
		}
	}
}

void Vst3Effect::loadSettings(const QDomElement& parent)
{
	Effect::loadSettings(parent);
	if (m_host == nullptr)
	{
		const auto file = parent.attribute(QStringLiteral("file"));
		const auto classId = parent.attribute(QStringLiteral("class"));
		if (!file.isEmpty() && !classId.isEmpty())
		{
			m_key.attributes[QStringLiteral("file")] = file;
			m_key.attributes[QStringLiteral("class")] = classId;
			openPlugin();
		}
	}
	if (m_host == nullptr)
	{
		return;
	}

	const auto component = QByteArray::fromBase64(
		parent.attribute(QStringLiteral("componentstate")).toLatin1());
	const auto controller = QByteArray::fromBase64(
		parent.attribute(QStringLiteral("controllerstate")).toLatin1());
	if (!component.isEmpty() || !controller.isEmpty())
	{
		m_host->setState(component, controller);
	}
	m_controls.refreshParameters();
	m_controls.loadSettings(parent);
}

extern "C"
{

PLUGIN_EXPORT Plugin* lmms_plugin_main(Model* parent, void* data)
{
	return new Vst3Effect(parent,
		static_cast<const Plugin::Descriptor::SubPluginFeatures::Key*>(data));
}

}

} // namespace lmms
