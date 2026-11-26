/*
 * Vst3EffectControls.h - controls for the VST3 effect host
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

#ifndef LMMS_VST3_EFFECT_CONTROLS_H
#define LMMS_VST3_EFFECT_CONTROLS_H

#include <QStringList>

#include "EffectControls.h"
#include "Vst3ParameterModels.h"

namespace lmms
{

class Vst3Effect;

class Vst3EffectControls : public EffectControls
{
	Q_OBJECT
public:
	explicit Vst3EffectControls(Vst3Effect* effect);
	~Vst3EffectControls() override = default;

	void saveSettings(QDomDocument& doc, QDomElement& parent) override;
	void loadSettings(const QDomElement& parent) override;

	QString nodeName() const override
	{
		return QStringLiteral("Vst3EffectControls");
	}

	gui::EffectControlDialog* createView() override;
	int controlCount() override;

	void setPluginInfo(const QString& name, const QString& vendor,
		const QStringList& parameterNames);
	void initializeParameters();
	void queueParameterChanges(int frames);
	void refreshParameters();

	const QString& pluginName() const { return m_pluginName; }
	const QString& pluginVendor() const { return m_pluginVendor; }
	const QStringList& parameterNames() const { return m_parameterNames; }
	const Vst3ParameterModels& parameterModels() const { return m_parameterModels; }
	Vst3Host* host() const;
	bool setProgram(int index);
	bool setUnit(int unitId);

private:
	Vst3Effect* m_effect;
	QString m_pluginName;
	QString m_pluginVendor;
	QStringList m_parameterNames;
	Vst3ParameterModels m_parameterModels;
};

} // namespace lmms

#endif // LMMS_VST3_EFFECT_CONTROLS_H
