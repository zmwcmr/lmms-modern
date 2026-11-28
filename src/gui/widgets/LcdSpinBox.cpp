/*
 * LcdSpinBox.cpp - class LcdSpinBox, an improved QLCDNumber
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

#include <cmath>
#include <QMouseEvent>
#include <QToolTip>

#include "LcdSpinBox.h"
#include "KeyboardShortcuts.h"
#include "CaptionMenu.h"
#include "DeprecationHelper.h"
#include "InlineValueEditor.h"


namespace lmms::gui
{

LcdSpinBox::LcdSpinBox( int numDigits, QWidget* parent, const QString& name ) :
	LcdWidget( numDigits, parent, name ),
	IntModelView( new IntModel( 0, 0, 0, nullptr, name, true ), this ),
	m_remainder( 0.f ),
	m_mouseMoving( false ),
	m_lastMousePos(),
	m_displayOffset( 0 )
{
	setAttribute(Qt::WA_Hover);
	setCursor(Qt::ArrowCursor);
}




LcdSpinBox::LcdSpinBox( int numDigits, const QString& style, QWidget* parent, const QString& name ) :
	LcdWidget( numDigits, style, parent, name ),
	IntModelView( new IntModel( 0, 0, 0, nullptr, name, true ), this ),
	m_remainder( 0.f ),
	m_mouseMoving( false ),
	m_lastMousePos(),
	m_displayOffset( 0 )
{
	setAttribute(Qt::WA_Hover);
	setCursor(Qt::ArrowCursor);
}

QRect LcdSpinBox::valueRect() const
{
	return QRect(0, 0, width(), cellHeight() + 2);
}

bool LcdSpinBox::event(QEvent* event)
{
	if (event->type() == QEvent::HoverEnter || event->type() == QEvent::HoverMove)
	{
		const auto pos = position(static_cast<QHoverEvent*>(event));
		const auto shape = isEnabled() && valueRect().contains(pos) ? Qt::SizeVerCursor : Qt::ArrowCursor;
		if (cursor().shape() != shape)
		{
			setCursor(shape);
		}
	}
	else if (event->type() == QEvent::ToolTip && isEnabled())
	{
		const auto* help = static_cast<QHelpEvent*>(event);
		const auto area = valueRect();
		if (area.contains(help->pos()))
		{
			const auto description = toolTip().isEmpty() ? windowTitle() : toolTip();
			const auto hint = tr("Drag or scroll to adjust. Double-click to enter a value.");
			QToolTip::showText(help->globalPos(),
				description.isEmpty() ? hint : description + QLatin1Char('\n') + hint, this, area);
			return true;
		}
	}
	return LcdWidget::event(event);
}

void LcdSpinBox::update()
{
	setValue( model()->value() + m_displayOffset );

	QWidget::update();
}



void LcdSpinBox::contextMenuEvent(QContextMenuEvent* event)
{
	CaptionMenu contextMenu(model()->displayName());
	addDefaultActions(&contextMenu);
	contextMenu.exec(QCursor::pos());
}




void LcdSpinBox::mousePressEvent( QMouseEvent* event )
{
	if (event->button() == Qt::LeftButton
		&& !(event->modifiers() & KBD_COPY_MODIFIER)
		&& valueRect().contains(position(event)))
	{
		m_mouseMoving = true;
		m_lastMousePos = globalPosition(event);

		AutomatableModel *thisModel = model();
		if( thisModel )
		{
			thisModel->addJournalCheckPoint();
			thisModel->saveJournallingState( false );
		}
	}
	else
	{
		IntModelView::mousePressEvent( event );
	}
}




void LcdSpinBox::mouseMoveEvent( QMouseEvent* event )
{
	if( m_mouseMoving )
	{
		const auto globalPos = globalPosition(event);
		int dy = globalPos.y() - m_lastMousePos.y();
		if( dy )
		{
			auto fdy = static_cast<float>(dy);
			if( event->modifiers() & Qt::ShiftModifier ) {
				fdy = qBound( -4.f, fdy/4.f, 4.f );
			}
			float floatValNotRounded =
				model()->value() + m_remainder - fdy / 2.f * model()->step<int>();
			float floatValRounded = roundf( floatValNotRounded );
			m_remainder = floatValNotRounded - floatValRounded;
			model()->setValue( floatValRounded );
			emit manualChange();
			m_lastMousePos = globalPos;
		}
	}
}




void LcdSpinBox::mouseReleaseEvent(QMouseEvent*)
{
	if (m_mouseMoving)
	{
		model()->restoreJournallingState();
		m_mouseMoving = false;
	}
}




void LcdSpinBox::wheelEvent(QWheelEvent * we)
{
	we->accept();
	const int direction = (we->angleDelta().y() > 0 ? 1 : -1) * (we->inverted() ? -1 : 1);

	model()->setValue(model()->value() + direction * model()->step<int>());
	emit manualChange();
}

void LcdSpinBox::mouseDoubleClickEvent( QMouseEvent * )
{
	mouseReleaseEvent(nullptr);
	InlineValueEditor::showEditorWithHint(this, model(),
		InlineValueEditor::rangeHint(model()->minValue(), model()->maxValue()), {}, {}, 0,
		cellHeight() + 2);
}

} // namespace lmms::gui
