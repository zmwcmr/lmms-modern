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

#include "Vst3Host.h"
#include "Vst3RemoteEditorClient.h"
#include "X11EditorWindow.h"

namespace lmms
{

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

	m_window = std::make_unique<X11EditorWindow>(this);
	X11EditorWindow::Options options;
	options.title = title;
	options.width = 800;
	options.height = 600;
	if (!m_window->open(options))
	{
		m_window.reset();
		return;
	}

	// Defer teardown: these callbacks run from inside the window's event
	// pump, and close() deletes the window object.
	m_window->closeRequested = [this] {
		QMetaObject::invokeMethod(this, [this] { close(); },
			Qt::QueuedConnection);
	};
	m_window->destroyed = [this] {
		// The plugin or the X server destroyed the window behind our back.
		QMetaObject::invokeMethod(this, [this] {
			if (m_open) { close(); }
		}, Qt::QueuedConnection);
	};
	m_window->resized = [this](int width, int height) {
		if (m_remoteClient != nullptr)
		{
			m_remoteClient->sendResize(width, height);
		}
		else if (m_host != nullptr)
		{
			m_host->resizeEditor(width, height);
		}
	};

	if (remoteEditor)
	{
		m_remoteClient = new Vst3RemoteEditorClient(m_host,
			m_window->window(), this);
		m_remoteClient->editorResized = [this](int width, int height) {
			QMetaObject::invokeMethod(this, [this, width, height] {
				if (m_window) { m_window->resize(width, height); }
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
			m_window->close();
			m_window.reset();
			return;
		}
	}
	else
	{
		QString error;
		if (!m_host->openEditor(
			reinterpret_cast<void*>(m_window->window()), &error))
		{
			m_window->close();
			m_window.reset();
			return;
		}

		m_host->setEditorResizeCallback([this](int width, int height) {
			QMetaObject::invokeMethod(this, [this, width, height] {
				if (m_window) { m_window->resize(width, height); }
			}, Qt::QueuedConnection);
		});
	}

	int width = 0;
	int height = 0;
	if (m_host->editorSize(width, height) && m_window != nullptr)
	{
		m_window->resize(width, height);
	}

	m_open = true;
}




Vst3X11Editor::~Vst3X11Editor()
{
	m_closed = {};
	close();
}




bool Vst3X11Editor::isAvailable()
{
	return X11EditorWindow::isAvailable();
}




void Vst3X11Editor::raise()
{
	if (m_window) { m_window->raise(); }
}




void Vst3X11Editor::activate()
{
	if (m_window) { m_window->activate(); }
}




void Vst3X11Editor::close()
{
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

	if (m_window != nullptr)
	{
		m_window->close();
		m_window.reset();
	}

	if (m_closed)
	{
		auto callback = std::move(m_closed);
		m_closed = {};
		callback();
	}
}

} // namespace lmms
