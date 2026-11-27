/*
 * PluginProcessSupervisor.h - shared lifecycle for plugin host processes
 *
 * Copyright (c) 2026 LMMS contributors
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

#ifndef LMMS_PLUGIN_PROCESS_SUPERVISOR_H
#define LMMS_PLUGIN_PROCESS_SUPERVISOR_H

#include <functional>

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

#include "lmms_export.h"

class QThread;

namespace lmms
{

/*! Owns one plugin host process and its lifecycle.
 *
 *  Both the VST2 remote host and the LV2 host child use this class, so the
 *  start/stop sequence, orphan prevention and crash reporting live in one
 *  place. The wire protocol and the audio transport stay in the clients.
 */
class LMMS_EXPORT PluginProcessSupervisor : public QObject
{
	Q_OBJECT
public:
	enum class Mode
	{
		//! Start on the calling thread; the caller polls the process state.
		Direct,
		//! Start on a watcher thread, like the VST2 remote plugin.
		WatcherThread,
	};

	explicit PluginProcessSupervisor(Mode mode, QObject* parent = nullptr);
	~PluginProcessSupervisor() override;

	/*! A callback used to drain pending plugin messages before declaring a
	 *  crash (the VST2 remote host needs it; LV2 leaves it empty).
	 */
	void setPendingMessagesCallback(std::function<bool()> callback);

	/*! Start the executable. Direct mode waits for QProcess::started; watcher
	 *  mode returns after the thread has been launched.
	 */
	bool start(const QString& executable, const QStringList& arguments,
		int timeoutMs = 10000);

	/*! Ask the child to quit (the callback sends the protocol message), then
	 *  wait, terminate and finally kill it.
	 */
	void stop(const std::function<void()>& sendQuit = {});

	bool isRunning() const;
	qint64 processId() const;
	int exitCode() const;
	QProcess::ExitStatus exitStatus() const;
	QString errorString() const;

	//! The QProcess, for callers that need to configure it before start().
	QProcess* process() { return &m_process; }

signals:
	//! The process finished without stop() being called.
	void crashed();

private:
	class Watcher;

	bool startDirect(const QString& executable, const QStringList& arguments,
		int timeoutMs);
	bool startOnWatcher(const QString& executable, const QStringList& arguments);

	Mode m_mode;
	QProcess m_process;
	Watcher* m_watcher = nullptr;
	std::function<bool()> m_hasPendingMessages;
	bool m_stopping = false;
};

} // namespace lmms

#endif // LMMS_PLUGIN_PROCESS_SUPERVISOR_H
