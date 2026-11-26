/*
 * Vst3X11Editor.cpp - floating X11 editor window for Wayland sessions
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

#include "Vst3X11Editor.h"

#include <QDebug>
#include <QMetaObject>
#include <QSocketNotifier>
#include <QString>

#if defined(Q_OS_LINUX)
#	include <X11/Xlib.h>
#	include <X11/Xutil.h>
#endif

#include "Vst3Host.h"
#include "Vst3RemoteEditorClient.h"

namespace lmms
{

#if defined(Q_OS_LINUX)

Vst3X11Editor::Vst3X11Editor(Vst3HostBase* host, const QString& title,
	std::function<void()> closed, QObject* parent, bool remoteEditor) :
	QObject(parent),
	m_host(host),
	m_closed(std::move(closed))
{
	if (m_host == nullptr)
	{
		return;
	}

	auto* const display = XOpenDisplay(nullptr);
	if (display == nullptr)
	{
		return;
	}
	m_display = display;

	const int screen = DefaultScreen(display);
	const Window root = RootWindow(display, screen);
	m_window = XCreateSimpleWindow(display, root, 80, 80, 800, 600, 0, 0, 0);
	if (m_window == 0)
	{
		XCloseDisplay(display);
		m_display = nullptr;
		return;
	}

	const auto titleUtf8 = title.toUtf8();
	XStoreName(display, m_window, titleUtf8.constData());
	XSelectInput(display, m_window,
		StructureNotifyMask | ExposureMask | FocusChangeMask);
	m_wmDelete = XInternAtom(display, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(display, m_window, &m_wmDelete, 1);
	XMapWindow(display, m_window);
	// Flush before the editor host starts: it connects to the same X server
	// and must find the parent window, and the WM must see the map request.
	XSync(display, False);

	if (remoteEditor)
	{
		m_remoteClient = new Vst3RemoteEditorClient(m_host, m_window, this);
		m_remoteClient->editorResized = [this](int width, int height) {
			QMetaObject::invokeMethod(this, [this, width, height] {
				resizeFromPlugin(width, height);
			}, Qt::QueuedConnection);
		};
		m_remoteClient->editorClosed = [this] {
			QMetaObject::invokeMethod(this, [this] { close(); },
				Qt::QueuedConnection);
		};
		m_remoteClient->editorError = [](const QString& error) {
			qWarning("VST3 remote editor: %s", error.toUtf8().constData());
		};
		QString error;
		if (!m_remoteClient->start(title, &error))
		{
			qWarning("VST3 remote editor failed: %s", error.toUtf8().constData());
			delete m_remoteClient;
			m_remoteClient = nullptr;
			XDestroyWindow(display, m_window);
			XCloseDisplay(display);
			m_display = nullptr;
			m_window = 0;
			return;
		}
	}
	else
	{
		QString error;
		if (!m_host->openEditor(reinterpret_cast<void*>(m_window), &error))
		{
			XDestroyWindow(display, m_window);
			XCloseDisplay(display);
			m_display = nullptr;
			m_window = 0;
			return;
		}

		m_host->setEditorResizeCallback([this](int width, int height) {
			QMetaObject::invokeMethod(this, [this, width, height] {
				resizeFromPlugin(width, height);
			}, Qt::QueuedConnection);
		});
	}

	int width = 0;
	int height = 0;
	if (m_host->editorSize(width, height))
	{
		XResizeWindow(display, m_window, static_cast<unsigned int>(width),
			static_cast<unsigned int>(height));
	}

	m_notifier = new QSocketNotifier(ConnectionNumber(display),
		QSocketNotifier::Read, this);
	connect(m_notifier, &QSocketNotifier::activated, this, [this] {
		processEvents();
	});
	m_open = true;
}

Vst3X11Editor::~Vst3X11Editor()
{
	m_closed = {};
	close();
}

bool Vst3X11Editor::isAvailable()
{
	return !qEnvironmentVariableIsEmpty("DISPLAY");
}

void Vst3X11Editor::raise()
{
	auto* const display = static_cast<Display*>(m_display);
	if (display != nullptr && m_window != 0)
	{
		XRaiseWindow(display, m_window);
		XFlush(display);
	}
}

void Vst3X11Editor::activate()
{
	auto* const display = static_cast<Display*>(m_display);
	if (display != nullptr && m_window != 0)
	{
		XSetInputFocus(display, m_window, RevertToParent, CurrentTime);
		XFlush(display);
	}
}

void Vst3X11Editor::close()
{
	if (m_notifier != nullptr)
	{
		m_notifier->setEnabled(false);
		m_notifier->deleteLater();
		m_notifier = nullptr;
	}
	auto* const display = static_cast<Display*>(m_display);
	if (m_remoteClient != nullptr)
	{
		m_remoteClient->editorResized = {};
		m_remoteClient->editorClosed = {};
		m_remoteClient->editorError = {};
		m_remoteClient->stop();
		delete m_remoteClient;
		m_remoteClient = nullptr;
		m_open = false;
	}
	else if (m_host != nullptr && m_open)
	{
		m_host->setEditorResizeCallback({});
		m_host->closeEditor();
		m_open = false;
	}
	if (display != nullptr)
	{
		if (m_window != 0)
		{
			XDestroyWindow(display, m_window);
			m_window = 0;
		}
		XCloseDisplay(display);
		m_display = nullptr;
	}
	if (m_closed)
	{
		auto callback = std::move(m_closed);
		m_closed = {};
		callback();
	}
}

void Vst3X11Editor::resizeFromPlugin(int width, int height)
{
	if (!m_open || width <= 0 || height <= 0)
	{
		return;
	}
	auto* const display = static_cast<Display*>(m_display);
	if (display == nullptr)
	{
		return;
	}
	m_resizing = true;
	XResizeWindow(display, m_window, static_cast<unsigned int>(width),
		static_cast<unsigned int>(height));
	XFlush(display);
	m_resizing = false;
}

void Vst3X11Editor::processEvents()
{
	auto* const display = static_cast<Display*>(m_display);
	if (display == nullptr)
	{
		return;
	}
	while (XPending(display) > 0)
	{
		XEvent event = {};
		XNextEvent(display, &event);
		if (event.type == ClientMessage
			&& static_cast<unsigned long>(event.xclient.data.l[0]) == m_wmDelete)
		{
			close();
			return;
		}
		if (event.type == ConfigureNotify && !m_resizing && m_open)
		{
			if (m_remoteClient != nullptr)
			{
				m_remoteClient->sendResize(event.xconfigure.width,
					event.xconfigure.height);
			}
			else
			{
				m_host->resizeEditor(event.xconfigure.width,
					event.xconfigure.height);
			}
		}
		if (event.type == DestroyNotify
			&& event.xdestroywindow.window == m_window)
		{
			m_window = 0;
			close();
			return;
		}
	}
}

#else

Vst3X11Editor::Vst3X11Editor(Vst3HostBase*, const QString&,
	std::function<void()>, QObject* parent, bool) :
	QObject(parent)
{
}

Vst3X11Editor::~Vst3X11Editor() = default;
bool Vst3X11Editor::isAvailable() { return false; }
void Vst3X11Editor::raise() {}
void Vst3X11Editor::activate() {}
void Vst3X11Editor::close() {}
void Vst3X11Editor::resizeFromPlugin(int, int) {}
void Vst3X11Editor::processEvents() {}

#endif

} // namespace lmms
