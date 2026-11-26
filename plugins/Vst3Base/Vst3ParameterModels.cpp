/*
 * Vst3ParameterModels.cpp - FloatModel bridge for VST3 parameters
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
 * Boston, MA 02110-1301, USA.
 *
 */

#include "Vst3ParameterModels.h"

#include <algorithm>
#include <cmath>

#include <QDomElement>
#include <QSet>

#include "AutomatableModel.h"
#include "ValueBuffer.h"

namespace lmms
{

void Vst3ParameterModels::initialize(Model* parent, Vst3HostBase* host)
{
	clear();
	m_host = host;
	if (m_host == nullptr)
	{
		return;
	}

	QSet<QString> names;
	for (const auto& info : m_host->parameters())
	{
		if (info.hidden || info.readOnly || !info.automatable)
		{
			continue;
		}

		QString name = info.name;
		if (name.isEmpty())
		{
			name = QStringLiteral("Parameter %1").arg(info.id);
		}
		if (names.contains(name))
		{
			name += QStringLiteral(" [%1]").arg(info.id);
		}
		names.insert(name);

		const float value = static_cast<float>(m_host->parameterNormalised(info.id));
		auto* model = new FloatModel(value, 0.0f, 1.0f, 0.001f, parent, name);
		model->setInitValue(value);
		m_models.push_back(model);
		m_parameters.push_back(info);
		m_lastValues.push_back(value);
	}
}

void Vst3ParameterModels::setQueueCallback(
	std::function<void(uint32_t, double, int)> callback)
{
	m_queueCallback = std::move(callback);
}

void Vst3ParameterModels::clear()
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

void Vst3ParameterModels::queueChanges(int frames)
{
	if (m_host == nullptr || frames <= 0)
	{
		return;
	}

	for (size_t index = 0; index < m_models.size(); ++index)
	{
		auto* const model = m_models[index];
		const auto parameterId = m_parameters[index].id;

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
						if (m_queueCallback)
						{
							m_queueCallback(parameterId, value, frame);
						}
						else
						{
							m_host->queueParameter(parameterId, value, frame);
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
			if (m_queueCallback)
			{
				m_queueCallback(parameterId, value, 0);
			}
			else
			{
				m_host->queueParameter(parameterId, value, 0);
			}
			m_lastValues[index] = value;
		}
	}
}

void Vst3ParameterModels::refreshFromHost()
{
	if (m_host == nullptr)
	{
		return;
	}
	for (size_t index = 0; index < m_models.size(); ++index)
	{
		const float value = static_cast<float>(
			m_host->parameterNormalised(m_parameters[index].id));
		m_models[index]->setValue(value);
		m_models[index]->setInitValue(value);
		m_lastValues[index] = value;
	}
}

void Vst3ParameterModels::saveSettings(
	QDomDocument& doc, QDomElement& parent) const
{
	for (size_t index = 0; index < m_models.size(); ++index)
	{
		if (m_models[index]->isAutomatedOrControlled())
		{
			m_models[index]->saveSettings(doc, parent,
				settingName(m_parameters[index].id));
		}
	}
}

void Vst3ParameterModels::loadSettings(const QDomElement& parent)
{
	for (size_t index = 0; index < m_models.size(); ++index)
	{
		const auto name = settingName(m_parameters[index].id);
		if (parent.hasAttribute(name) || !parent.firstChildElement(name).isNull())
		{
			m_models[index]->loadSettings(parent, name);
			m_lastValues[index] = m_models[index]->value();
		}
	}
}

FloatModel* Vst3ParameterModels::model(int index) const
{
	if (index < 0 || static_cast<size_t>(index) >= m_models.size())
	{
		return nullptr;
	}
	return m_models[static_cast<size_t>(index)];
}

const Vst3HostBase::Parameter& Vst3ParameterModels::parameter(int index) const
{
	return m_parameters[static_cast<size_t>(index)];
}

QString Vst3ParameterModels::settingName(uint32_t id)
{
	return QStringLiteral("vst3p%1").arg(id);
}

} // namespace lmms
