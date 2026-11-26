/*
 * Vst3EditorWindow.h - floating window for a VST3 editor
 *
 * Copyright (c) 2026 LMMS developers
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

#ifndef LMMS_VST3_EDITOR_WINDOW_H
#define LMMS_VST3_EDITOR_WINDOW_H

#include <functional>

#include <QDialog>

#include "vst3base_export.h"

namespace lmms
{

class Vst3Host;
class Vst3EditorWidget;
class Vst3X11Editor;

class VST3BASE_EXPORT Vst3EditorWindow : public QDialog
{
public:
	Vst3EditorWindow(Vst3Host* host, const QString& title,
		std::function<void(uint32_t, double)> parameterHandler = {},
		QWidget* parent = nullptr);

	static Vst3EditorWindow* open(Vst3Host* host, QWidget* parent,
		const QString& title,
		std::function<void(uint32_t, double)> parameterHandler = {});

	bool isX11Forwarded() const
	{
		return m_x11Editor != nullptr;
	}

	void raiseEditor();
	void activateEditor();

private:
	Vst3EditorWidget* m_editor = nullptr;
	Vst3X11Editor* m_x11Editor = nullptr;
};

} // namespace lmms

#endif // LMMS_VST3_EDITOR_WINDOW_H
