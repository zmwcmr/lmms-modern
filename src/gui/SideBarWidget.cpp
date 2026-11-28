/*
 * SideBarWidget.cpp - implementation of base-widget for side-bar
 *
 * Copyright (c) 2004-2009 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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

#include "SideBarWidget.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPushButton>

#include "embed.h"
#include "FontHelper.h"

namespace lmms::gui
{

namespace
{
int titleBarHeight(const QFont& font)
{
	return qMax(27, QFontMetrics(titleFont(font)).height() + 6);
}
}



SideBarWidget::SideBarWidget( const QString & _title, const QPixmap & _icon,
							QWidget * _parent ) :
	QWidget( _parent ),
	m_title( _title ),
	m_icon(_icon),
	m_buttonSize(17, 17)
{
	m_contents = new QWidget( this );
	m_layout = new QVBoxLayout( m_contents );
	m_layout->setSpacing( 5 );
	m_layout->setContentsMargins(0, 0, 0, 0);
	m_closeBtn = new QPushButton(embed::getIconPixmap("close"), QString(), this);
	m_closeBtn->resize(m_buttonSize);
	m_closeBtn->setFocusPolicy(Qt::TabFocus);
	m_closeBtn->setToolTip(tr("Close"));
	connect(m_closeBtn, &QPushButton::clicked,
		[this]() { this->closeButtonClicked(); });
}




void SideBarWidget::paintEvent( QPaintEvent * )
{
	QPainter p( this );
	const auto barFont = titleFont(font());
	const QFontMetrics metrics(barFont);
	const int barHeight = titleBarHeight(font());
	p.fillRect( 0, 0, width(), barHeight, palette().highlight().color() );

	p.setFont(barFont);
	p.setPen( palette().highlightedText().color() );

	const auto iconSize = embed::logicalSize(m_icon);
	const int tx = iconSize.width() + 8;
	const int ty = (barHeight - metrics.height()) / 2 + metrics.ascent();
	const int titleWidth = qMax(0, width() - tx - m_buttonSize.width() - 12);
	p.drawText( tx, ty, metrics.elidedText(m_title, Qt::ElideRight, titleWidth) );

	const int iconTop = (barHeight - iconSize.height()) / 2;
	p.drawPixmap( 2, iconTop, m_icon.transformed( QTransform().rotate( -90 ) ) );
}



void SideBarWidget::resizeEvent( QResizeEvent * )
{
	const int MARGIN = 6;
	const int barHeight = titleBarHeight(font());
	m_contents->setGeometry( MARGIN, barHeight + MARGIN, width() - MARGIN * 2,
						height() - MARGIN * 2 - barHeight );
	m_closeBtn->move(m_contents->geometry().width() - MARGIN - 5,
		(barHeight - m_closeBtn->height()) / 2);
}


} // namespace lmms::gui
