/*
 * Vst3EffectControls.cpp - controls for the VST3 effect host
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

#include "Vst3EffectControls.h"

#include "Vst3Effect.h"
#include "Vst3EffectControlDialog.h"
#include "Vst3Host.h"

namespace lmms
{

Vst3EffectControls::Vst3EffectControls(Vst3Effect* effect) :
	EffectControls(effect),
	m_effect(effect)
{
}

void Vst3EffectControls::saveSettings(QDomDocument& doc, QDomElement& parent)
{
	m_parameterModels.saveSettings(doc, parent);
}

void Vst3EffectControls::loadSettings(const QDomElement& parent)
{
	m_parameterModels.loadSettings(parent);
	m_parameterModels.refreshFromHost();
}

gui::EffectControlDialog* Vst3EffectControls::createView()
{
	return new gui::Vst3EffectControlDialog(this);
}

int Vst3EffectControls::controlCount()
{
	return m_parameterModels.count();
}

void Vst3EffectControls::setPluginInfo(const QString& name, const QString& vendor,
	const QStringList& parameterNames)
{
	m_pluginName = name;
	m_pluginVendor = vendor;
	m_parameterNames = parameterNames;
}

void Vst3EffectControls::initializeParameters()
{
	m_parameterModels.initialize(this, m_effect != nullptr ? m_effect->host() : nullptr);
}

void Vst3EffectControls::queueParameterChanges(int frames)
{
	m_parameterModels.queueChanges(frames);
}

void Vst3EffectControls::refreshParameters()
{
	m_parameterModels.refreshFromHost();
}

} // namespace lmms
