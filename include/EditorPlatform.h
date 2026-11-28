/*
 * EditorPlatform.h - platform policy for hosted plugin editors
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

#ifndef LMMS_EDITOR_PLATFORM_H
#define LMMS_EDITOR_PLATFORM_H

#include <QString>

#include "lmms_export.h"

class QWidget;

namespace lmms
{

enum class EditorPresentation
{
	InProcessWidget,
	X11Window,
	Remote,
	Unavailable,
};

/*! Central Wayland/X11 policy for plugin editors.
 *
 *  Wayland does not allow a host to reparent another client's surface. All
 *  plugin UIs, including XWayland ones, therefore open as floating windows on
 *  Wayland; the only exception is an editor that is genuinely an in-process
 *  Qt widget.
 */
class LMMS_EXPORT EditorPlatform
{
public:
	static bool isWayland();
	//! True when DISPLAY is set, i.e. an XWayland or X11 server is reachable.
	static bool hasXWayland();

	//! Presentation to use for an external native plugin editor.
	static EditorPresentation preferredPresentation();

	//! Display scale of the widget's screen, or the primary screen.
	static qreal scaleFactor(const QWidget* widget = nullptr);

	//! Explanation suitable for a disabled Show GUI button.
	static QString unavailableReason();
};

} // namespace lmms

#endif // LMMS_EDITOR_PLATFORM_H
