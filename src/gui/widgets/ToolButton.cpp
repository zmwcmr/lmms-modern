/*
 * ToolButton.cpp - implementation of LMMS-tool-button for common (cool) look
 *
 * Copyright (c) 2005-2006 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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
#include <QStyleOptionToolButton>
#include <QStylePainter>

#include "LmmsStyle.h"
#include "ToolButton.h"

namespace lmms::gui
{


ToolButton::ToolButton( const QPixmap & _pixmap, const QString & _tooltip,
			QObject * _receiver, const char * _slot,
			QWidget * _parent ) :
	QToolButton( _parent )
{
	setAutoFillBackground( false );

	if( _receiver != nullptr && _slot != nullptr )
	{
		connect( this, SIGNAL(clicked()), _receiver, _slot );
	}
	setToolTip(_tooltip);
	setIcon( _pixmap );
}

void ToolButton::setHoverTransitionEnabled(bool enabled)
{
	if (enabled == bool(m_hoverAnimation)) { return; }
	if (enabled)
	{
		m_hoverAnimation = std::make_unique<QVariantAnimation>(this);
		const qreal progress = isEnabled() && underMouse() ? 1.0 : 0.0;
		m_hoverAnimation->setStartValue(progress);
		m_hoverAnimation->setEndValue(progress);
		m_hoverAnimation->setEasingCurve(QEasingCurve::OutCubic);
		connect(m_hoverAnimation.get(), &QVariantAnimation::valueChanged, this, [this] { update(); });
		connect(m_hoverAnimation.get(), &QVariantAnimation::finished, this, qOverload<>(&ToolButton::update));
	}
	else
	{
		m_hoverAnimation.reset();
		update();
	}
}

bool ToolButton::event(QEvent* event)
{
	if (m_hoverAnimation)
	{
		switch (event->type())
		{
		case QEvent::Enter: animateHover(1.0); break;
		case QEvent::Leave: animateHover(0.0); break;
		case QEvent::Hide:
		case QEvent::EnabledChange:
			m_hoverAnimation->stop();
			m_hoverAnimation->setStartValue(0.0);
			m_hoverAnimation->setEndValue(0.0);
			break;
		default: break;
		}
	}
	return QToolButton::event(event);
}

void ToolButton::animateHover(qreal target)
{
	if (!isEnabled() || menu()) { return; }
	const auto current = m_hoverAnimation->currentValue().toReal();
	m_hoverAnimation->stop();
	const int duration = LmmsStyle::uiAnimationDuration(this, 100);
	const int remaining = qRound(duration * qAbs(target - current));
	m_hoverAnimation->setStartValue(remaining > 0 ? current : target);
	m_hoverAnimation->setEndValue(target);
	if (remaining > 0)
	{
		m_hoverAnimation->setDuration(remaining);
		m_hoverAnimation->start();
	}
}

void ToolButton::paintEvent(QPaintEvent* event)
{
	if (!m_hoverAnimation || m_hoverAnimation->state() != QAbstractAnimation::Running || isDown() || !isEnabled()
		|| menu() || arrowType() != Qt::NoArrow)
	{
		QToolButton::paintEvent(event);
		return;
	}

	QStylePainter painter(this);
	QStyleOptionToolButton label;
	initStyleOption(&label);
	auto background = label;
	background.icon = QIcon{};
	background.text.clear();
	background.state &= ~QStyle::State_MouseOver;
	painter.drawComplexControl(QStyle::CC_ToolButton, background);
	painter.save();
	painter.setOpacity(m_hoverAnimation->currentValue().toReal());
	background.state |= QStyle::State_MouseOver;
	painter.drawComplexControl(QStyle::CC_ToolButton, background);
	painter.restore();

	label.rect = style()->subControlRect(QStyle::CC_ToolButton, &label, QStyle::SC_ToolButton, this);
	painter.drawControl(QStyle::CE_ToolButtonLabel, label);
}


} // namespace lmms::gui
