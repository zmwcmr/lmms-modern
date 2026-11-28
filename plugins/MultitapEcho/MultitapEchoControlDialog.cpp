/*
 * MultitapEchoControlDialog.cpp - a multitap echo delay plugin
 *
 * Copyright (c) 2014 Vesa Kivimäki <contact/dot/diizy/at/nbl/dot/fi>
 * Copyright (c) 2008-2014 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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


#include "MultitapEchoControlDialog.h"
#include "MultitapEchoControls.h"
#include "embed.h"
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QVBoxLayout>

#include "FontHelper.h"
#include "Graph.h"
#include "LedCheckBox.h"
#include "Knob.h"
#include "TempoSyncKnob.h"
#include "LcdSpinBox.h"

namespace lmms::gui
{


MultitapEchoControlDialog::MultitapEchoControlDialog( MultitapEchoControls * controls ) :
	EffectControlDialog( controls )
{
	setAutoFillBackground( true );

	// graph widgets
	auto ampGraph = new Graph(this, Graph::Style::Bar, 204, 105);
	auto lpGraph = new Graph(this, Graph::Style::Bar, 204, 105);

	ampGraph->setModel( & controls->m_ampGraph );
	lpGraph->setModel( & controls->m_lpGraph );

	QPalette pal;
	pal.setBrush( backgroundRole(),	PLUGIN_NAME::getIconPixmap("graph_bg") );
	ampGraph->setAutoFillBackground( true );
	ampGraph->setPalette( pal );
	ampGraph->setGraphColor( QColor( 11, 213, 86) );
	ampGraph->setMaximumSize( 204, 105 );
	lpGraph->setAutoFillBackground( true );
	lpGraph->setPalette( pal );
	lpGraph->setGraphColor( QColor( 0, 200, 187) );
	lpGraph->setMaximumSize( 204, 105 );

	auto steps = new LcdSpinBox(2, this, "Steps");
	steps->setModel( & controls->m_steps );

	auto stepLength = new TempoSyncKnob(KnobType::Bright26, tr("Length"), SMALL_FONT_SIZE,  this);
	stepLength->setModel( & controls->m_stepLength );
	stepLength->setHintText( tr( "Step length:" ) , " ms" );

	auto dryGain = new Knob(KnobType::Bright26, tr("Dry"), SMALL_FONT_SIZE, this);
	dryGain->setModel( & controls->m_dryGain );
	dryGain->setHintText( tr( "Dry gain:" ) , " dBFS" );

	auto stages = new Knob(KnobType::Bright26, tr("Stages"), SMALL_FONT_SIZE, this);
	stages->setModel( & controls->m_stages );
	stages->setHintText( tr( "Low-pass stages:" ) , "x" );

	auto swapInputs = new LedCheckBox("Swap inputs", this, tr("Swap inputs"), LedCheckBox::LedColor::Green);
	swapInputs->setModel( & controls->m_swapInputs );
	swapInputs->setToolTip(tr("Swap left and right input channels for reflections"));

	auto* controlsLayout = new QHBoxLayout;
	controlsLayout->setSpacing(4);
	controlsLayout->addWidget(steps);
	controlsLayout->addWidget(stepLength);
	controlsLayout->addWidget(dryGain);
	controlsLayout->addWidget(stages);

	auto* mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(10, 8, 10, 8);
	mainLayout->setSpacing(2);
	mainLayout->addWidget(ampGraph, 0, Qt::AlignHCenter);
	mainLayout->addWidget(lpGraph, 0, Qt::AlignHCenter);
	mainLayout->addLayout(controlsLayout);
	mainLayout->addWidget(swapInputs);
	mainLayout->addStretch();

	setMinimumSize(245, 300);
	adjustSize();
	updateBackground();
}



void MultitapEchoControlDialog::resizeEvent(QResizeEvent* event)
{
	EffectControlDialog::resizeEvent(event);
	updateBackground();
}


void MultitapEchoControlDialog::updateBackground()
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
