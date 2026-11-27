/*
 * PluginProcessSupervisor.cpp - shared lifecycle for plugin host processes
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

#include "PluginProcessSupervisor.h"

#include <QDebug>
#include <QThread>

#include "lmms_export.h"

namespace lmms
{

/*! Starts and watches the child on its own thread (VST2 behavior). */
class PluginProcessSupervisor::Watcher : public QThread
{
public:
	explicit Watcher(PluginProcessSupervisor* owner) : m_owner(owner) {}

	void setCommand(const QString& executable, const QStringList& arguments)
	{
		m_exec = executable;
		m_args = arguments;
	}

	void stop()
	{
		m_quit = true;
		quit();
	}

	void reset()
	{
		m_quit = false;
	}

protected:
	void run() override
	{
		auto& process = m_owner->m_process;
		process.start(m_exec, m_args);
		exec();
		process.moveToThread(m_owner->thread());

		// Give queued protocol messages a chance to drain before the client
		// declares the transport dead; bounded so a half-closed socket cannot
		// keep the watcher alive forever.
		for (int waited = 0; !m_quit && waited < 5000
			&& m_owner->m_hasPendingMessages && m_owner->m_hasPendingMessages();
			waited += 200)
		{
			msleep(200);
		}
		if (!m_quit)
		{
			qWarning() << "Plugin host process died";
			emit m_owner->crashed();
		}
	}

private:
	PluginProcessSupervisor* const m_owner;
	QString m_exec;
	QStringList m_args;
	bool m_quit = false;
};




PluginProcessSupervisor::PluginProcessSupervisor(Mode mode, QObject* parent) :
	QObject(parent),
	m_mode(mode)
{
	m_process.setProcessChannelMode(QProcess::ForwardedChannels);
	if (m_mode == Mode::Direct)
	{
		// In watcher mode the watcher emits crashed() after draining messages.
		connect(&m_process,
			QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
			this,
			[this](int, QProcess::ExitStatus) {
				if (!m_stopping) { emit crashed(); }
			},
			Qt::DirectConnection);
	}
}




PluginProcessSupervisor::~PluginProcessSupervisor()
{
	stop();
}




void PluginProcessSupervisor::setPendingMessagesCallback(
	std::function<bool()> callback)
{
	m_hasPendingMessages = std::move(callback);
}




bool PluginProcessSupervisor::start(const QString& executable,
	const QStringList& arguments, int timeoutMs)
{
	if (m_mode == Mode::Direct)
	{
		return startDirect(executable, arguments, timeoutMs);
	}
	return startOnWatcher(executable, arguments);
}




bool PluginProcessSupervisor::startDirect(const QString& executable,
	const QStringList& arguments, int timeoutMs)
{
	m_stopping = false;
	m_process.start(executable, arguments);
	if (!m_process.waitForStarted(timeoutMs))
	{
		return false;
	}
	return true;
}




bool PluginProcessSupervisor::startOnWatcher(const QString& executable,
	const QStringList& arguments)
{
	// A previous instance may still be alive (32-bit VST reloads); stop it
	// before handing the process to a new watcher thread.
	if (m_watcher || m_process.state() != QProcess::NotRunning)
	{
		stop();
	}

	m_watcher = new Watcher(this);
	m_watcher->setCommand(executable, arguments);
	connect(&m_process,
		QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
		m_watcher, &QThread::quit, Qt::DirectConnection);
	connect(&m_process, &QProcess::errorOccurred,
		m_watcher, &QThread::quit, Qt::DirectConnection);

	m_stopping = false;
	// The process object must live on the watcher thread while it runs.
	m_process.moveToThread(m_watcher);
	m_watcher->start(QThread::LowestPriority);
	return true;
}




void PluginProcessSupervisor::stop(const std::function<void()>& sendQuit)
{
	m_stopping = true;

	// In watcher mode the QProcess lives on the watcher thread. Stop that
	// thread first: its run() moves the process back to the owner's thread
	// before returning, so the wait/kill calls below are safe.
	if (m_watcher)
	{
		m_watcher->stop();
		m_watcher->wait();
		delete m_watcher;
		m_watcher = nullptr;
	}

	if (m_process.state() != QProcess::NotRunning)
	{
		if (sendQuit)
		{
			sendQuit();
		}

		m_process.waitForFinished(1000);
		if (m_process.state() != QProcess::NotRunning)
		{
			m_process.terminate();
			m_process.waitForFinished(500);
		}
		if (m_process.state() != QProcess::NotRunning)
		{
			m_process.kill();
			m_process.waitForFinished(500);
		}
	}

	m_stopping = false;
}




bool PluginProcessSupervisor::isRunning() const
{
	return m_process.state() != QProcess::NotRunning;
}




qint64 PluginProcessSupervisor::processId() const
{
	return m_process.processId();
}




int PluginProcessSupervisor::exitCode() const
{
	return m_process.exitCode();
}




QProcess::ExitStatus PluginProcessSupervisor::exitStatus() const
{
	return m_process.exitStatus();
}




QString PluginProcessSupervisor::errorString() const
{
	return m_process.errorString();
}

} // namespace lmms
