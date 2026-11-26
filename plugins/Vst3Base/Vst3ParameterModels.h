/*
 * Vst3ParameterModels.h - FloatModel bridge for VST3 parameters
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

#ifndef LMMS_VST3_PARAMETER_MODELS_H
#define LMMS_VST3_PARAMETER_MODELS_H

#include <vector>

#include <QString>

#include "Vst3Host.h"
#include "vst3base_export.h"

class QDomDocument;
class QDomElement;

namespace lmms
{

class Model;
class FloatModel;

/**
 * Owns one LMMS FloatModel per automatable VST3 parameter and translates
 * model values into sample-offset parameter changes for Vst3Host.
 */
class VST3BASE_EXPORT Vst3ParameterModels
{
public:
	void initialize(Model* parent, Vst3Host* host);
	void clear();

	//! Queue model changes for the next process() call.
	void queueChanges(int frames);

	//! Refresh all models from the host, e.g. after restoring state.
	void refreshFromHost();

	void saveSettings(QDomDocument& doc, QDomElement& parent) const;
	void loadSettings(const QDomElement& parent);

	int count() const { return static_cast<int>(m_models.size()); }
	FloatModel* model(int index) const;
	const Vst3Host::Parameter& parameter(int index) const;
	const std::vector<FloatModel*>& models() const { return m_models; }

private:
	static QString settingName(uint32_t id);

	Vst3Host* m_host = nullptr;
	std::vector<Vst3Host::Parameter> m_parameters;
	std::vector<FloatModel*> m_models;
	std::vector<float> m_lastValues;
};

} // namespace lmms

#endif // LMMS_VST3_PARAMETER_MODELS_H
