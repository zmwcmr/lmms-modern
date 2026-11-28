/*
 * HostedEffect.h - generic Effect wrapper for a PluginHost
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

#ifndef LMMS_HOSTED_EFFECT_H
#define LMMS_HOSTED_EFFECT_H

#include <memory>
#include <vector>

#include "Effect.h"
#include "EffectControls.h"
#include "HostedPluginModels.h"
#include "PluginHost.h"
#include "lmms_export.h"

namespace lmms
{

/*! EffectControls for HostedEffect. It owns one FloatModel per parameter and
 *  stores the plugin state next to the models in the effect XML.
 *
 *  The view is intentionally not implemented yet; editor hosting is a later
 *  stage. Existing format plugins keep their own views.
 */
class LMMS_EXPORT HostedEffectControls : public EffectControls
{
public:
	HostedEffectControls(Effect* effect, PluginHost* host);
	~HostedEffectControls() override = default;

	void initializeModels();
	void saveSettings(QDomDocument& doc, QDomElement& element) override;
	void loadSettings(const QDomElement& element) override;

	QString nodeName() const override { return QStringLiteral("hostedeffect"); }
	int controlCount() override { return m_models.count(); }
	gui::EffectControlDialog* createView() override { return nullptr; }

	HostedPluginModels& parameterModels() { return m_models; }
	const HostedPluginModels& parameterModels() const { return m_models; }

private:
	PluginHost* m_host;
	HostedPluginModels m_models;
};

/*! Effect wrapper that owns a PluginHost.
 *
 *  Format plugins subclass this, create the host in their constructor and
 *  call initializeModels() after loading. The base handles the audio block,
 *  parameters, latency and the generic state representation.
 */
class LMMS_EXPORT HostedEffect : public Effect
{
public:
	HostedEffect(const Descriptor* descriptor, Model* parent,
		const Descriptor::SubPluginFeatures::Key* key,
		std::unique_ptr<PluginHost> host);
	~HostedEffect() override = default;

	ProcessStatus processImpl(SampleFrame* buffer,
		const f_cnt_t frames) override;

	EffectControls* controls() override { return &m_controls; }
	f_cnt_t latency() const override;
	bool supportsSidechain() const override
	{
		return m_host != nullptr && m_host->inputBuses().size() > 1;
	}

	void initializeModels();

	PluginHost* host() const { return m_host.get(); }
	HostedEffectControls& hostedControls() { return m_controls; }

private:
	std::unique_ptr<PluginHost> m_host;
	HostedEffectControls m_controls;
	std::vector<float> m_inputLeft;
	std::vector<float> m_inputRight;
	std::vector<float> m_sidechainLeft;
	std::vector<float> m_sidechainRight;
	std::vector<float> m_outputLeft;
	std::vector<float> m_outputRight;
};

} // namespace lmms

#endif // LMMS_HOSTED_EFFECT_H
