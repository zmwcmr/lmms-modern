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

#include <algorithm>
#include <cmath>
#include <numbers>

#include <QGraphicsColorizeEffect>
#include <QPainter>
#include <QPointer>
#include <QTimer>
#include <QVariantAnimation>
#include <QWidget>

#include "LmmsStyle.h"

namespace lmms::gui
{

namespace
{

constexpr qreal HighlightStrength = 0.65;
constexpr qreal MaxPulseDarkening = 0.20;
constexpr qreal MaxPulseStrength = 0.42;
constexpr qreal MinPulseColorLightness = 0.78;
constexpr int PulsePeriodMs = 1000;

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

class FeedbackTint : public QGraphicsColorizeEffect
{
public:
	FeedbackTint(QWidget* target, bool pulse) : QGraphicsColorizeEffect(target), pulsing(pulse)
	{
		setColor(accentFor(target));
		setStrength(HighlightStrength);
	}

	const bool pulsing;

	void setPulsePhase(qreal phase)
	{
		const qreal darkening = MaxPulseDarkening * std::max(-phase, qreal(0));
		const bool darkeningChanged = m_darkening != darkening;
		m_darkening = darkening;
		setStrength(MaxPulseStrength * std::max(phase, qreal(0)));
		if (darkeningChanged) { update(); }
	}

protected:
	void draw(QPainter* painter) override
	{
		if (m_darkening == 0)
		{
			QGraphicsColorizeEffect::draw(painter);
			return;
		}

		QPoint offset;
		auto source = sourcePixmap(Qt::LogicalCoordinates, &offset, NoPad);
		if (source.isNull()) { return; }

		QPainter shade(&source);
		// Darken the widget pixels while preserving their alpha and shape.
		shade.setCompositionMode(QPainter::CompositionMode_SourceAtop);
		shade.fillRect(QRectF(QPointF{}, QSizeF(source.size()) / source.devicePixelRatio()),
			QColor::fromRgbF(0, 0, 0, m_darkening));
		shade.end();
		painter->drawPixmap(offset, source);
	}

private:
	qreal m_darkening = 0.0;
};

} // namespace


void UiFeedback::flash(QWidget* target)
{
	if (!target || target->size().isEmpty()) { return; }

	const auto* existing = dynamic_cast<const FeedbackTint*>(target->graphicsEffect());
	if (target->graphicsEffect() && (!existing || existing->pulsing))
	{
		return;
	}

	const int duration = LmmsStyle::uiAnimationDuration(target, 260);
	if (duration <= 0) { return; }

	auto* effect = new FeedbackTint(target, false);
	target->setGraphicsEffect(effect);

	auto* animation = new QVariantAnimation(effect);
	animation->setDuration(duration);
	animation->setStartValue(HighlightStrength);
	animation->setKeyValueAt(0.3, HighlightStrength);
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


void UiFeedback::setPulsingHighlight(QWidget* target, bool highlighted)
{
	if (!target) { return; }

	const auto* existing = dynamic_cast<const FeedbackTint*>(target->graphicsEffect());
	if (!highlighted)
	{
		if (existing && existing->pulsing) { target->setGraphicsEffect(nullptr); }
		return;
	}
	if (target->graphicsEffect() && (!existing || existing->pulsing)) { return; }

	auto* effect = new FeedbackTint(target, true);
	const QColor accent = effect->color();
	effect->setColor(QColor::fromHslF(accent.hslHueF(), accent.hslSaturationF(),
		std::max<qreal>(accent.lightnessF(), MinPulseColorLightness), accent.alphaF()));
	effect->setStrength(MaxPulseStrength);
	target->setGraphicsEffect(effect);
	if (LmmsStyle::uiAnimationDuration(target, PulsePeriodMs) <= 0) { return; }

	auto* animation = new QVariantAnimation(effect);
	animation->setDuration(PulsePeriodMs);
	animation->setLoopCount(-1);
	animation->setStartValue(0.0);
	animation->setEndValue(1.0);
	QObject::connect(animation, &QVariantAnimation::valueChanged, effect, [effect](const QVariant& value) {
		effect->setPulsePhase(std::cos(2 * std::numbers::pi * value.toReal()));
	});
	animation->start();
}

} // namespace lmms::gui
