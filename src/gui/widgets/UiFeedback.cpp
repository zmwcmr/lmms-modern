/*
 * UiFeedback.cpp - brief visual feedback for completed actions
 *
 * Copyright (c) 2026 LMMS Developers
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

#include "UiFeedback.h"

#include <cmath>

#include <QGraphicsColorizeEffect>
#include <QPointer>
#include <QTimer>
#include <QVariantAnimation>
#include <QWidget>

#include "LmmsStyle.h"

namespace lmms::gui
{

namespace
{

QColor accentFor(const QWidget* target)
{
	QColor accent = target->palette().color(QPalette::Highlight);
	const QColor background = target->palette().color(QPalette::Window);
	if (std::abs(accent.lightness() - background.lightness()) < 64)
	{
		accent = target->palette().color(QPalette::HighlightedText);
	}
	return accent.isValid() ? accent : QColor(Qt::white);
}

} // namespace


void UiFeedback::flash(QWidget* target)
{
	if (!target || target->size().isEmpty()) { return; }

	// Do not steal an effect that some other code installed on this widget.
	if (target->graphicsEffect()
		&& !qobject_cast<QGraphicsColorizeEffect*>(target->graphicsEffect()))
	{
		return;
	}

	const int duration = LmmsStyle::uiAnimationDuration(target, 260);
	if (duration <= 0) { return; }

	// Restart an earlier confirmation on the same widget instead of stacking it.
	if (target->graphicsEffect()) { target->setGraphicsEffect(nullptr); }

	auto* effect = new QGraphicsColorizeEffect(target);
	effect->setColor(accentFor(target));
	effect->setStrength(0.65);
	target->setGraphicsEffect(effect);

	auto* animation = new QVariantAnimation(effect);
	animation->setDuration(duration);
	animation->setStartValue(0.65);
	animation->setKeyValueAt(0.3, 0.65);
	animation->setEndValue(0.0);
	animation->setEasingCurve(QEasingCurve::OutCubic);
	QObject::connect(animation, &QVariantAnimation::valueChanged, effect,
		[effect](const QVariant& value) { effect->setStrength(value.toReal()); });
	animation->start();

	// Removing the effect deletes it together with the animation.
	QPointer<QGraphicsColorizeEffect> guard{effect};
	QTimer::singleShot(duration, target, [target, guard] {
		if (guard && target->graphicsEffect() == guard) { target->setGraphicsEffect(nullptr); }
	});
}

} // namespace lmms::gui
