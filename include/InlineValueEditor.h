/*
 * InlineValueEditor.h - small editor that replaces the modal value dialogs
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

#ifndef LMMS_GUI_INLINE_VALUE_EDITOR_H
#define LMMS_GUI_INLINE_VALUE_EDITOR_H

#include <functional>

#include <QLineEdit>
#include <QPointer>

#include "lmms_export.h"

namespace lmms
{

class AutomatableModel;

namespace gui
{

/*! \brief A line edit that appears on top of a control for typing a value.
 *
 * It replaces the modal QInputDialog that used to be opened on double click.
 * The editor is a child of the control's window, so it can be wider than a
 * knob or a fader without being clipped. Enter, a click outside the editor,
 * or losing focus commits the value. Escape cancels.
 */
class LMMS_EXPORT InlineValueEditor : public QLineEdit
{
	Q_OBJECT
public:
	//! Converts between the value stored in the model and the value shown.
	using ValueTransform = std::function<float(float)>;

	/*! Shows an editor over \a control for \a model.
	 *
	 * \param toDisplay   model value -> shown value (optional, identity by default)
	 * \param fromDisplay shown value -> model value (optional, identity by default)
	 * \param decimals    number of decimals to show, -1 uses the model's digit count
	 */
	static void showEditor(QWidget* control, AutomatableModel* model,
		const ValueTransform& toDisplay = {}, const ValueTransform& fromDisplay = {},
		int decimals = -1);

	static void showEditorWithHint(QWidget* control, AutomatableModel* model, const QString& hint,
		const ValueTransform& toDisplay, const ValueTransform& fromDisplay, int decimals, int anchorBottom);

	//! Shows the editor with a visible description of the input unit and range.
	static void showEditorWithHint(QWidget* control, AutomatableModel* model, const QString& hint,
		const ValueTransform& toDisplay = {}, const ValueTransform& fromDisplay = {},
		int decimals = -1);

	static QString rangeHint(float minimum, float maximum, const QString& unit = {});

protected:
	bool eventFilter(QObject* watched, QEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;
	void focusOutEvent(QFocusEvent* event) override;

private:
	explicit InlineValueEditor(QWidget* parent);

	void commit();
	void cancel();

	QPointer<AutomatableModel> m_model;
	QPointer<QWidget> m_control;
	ValueTransform m_toDisplay;
	ValueTransform m_fromDisplay;
	int m_decimals;
	bool m_finished = false;
};

} // namespace gui
} // namespace lmms

#endif // LMMS_GUI_INLINE_VALUE_EDITOR_H
