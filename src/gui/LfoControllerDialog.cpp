/*
 * LfoControllerDialog.cpp - per-controller-specific view for changing a
 *                           controller's settings
 *
 * Copyright (c) 2008-2009 Paul Giblock <drfaygo/at/gmail.com>
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


#include <QGridLayout>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QVBoxLayout>

#include <string>

#include "FileDialog.h"
#include "embed.h"


#include "FontHelper.h"
#include "LfoController.h"
#include "Knob.h"
#include "TempoSyncKnob.h"
#include "PixmapButton.h"

namespace lmms::gui
{


LfoControllerDialog::LfoControllerDialog( Controller * _model, QWidget * _parent ) :
	ControllerDialog( _model, _parent )
{
	QString title = tr( "LFO" );
	title.append( " (" );
	title.append( _model->name() );
	title.append( ")" );
	setWindowTitle( title );
	setWindowIcon( embed::getIconPixmap( "controller" ) );

	m_baseKnob = new Knob(KnobType::Bright26, tr("BASE"), SMALL_FONT_SIZE, this);
	m_baseKnob->setHintText( tr( "Base:" ), "" );

	m_speedKnob = new TempoSyncKnob(KnobType::Bright26, tr("FREQ"), SMALL_FONT_SIZE, this);
	m_speedKnob->setHintText( tr( "LFO frequency:" ), "" );

	m_amountKnob = new Knob(KnobType::Bright26, tr("AMNT"), SMALL_FONT_SIZE, this);
	m_amountKnob->setHintText( tr( "Modulation amount:" ), "" );

	m_phaseKnob = new Knob(KnobType::Bright26, tr("PHS"), SMALL_FONT_SIZE, this);
	m_phaseKnob->setHintText( tr( "Phase offset:" ) , "" + tr( " degrees" ) );

	auto* knobLayout = new QHBoxLayout;
	knobLayout->setSpacing(4);
	for (auto* knob : {m_baseKnob, static_cast<Knob*>(m_speedKnob), m_amountKnob, m_phaseKnob})
	{
		knobLayout->addWidget(knob, 0, Qt::AlignTop);
	}

	auto* waveGrid = new QGridLayout;
	waveGrid->setHorizontalSpacing(2);
	waveGrid->setVerticalSpacing(2);

	const auto addWaveButton = [this, waveGrid](const char* icon, const QString& tooltip, int row, int column)
	{
		auto* button = new PixmapButton(this, nullptr);
		button->setActiveGraphic(embed::getIconPixmap(std::string(icon) + "_active"));
		button->setInactiveGraphic(embed::getIconPixmap(std::string(icon) + "_inactive"));
		button->setToolTip(tooltip);
		waveGrid->addWidget(button, row, column);
		return button;
	};

	auto* sin_wave_btn = addWaveButton("sin_wave", tr("Sine wave"), 0, 0);
	auto* triangle_wave_btn = addWaveButton("triangle_wave", tr("Triangle wave"), 0, 1);
	auto* saw_wave_btn = addWaveButton("saw_wave", tr("Saw wave"), 0, 2);
	auto* sqr_wave_btn = addWaveButton("square_wave", tr("Square wave"), 0, 3);
	auto* moog_saw_wave_btn = addWaveButton("moog_saw_wave", tr("Moog saw wave"), 1, 0);
	auto* exp_wave_btn = addWaveButton("exp_wave", tr("Exponential wave"), 1, 1);
	auto* white_noise_btn = addWaveButton("white_noise_wave", tr("White noise"), 1, 2);

	m_userWaveBtn = addWaveButton("usr_wave",
		tr("User-defined shape.\nDouble click to pick a file."), 1, 3);
	connect(m_userWaveBtn, SIGNAL(doubleClicked()), this, SLOT(askUserDefWave()));

	m_waveBtnGrp = new AutomatableButtonGroup( this );
	m_waveBtnGrp->addButton( sin_wave_btn );
	m_waveBtnGrp->addButton( triangle_wave_btn );
	m_waveBtnGrp->addButton( saw_wave_btn );
	m_waveBtnGrp->addButton( sqr_wave_btn );
	m_waveBtnGrp->addButton( moog_saw_wave_btn );
	m_waveBtnGrp->addButton( exp_wave_btn );
	m_waveBtnGrp->addButton( white_noise_btn );
	m_waveBtnGrp->addButton( m_userWaveBtn );

	auto* x1 = new PixmapButton(this, nullptr);
	x1->setActiveGraphic(embed::getIconPixmap("lfo_x1_active"));
	x1->setInactiveGraphic(embed::getIconPixmap("lfo_x1_inactive"));
	x1->setToolTip(tr("Multiply modulation frequency by 1"));

	auto* x100 = new PixmapButton(this, nullptr);
	x100->setActiveGraphic(embed::getIconPixmap("lfo_x100_active"));
	x100->setInactiveGraphic(embed::getIconPixmap("lfo_x100_inactive"));
	x100->setToolTip(tr("Multiply modulation frequency by 100"));

	auto* d100 = new PixmapButton(this, nullptr);
	d100->setActiveGraphic(embed::getIconPixmap("lfo_d100_active"));
	d100->setInactiveGraphic(embed::getIconPixmap("lfo_d100_inactive"));
	d100->setToolTip(tr("Divide modulation frequency by 100"));

	m_multiplierBtnGrp = new AutomatableButtonGroup( this );
	m_multiplierBtnGrp->addButton( x1 );
	m_multiplierBtnGrp->addButton( x100 );
	m_multiplierBtnGrp->addButton( d100 );

	auto* multiplierLayout = new QVBoxLayout;
	multiplierLayout->setSpacing(2);
	multiplierLayout->addWidget(x100);
	multiplierLayout->addWidget(x1);
	multiplierLayout->addWidget(d100);
	multiplierLayout->addStretch();

	auto* mainLayout = new QHBoxLayout(this);
	mainLayout->setContentsMargins(6, 6, 6, 6);
	mainLayout->setSpacing(6);
	mainLayout->addLayout(waveGrid);
	mainLayout->addLayout(knobLayout, 1);
	mainLayout->addLayout(multiplierLayout);

	setAutoFillBackground( true );
	adjustSize();
	updateBackground();

	setModel( _model );
}




void LfoControllerDialog::resizeEvent(QResizeEvent* event)
{
	QWidget::resizeEvent(event);
	updateBackground();
}


void LfoControllerDialog::updateBackground()
{
	auto background = embed::getIconPixmap("lfo_controller_artwork");
	const auto dpr = background.devicePixelRatio();
	background = background.scaled(qRound(width() * dpr), qRound(height() * dpr),
		Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	background.setDevicePixelRatio(dpr);
	QPalette pal = palette();
	pal.setBrush(backgroundRole(), background);
	setPalette(pal);
}


LfoControllerDialog::~LfoControllerDialog()
{
	m_userWaveBtn->disconnect( this );
}



void LfoControllerDialog::askUserDefWave()
{
	const auto fileName = FileDialog::openWaveformFile();
	if (fileName.isEmpty()) { return; }

	auto lfoModel = dynamic_cast<LfoController*>(model());
	auto& buffer = lfoModel->m_userDefSampleBuffer;
	buffer = SampleBuffer::fromFile(fileName);

	m_userWaveBtn->setToolTip(buffer->audioFile());
}



void LfoControllerDialog::contextMenuEvent( QContextMenuEvent * )
{
	/*
	QPointer<captionMenu> contextMenu = new captionMenu(
						getEffect()->publicName() );
		// ^ if this ever gets activated again:
		//   a47a36c86e4473af862df58d2b59f6cb8620b9ee says that publicName
		//   should be replaced by Model::displayName
	contextMenu->addAction( embed::getIconPixmap( "arp_up_on" ),
						tr( "Move &up" ),
						this, SLOT(moveUp()));
	contextMenu->addAction( embed::getIconPixmap( "arp_down_on" ),
						tr( "Move &down" ),
						this, SLOT(moveDown()));
	contextMenu->addSeparator();
	contextMenu->addAction( embed::getIconPixmap( "cancel" ),
						tr( "&Remove this plugin" ),
						this, SLOT(deletePlugin()));
	contextMenu->exec( QCursor::pos() );
	delete contextMenu;
	*/
}



void LfoControllerDialog::modelChanged()
{
	m_lfo = castModel<LfoController>();

	m_baseKnob->setModel( &m_lfo->m_baseModel );
	m_speedKnob->setModel( &m_lfo->m_speedModel );
	m_amountKnob->setModel( &m_lfo->m_amountModel );
	m_phaseKnob->setModel( &m_lfo->m_phaseModel );
	m_waveBtnGrp->setModel( &m_lfo->m_waveModel );
	m_multiplierBtnGrp->setModel( &m_lfo->m_multiplierModel );
}


} // namespace lmms::gui
