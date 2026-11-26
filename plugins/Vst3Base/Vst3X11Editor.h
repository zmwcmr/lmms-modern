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

#include <QObject>
#include <QString>

#include "vst3base_export.h"

class QSocketNotifier;

namespace lmms
{

class Vst3Host;

/**
 * Creates a plain X11 top-level window through XWayland and attaches the
 * plugin view there. This is the fallback for Qt Wayland sessions, where a
 * native Qt widget has no X11 window id to embed into.
 */
class VST3BASE_EXPORT Vst3X11Editor : public QObject
{
public:
	Vst3X11Editor(Vst3Host* host, const QString& title,
		std::function<void()> closed, QObject* parent = nullptr);
	~Vst3X11Editor() override;

	static bool isAvailable();

	bool isOpen() const { return m_open; }
	void raise();
	void activate();

private:
	void close();
	void processEvents();
	void resizeFromPlugin(int width, int height);

	Vst3Host* m_host = nullptr;
	void* m_display = nullptr;
	unsigned long m_window = 0;
	unsigned long m_wmDelete = 0;
	QSocketNotifier* m_notifier = nullptr;
	std::function<void()> m_closed;
	bool m_open = false;
	bool m_resizing = false;
};

} // namespace lmms

#endif // LMMS_VST3_X11_EDITOR_H
