/*
 * SideBar.cpp - side-bar in LMMS' MainWindow
 *
 * Copyright (c) 2004-2014 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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

#include <QStyleOptionToolButton>
#include <QStylePainter>
#include <QToolButton>

#include "SideBar.h"
#include "SideBarWidget.h"

namespace lmms::gui
{


// internal helper class allowing to create QToolButtons with
// vertical orientation
class SideBarButton : public QToolButton
{
public:
	SideBarButton( Qt::Orientation _orientation, QWidget * _parent ) :
		QToolButton( _parent ),
		m_orientation( _orientation )
	{
	}

	~SideBarButton() override = default;

	Qt::Orientation orientation() const
	{
		return m_orientation;
	}

	QSize sizeHint() const override
	{
		QSize s = QToolButton::sizeHint();
		s.rwidth() += ButtonPadding;
		if( orientation() == Qt::Horizontal )
		{
			return s;
		}
		const QSize iconSlot = iconSlotSize();
		return QSize( iconSlot.height(), qMax( s.width(), iconSlot.width() ) );
	}


protected:
	void paintEvent( QPaintEvent * ) override
	{
		QStylePainter p( this );
		QStyleOptionToolButton opt;
		initStyleOption( &opt );
		if( orientation() == Qt::Vertical )
		{
			p.rotate( 270 );
			p.translate( -height(), 0 );
			opt.rect = QRect( 0, 0, height(), width() );

			auto background = opt;
			background.icon = QIcon();
			background.text.clear();
			p.drawComplexControl( QStyle::CC_ToolButton, background );

			// Reserve the same icon slot at the top whether or not the title is visible.
			auto icon = opt;
			icon.toolButtonStyle = Qt::ToolButtonIconOnly;
			icon.text.clear();
			icon.state &= ~(QStyle::State_Sunken | QStyle::State_On);
			const int iconWidth = qMin( height(), iconSlotSize().width() );
			p.save();
			p.translate( height() - iconWidth, 0 );
			icon.rect = QRect( 0, 0, iconWidth, width() );
			p.drawControl( QStyle::CE_ToolButtonLabel, icon );
			p.restore();

			if( opt.toolButtonStyle == Qt::ToolButtonTextBesideIcon )
			{
				opt.toolButtonStyle = Qt::ToolButtonTextOnly;
				opt.icon = QIcon();
				opt.rect.setWidth( height() - iconWidth );
				p.drawControl( QStyle::CE_ToolButtonLabel, opt );
			}
			return;
		}
		p.drawComplexControl( QStyle::CC_ToolButton, opt );
	}


private:
	QSize iconSlotSize() const
	{
		QStyleOptionToolButton option;
		initStyleOption( &option );
		option.toolButtonStyle = Qt::ToolButtonIconOnly;
		option.text.clear();
		option.state &= ~(QStyle::State_Sunken | QStyle::State_On);
		const QSize iconContents = option.iconSize + QSize( 2, 2 );
		QSize size = style()->sizeFromContents( QStyle::CT_ToolButton, &option, iconContents, this );
		size.rwidth() += ButtonPadding;
		return size;
	}

	static constexpr int ButtonPadding = 8;
	Qt::Orientation m_orientation;

} ;


SideBar::SideBar( Qt::Orientation _orientation, QWidget * _parent ) :
	QToolBar( _parent ),
	m_btnGroup( this )
{
	setOrientation( _orientation );
	setIconSize( QSize( 16, 16 ) );

	m_btnGroup.setExclusive( false );
	connect( &m_btnGroup, SIGNAL(buttonClicked(QAbstractButton*)),
				this, SLOT(toggleButton(QAbstractButton*)));
}




void SideBar::appendTab( SideBarWidget *widget )
{
	auto button = new SideBarButton(orientation(), this);
	button->setText( " " + widget->title() );
	button->setIcon( widget->icon() );
	button->setLayoutDirection( Qt::RightToLeft );
	button->setCheckable( true );
	m_widgets[button] = widget;
	m_btnGroup.addButton( button );
	addWidget( button );

	widget->hide();
	widget->setMinimumWidth( 200 );

	button->setToolTip(widget->title());

	connect(widget, &SideBarWidget::closeButtonClicked,
		[=]() { button->click(); });
}




void SideBar::toggleButton( QAbstractButton * button )
{
	QToolButton *toolButton = nullptr;
	QWidget *activeWidget = nullptr;

	for( auto it = m_widgets.begin(); it != m_widgets.end(); ++it )
	{
		QToolButton *curBtn = it.key();
		QWidget *curWidget = it.value();

		if( curBtn == button )
		{
			toolButton = curBtn;
			activeWidget = curWidget;
		}
		else
		{
			curBtn->setChecked( false );
			curBtn->setToolButtonStyle( Qt::ToolButtonIconOnly );
		}

		if( curWidget )
		{
			curWidget->hide();
		}
	}

	if( toolButton && activeWidget )
	{
		activeWidget->setVisible( button->isChecked() );
		toolButton->setToolButtonStyle( button->isChecked() ?
				Qt::ToolButtonTextBesideIcon : Qt::ToolButtonIconOnly );
	}
}


} // namespace lmms::gui
