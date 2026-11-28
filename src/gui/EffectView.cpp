/*
 * EffectView.cpp - view-component for an effect
 *
 * Copyright (c) 2006-2007 Danny McRae <khjklujn/at/users.sourceforge.net>
 * Copyright (c) 2007-2014 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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

#include <QGraphicsOpacityEffect>
#include <QLayout>
#include <QMouseEvent>
#include <QPushButton>
#include <QPainter>

#include "EffectView.h"
#include "DummyEffect.h"
#include "CaptionMenu.h"
#include "embed.h"
#include "GuiApplication.h"
#include "FontHelper.h"
#include "Knob.h"
#include "LedCheckBox.h"
#include "MainWindow.h"
#include "SubWindow.h"
#include "TempoSyncKnob.h"


namespace lmms::gui
{

EffectView::EffectView( Effect * _model, QWidget * _parent ) :
	PluginView( _model, _parent ),
	m_bg( embed::getIconPixmap( "effect_plugin" ) ),
	m_subWindow( nullptr ),
	m_controlView(nullptr),
	m_dragging(false)
{
	setMinimumWidth(EffectView::DEFAULT_WIDTH);
	resize(EffectView::DEFAULT_WIDTH, EffectView::DEFAULT_HEIGHT);
	setFocusPolicy(Qt::StrongFocus);

	// Disable effects that are of type "DummyEffect"
	bool isEnabled = !dynamic_cast<DummyEffect *>( effect() );
	m_bypass = new LedCheckBox( this, "", isEnabled ? LedCheckBox::LedColor::Green : LedCheckBox::LedColor::Red );
	m_bypass->move( 3, 3 );
	m_bypass->setEnabled( isEnabled );
	m_bypass->setToolTip(tr("On/Off"));

	connect(getGUI()->mainWindow(), &MainWindow::periodicUpdate, this, &EffectView::corruptStateUpdate);

	m_wetDry = new Knob(KnobType::Bright26, tr("W/D"), SMALL_FONT_SIZE, this);
	m_wetDry->move( 40 - m_wetDry->width() / 2, 5 );
	m_wetDry->setEnabled( isEnabled );
	m_wetDry->setHintText( tr( "Wet level:" ), "" );


	m_autoQuit = new TempoSyncKnob(KnobType::Bright26, tr("DECAY"), SMALL_FONT_SIZE, this);
	m_autoQuit->move(qMax(78 - m_autoQuit->width() / 2, m_wetDry->geometry().right() + 5), 5);
	m_autoQuit->setEnabled(isEnabled && effect()->autoQuitEnabled());
	m_autoQuit->setHintText( tr( "Time:" ), "ms" );
	const int titleHeight = QFontMetrics(bodyFont(font())).height();
	setFixedHeight(qMax(DEFAULT_HEIGHT, 5 + qMax(m_wetDry->height(), m_autoQuit->height()) + 6 + titleHeight + 5));

	setModel( _model );

	if( effect()->controls()->controlCount() > 0 )
	{
		auto ctls_btn = new QPushButton(tr("Controls"), this);
		QFont f = adjustedToPixelSize(ctls_btn->font(), DEFAULT_FONT_SIZE);
		ctls_btn->setFont(f);
		// The button sits in a fixed-size effect view. Size it from the scaled
		// font and keep its original right/bottom corner anchored.
		const QFontMetrics buttonMetrics(f);
		const int buttonWidth = buttonMetrics.horizontalAdvance(ctls_btn->text()) + 12;
		const int buttonHeight = buttonMetrics.height() + 4;
		const int buttonX = qMax(200 - buttonWidth, m_autoQuit->geometry().right() + 8);
		ctls_btn->setGeometry(buttonX, 34 - buttonHeight, buttonWidth, buttonHeight);
		setMinimumWidth(qMax(DEFAULT_WIDTH, buttonX + buttonWidth + 15));
		ctls_btn->setFocusPolicy(Qt::NoFocus);
		connect( ctls_btn, SIGNAL(clicked()),
					this, SLOT(editControls()));

		m_controlView = effect()->controls()->createView();
		if (m_controlView)
		{
			m_subWindow = getGUI()->mainWindow()->addWindowedWidget(m_controlView);
			m_subWindow->setWindowFlag(Qt::WindowMaximizeButtonHint, false);
			m_subWindow->hide();
		}
	}

	m_opacityEffect = new QGraphicsOpacityEffect(this);
	m_opacityEffect->setOpacity(1);
	setGraphicsEffect(m_opacityEffect);

	//move above vst effect view creation
	//setModel( _model );
}




EffectView::~EffectView()
{
	delete m_subWindow;
}




void EffectView::editControls()
{
	if( m_subWindow )
	{
		if (!m_controlView->isVisible())
		{
			m_subWindow->show();
			m_subWindow->raise();
			effect()->controls()->setViewVisible(true); // TODO is this even needed?
		}
		else
		{
			m_subWindow->hide();
			effect()->controls()->setViewVisible( false );
		}
	}
}




void EffectView::moveUp()
{
	emit movedUp(this);
}




void EffectView::moveDown()
{
	emit movedDown(this);
}



void EffectView::deletePlugin()
{
	emit deletedPlugin(this);
}




void EffectView::contextMenuEvent( QContextMenuEvent * )
{
	QPointer<CaptionMenu> contextMenu = new CaptionMenu( model()->displayName(), this );
	contextMenu->addAction( embed::getIconPixmap( "arp_up" ),
						tr( "Move &up" ),
						this, SLOT(moveUp()));
	contextMenu->addAction( embed::getIconPixmap( "arp_down" ),
						tr( "Move &down" ),
						this, SLOT(moveDown()));
	contextMenu->addSeparator();
	contextMenu->addAction( embed::getIconPixmap( "cancel" ),
						tr( "&Remove this plugin" ),
						this, SLOT(deletePlugin()));
	contextMenu->addSeparator();
	contextMenu->exec( QCursor::pos() );
	delete contextMenu;
}



void EffectView::mousePressEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
	{
		m_dragging = true;
		m_opacityEffect->setOpacity(0.3);
		QCursor c(Qt::SizeVerCursor);
		QApplication::setOverrideCursor(c);
		update();
	}
}

void EffectView::mouseReleaseEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
	{
		m_dragging = false;
		m_opacityEffect->setOpacity(1);
		QApplication::restoreOverrideCursor();
		update();
	}
}

void EffectView::mouseMoveEvent(QMouseEvent* event)
{
	if (!m_dragging) { return; }
	if (event->pos().y() < 0)
	{
		moveUp();
	}
	else if (event->pos().y() > height())
	{
		moveDown();
	}
}



void EffectView::paintEvent( QPaintEvent * )
{
	QPainter p( this );
	p.drawPixmap(rect(), m_bg);

	QFont f = adjustedToPixelSize(font(), DEFAULT_FONT_SIZE);
	p.setFont( f );

	QString elidedText = p.fontMetrics().elidedText( model()->displayName(), Qt::ElideRight, width() - 22 );

	p.setPen( palette().shadow().color() );
	const int baseline = height() - 5 - p.fontMetrics().descent();
	p.drawText(6, baseline + 1, elidedText);
	p.setPen( palette().text().color() );
	p.drawText(5, baseline, elidedText);
}




void EffectView::modelChanged()
{
	setToolTip(model()->displayName());
	m_bypass->setModel( &effect()->m_enabledModel );
	m_wetDry->setModel( &effect()->m_wetDryModel );
	m_autoQuit->setModel( &effect()->m_autoQuitModel );
}

void EffectView::corruptStateUpdate()
{
	if (dynamic_cast<DummyEffect*>(effect())) { return; }

	if (effect()->isCorrupted())
	{
		m_bypass->setLedColor(LedCheckBox::LedColor::Red);
		m_bypass->setToolTip(tr("Corrupted audio detected: muting affected channels"));
	}
	else
	{
		m_bypass->setLedColor(LedCheckBox::LedColor::Green);
		m_bypass->setToolTip(tr("On/Off"));
	}
}

} // namespace lmms::gui
