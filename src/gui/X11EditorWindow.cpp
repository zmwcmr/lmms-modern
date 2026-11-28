/*
 * X11EditorWindow.cpp - shared floating X11 editor window
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

#include "X11EditorWindow.h"

#include <algorithm>
#include <utility>

#include <QDebug>
#include <QMetaObject>
#include <QSocketNotifier>

#if defined(Q_OS_LINUX)
#	include <X11/Xlib.h>
#	include "X11LoadingPlaceholder.h"
#	include <X11/Xutil.h>
#endif

namespace lmms
{

#if defined(Q_OS_LINUX)

X11EditorWindow::X11EditorWindow(QObject* parent) :
	QObject(parent)
{
}




X11EditorWindow::~X11EditorWindow()
{
	close();
}




bool X11EditorWindow::isAvailable()
{
	return !qEnvironmentVariableIsEmpty("DISPLAY");
}




bool X11EditorWindow::open(const Options& options)
{
	if (isOpen())
	{
		return false;
	}

	auto* const display = XOpenDisplay(nullptr);
	if (display == nullptr)
	{
		return false;
	}

	const int screen = DefaultScreen(display);
	const Window root = RootWindow(display, screen);
	const unsigned long border = options.screenBorderColors
		? BlackPixel(display, screen) : 0;
	const unsigned long background = options.screenBorderColors
		? WhitePixel(display, screen) : 0;
	const Window window = XCreateSimpleWindow(display, root,
		options.x, options.y,
		static_cast<unsigned int>(std::max(options.width, 1)),
		static_cast<unsigned int>(std::max(options.height, 1)),
		static_cast<unsigned int>(std::max(options.borderWidth, 0)),
		border, background);
	if (window == 0)
	{
		XCloseDisplay(display);
		return false;
	}

	m_display = display;
	m_window = window;
	paintLoadingPlaceholder(display, window);

	if (!options.title.isEmpty())
	{
		setTitle(options.title);
	}
	XSelectInput(display, window,
		StructureNotifyMask | ExposureMask | FocusChangeMask);
	m_wmDelete = XInternAtom(display, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(display, window, &m_wmDelete, 1);

	if (!options.startHidden)
	{
		XMapWindow(display, window);
		m_visible = true;
	}
	// Flush before another process attaches to the window: an out-of-process
	// editor connects to the same X server and must find the parent window,
	// and the WM must see the map request.
	XSync(display, False);

	m_notifier = new QSocketNotifier(ConnectionNumber(display),
		QSocketNotifier::Read, this);
	connect(m_notifier, &QSocketNotifier::activated, this, [this] {
		processEvents();
	});
	return true;
}




void X11EditorWindow::close()
{
	if (m_notifier != nullptr)
	{
		m_notifier->setEnabled(false);
		m_notifier->deleteLater();
		m_notifier = nullptr;
	}

	destroyWindow();
	m_visible = false;
	m_hasFixedSize = false;
	m_expectResize = false;
	clearCallbacks();
}




void X11EditorWindow::destroyWindow()
{
	auto* const display = static_cast<Display*>(m_display);
	if (display == nullptr)
	{
		m_window = 0;
		return;
	}
	if (m_window != 0)
	{
		XDestroyWindow(display, static_cast<::Window>(m_window));
		m_window = 0;
	}
	XCloseDisplay(display);
	m_display = nullptr;
}




void X11EditorWindow::clearCallbacks()
{
	resized = {};
	closeRequested = {};
	destroyed = {};
}




void X11EditorWindow::show()
{
	auto* const display = static_cast<Display*>(m_display);
	if (display == nullptr || m_window == 0)
	{
		return;
	}
	XMapWindow(display, static_cast<::Window>(m_window));
	XRaiseWindow(display, static_cast<::Window>(m_window));
	m_visible = true;
	XFlush(display);
}




void X11EditorWindow::hide()
{
	auto* const display = static_cast<Display*>(m_display);
	if (display == nullptr || m_window == 0)
	{
		return;
	}
	XUnmapWindow(display, static_cast<::Window>(m_window));
	m_visible = false;
	XFlush(display);
}




void X11EditorWindow::raise()
{
	auto* const display = static_cast<Display*>(m_display);
	if (display == nullptr || m_window == 0)
	{
		return;
	}
	XRaiseWindow(display, static_cast<::Window>(m_window));
	XFlush(display);
}




void X11EditorWindow::activate()
{
	auto* const display = static_cast<Display*>(m_display);
	if (display == nullptr || m_window == 0)
	{
		return;
	}
	XSetInputFocus(display, static_cast<::Window>(m_window),
		RevertToParent, CurrentTime);
	XFlush(display);
}




void X11EditorWindow::setTitle(const QString& title)
{
	auto* const display = static_cast<Display*>(m_display);
	if (display == nullptr || m_window == 0)
	{
		return;
	}
	const QByteArray utf8 = title.toUtf8();
	XStoreName(display, static_cast<::Window>(m_window), utf8.constData());
	XFlush(display);
}




void X11EditorWindow::resize(int width, int height)
{
	auto* const display = static_cast<Display*>(m_display);
	if (display == nullptr || m_window == 0 || width <= 0 || height <= 0)
	{
		return;
	}
	if (m_placeholderPending)
	{
		paintLoadingPlaceholder(display, m_window);
		m_placeholderPending = false;
	}
	m_expectResize = true;
	m_expectedWidth = width;
	m_expectedHeight = height;
	XResizeWindow(display, static_cast<::Window>(m_window),
		static_cast<unsigned int>(width),
		static_cast<unsigned int>(height));
	XFlush(display);
}




QSize X11EditorWindow::resizeClamped(int width, int height, int marginX,
	int marginY)
{
	const QSize clamped = clampToDisplay(width, height, marginX, marginY);
	resize(clamped.width(), clamped.height());
	return clamped;
}




QSize X11EditorWindow::clampToDisplay(int width, int height, int marginX,
	int marginY) const
{
	const int maxWidth = std::max(displayWidth() - marginX, 320);
	const int maxHeight = std::max(displayHeight() - marginY, 240);
	return QSize(std::clamp(width, 1, maxWidth),
		std::clamp(height, 1, maxHeight));
}




void X11EditorWindow::setFixedSize(int width, int height)
{
	auto* const display = static_cast<Display*>(m_display);
	if (display == nullptr || m_window == 0 || width <= 0 || height <= 0)
	{
		return;
	}
	XSizeHints hints{};
	hints.flags = PMinSize | PMaxSize;
	hints.min_width = hints.max_width = width;
	hints.min_height = hints.max_height = height;
	XSetNormalHints(display, static_cast<::Window>(m_window), &hints);
	m_hasFixedSize = true;
	XFlush(display);
}




void X11EditorWindow::clearFixedSize()
{
	auto* const display = static_cast<Display*>(m_display);
	if (display == nullptr || m_window == 0 || !m_hasFixedSize)
	{
		return;
	}
	XSizeHints hints{};
	hints.flags = 0;
	XSetNormalHints(display, static_cast<::Window>(m_window), &hints);
	m_hasFixedSize = false;
	XFlush(display);
}




int X11EditorWindow::displayWidth() const
{
	auto* const display = static_cast<Display*>(m_display);
	return display != nullptr
		? DisplayWidth(display, DefaultScreen(display)) : 0;
}




int X11EditorWindow::displayHeight() const
{
	auto* const display = static_cast<Display*>(m_display);
	return display != nullptr
		? DisplayHeight(display, DefaultScreen(display)) : 0;
}




void X11EditorWindow::processEvents()
{
	auto* const display = static_cast<Display*>(m_display);
	if (display == nullptr)
	{
		return;
	}

	while (m_display != nullptr && m_window != 0 && XPending(display) > 0)
	{
		XEvent event = {};
		XNextEvent(display, &event);

		if (event.type == ClientMessage
			&& m_wmDelete != 0
			&& static_cast<unsigned long>(event.xclient.data.l[0])
				== m_wmDelete)
		{
			// Invoke a copy: a callback may clear or replace the original.
			auto callback = closeRequested;
			if (callback) { callback(); }
			// The callback may have closed the window.
			if (m_display == nullptr) { return; }
			continue;
		}

		if (event.type == ConfigureNotify
			&& static_cast<unsigned long>(event.xconfigure.window) == m_window)
		{
			const int width = event.xconfigure.width;
			const int height = event.xconfigure.height;
			if (m_expectResize)
			{
				m_expectResize = false;
				if (width == m_expectedWidth && height == m_expectedHeight)
				{
					continue;
				}
			}
			auto callback = resized;
			if (callback) { callback(width, height); }
			continue;
		}

		if (event.type == DestroyNotify
			&& static_cast<unsigned long>(event.xdestroywindow.window) == m_window)
		{
			m_window = 0;
			m_visible = false;
			m_expectResize = false;
			auto callback = destroyed;
			if (callback) { callback(); }
			return;
		}
	}
}

#else // Q_OS_LINUX

X11EditorWindow::X11EditorWindow(QObject* parent) :
	QObject(parent)
{
}

X11EditorWindow::~X11EditorWindow() = default;
bool X11EditorWindow::isAvailable() { return false; }
bool X11EditorWindow::open(const Options&) { return false; }
void X11EditorWindow::close() {}
void X11EditorWindow::destroyWindow() {}
void X11EditorWindow::clearCallbacks() {}
void X11EditorWindow::show() {}
void X11EditorWindow::hide() {}
void X11EditorWindow::raise() {}
void X11EditorWindow::activate() {}
void X11EditorWindow::setTitle(const QString&) {}
void X11EditorWindow::resize(int, int) {}
QSize X11EditorWindow::resizeClamped(int, int, int, int) { return {}; }
QSize X11EditorWindow::clampToDisplay(int, int, int, int) const { return {}; }
void X11EditorWindow::setFixedSize(int, int) {}
void X11EditorWindow::clearFixedSize() {}
int X11EditorWindow::displayWidth() const { return 0; }
int X11EditorWindow::displayHeight() const { return 0; }
void X11EditorWindow::processEvents() {}

#endif // Q_OS_LINUX

} // namespace lmms
