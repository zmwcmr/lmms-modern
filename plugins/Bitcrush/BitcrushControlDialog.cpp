/*
 * BitcrushControlDialog.cpp - A native bitcrusher
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


#include <QHBoxLayout>
#include <QLabel>
#include <QResizeEvent>
#include <QVBoxLayout>

#include "embed.h"
#include "BitcrushControlDialog.h"
#include "BitcrushControls.h"
#include "FontHelper.h"
#include "LedCheckBox.h"
#include "Knob.h"

namespace lmms::gui
{


BitcrushControlDialog::BitcrushControlDialog( BitcrushControls * controls ) :
	EffectControlDialog( controls )
{
	setAutoFillBackground( true );

	// labels
	const auto labelFont = adjustedToPixelSize(font(), DEFAULT_FONT_SIZE);

	auto inLabel = new QLabel(tr("IN"), this);
	inLabel->setFont(labelFont);
	inLabel->setAlignment(Qt::AlignHCenter);

	auto outLabel = new QLabel(tr("OUT"), this);
	outLabel->setFont(labelFont);
	outLabel->setAlignment(Qt::AlignHCenter);

	// leds
	auto rateEnabled = new LedCheckBox("", this, tr("Rate enabled"), LedCheckBox::LedColor::Green);
	rateEnabled->setModel( & controls->m_rateEnabled );
	rateEnabled->setToolTip(tr("Enable sample-rate crushing"));

	auto depthEnabled = new LedCheckBox("", this, tr("Depth enabled"), LedCheckBox::LedColor::Green);
	depthEnabled->setModel( & controls->m_depthEnabled );
	depthEnabled->setToolTip(tr("Enable bit-depth crushing"));

	// input knobs
	auto inGain = new Knob(KnobType::Bright26, tr("GAIN"), SMALL_FONT_SIZE, this);
	inGain->setModel( & controls->m_inGain );
	inGain->setHintText( tr( "Input gain:" ) , " dBFS" );

	auto inNoise = new Knob(KnobType::Bright26, tr("NOISE"), SMALL_FONT_SIZE, this);
	inNoise->setModel( & controls->m_inNoise );
	inNoise->setHintText( tr( "Input noise:" ) , "%" );

	// output knobs
	auto outGain = new Knob(KnobType::Bright26, tr("GAIN"), SMALL_FONT_SIZE, this);
	outGain->setModel( & controls->m_outGain );
	outGain->setHintText( tr( "Output gain:" ) , " dBFS" );

	auto outClip = new Knob(KnobType::Bright26, tr("CLIP"), SMALL_FONT_SIZE, this);
	outClip->setModel( & controls->m_outClip );
	outClip->setHintText( tr( "Output clip:" ) , " dBFS");

	// rate crushing knobs
	auto rate = new Knob(KnobType::Bright26, tr("FREQ"), SMALL_FONT_SIZE, this);
	rate->setModel( & controls->m_rate );
	rate->setHintText( tr( "Sample rate:" ) , " Hz" );

	auto stereoDiff = new Knob(KnobType::Bright26, tr("STEREO"), SMALL_FONT_SIZE, this);
	stereoDiff->setModel( & controls->m_stereoDiff );
	stereoDiff->setHintText( tr( "Stereo difference:" ) , "%" );

	// depth crushing knob
	auto levels = new Knob(KnobType::Bright26, tr("QUANT"), SMALL_FONT_SIZE, this);
	levels->setModel( & controls->m_levels );
	levels->setHintText( tr( "Levels:" ) , "" );

	auto* leftLayout = new QVBoxLayout;
	leftLayout->setSpacing(2);
	leftLayout->addWidget(inLabel);
	leftLayout->addWidget(inGain, 0, Qt::AlignHCenter);
	leftLayout->addWidget(inNoise, 0, Qt::AlignHCenter);
	leftLayout->addStretch();

	auto* centerTop = new QHBoxLayout;
	centerTop->setSpacing(6);
	centerTop->addStretch();
	centerTop->addWidget(rateEnabled);
	centerTop->addWidget(depthEnabled);
	centerTop->addStretch();

	auto* centerKnobsTop = new QHBoxLayout;
	centerKnobsTop->setSpacing(4);
	centerKnobsTop->addStretch();
	centerKnobsTop->addWidget(rate);
	centerKnobsTop->addWidget(levels);
	centerKnobsTop->addStretch();

	auto* centerLayout = new QVBoxLayout;
	centerLayout->setSpacing(2);
	centerLayout->addLayout(centerTop);
	centerLayout->addLayout(centerKnobsTop);
	centerLayout->addWidget(stereoDiff, 0, Qt::AlignHCenter);
	centerLayout->addStretch();

	auto* rightLayout = new QVBoxLayout;
	rightLayout->setSpacing(2);
	rightLayout->addWidget(outLabel);
	rightLayout->addWidget(outGain, 0, Qt::AlignHCenter);
	rightLayout->addWidget(outClip, 0, Qt::AlignHCenter);
	rightLayout->addStretch();

	auto* mainLayout = new QHBoxLayout(this);
	mainLayout->setContentsMargins(8, 6, 8, 6);
	mainLayout->setSpacing(8);
	mainLayout->addLayout(leftLayout);
	mainLayout->addLayout(centerLayout, 1);
	mainLayout->addLayout(rightLayout);

	setMinimumSize(181, 128);
	adjustSize();
	updateBackground();
}


void BitcrushControlDialog::resizeEvent(QResizeEvent* event)
{
	EffectControlDialog::resizeEvent(event);
	updateBackground();
}


void BitcrushControlDialog::updateBackground()
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
