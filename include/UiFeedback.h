/*
 * UiFeedback.h - brief visual feedback for completed actions
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

#ifndef LMMS_GUI_UI_FEEDBACK_H
#define LMMS_GUI_UI_FEEDBACK_H

class QWidget;

namespace lmms::gui
{

/*! \brief Brief, non-interactive confirmation of a completed action.
 *
 * Tints the target widget itself so its own shape is highlighted. No
 * additional frame or overlay is drawn. The tint fades out on its own.
 * When interface animations are disabled, nothing is changed.
 */
class UiFeedback
{
public:
	//! Briefly tints \a target to confirm a completed action.
	static void flash(QWidget* target);
};

} // namespace lmms::gui

#endif // LMMS_GUI_UI_FEEDBACK_H
