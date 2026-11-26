/*
 * Vst3EditorWidget.h - native VST3 editor widget
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

#ifndef LMMS_VST3_EDITOR_WIDGET_H
#define LMMS_VST3_EDITOR_WIDGET_H

#include <QWidget>

#include "vst3base_export.h"

namespace lmms
{

class Vst3Host;

/**
 * Creates a native X11 child window and lets the plugin's IPlugView attach
 * to it. Only available on the xcb Qt platform; Wayland would need a separate
 * XWayland window and is handled by the caller as unsupported for now.
 */
class VST3BASE_EXPORT Vst3EditorWidget : public QWidget
{
public:
	explicit Vst3EditorWidget(Vst3Host* host, QWidget* parent = nullptr);
	~Vst3EditorWidget() override;

	static bool isSupported();

	QSize sizeHint() const override;

protected:
	void resizeEvent(QResizeEvent* event) override;
	void closeEvent(QCloseEvent* event) override;
	void focusInEvent(QFocusEvent* event) override;
	void focusOutEvent(QFocusEvent* event) override;

private:
	void resizeFromPlugin(int width, int height);

	Vst3Host* m_host = nullptr;
	bool m_open = false;
	bool m_resizing = false;
};

} // namespace lmms

#endif // LMMS_VST3_EDITOR_WIDGET_H
