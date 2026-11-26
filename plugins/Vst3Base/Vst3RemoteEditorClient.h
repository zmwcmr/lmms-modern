/*
 * Vst3RemoteEditorClient.h - client for the out-of-process VST3 editor
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

#ifndef LMMS_VST3_REMOTE_EDITOR_CLIENT_H
#define LMMS_VST3_REMOTE_EDITOR_CLIENT_H

#include <functional>

#include <QByteArray>
#include <QObject>
#include <QString>

#include "vst3base_export.h"

class QJsonObject;
class QLocalServer;
class QLocalSocket;
class QProcess;

namespace lmms
{

class Vst3HostBase;

/**
 * Starts `lmms-vst3-editor-host`, gives it an X11 parent window and mirrors
 * state, parameter changes and resize requests over a local socket.
 *
 * The DSP instance stays in the LMMS process. This client only isolates the
 * editor, so an editor crash no longer takes down the whole application.
 */
class VST3BASE_EXPORT Vst3RemoteEditorClient : public QObject
{
public:
	Vst3RemoteEditorClient(
		Vst3HostBase* host, quintptr parentWindow, QObject* parent = nullptr);
	~Vst3RemoteEditorClient() override;

	bool start(const QString& title, QString* error = nullptr);
	bool isRunning() const;
	void stop();
	void sendResize(int width, int height);

	static QString editorHostExecutable();

	std::function<void(int, int)> editorResized;
	std::function<void()> editorClosed;
	std::function<void(const QString&)> editorError;
	std::function<void(uint32_t, double)> parameterChanged;

private:
	void sendMessage(const QJsonObject& message);
	void handleSocketData();
	void handleMessage(const QJsonObject& message);
	void sendState();

	Vst3HostBase* m_host = nullptr;
	quintptr m_parentWindow = 0;
	QLocalServer* m_server = nullptr;
	QLocalSocket* m_socket = nullptr;
	QProcess* m_process = nullptr;
	QByteArray m_readBuffer;
	bool m_running = false;
	bool m_ready = false;
};

} // namespace lmms

#endif // LMMS_VST3_REMOTE_EDITOR_CLIENT_H
