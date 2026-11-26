/*
 * Vst3EditorWindow.cpp - floating window for a VST3 editor
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

#include "Vst3EditorWindow.h"

#include <QMetaObject>
#include <QVBoxLayout>

#include "Vst3EditorWidget.h"
#include "Vst3Host.h"
#include "Vst3RemoteEditorWidget.h"
#include "Vst3X11Editor.h"

namespace lmms
{

Vst3EditorWindow::Vst3EditorWindow(
	Vst3HostBase* host, const QString& title,
	std::function<void(uint32_t, double)> parameterHandler, QWidget* parent) :
	QDialog(parent)
{
	setAttribute(Qt::WA_DeleteOnClose, true);
	setWindowTitle(title);

	auto* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	if (Vst3RemoteEditorWidget::isSupported())
	{
		auto* remoteEditor = new Vst3RemoteEditorWidget(
			host, std::move(parameterHandler), this);
		if (remoteEditor->isRunning())
		{
			layout->addWidget(remoteEditor);
			resize(remoteEditor->sizeHint());
			return;
		}
		delete remoteEditor;
	}

	if (!Vst3EditorWidget::isSupported() && Vst3X11Editor::isAvailable())
	{
		const auto closed = [this] {
			QMetaObject::invokeMethod(this, [this] { deleteLater(); },
				Qt::QueuedConnection);
		};
		m_x11Editor = new Vst3X11Editor(host, title, closed, this, true);
		if (m_x11Editor->isOpen())
		{
			return;
		}
		delete m_x11Editor;
		m_x11Editor = nullptr;

		m_x11Editor = new Vst3X11Editor(host, title, closed, this, false);
		if (m_x11Editor->isOpen())
		{
			return;
		}
		delete m_x11Editor;
		m_x11Editor = nullptr;
	}

	m_editor = new Vst3EditorWidget(host, this);
	layout->addWidget(m_editor);
	resize(m_editor->sizeHint());
}

Vst3EditorWindow* Vst3EditorWindow::open(
	Vst3HostBase* host, QWidget* parent, const QString& title,
	std::function<void(uint32_t, double)> parameterHandler)
{
	auto* window = new Vst3EditorWindow(
		host, title, std::move(parameterHandler), parent);
	if (!window->isX11Forwarded())
	{
		window->show();
	}
	return window;
}

bool Vst3EditorWindow::isSupported(Vst3HostBase* host)
{
	if (host == nullptr || !host->hasEditor())
	{
		return false;
	}
	return Vst3RemoteEditorWidget::isSupported()
		|| Vst3EditorWidget::isSupported()
		|| Vst3X11Editor::isAvailable();
}

void Vst3EditorWindow::raiseEditor()
{
	if (m_x11Editor != nullptr)
	{
		m_x11Editor->raise();
	}
	else
	{
		raise();
	}
}

void Vst3EditorWindow::activateEditor()
{
	if (m_x11Editor != nullptr)
	{
		m_x11Editor->activate();
	}
	else
	{
		activateWindow();
	}
}

} // namespace lmms
