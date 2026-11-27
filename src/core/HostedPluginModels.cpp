/*
 * HostedPluginModels.cpp - FloatModel bridge for a PluginHost
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

#include "HostedPluginModels.h"

#include <algorithm>
#include <cmath>

#include <QDomElement>
#include <QSet>

#include "AutomatableModel.h"
#include "ValueBuffer.h"

namespace lmms
{

void HostedPluginModels::initialize(Model* parent, PluginHost* host,
	const QString& settingPrefix)
{
	clear();
	m_host = host;
	if (m_host == nullptr)
	{
		return;
	}

	m_settingPrefix = settingPrefix.isEmpty()
		? defaultPrefix(m_host->capabilities().parameterKind)
		: settingPrefix;

	QSet<QString> displayNames;
	for (const auto& info : m_host->parameters())
	{
		if (info.hidden || info.readOnly || !info.automatable)
		{
			continue;
		}

		QString displayName = info.name;
		if (displayName.isEmpty())
		{
			displayName = QStringLiteral("Parameter %1").arg(info.id);
		}
		if (displayNames.contains(displayName))
		{
			displayName += QStringLiteral(" [%1]").arg(info.id);
		}
		displayNames.insert(displayName);

		const float min = static_cast<float>(info.min);
		const float max = static_cast<float>(info.max);
		float step = 0.001f;
		if (info.stepCount > 0 && max > min)
		{
			step = (max - min) / static_cast<float>(info.stepCount);
		}
		else if (max > min)
		{
			step = (max - min) / 1000.0f;
		}

		float value = static_cast<float>(m_host->parameterValue(info.id));
		value = std::clamp(value, min, max);

		auto* model = new FloatModel(value, min, max, step, parent,
			displayName);
		model->setInitValue(value);

		m_parameters.push_back(info);
		m_models.push_back(model);
		m_lastValues.push_back(value);
	}
}




void HostedPluginModels::clear()
{
	for (auto* model : m_models)
	{
		delete model;
	}
	m_models.clear();
	m_parameters.clear();
	m_lastValues.clear();
	m_host = nullptr;
}




void HostedPluginModels::setParameterChangeCallback(
	std::function<void(uint32_t, double, int)> callback)
{
	m_changeCallback = std::move(callback);
}




void HostedPluginModels::queueChanges(int frames)
{
	if (m_host == nullptr || frames <= 0)
	{
		return;
	}

	for (std::size_t index = 0; index < m_models.size(); ++index)
	{
		auto* const model = m_models[index];
		const uint32_t parameterId = m_parameters[index].id;

		if (model->isAutomated())
		{
			if (auto* const buffer = model->valueBuffer())
			{
				float last = m_lastValues[index];
				for (int frame = 0; frame < frames; ++frame)
				{
					const float value = buffer->value(frame);
					if (value != last)
					{
						if (m_changeCallback)
						{
							m_changeCallback(parameterId, value, frame);
						}
						else
						{
							m_host->setParameter(parameterId, value, frame);
						}
						last = value;
					}
				}
				m_lastValues[index] = last;
				continue;
			}
		}

		const float value = model->value();
		if (value != m_lastValues[index])
		{
			if (m_changeCallback)
			{
				m_changeCallback(parameterId, value, 0);
			}
			else
			{
				m_host->setParameter(parameterId, value, 0);
			}
			m_lastValues[index] = value;
		}
	}
}




void HostedPluginModels::refreshFromHost()
{
	if (m_host == nullptr)
	{
		return;
	}

	for (std::size_t index = 0; index < m_models.size(); ++index)
	{
		float value = static_cast<float>(
			m_host->parameterValue(m_parameters[index].id));
		value = std::clamp(value, static_cast<float>(m_parameters[index].min),
			static_cast<float>(m_parameters[index].max));
		m_models[index]->setValue(value);
		m_models[index]->setInitValue(value);
		m_lastValues[index] = value;
	}
}




void HostedPluginModels::saveSettings(
	QDomDocument& doc, QDomElement& parent) const
{
	for (std::size_t index = 0; index < m_models.size(); ++index)
	{
		m_models[index]->saveSettings(doc, parent, settingName(
			static_cast<int>(index)));
	}
}




void HostedPluginModels::loadSettings(const QDomElement& parent)
{
	for (std::size_t index = 0; index < m_models.size(); ++index)
	{
		const QString name = settingName(static_cast<int>(index));
		if (parent.hasAttribute(name)
			|| !parent.firstChildElement(name).isNull())
		{
			m_models[index]->loadSettings(parent, name);
			m_lastValues[index] = m_models[index]->value();
		}
	}
}




FloatModel* HostedPluginModels::model(int index) const
{
	if (index < 0 || static_cast<std::size_t>(index) >= m_models.size())
	{
		return nullptr;
	}
	return m_models[static_cast<std::size_t>(index)];
}




const PluginParameterInfo& HostedPluginModels::parameter(int index) const
{
	return m_parameters[static_cast<std::size_t>(index)];
}




QString HostedPluginModels::settingName(int index) const
{
	return m_settingPrefix + QString::number(
		m_parameters[static_cast<std::size_t>(index)].id);
}




QString HostedPluginModels::defaultPrefix(PluginParameterKind kind)
{
	switch (kind)
	{
		case PluginParameterKind::NormalisedId: return QStringLiteral("vst3p");
		case PluginParameterKind::PortIndex: return QStringLiteral("lv2p");
		case PluginParameterKind::FlatIndex: break;
	}
	return QStringLiteral("param");
}

} // namespace lmms
