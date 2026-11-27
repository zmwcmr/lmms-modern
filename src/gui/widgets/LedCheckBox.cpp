/*
 * LedCheckBox.cpp - class LedCheckBox, an improved QCheckBox
 *
 * Copyright (c) 2005-2014 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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

#include "LedCheckBox.h"

#include <QFontMetrics>
#include <QPainter>
#include <QVariant>
#include <array>

#include "DeprecationHelper.h"
#include "embed.h"
#include "FontHelper.h"

namespace lmms::gui
{


static const auto names = std::array<QString, 3>
{
	"led_yellow", "led_green", "led_red"
} ;




LedCheckBox::LedCheckBox( const QString & _text, QWidget * _parent,
				const QString & _name, LedColor _color, bool legacyMode ) :
	AutomatableButton( _parent, _name ),
	m_text( _text ),
	m_legacyMode(legacyMode)
{
	setLedColor(_color);
}




LedCheckBox::LedCheckBox( QWidget * _parent,
				const QString & _name, LedColor _color, bool legacyMode ) :
	LedCheckBox( QString(), _parent, _name, _color, legacyMode )
{
}

void LedCheckBox::setText( const QString &s )
{
	m_text = s;
	updateContentSize();
}




void LedCheckBox::paintEvent( QPaintEvent * pe )
{
	if (!m_legacyMode)
	{
		paintNonLegacy(pe);
	}
	else
	{
		paintLegacy(pe);
	}
}




void LedCheckBox::setLedColor(LedColor color)
{
	setCheckable( true );

	m_ledOnPixmap = embed::getIconPixmap(names[static_cast<std::size_t>(color)].toUtf8().constData());
	m_ledOffPixmap = embed::getIconPixmap("led_off");

	updateContentSize();
}

QFont LedCheckBox::captionFont() const
{
	return m_legacyMode ? adjustedToPixelSize(font(), DEFAULT_FONT_SIZE) : font();
}

void LedCheckBox::changeEvent(QEvent* event)
{
	AutomatableButton::changeEvent(event);
	if (event->type() == QEvent::FontChange) { updateContentSize(); }
}

void LedCheckBox::updateContentSize()
{
	const QFontMetrics metrics(captionFont());
	const QSize ledSize = embed::logicalSize(m_ledOffPixmap).expandedTo(embed::logicalSize(m_ledOnPixmap));
	const int captionWidth = qMax(metrics.horizontalAdvance(m_text), metrics.boundingRect(m_text).right() + 1);
	const int width = ledSize.width() + 5 + captionWidth;
	const int height = m_text.isEmpty() ? ledSize.height() : qMax(ledSize.height(), metrics.height() + (m_legacyMode ? 2 : 0));

	setFixedSize(width, height);
	update();
}

QColor LedCheckBox::legacyTextColor() const
{
	const auto color = property("_lmms_ledTextColor").value<QColor>();
	return color.isValid() ? color : QColor(255, 255, 255);
}

void LedCheckBox::setLegacyTextColor(const QColor& color)
{
	setProperty("_lmms_ledTextColor", color);
	update();
}

QColor LedCheckBox::legacyTextShadowColor() const
{
	const auto color = property("_lmms_ledTextShadowColor").value<QColor>();
	return color.isValid() ? color : QColor(64, 64, 64);
}

void LedCheckBox::setLegacyTextShadowColor(const QColor& color)
{
	setProperty("_lmms_ledTextShadowColor", color);
	update();
}

void LedCheckBox::paintLegacy(QPaintEvent * pe)
{
	QPainter p( this );
	p.setFont(captionFont());

	const auto& pixmap = model()->value() ? m_ledOnPixmap : m_ledOffPixmap;
	p.drawPixmap(0, (height() - embed::logicalSize(pixmap).height()) / 2, pixmap);

	const int ledWidth = embed::logicalSize(m_ledOffPixmap).width();
	const int baseline = (height() - p.fontMetrics().height()) / 2 + p.fontMetrics().ascent();
	p.setPen(legacyTextShadowColor());
	p.drawText(ledWidth + 4, baseline + 1, text());
	p.setPen(legacyTextColor());
	p.drawText(ledWidth + 3, baseline, text());
}

void LedCheckBox::paintNonLegacy(QPaintEvent * pe)
{
	QPainter p(this);
	p.setFont(captionFont());

	auto drawnPixmap = model()->value() ? m_ledOnPixmap : m_ledOffPixmap;
	const QSize ledSize = embed::logicalSize(drawnPixmap);

	p.drawPixmap(0, rect().height() / 2 - ledSize.height() / 2, drawnPixmap);

	QRect r = rect();
	r -= QMargins(embed::logicalSize(m_ledOffPixmap).width() + 5, 0, 0, 0);
	p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, text());
}


} // namespace lmms::gui
