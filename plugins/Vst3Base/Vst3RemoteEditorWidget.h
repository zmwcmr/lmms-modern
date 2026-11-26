/*
 * Vst3RemoteEditorWidget.h - widget for the out-of-process editor
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

#ifndef LMMS_VST3_REMOTE_EDITOR_WIDGET_H
#define LMMS_VST3_REMOTE_EDITOR_WIDGET_H

#include <functional>

#include <QWidget>

#include "vst3base_export.h"

namespace lmms
{

class Vst3Host;
class Vst3RemoteEditorClient;

/**
 * Creates a native X11 parent window and lets `lmms-vst3-editor-host` attach
 * the plugin view to it. Only the editor is out of process.
 */
class VST3BASE_EXPORT Vst3RemoteEditorWidget : public QWidget
{
public:
	Vst3RemoteEditorWidget(Vst3Host* host,
		std::function<void(uint32_t, double)> parameterHandler = {},
		QWidget* parent = nullptr);
	~Vst3RemoteEditorWidget() override;

	static bool isSupported();

	bool isRunning() const;
	QSize sizeHint() const override;

protected:
	void resizeEvent(QResizeEvent* event) override;
	void closeEvent(QCloseEvent* event) override;

private:
	Vst3Host* m_host = nullptr;
	Vst3RemoteEditorClient* m_client = nullptr;
	std::function<void(uint32_t, double)> m_parameterHandler;
	bool m_resizing = false;
};

} // namespace lmms

#endif // LMMS_VST3_REMOTE_EDITOR_WIDGET_H
