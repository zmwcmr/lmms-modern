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

#include <QFontMetrics>
#include <QKeyEvent>
#include <QLabel>

#include "AutomatableModel.h"
#include "LocaleHelper.h"

namespace lmms::gui
{

namespace
{

constexpr int MinWidth = 64;
constexpr int MinHeight = 20;
constexpr int HorizontalMargin = 2;

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
	int y = topLeft.y() + (control->height() - height) / 2;
	x = std::clamp(x, HorizontalMargin, std::max(window->width() - width - HorizontalMargin, HorizontalMargin));
	y = std::clamp(y, HorizontalMargin, std::max(window->height() - height - HorizontalMargin, HorizontalMargin));
	editor->setGeometry(x, y, width, height);

	editor->show();
	editor->raise();
	editor->setFocus(Qt::MouseFocusReason);

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
		if (hintY < HorizontalMargin) { hintY = y + height; }
		hintY = std::clamp(hintY, HorizontalMargin,
			std::max(HorizontalMargin, window->height() - label->height() - HorizontalMargin));
		label->move(hintX, hintY);
		QObject::connect(editor, &QObject::destroyed, label, &QObject::deleteLater);
		label->show();
		label->raise();
	}
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
	if (ok)
	{
		float value = m_fromDisplay ? m_fromDisplay(typed) : typed;
		value = std::clamp(value, m_model->minValue<float>(), m_model->maxValue<float>());
		m_model->setValue(value);
	}

	deleteLater();
}


void InlineValueEditor::cancel()
{
	m_finished = true;
	deleteLater();
}

} // namespace lmms::gui
