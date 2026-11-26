/*
 * Vst3RemoteEditorClient.cpp - client for the out-of-process VST3 editor
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

#include "Vst3RemoteEditorClient.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QProcess>
#include <QProcessEnvironment>

#include "Vst3Host.h"

namespace lmms
{

namespace
{

constexpr int kConnectionTimeoutMs = 5000;

} // namespace

Vst3RemoteEditorClient::Vst3RemoteEditorClient(
	Vst3HostBase* host, quintptr parentWindow, QObject* parent) :
	QObject(parent),
	m_host(host),
	m_parentWindow(parentWindow)
{
}

Vst3RemoteEditorClient::~Vst3RemoteEditorClient()
{
	stop();
}

bool Vst3RemoteEditorClient::start(const QString& title, QString* error)
{
	if (m_host == nullptr)
	{
		if (error != nullptr)
		{
			*error = tr("No VST3 host");
		}
		return false;
	}
	if (m_host->bundlePath().isEmpty() || m_host->classId().isEmpty())
	{
		if (error != nullptr)
		{
			*error = tr("VST3 plugin information is missing");
		}
		return false;
	}

	const auto executable = editorHostExecutable();
	if (executable.isEmpty())
	{
		if (error != nullptr)
		{
			*error = tr("lmms-vst3-editor-host was not found");
		}
		return false;
	}

	const auto serverName = QStringLiteral("lmms-vst3-editor-%1-%2")
		.arg(QCoreApplication::applicationPid())
		.arg(reinterpret_cast<quintptr>(this));

	QLocalServer::removeServer(serverName);
	m_server = new QLocalServer(this);
	if (!m_server->listen(serverName))
	{
		if (error != nullptr)
		{
			*error = tr("Unable to create the editor socket");
		}
		delete m_server;
		m_server = nullptr;
		return false;
	}
	connect(m_server, &QLocalServer::newConnection, this, [this] {
		if (m_socket != nullptr)
		{
			return;
		}
		m_socket = m_server->nextPendingConnection();
		if (m_socket == nullptr)
		{
			return;
		}
		connect(m_socket, &QLocalSocket::readyRead,
			this, &Vst3RemoteEditorClient::handleSocketData);
		connect(m_socket, &QLocalSocket::disconnected, this, [this] {
			if (m_running && editorClosed)
			{
				m_running = false;
				editorClosed();
			}
		});
		sendState();
	});

	m_process = new QProcess(this);
	connect(m_process, &QProcess::errorOccurred, this,
		[this](QProcess::ProcessError) {
			if (editorError)
			{
				editorError(tr("The VST3 editor process failed: %1")
					.arg(m_process != nullptr ? m_process->errorString() : QString()));
			}
		});
	connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
		this, [this](int, QProcess::ExitStatus) {
			if (m_running)
			{
				m_running = false;
				if (editorClosed)
				{
					editorClosed();
				}
			}
		});

	m_process->setProgram(executable);
	m_process->setArguments({
		serverName,
		m_host->bundlePath(),
		m_host->classId(),
		QString::number(m_parentWindow),
		title,
	});
	m_process->start();
	if (!m_process->waitForStarted(kConnectionTimeoutMs))
	{
		if (error != nullptr)
		{
			*error = tr("Unable to start lmms-vst3-editor-host");
		}
		stop();
		return false;
	}

	m_running = true;
	return true;
}

bool Vst3RemoteEditorClient::isRunning() const
{
	return m_running;
}

void Vst3RemoteEditorClient::stop()
{
	if (m_socket != nullptr)
	{
		QJsonObject message;
		message.insert(QStringLiteral("type"), QStringLiteral("close"));
		m_socket->write(QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n');
		m_socket->flush();
		m_socket->disconnectFromServer();
		m_socket->deleteLater();
		m_socket = nullptr;
	}
	if (m_process != nullptr)
	{
		if (m_process->state() != QProcess::NotRunning)
		{
			m_process->terminate();
			if (!m_process->waitForFinished(1000))
			{
				m_process->kill();
				m_process->waitForFinished(1000);
			}
		}
		delete m_process;
		m_process = nullptr;
	}
	if (m_server != nullptr)
	{
		m_server->close();
		delete m_server;
		m_server = nullptr;
	}
	m_running = false;
	m_ready = false;
	m_readBuffer.clear();
}

void Vst3RemoteEditorClient::sendResize(int width, int height)
{
	if (!m_ready)
	{
		return;
	}
	QJsonObject message;
	message.insert(QStringLiteral("type"), QStringLiteral("resize"));
	message.insert(QStringLiteral("width"), width);
	message.insert(QStringLiteral("height"), height);
	sendMessage(message);
}

QString Vst3RemoteEditorClient::editorHostExecutable()
{
	const auto name = QStringLiteral("lmms-vst3-editor-host");
	const char* env = std::getenv("LMMS_VST3_EDITOR_HOST");
	if (env != nullptr && QFileInfo::exists(QString::fromLocal8Bit(env)))
	{
		return QString::fromLocal8Bit(env);
	}
	const QStringList candidates = {
		QFileInfo(QDir(QStringLiteral("plugins:")), name).absoluteFilePath(),
		QDir(QCoreApplication::applicationDirPath()).filePath(name),
		QDir(QCoreApplication::applicationDirPath()).filePath(
			QStringLiteral("plugins/") + name),
	};
	for (const auto& candidate : candidates)
	{
		if (!candidate.isEmpty() && QFileInfo::exists(candidate))
		{
			return candidate;
		}
	}
	return {};
}

void Vst3RemoteEditorClient::sendMessage(const QJsonObject& message)
{
	if (m_socket == nullptr || m_socket->state() != QLocalSocket::ConnectedState)
	{
		return;
	}
	m_socket->write(QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n');
}

void Vst3RemoteEditorClient::handleSocketData()
{
	if (m_socket == nullptr)
	{
		return;
	}
	m_readBuffer += m_socket->readAll();
	while (true)
	{
		const int newline = m_readBuffer.indexOf('\n');
		if (newline < 0)
		{
			break;
		}
		const auto line = m_readBuffer.left(newline);
		m_readBuffer.remove(0, newline + 1);
		const auto document = QJsonDocument::fromJson(line);
		if (document.isObject())
		{
			handleMessage(document.object());
		}
	}
}

void Vst3RemoteEditorClient::handleMessage(const QJsonObject& message)
{
	const auto type = message.value(QStringLiteral("type")).toString();
	if (type == QLatin1String("ready") || type == QLatin1String("resize"))
	{
		m_ready = true;
		const int width = message.value(QStringLiteral("width")).toInt();
		const int height = message.value(QStringLiteral("height")).toInt();
		if (width > 0 && height > 0 && editorResized)
		{
			editorResized(width, height);
		}
	}
	else if (type == QLatin1String("parameter"))
	{
		const auto id = static_cast<uint32_t>(
			message.value(QStringLiteral("id")).toDouble());
		const double value = message.value(QStringLiteral("value")).toDouble();
		if (parameterChanged)
		{
			parameterChanged(id, value);
		}
	}
	else if (type == QLatin1String("error"))
	{
		if (editorError)
		{
			editorError(message.value(QStringLiteral("message")).toString());
		}
	}
	else if (type == QLatin1String("closed"))
	{
		m_running = false;
		if (editorClosed)
		{
			editorClosed();
		}
	}
}

void Vst3RemoteEditorClient::sendState()
{
	if (m_host == nullptr)
	{
		return;
	}
	QJsonObject message;
	message.insert(QStringLiteral("type"), QStringLiteral("state"));
	message.insert(QStringLiteral("component"),
		QString::fromLatin1(m_host->componentState().toBase64()));
	message.insert(QStringLiteral("controller"),
		QString::fromLatin1(m_host->controllerState().toBase64()));
	sendMessage(message);
}

} // namespace lmms
