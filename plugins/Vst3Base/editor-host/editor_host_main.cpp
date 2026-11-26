/*
 * editor_host_main.cpp - out-of-process VST3 editor host
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

#if defined(__linux__)
#	include <csignal>
#	include <sys/prctl.h>
#endif

#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QTextStream>
#include <QTimer>

#include "Vst3Host.h"

namespace
{

void sendMessage(QLocalSocket& socket, const QJsonObject& message)
{
	if (socket.state() != QLocalSocket::ConnectedState)
	{
		return;
	}
	socket.write(QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n');
	socket.flush();
}

bool readMessage(QLocalSocket& socket, QJsonObject& message)
{
	QByteArray buffer;
	while (true)
	{
		const int newline = buffer.indexOf('\n');
		if (newline >= 0)
		{
			const auto document = QJsonDocument::fromJson(buffer.left(newline));
			if (document.isObject())
			{
				message = document.object();
				return true;
			}
			buffer.remove(0, newline + 1);
			continue;
		}
		if (!socket.waitForReadyRead(10000))
		{
			return false;
		}
		buffer += socket.readAll();
	}
}

void sendError(QLocalSocket& socket, const QString& error)
{
	QJsonObject message;
	message.insert(QStringLiteral("type"), QStringLiteral("error"));
	message.insert(QStringLiteral("message"), error);
	sendMessage(socket, message);
}

} // namespace

int main(int argc, char* argv[])
{
	QApplication application(argc, argv);
#if defined(__linux__)
	prctl(PR_SET_PDEATHSIG, SIGKILL);
#endif
#if defined(Q_OS_LINUX)
	prctl(PR_SET_PDEATHSIG, SIGKILL);
#endif
	const auto arguments = application.arguments();
	if (arguments.size() < 5)
	{
		QTextStream(stderr)
			<< "Usage: lmms-vst3-editor-host <socket> <bundle> <class-id> "
			   "<parent-window> [title]\n";
		return 2;
	}

	const auto socketName = arguments.at(1);
	const auto bundlePath = arguments.at(2);
	const auto classId = arguments.at(3);
	bool windowOk = false;
	const auto parentWindow = arguments.at(4).toULongLong(&windowOk, 10);
	const auto title = arguments.size() > 5
		? arguments.at(5)
		: QStringLiteral("VST3 editor");

	QLocalSocket socket;
	socket.connectToServer(socketName);
	if (!socket.waitForConnected(5000) || !windowOk || parentWindow == 0)
	{
		QTextStream(stderr) << "Unable to connect to the LMMS editor client\n";
		return 1;
	}

	lmms::Vst3Host host;
	QString error;
	if (!host.load(bundlePath, classId, 48000.0, 512, &error))
	{
		sendError(socket, error);
		return 1;
	}

	QJsonObject stateMessage;
	if (!readMessage(socket, stateMessage)
		|| stateMessage.value(QStringLiteral("type")).toString()
			!= QLatin1String("state"))
	{
		sendError(socket, QStringLiteral("No initial plugin state received"));
		return 1;
	}

	const auto componentState = QByteArray::fromBase64(
		stateMessage.value(QStringLiteral("component")).toString().toLatin1());
	const auto controllerState = QByteArray::fromBase64(
		stateMessage.value(QStringLiteral("controller")).toString().toLatin1());
	if (!componentState.isEmpty() || !controllerState.isEmpty())
	{
		host.setState(componentState, controllerState);
	}

	host.setParameterChangeCallback([&socket](uint32_t id, double value) {
		QJsonObject message;
		message.insert(QStringLiteral("type"), QStringLiteral("parameter"));
		message.insert(QStringLiteral("id"), static_cast<double>(id));
		message.insert(QStringLiteral("value"), value);
		sendMessage(socket, message);
	});
	host.setEditorResizeCallback([&socket](int width, int height) {
		QJsonObject message;
		message.insert(QStringLiteral("type"), QStringLiteral("resize"));
		message.insert(QStringLiteral("width"), width);
		message.insert(QStringLiteral("height"), height);
		sendMessage(socket, message);
	});

	if (!host.openEditor(reinterpret_cast<void*>(parentWindow), &error))
	{
		sendError(socket, error);
		return 1;
	}

	int width = 0;
	int height = 0;
	host.editorSize(width, height);
	QJsonObject readyMessage;
	readyMessage.insert(QStringLiteral("type"), QStringLiteral("ready"));
	readyMessage.insert(QStringLiteral("width"), width);
	readyMessage.insert(QStringLiteral("height"), height);
	readyMessage.insert(QStringLiteral("title"), title);
	sendMessage(socket, readyMessage);

	QObject::connect(&socket, &QLocalSocket::readyRead, [&] {
		while (socket.canReadLine())
		{
			const auto document = QJsonDocument::fromJson(socket.readLine());
			if (!document.isObject())
			{
				continue;
			}
			const auto message = document.object();
			const auto type = message.value(QStringLiteral("type")).toString();
			if (type == QLatin1String("resize"))
			{
				host.resizeEditor(message.value(QStringLiteral("width")).toInt(),
					message.value(QStringLiteral("height")).toInt());
			}
			else if (type == QLatin1String("parameter"))
			{
				host.setParameterNormalised(
					static_cast<uint32_t>(message.value(QStringLiteral("id")).toDouble()),
					message.value(QStringLiteral("value")).toDouble());
			}
			else if (type == QLatin1String("close"))
			{
				host.closeEditor();
				socket.disconnectFromServer();
				QCoreApplication::quit();
			}
		}
	});
	QObject::connect(&socket, &QLocalSocket::disconnected,
		&application, &QCoreApplication::quit);

	return application.exec();
}
