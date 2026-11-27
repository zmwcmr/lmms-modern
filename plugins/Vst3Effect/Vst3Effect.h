/*
 * Vst3Effect.h - VST3 effect host
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

#ifndef LMMS_VST3_EFFECT_H
#define LMMS_VST3_EFFECT_H

#include <memory>
#include <vector>

#include <QMutex>

#include "Effect.h"
#include "HostedEffect.h"
#include "Vst3EffectControls.h"

namespace lmms
{

class Vst3HostBase;
class Vst3RemoteHostClient;

class Vst3Effect : public HostedEffect
{
	Q_OBJECT
public:
	Vst3Effect(Model* parent, const Descriptor::SubPluginFeatures::Key* key);
	~Vst3Effect() override;

	ProcessStatus processImpl(SampleFrame* buf, const f_cnt_t frames) override;

	EffectControls* controls() override
	{
		return &m_controls;
	}

	void saveSettings(QDomDocument& doc, QDomElement& parent) override;
	void loadSettings(const QDomElement& parent) override;

	Vst3HostBase* host() const { return m_host.get(); }
	QMutex& hostMutex() { return m_mutex; }
	void setParameterValue(uint32_t id, double value);
	bool setProgram(int index);
	bool setUnit(int unitId);

private:
	bool openPlugin();

	std::unique_ptr<Vst3HostBase> m_host;
		QMutex m_mutex;
	EffectKey m_key;
	Vst3EffectControls m_controls;

	friend class Vst3EffectControls;
};

} // namespace lmms

#endif // LMMS_VST3_EFFECT_H
