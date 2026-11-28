/*
 * PluginLoadingWindow.h - immediate SVG loading placeholder
 *
 * Copyright (c) 2026 LMMS contributors
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#ifndef LMMS_GUI_PLUGIN_LOADING_WINDOW_H
#define LMMS_GUI_PLUGIN_LOADING_WINDOW_H

#include <QString>
#include <QWidget>

class QTimer;

#include "lmms_export.h"

namespace lmms::gui
{

/*! Small top-level window shown immediately while a plugin GUI is loading.
 *
 *  The logo is rendered from the LMMS SVG, so it stays sharp on HiDPI/XWayland
 *  displays. The window is intentionally independent of the plugin host
 *  process and is closed once the editor has had time to open.
 */
class LMMS_EXPORT PluginLoadingWindow : public QWidget
{
	Q_OBJECT
public:
	static void showFor(const QString& pluginName);
	static void hide();

private:
	explicit PluginLoadingWindow(const QString& pluginName, QWidget* parent);
	~PluginLoadingWindow() override;

	QTimer* m_raiseTimer = nullptr;
};

} // namespace lmms::gui

#endif // LMMS_GUI_PLUGIN_LOADING_WINDOW_H
