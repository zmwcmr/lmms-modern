/*
 * InlineValueEditor.cpp - small editor that replaces the modal value dialogs
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

#include "InlineValueEditor.h"

#include <algorithm>
#include <cmath>

#include <QApplication>
#include <QFontMetrics>
#include <QGraphicsOpacityEffect>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPointer>
#include <QPropertyAnimation>
#include <QTimer>

#include "AutomatableModel.h"
#include "DeprecationHelper.h"
#include "LmmsStyle.h"
#include "LocaleHelper.h"
#include "UiFeedback.h"

namespace lmms::gui
{

namespace
{

constexpr int MinWidth = 64;
constexpr int MinHeight = 20;
constexpr int HorizontalMargin = 2;
constexpr int MaxFadeInDuration = 120;

//! Briefly fades \a widget in without delaying focus or input handling.
void fadeIn(QWidget* widget)
{
	const int duration = LmmsStyle::uiAnimationDuration(widget, MaxFadeInDuration);
	if (duration <= 0) { return; }

	auto* effect = new QGraphicsOpacityEffect(widget);
	effect->setOpacity(0.0);
	widget->setGraphicsEffect(effect);

	auto* animation = new QPropertyAnimation(effect, "opacity", effect);
	animation->setDuration(duration);
	animation->setStartValue(0.0);
	animation->setEndValue(1.0);
	animation->setEasingCurve(QEasingCurve::OutCubic);
	QObject::connect(animation, &QPropertyAnimation::finished, widget, [widget, effect] {
		// Removing the effect inside the animation's finished signal would
		// delete the animation while it is still emitting. Defer it by one
		// event-loop pass and guard against an early teardown.
		QPointer<QGraphicsOpacityEffect> guard{effect};
		QTimer::singleShot(0, widget, [widget, guard] {
			if (guard && widget->graphicsEffect() == guard) { widget->setGraphicsEffect(nullptr); }
		});
	});
	animation->start();
}

//! How many decimals the model's step suggests, used when the caller does not
//! specify a number.
int decimalsFor(const AutomatableModel* model)
{
	const float step = model->step<float>();
	if (step <= 0.f || step >= 1.f) { return 0; }
	const int decimals = static_cast<int>(std::ceil(-std::log10(step)));
	return std::clamp(decimals, 0, 4);
}

} // namespace


InlineValueEditor::InlineValueEditor(QWidget* parent) :
	QLineEdit(parent),
	m_model(nullptr),
	m_decimals(-1)
{
	setObjectName("inlineValueEditor");
	setAlignment(Qt::AlignCenter);
	setFrame(true);
}


void InlineValueEditor::showEditor(QWidget* control, AutomatableModel* model,
	const ValueTransform& toDisplay, const ValueTransform& fromDisplay, int decimals)
{
	showEditorWithHint(control, model, {}, toDisplay, fromDisplay, decimals);
}


QString InlineValueEditor::rangeHint(float minimum, float maximum, const QString& unit)
{
	const auto number = [](float value) {
		return std::isinf(value) ? (value < 0 ? QStringLiteral("−∞") : QStringLiteral("∞"))
			: QString::number(value, 'g', 7);
	};
	const auto range = tr("Range: %1 to %2").arg(number(minimum), number(maximum));
	return unit.trimmed().isEmpty() ? range : range + QLatin1Char(' ') + unit.trimmed();
}


void InlineValueEditor::showEditorWithHint(QWidget* control, AutomatableModel* model, const QString& hint,
	const ValueTransform& toDisplay, const ValueTransform& fromDisplay, int decimals)
{
	showEditorWithHint(control, model, hint, toDisplay, fromDisplay, decimals, -1);
}


void InlineValueEditor::showEditorWithHint(QWidget* control, AutomatableModel* model, const QString& hint,
	const ValueTransform& toDisplay, const ValueTransform& fromDisplay, int decimals, int anchorBottom)
{
	if (!control || !model) { return; }

	// Only one editor at a time per window, otherwise a double click on a
	// second control would leave the first one floating around.
	if (auto* window = control->window())
	{
		for (auto* old : window->findChildren<InlineValueEditor*>(QString(), Qt::FindDirectChildrenOnly))
		{
			old->deleteLater();
		}
	}

	auto* editor = new InlineValueEditor(control->window());
	editor->m_model = model;
	editor->m_control = control;
	connect(model, &QObject::destroyed, editor, &InlineValueEditor::cancel);
	editor->m_toDisplay = toDisplay;
	editor->m_fromDisplay = fromDisplay;
	editor->m_decimals = decimals < 0 ? decimalsFor(model) : decimals;
	editor->setFont(control->font());

	const float modelValue = model->value<float>();
	const float displayValue = toDisplay ? toDisplay(modelValue) : modelValue;
	editor->setText(QString::number(displayValue, 'f', editor->m_decimals));
	editor->selectAll();

	// Place the editor centred on the control, but never outside the window.
	QWidget* window = control->window();
	const QPoint topLeft = control->mapTo(window, QPoint(0, 0));
	const int width = std::max(control->width(), MinWidth);
	const int height = std::max(control->fontMetrics().height() + 8, MinHeight);
	int x = topLeft.x() + (control->width() - width) / 2;
	int y = topLeft.y() + (anchorBottom < 0 ? (control->height() - height) / 2 : anchorBottom - height);
	x = std::clamp(x, HorizontalMargin, std::max(window->width() - width - HorizontalMargin, HorizontalMargin));
	y = std::clamp(y, HorizontalMargin, std::max(window->height() - height - HorizontalMargin, HorizontalMargin));
	editor->setGeometry(x, y, width, height);

	editor->show();
	editor->raise();
	editor->setFocus(Qt::MouseFocusReason);
	fadeIn(editor);
	qApp->installEventFilter(editor);

	if (!hint.trimmed().isEmpty())
	{
		editor->setAccessibleDescription(hint.trimmed());
		auto* label = new QLabel(hint.trimmed(), window);
		label->setObjectName("inlineValueEditorHint");
		label->setAttribute(Qt::WA_TransparentForMouseEvents);
		auto palette = label->palette();
		palette.setColor(QPalette::Window, editor->palette().color(QPalette::Base));
		palette.setColor(QPalette::WindowText, editor->palette().color(QPalette::Text));
		label->setPalette(palette);
		label->setAutoFillBackground(true);
		label->setMargin(4);
		label->setFixedWidth(std::min(label->fontMetrics().horizontalAdvance(label->text()) + 12,
			std::max(1, window->width() - 2 * HorizontalMargin)));
		label->setWordWrap(true);
		label->adjustSize();
		const int hintX = std::clamp(x, HorizontalMargin,
			std::max(HorizontalMargin, window->width() - label->width() - HorizontalMargin));
		int hintY = y - label->height();
		if (hintY < HorizontalMargin)
		{
			hintY = y + height;
			if (anchorBottom >= 0) { hintY += control->fontMetrics().height(); }
		}
		hintY = std::clamp(hintY, HorizontalMargin,
			std::max(HorizontalMargin, window->height() - label->height() - HorizontalMargin));
		label->move(hintX, hintY);
		QObject::connect(editor, &QObject::destroyed, label, &QObject::deleteLater);
		label->show();
		label->raise();
		fadeIn(label);
	}
}


bool InlineValueEditor::eventFilter(QObject* watched, QEvent* event)
{
	if (event->type() == QEvent::MouseButtonPress
		|| event->type() == QEvent::MouseButtonDblClick
		|| event->type() == QEvent::NonClientAreaMouseButtonPress)
	{
		const auto* mouseEvent = static_cast<QMouseEvent*>(event);
		if (!rect().contains(mapFromGlobal(globalPosition(mouseEvent))))
		{
			commit();
		}
	}

	return QLineEdit::eventFilter(watched, event);
}


void InlineValueEditor::keyPressEvent(QKeyEvent* event)
{
	switch (event->key())
	{
	case Qt::Key_Return:
	case Qt::Key_Enter:
		commit();
		return;
	case Qt::Key_Escape:
		cancel();
		return;
	default:
		break;
	}
	QLineEdit::keyPressEvent(event);
}


void InlineValueEditor::focusOutEvent(QFocusEvent* event)
{
	QLineEdit::focusOutEvent(event);
	commit();
}


void InlineValueEditor::commit()
{
	if (m_finished) { return; }
	m_finished = true;

	bool ok = false;
	const float typed = LocaleHelper::toFloat(text(), &ok);
	if (ok && m_model)
	{
		float value = m_fromDisplay ? m_fromDisplay(typed) : typed;
		value = std::clamp(value, m_model->minValue<float>(), m_model->maxValue<float>());
		m_model->setValue(value);
		UiFeedback::flash(m_control);
	}

	deleteLater();
}


void InlineValueEditor::cancel()
{
	m_finished = true;
	deleteLater();
}

} // namespace lmms::gui
