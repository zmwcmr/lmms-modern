/*
 * DynamicsProcessorControlDialog.cpp - control-dialog for DynamicsProcessor-effect
 *
 * Copyright (c) 2014 Vesa Kivimäki <contact/dot/diizy/at/nbl/dot/fi>
 * Copyright (c) 2006-2008 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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



#include "DynamicsProcessorControlDialog.h"
#include "DynamicsProcessorControls.h"
#include "embed.h"
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QVBoxLayout>

#include "FontHelper.h"
#include "Graph.h"
#include "Knob.h"
#include "PixmapButton.h"

namespace lmms::gui
{


DynProcControlDialog::DynProcControlDialog(
					DynProcControls * _controls ) :
	EffectControlDialog( _controls )
{
	setAutoFillBackground( true );

	auto waveGraph = new Graph(this, Graph::Style::LinearNonCyclic, 204, 205);
	waveGraph->setModel( &_controls->m_wavegraphModel );
	waveGraph->setAutoFillBackground( true );
	QPalette pal;
	pal.setBrush( backgroundRole(), PLUGIN_NAME::getIconPixmap("wavegraph") );
	waveGraph->setPalette( pal );
	waveGraph->setGraphColor( QColor( 85, 204, 145 ) );
	waveGraph->setMaximumSize( 204, 205 );

	auto inputKnob = new VolumeKnob(KnobType::Bright26, tr("INPUT"), SMALL_FONT_SIZE, this);
	inputKnob->setZeroDbfsPoint(1.f);
	inputKnob->setModel( &_controls->m_inputModel );
	inputKnob->setHintText( tr( "Input gain:" ) , "" );

	auto outputKnob = new VolumeKnob(KnobType::Bright26, tr("OUTPUT"), SMALL_FONT_SIZE, this);
	outputKnob->setZeroDbfsPoint(1.f);
	outputKnob->setModel( &_controls->m_outputModel );
	outputKnob->setHintText( tr( "Output gain:" ), "" );

	auto attackKnob = new Knob(KnobType::Bright26, tr("ATTACK"), SMALL_FONT_SIZE, this);
	attackKnob->setModel( &_controls->m_attackModel );
	attackKnob->setHintText( tr( "Peak attack time:" ) , "ms" );

	auto releaseKnob = new Knob(KnobType::Bright26, tr("RELEASE"), SMALL_FONT_SIZE, this);
	releaseKnob->setModel( &_controls->m_releaseModel );
	releaseKnob->setHintText( tr( "Peak release time:" ) , "ms" );

	auto resetButton = new PixmapButton(this, tr("Reset wavegraph"));
	resetButton->resize( 13, 48 );
	resetButton->setActiveGraphic( PLUGIN_NAME::getIconPixmap( "reset_active" ) );
	resetButton->setInactiveGraphic( PLUGIN_NAME::getIconPixmap( "reset_inactive" ) );
	resetButton->setToolTip(tr("Reset wavegraph"));

	auto smoothButton = new PixmapButton(this, tr("Smooth wavegraph"));
	smoothButton->resize( 13, 48 );
	smoothButton->setActiveGraphic( PLUGIN_NAME::getIconPixmap( "smooth_active" ) );
	smoothButton->setInactiveGraphic( PLUGIN_NAME::getIconPixmap( "smooth_inactive" ) );
	smoothButton->setToolTip(tr("Smooth wavegraph"));

	auto addOneButton = new PixmapButton(this, tr("Increase wavegraph amplitude by 1 dB"));
	addOneButton->resize( 13, 29 );
	addOneButton->setActiveGraphic( PLUGIN_NAME::getIconPixmap( "add1_active" ) );
	addOneButton->setInactiveGraphic( PLUGIN_NAME::getIconPixmap( "add1_inactive" ) );
	addOneButton->setToolTip(tr("Increase wavegraph amplitude by 1 dB"));

	auto subOneButton = new PixmapButton(this, tr("Decrease wavegraph amplitude by 1 dB"));
	subOneButton->resize( 13, 29 );
	subOneButton->setActiveGraphic( PLUGIN_NAME::getIconPixmap( "sub1_active" ) );
	subOneButton->setInactiveGraphic( PLUGIN_NAME::getIconPixmap( "sub1_inactive" ) );
	subOneButton->setToolTip(tr("Decrease wavegraph amplitude by 1 dB"));

	auto smMaxButton = new PixmapButton(this, tr("Stereo mode: maximum"));
	smMaxButton->resize( 78, 17 );
	smMaxButton->setActiveGraphic( PLUGIN_NAME::getIconPixmap( "max_active" ) );
	smMaxButton->setInactiveGraphic( PLUGIN_NAME::getIconPixmap( "max_inactive" ) );
	smMaxButton->setToolTip(tr("Process based on the maximum of both stereo channels"));

	auto smAvgButton = new PixmapButton(this, tr("Stereo mode: average"));
	smAvgButton->resize( 78, 16 );
	smAvgButton->setActiveGraphic( PLUGIN_NAME::getIconPixmap( "avg_active" ) );
	smAvgButton->setInactiveGraphic( PLUGIN_NAME::getIconPixmap( "avg_inactive" ) );
	smAvgButton->setToolTip(tr("Process based on the average of both stereo channels"));

	auto smUnlButton = new PixmapButton(this, tr("Stereo mode: unlinked"));
	smUnlButton->resize( 78, 17 );
	smUnlButton->setActiveGraphic( PLUGIN_NAME::getIconPixmap( "unl_active" ) );
	smUnlButton->setInactiveGraphic( PLUGIN_NAME::getIconPixmap( "unl_inactive" ) );
	smUnlButton->setToolTip(tr("Process each stereo channel independently"));

	auto smGroup = new AutomatableButtonGroup(this);
	smGroup->addButton( smMaxButton );
	smGroup->addButton( smAvgButton );
	smGroup->addButton( smUnlButton );
	smGroup->setModel( &_controls->m_stereomodeModel );

	auto* knobRow = new QHBoxLayout;
	knobRow->setSpacing(4);
	knobRow->addWidget(inputKnob);
	knobRow->addWidget(outputKnob);
	auto* timeRow = new QHBoxLayout;
	timeRow->setSpacing(4);
	timeRow->addWidget(attackKnob);
	timeRow->addWidget(releaseKnob);

	auto* buttonRow = new QHBoxLayout;
	buttonRow->setSpacing(2);
	buttonRow->addWidget(addOneButton);
	buttonRow->addWidget(subOneButton);
	buttonRow->addWidget(resetButton);
	buttonRow->addWidget(smoothButton);
	auto* stereoLayout = new QVBoxLayout;
	stereoLayout->setSpacing(0);
	stereoLayout->addWidget(smMaxButton);
	stereoLayout->addWidget(smAvgButton);
	stereoLayout->addWidget(smUnlButton);
	buttonRow->addLayout(stereoLayout);
	buttonRow->addStretch();

	auto* mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(10, 8, 10, 8);
	mainLayout->setSpacing(4);
	mainLayout->addWidget(waveGraph, 0, Qt::AlignHCenter);
	mainLayout->addLayout(knobRow);
	mainLayout->addLayout(timeRow);
	mainLayout->addLayout(buttonRow);
	mainLayout->addStretch();

	setMinimumSize(224, 319);
	adjustSize();
	updateBackground();

	connect( resetButton, SIGNAL (clicked () ),
			_controls, SLOT ( resetClicked() ) );
	connect( smoothButton, SIGNAL (clicked () ),
			_controls, SLOT ( smoothClicked() ) );
	connect( addOneButton, SIGNAL( clicked() ),
			_controls, SLOT( addOneClicked() ) );
	connect( subOneButton, SIGNAL( clicked() ),
			_controls, SLOT( subOneClicked() ) );
}



void DynProcControlDialog::resizeEvent(QResizeEvent* event)
{
	EffectControlDialog::resizeEvent(event);
	updateBackground();
}


void DynProcControlDialog::updateBackground()
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
