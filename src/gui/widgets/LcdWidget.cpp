/*
 * LcdWidget.cpp - a widget for displaying numbers in LCD style
 *
 * Copyright (c) 2005-2014 Tobias Doerffel <tobydox/at/users.sourceforge.net>
 * Copyright (c) 2008 Paul Giblock <pgllama/at/gmail.com>
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



#include <QEvent>
#include <QStyleOptionFrame>
#include <QPainter>

#include "LcdWidget.h"
#include "DeprecationHelper.h"
#include "embed.h"
#include "FontHelper.h"


namespace lmms::gui
{

LcdWidget::LcdWidget(QWidget* parent, const QString& name, bool leadingZero) :
	LcdWidget(1, parent, name, leadingZero)
{
}




LcdWidget::LcdWidget(int numDigits, QWidget* parent, const QString& name, bool leadingZero) :
	LcdWidget(numDigits, QString("19green"), parent, name, leadingZero)
{
}




LcdWidget::LcdWidget(int numDigits, const QString& style, QWidget* parent, const QString& name, bool leadingZero) :
	QWidget( parent ),
	m_label(),
	m_textColor( 255, 255, 255 ),
	m_textShadowColor( 64, 64, 64 ),
	m_numDigits(numDigits),
	m_seamlessLeft(false),
	m_seamlessRight(false),
	m_leadingZero(leadingZero)
{
	initUi( name, style );
}

void LcdWidget::setValue(int value)
{
	QString s = m_textForValue[value];
	if (s.isEmpty())
	{
		s = QString::number(value);
		if (m_leadingZero)
		{
			s = s.rightJustified(m_numDigits, '0');
		}
	}

	if (m_display != s)
	{
		m_display = s;

		update();
	}
}

void LcdWidget::setValue(float value)
{
	if (-1 < value && value < 0)
	{
		QString s = QString::number(static_cast<int>(value));
		s.prepend('-');
		
		if (m_display != s)
		{
			m_display = s;
			update();
		}
	}
	else
	{
		setValue(static_cast<int>(value));
	}
}




QColor LcdWidget::textColor() const
{
	return m_textColor;
}

void LcdWidget::setTextColor( const QColor & c )
{
	m_textColor = c;
}




QColor LcdWidget::textShadowColor() const
{
	return m_textShadowColor;
}

void LcdWidget::setTextShadowColor( const QColor & c )
{
	m_textShadowColor = c;
}




void LcdWidget::paintEvent( QPaintEvent* )
{
	QPainter p( this );

	QRect cellRect( 0, 0, m_cellWidth, m_cellHeight );
	const auto spriteSize = embed::logicalSize(m_lcdPixmap);
	const qreal scaleX = qreal(m_lcdPixmap.width()) / spriteSize.width();
	const qreal sourceHeight = m_cellHeight * (qreal(m_lcdPixmap.height()) / spriteSize.height());
	const qreal sourceY = isEnabled() ? 0.0 : sourceHeight;
	const auto drawCell = [&](const QRect& target, int column, int sourceWidth)
	{
		const QRectF source{column * m_cellWidth * scaleX, sourceY, sourceWidth * scaleX, sourceHeight};
		p.drawPixmap(target, m_lcdPixmap, source);
	};

	const int margin = 1;
	const int lcdWidth = displayWidth();
	const int displayLeft = (width() - lcdWidth) / 2;
	p.save();
	p.translate(displayLeft, 0);

	// Don't skip any space and don't draw margin on the left side in seamless mode
	if (m_seamlessLeft)
	{
		p.translate(0, margin);
	}
	else
	{
		p.translate(margin, margin);
		// Left Margin
		drawCell(cellRect, charsPerPixmap, m_cellWidth);

		p.translate(m_marginWidth, 0);
	}

	// Padding
	for( int i=0; i < m_numDigits - m_display.length(); i++ ) 
	{
		drawCell(cellRect, 10, m_cellWidth);
		p.translate( m_cellWidth, 0 );
	}

	// Digits
	for (const auto& digit : m_display)
	{
		int val = digit.digitValue();
		if( val < 0 ) 
		{
			if (digit == '-') val = 11;
			else
				val = 10;
		}
		drawCell(cellRect, val, m_cellWidth);
		p.translate( m_cellWidth, 0 );
	}

	// Right Margin
	drawCell(QRect(0, 0, m_seamlessRight ? 0 : m_marginWidth - 1, m_cellHeight),
		charsPerPixmap, m_cellWidth / 2);

	p.restore();

	// Border
	// When either the left or right edge is seamless, the border drawing must be done
	// by the encapsulating class (usually LcdFloatSpinBox).
	if (!m_seamlessLeft && !m_seamlessRight)
	{
		QStyleOptionFrame opt;
		opt.initFrom(this);
		opt.state = QStyle::State_Sunken;
		opt.rect = QRect(displayLeft, 0, lcdWidth - 1, m_cellHeight + 2 * margin);

		style()->drawPrimitive(QStyle::PE_Frame, &opt, &p, this);
	}

	// Label
	if( !m_label.isEmpty() )
	{
		drawLabel(p, rect(), m_label);
	}

}

QFont LcdWidget::labelFont() const
{
	return auxiliaryFont(font());
}

QSize LcdWidget::labelSize(const QString& label) const
{
	if (label.isEmpty()) { return {0, 0}; }

	const QFontMetrics metrics(labelFont());
	// Include one pixel for the shadow.
	return {metrics.horizontalAdvance(label) + 1, metrics.height() + 1};
}

void LcdWidget::drawLabel(QPainter& painter, const QRect& area, const QString& label) const
{
	painter.setFont(labelFont());
	const auto metrics = painter.fontMetrics();
	const int left = area.x() + (area.width() - metrics.horizontalAdvance(label)) / 2;
	const int baseline = area.bottom() - metrics.descent() - 1;
	painter.setPen(textShadowColor());
	painter.drawText(left + 1, baseline + 1, label);
	painter.setPen(textColor());
	painter.drawText(left, baseline, label);
}




void LcdWidget::setLabel( const QString& label )
{
	m_label = label;
	updateSize();
}




void LcdWidget::setMarginWidth( int width )
{
	m_marginWidth = width;

	updateSize();
}




int LcdWidget::displayWidth() const
{
	const int leftMargin = m_seamlessLeft ? 0 : 1 + m_marginWidth;
	const int rightMargin = m_seamlessRight ? 0 : 1 + m_marginWidth;
	return m_cellWidth * m_numDigits + leftMargin + rightMargin;
}

void LcdWidget::changeEvent(QEvent* event)
{
	QWidget::changeEvent(event);
	if (event->type() == QEvent::FontChange && !m_lcdPixmap.isNull() && hasLabel())
	{
		updateSize();
	}
}

void LcdWidget::updateSize()
{
	const auto caption = labelSize(m_label);
	setFixedSize(qMax(displayWidth(), caption.width()), m_cellHeight + 2 + caption.height());
	update();
}




void LcdWidget::initUi(const QString& name , const QString& style)
{
	setEnabled( true );

	setWindowTitle( name );

	// We should make a factory for these or something.
	//m_lcdPixmap = embed::getIconPixmap(QString("lcd_" + style).toUtf8().constData());
	//m_lcdPixmap = embed::getIconPixmap("lcd_19green"); // TODO!!

	m_lcdPixmap = embed::getIconPixmap(QString("lcd_" + style).toUtf8().constData());
	const auto spriteSize = embed::logicalSize(m_lcdPixmap);
	m_cellWidth = spriteSize.width() / LcdWidget::charsPerPixmap;
	m_cellHeight = spriteSize.height() / 2;

	m_marginWidth =  m_cellWidth / 2;

	updateSize();
}

} // namespace lmms::gui
