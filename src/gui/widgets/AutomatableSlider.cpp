/*
 * AutomatableSlider.cpp - implementation of class AutomatableSlider
 *
 * Copyright (c) 2006-2007 Javier Serrano Polo <jasp00/at/users.sourceforge.net>
 * Copyright (c) 2007-2009 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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

#include "AutomatableSlider.h"

#include <QMouseEvent>
#include <QStyleOptionSlider>
#include <QToolTip>

#include "CaptionMenu.h"
#include "DeprecationHelper.h"
#include "GuiApplication.h"
#include "InlineValueEditor.h"
#include "MainWindow.h"


namespace lmms::gui
{


AutomatableSlider::AutomatableSlider( QWidget * _parent,
						const QString & _name ) :
	QSlider( _parent ),
	IntModelView( new IntModel( 0, 0, 0, nullptr, _name, true ), this ),
	m_showStatus( false )
{
	setWindowTitle( _name );
	setAttribute(Qt::WA_Hover);
	setCursor(Qt::ArrowCursor);

	connect( this, SIGNAL(valueChanged(int)),
					this, SLOT(changeValue(int)));
	connect( this, SIGNAL(sliderMoved(int)),
					this, SLOT(moveSlider(int)));
}







void AutomatableSlider::updateHoverCursor(const QPoint& pos)
{
	QStyleOptionSlider option;
	initStyleOption(&option);
	const bool overHandle = isEnabled()
		&& style()->hitTestComplexControl(QStyle::CC_Slider, &option, pos, this) == QStyle::SC_SliderHandle;
	const auto shape = overHandle
		? (orientation() == Qt::Horizontal ? Qt::SizeHorCursor : Qt::SizeVerCursor)
		: Qt::ArrowCursor;
	if (cursor().shape() != shape)
	{
		setCursor(shape);
	}
}

bool AutomatableSlider::event(QEvent* event)
{
	if (event->type() == QEvent::HoverEnter || event->type() == QEvent::HoverMove)
	{
		updateHoverCursor(position(static_cast<QHoverEvent*>(event)));
	}
	else if (event->type() == QEvent::ToolTip && isEnabled())
	{
		const auto* help = static_cast<QHelpEvent*>(event);
		const auto description = toolTip().isEmpty() ? windowTitle() : toolTip();
		const auto hint = tr("Drag the handle or click the track to adjust. Double-click to enter a value.");
		QToolTip::showText(help->globalPos(),
			description.isEmpty() ? hint : description + QLatin1Char('\n') + hint, this);
		return true;
	}
	return QSlider::event(event);
}

void AutomatableSlider::contextMenuEvent( QContextMenuEvent * _me )
{
	CaptionMenu contextMenu( model()->displayName() );
	addDefaultActions( &contextMenu );
	contextMenu.exec( QCursor::pos() );
}




void AutomatableSlider::mousePressEvent( QMouseEvent * _me )
{
	if( _me->button() == Qt::LeftButton &&
	   ! ( _me->modifiers() & Qt::ControlModifier ) )
	{
		AutomatableModel* thisModel = model();
		if (thisModel)
		{
			thisModel->addJournalCheckPoint();
			thisModel->saveJournallingState(false);
		}

		m_showStatus = true;
		QSlider::mousePressEvent( _me );
	}
	else
	{
		IntModelView::mousePressEvent( _me );
	}
}




void AutomatableSlider::mouseReleaseEvent( QMouseEvent * _me )
{
	AutomatableModel* thisModel = model();
	if (thisModel)
	{
		thisModel->restoreJournallingState();
	}

	m_showStatus = false;
	QSlider::mouseReleaseEvent( _me );
	updateHoverCursor(position(_me));
}




#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
void AutomatableSlider::enterEvent(QEnterEvent* event)
#else
void AutomatableSlider::enterEvent(QEvent* event)
#endif
{
	QSlider::enterEvent(event);
	if (getGUI() && getGUI()->mainWindow())
	{
		getGUI()->mainWindow()->showStatusHint(statusHint());
	}
}


QString AutomatableSlider::statusHint() const
{
	return tr("Drag to adjust · Wheel steps · Double-click to type a value");
}


void AutomatableSlider::leaveEvent(QEvent* event)
{
	QSlider::leaveEvent(event);
	if (getGUI() && getGUI()->mainWindow())
	{
		getGUI()->mainWindow()->clearStatusHint(statusHint());
	}
}


void AutomatableSlider::wheelEvent( QWheelEvent * _me )
{
	bool old_status = m_showStatus;
	m_showStatus = true;
	QSlider::wheelEvent( _me );
	m_showStatus = old_status;
}

void AutomatableSlider::mouseDoubleClickEvent(QMouseEvent*) { enterValue(); }

void AutomatableSlider::enterValue()
{
	if (!model()) { return; }
	InlineValueEditor::showEditorWithHint(this, model(),
		InlineValueEditor::rangeHint(model()->minValue(), model()->maxValue(), m_unit), {}, {}, 0);
}


void AutomatableSlider::modelChanged()
{
	QSlider::setRange( model()->minValue(), model()->maxValue() );
	updateSlider();
	connect( model(), SIGNAL(dataChanged()),
				this, SLOT(updateSlider()));
}




void AutomatableSlider::changeValue( int _value )
{
	if (underMouse())
	{
		updateHoverCursor(mapFromGlobal(QCursor::pos()));
	}
	model()->setValue( _value );
	emit logicValueChanged( model()->value() );
}




void AutomatableSlider::moveSlider( int _value )
{
	model()->setValue( _value );
	emit logicSliderMoved( model()->value() );
}




void AutomatableSlider::updateSlider()
{
	QSlider::setValue( model()->value() );
}




} // namespace lmms::gui

