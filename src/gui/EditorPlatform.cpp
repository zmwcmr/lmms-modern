/*
 * EditorPlatform.cpp - platform policy for hosted plugin editors
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

#include "EditorPlatform.h"

#include <QGuiApplication>
#include <QScreen>
#include <QWidget>

namespace lmms
{

bool EditorPlatform::isWayland()
{
	return QGuiApplication::platformName().contains(
		QStringLiteral("wayland"), Qt::CaseInsensitive);
}




bool EditorPlatform::hasXWayland()
{
	return !qEnvironmentVariableIsEmpty("DISPLAY");
}




EditorPresentation EditorPlatform::preferredPresentation()
{
	if (isWayland())
	{
		return hasXWayland() ? EditorPresentation::X11Window
			: EditorPresentation::Unavailable;
	}
	const auto platform = QGuiApplication::platformName();
	if (platform.startsWith(QStringLiteral("xcb"), Qt::CaseInsensitive)
		|| platform.startsWith(QStringLiteral("x11"), Qt::CaseInsensitive))
	{
		return EditorPresentation::X11Window;
	}
	return EditorPresentation::Unavailable;
}




qreal EditorPlatform::scaleFactor(const QWidget* widget)
{
	const auto* screen = widget != nullptr ? widget->screen()
		: QGuiApplication::primaryScreen();
	return screen != nullptr ? screen->devicePixelRatio() : 1.0;
}




QString EditorPlatform::unavailableReason()
{
	if (isWayland() && !hasXWayland())
	{
		return QStringLiteral(
			"The plugin editor needs XWayland; DISPLAY is not set.");
	}
	return QStringLiteral(
		"The plugin editor needs an X11 or XWayland display.");
}

} // namespace lmms
