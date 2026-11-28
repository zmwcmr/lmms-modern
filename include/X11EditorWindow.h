/*
 * X11EditorWindow.h - shared floating X11 editor window
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

#ifndef LMMS_X11_EDITOR_WINDOW_H
#define LMMS_X11_EDITOR_WINDOW_H

#include <functional>

#include <QObject>
#include <QSize>
#include <QString>

class QSocketNotifier;

namespace lmms
{

/*! A plain X11 top-level window for plugin editors.
 *
 *  On Wayland the window is provided by XWayland. The controller owns the
 *  display connection, the window, the WM_DELETE_WINDOW protocol and the X
 *  event pump, so VST2, VST3 and LV2 editors share one window lifecycle.
 *
 *  The callbacks run on the GUI thread from inside the X event pump. Closing
 *  the Qt side calls close(); a window-manager close only invokes
 *  closeRequested so the caller can hide instead of destroy (LV2 keeps its
 *  suil instance alive across hide/show).
 */
class X11EditorWindow : public QObject
{
public:
	struct Options
	{
		QString title;
		int x = 80;
		int y = 80;
		int width = 800;
		int height = 600;
		int borderWidth = 0;
		//! Use the screen's black border and white background pixels.
		bool screenBorderColors = false;
		//! Do not map the window until show() is called.
		bool startHidden = false;
	};

	explicit X11EditorWindow(QObject* parent = nullptr);
	~X11EditorWindow() override;

	//! True when an X11/XWayland display is reachable.
	static bool isAvailable();

	bool open(const Options& options);
	void close();

	bool isOpen() const { return m_display != nullptr && m_window != 0; }
	bool isVisible() const { return m_visible; }

	void* display() const { return m_display; }
	unsigned long window() const { return m_window; }

	void show();
	void hide();
	void raise();
	void activate();

	void setTitle(const QString& title);

	//! Resize the outer window. The ConfigureNotify caused by this call is
	//! not forwarded to resized().
	void resize(int width, int height);
	//! Clamp to the display, then resize; returns the applied size.
	QSize resizeClamped(int width, int height, int marginX = 80,
		int marginY = 120);
	//! Clamp to the display without resizing.
	QSize clampToDisplay(int width, int height, int marginX = 80,
		int marginY = 120) const;

	//! Pin the window to a size with PMinSize|PMaxSize hints.
	void setFixedSize(int width, int height);
	void clearFixedSize();

	int displayWidth() const;
	int displayHeight() const;

	//! A user or plugin driven resize of the outer window.
	std::function<void(int, int)> resized;
	//! The window manager asked to close the window.
	std::function<void()> closeRequested;
	//! The X window disappeared without close() being called.
	std::function<void()> destroyed;

private:
	void processEvents();
	void destroyWindow();
	void clearCallbacks();

	void* m_display = nullptr;
	unsigned long m_window = 0;
	unsigned long m_wmDelete = 0;
	QSocketNotifier* m_notifier = nullptr;
	bool m_visible = false;
	bool m_hasFixedSize = false;
	//! Size of a resize() call whose ConfigureNotify must be ignored.
	bool m_expectResize = false;
	int m_expectedWidth = 0;
	int m_expectedHeight = 0;
};

} // namespace lmms

#endif // LMMS_X11_EDITOR_WINDOW_H
