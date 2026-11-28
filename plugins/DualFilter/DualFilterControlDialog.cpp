/*
 * DualFilterControlDialog.cpp - control dialog for dual filter effect
 *
 * Copyright (c) 2014 Vesa Kivimäki <contact/dot/diizy/at/nbl/dot/fi>
 * Copyright (c) 2006-2014 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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


#include "DualFilterControlDialog.h"

#include <QHBoxLayout>
#include <QResizeEvent>
#include <QVBoxLayout>
#include "DualFilterControls.h"
#include "FontHelper.h"
#include "Knob.h"
#include "LedCheckBox.h"
#include "ComboBox.h"

namespace lmms::gui
{

DualFilterControlDialog::DualFilterControlDialog( DualFilterControls* controls ) :
	EffectControlDialog( controls )
{
	setAutoFillBackground( true );

	auto makeKnob = [this](Model* model, const QString& label,
		const QString& hint, const QString& unit, bool isVolumeKnob = false) -> Knob*
	{
		Knob* knob = isVolumeKnob
			? new VolumeKnob(KnobType::Bright26, label, SMALL_FONT_SIZE, this)
			: new Knob(KnobType::Bright26, label, SMALL_FONT_SIZE, this);
		knob->setModel(model);
		knob->setHintText(hint, unit);
		return knob;
	};

	auto enabled1Toggle = new LedCheckBox("", this, tr("Filter 1 enabled"), LedCheckBox::LedColor::Green);
	enabled1Toggle->setModel( &controls->m_enabled1Model );
	enabled1Toggle->setToolTip(tr("Enable/disable filter 1"));

	auto enabled2Toggle = new LedCheckBox("", this, tr("Filter 2 enabled"), LedCheckBox::LedColor::Green);
	enabled2Toggle->setModel( &controls->m_enabled2Model );
	enabled2Toggle->setToolTip(tr("Enable/disable filter 2"));

	auto* filter1Knobs = new QHBoxLayout;
	filter1Knobs->setSpacing(4);
	filter1Knobs->addWidget(makeKnob(&controls->m_cut1Model, tr("FREQ"), tr("Cutoff frequency"), "Hz"));
	filter1Knobs->addWidget(makeKnob(&controls->m_res1Model, tr("RESO"), tr("Resonance"), ""));
	filter1Knobs->addWidget(makeKnob(&controls->m_gain1Model, tr("GAIN"), tr("Gain"), "%", true));

	auto m_filter1ComboBox = new ComboBox(this);
	m_filter1ComboBox->setModel( &controls->m_filter1Model );

	auto* filter1Layout = new QVBoxLayout;
	filter1Layout->setSpacing(2);
	auto* filter1Header = new QHBoxLayout;
	filter1Header->addWidget(enabled1Toggle);
	filter1Header->addLayout(filter1Knobs, 1);
	filter1Layout->addLayout(filter1Header);
	filter1Layout->addWidget(m_filter1ComboBox);

	auto* filter2Knobs = new QHBoxLayout;
	filter2Knobs->setSpacing(4);
	filter2Knobs->addWidget(makeKnob(&controls->m_cut2Model, tr("FREQ"), tr("Cutoff frequency"), "Hz"));
	filter2Knobs->addWidget(makeKnob(&controls->m_res2Model, tr("RESO"), tr("Resonance"), ""));
	filter2Knobs->addWidget(makeKnob(&controls->m_gain2Model, tr("GAIN"), tr("Gain"), "%", true));

	auto m_filter2ComboBox = new ComboBox(this);
	m_filter2ComboBox->setModel( &controls->m_filter2Model );

	auto* filter2Layout = new QVBoxLayout;
	filter2Layout->setSpacing(2);
	auto* filter2Header = new QHBoxLayout;
	filter2Header->addWidget(enabled2Toggle);
	filter2Header->addLayout(filter2Knobs, 1);
	filter2Layout->addLayout(filter2Header);
	filter2Layout->addWidget(m_filter2ComboBox);

	auto* mixKnob = makeKnob(&controls->m_mixModel, tr("MIX"), tr("Mix"), "");
	auto* mixLayout = new QVBoxLayout;
	mixLayout->addWidget(mixKnob, 0, Qt::AlignHCenter);
	mixLayout->addStretch();

	auto* mainLayout = new QHBoxLayout(this);
	mainLayout->setContentsMargins(10, 8, 10, 8);
	mainLayout->setSpacing(8);
	mainLayout->addLayout(filter1Layout, 1);
	mainLayout->addLayout(mixLayout);
	mainLayout->addLayout(filter2Layout, 1);

	setMinimumSize(373, 109);
	adjustSize();
	updateBackground();
}



void DualFilterControlDialog::resizeEvent(QResizeEvent* event)
{
	EffectControlDialog::resizeEvent(event);
	updateBackground();
}


void DualFilterControlDialog::updateBackground()
{
	auto background = PLUGIN_NAME::getIconPixmap("artwork");
	const auto dpr = background.devicePixelRatio();
	background = background.scaled(qRound(width() * dpr), qRound(height() * dpr),
		Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	background.setDevicePixelRatio(dpr);
	QPalette pal = palette();
	pal.setBrush(backgroundRole(), background);
	setPalette(pal);
}


} // namespace lmms::gui
