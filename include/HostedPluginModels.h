/*
 * HostedPluginModels.h - FloatModel bridge for a PluginHost
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

#ifndef LMMS_HOSTED_PLUGIN_MODELS_H
#define LMMS_HOSTED_PLUGIN_MODELS_H

#include <functional>
#include <vector>

#include <QString>

#include "PluginHost.h"
#include "lmms_export.h"

class QDomDocument;
class QDomElement;

namespace lmms
{

class FloatModel;
class Model;

/*! One FloatModel per automatable PluginHost parameter.
 *
 *  The class is the format-neutral equivalent of Vst3ParameterModels. It is
 *  used by HostedInstrument and HostedEffect, and it also keeps the project
 *  XML naming configurable so legacy plugins can keep their existing
 *  `paramN` / `vst3pN` attribute names.
 */
class LMMS_EXPORT HostedPluginModels
{
public:
	//! @param settingPrefix XML prefix for the models; empty selects a
	//!        default based on PluginHostCapabilities::parameterKind
	void initialize(Model* parent, PluginHost* host,
		const QString& settingPrefix = QString());
	void clear();

	//! Optional override for parameter changes; when empty, setParameter()
	//! is called on the host.
	void setParameterChangeCallback(
		std::function<void(uint32_t, double, int)> callback);

	//! Push model changes for the next process() call.
	void queueChanges(int frames);

	//! Refresh all models from the host, e.g. after restoring state.
	void refreshFromHost();

	void saveSettings(QDomDocument& doc, QDomElement& parent) const;
	void loadSettings(const QDomElement& parent);

	int count() const { return static_cast<int>(m_models.size()); }
	FloatModel* model(int index) const;
	const PluginParameterInfo& parameter(int index) const;
	const std::vector<FloatModel*>& models() const { return m_models; }
	const QString& settingPrefix() const { return m_settingPrefix; }

private:
	QString settingName(int index) const;
	static QString defaultPrefix(PluginParameterKind kind);

	PluginHost* m_host = nullptr;
	QString m_settingPrefix;
	std::function<void(uint32_t, double, int)> m_changeCallback;
	std::vector<PluginParameterInfo> m_parameters;
	std::vector<FloatModel*> m_models;
	std::vector<float> m_lastValues;
};

} // namespace lmms

#endif // LMMS_HOSTED_PLUGIN_MODELS_H
