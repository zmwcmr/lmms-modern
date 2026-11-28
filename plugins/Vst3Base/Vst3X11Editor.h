/*
 * Vst3X11Editor.h - floating X11 editor window for Wayland sessions
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

#ifndef LMMS_VST3_X11_EDITOR_H
#define LMMS_VST3_X11_EDITOR_H

#include <functional>
#include <memory>

#include <QObject>
#include <QString>

#include "vst3base_export.h"

namespace lmms
{

class Vst3HostBase;
class Vst3RemoteEditorClient;
class X11EditorWindow;

/**
 * Attaches a VST3 editor to the shared X11 editor window. This is the
 * fallback for Qt Wayland sessions, where a native Qt widget has no X11
 * window id to embed into.
 */
class VST3BASE_EXPORT Vst3X11Editor : public QObject
{
public:
	Vst3X11Editor(Vst3HostBase* host, const QString& title,
		std::function<void()> closed, QObject* parent = nullptr,
		bool remoteEditor = false);
	~Vst3X11Editor() override;

	static bool isAvailable();

	bool isOpen() const { return m_open; }
	void raise();
	void activate();

private:
	void close();

	Vst3HostBase* m_host = nullptr;
	std::unique_ptr<X11EditorWindow> m_window;
	Vst3RemoteEditorClient* m_remoteClient = nullptr;
	std::function<void()> m_closed;
	bool m_open = false;
};

} // namespace lmms

#endif // LMMS_VST3_X11_EDITOR_H
