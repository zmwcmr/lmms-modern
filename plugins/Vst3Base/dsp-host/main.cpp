/*
 * main.cpp - out-of-process VST3 DSP host
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

#include <algorithm>
#include <atomic>
#include <cmath>
#include <csignal>

#if defined(__linux__)
#	include <sys/prctl.h>
#endif
#include <thread>

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QSharedMemory>
#include <QTextStream>

#include <semaphore.h>

#include "Vst3Host.h"
#include "Vst3SharedAudio.h"

namespace
{

sem_t* g_request = SEM_FAILED;
sem_t* g_done = SEM_FAILED;
std::atomic<bool> g_stop{false};

void stopHandler(int)
{
	g_stop = true;
	if (g_request != SEM_FAILED)
	{
		sem_post(g_request);
	}
}

void sendMessage(QLocalSocket& socket, const QJsonObject& message)
{
	if (socket.state() != QLocalSocket::ConnectedState)
	{
		return;
	}
	socket.write(QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n');
	socket.flush();
}

bool readMessage(QLocalSocket& socket, QJsonObject& message, QByteArray& buffer)
{
	while (true)
	{
		const int newline = buffer.indexOf('\n');
		if (newline >= 0)
		{
			const auto document = QJsonDocument::fromJson(buffer.left(newline));
			buffer.remove(0, newline + 1);
			if (document.isObject())
			{
				message = document.object();
				return true;
			}
			continue;
		}
		if (!socket.waitForReadyRead(10000))
		{
			return false;
		}
		buffer += socket.readAll();
	}
}

} // namespace

int main(int argc, char* argv[])
{
	QCoreApplication application(argc, argv);
#if defined(__linux__)
	prctl(PR_SET_PDEATHSIG, SIGKILL);
#endif
	const auto arguments = application.arguments();
	if (arguments.size() < 9)
	{
		QTextStream(stderr)
			<< "Usage: lmms-vst3-host <socket> <shm-key> <request-sem> "
			   "<done-sem> <bundle> <class-id> <sample-rate> <block-size>\n";
		return 2;
	}

	const auto socketName = arguments.at(1);
	const auto sharedKey = arguments.at(2);
	const auto requestName = arguments.at(3);
	const auto doneName = arguments.at(4);
	const auto bundlePath = arguments.at(5);
	const auto classId = arguments.at(6);
	const double sampleRate = arguments.at(7).toDouble();
	const int blockSize = arguments.at(8).toInt();

	g_request = sem_open(requestName.toUtf8().constData(), 0);
	g_done = sem_open(doneName.toUtf8().constData(), 0);
	if (g_request == SEM_FAILED || g_done == SEM_FAILED)
	{
		QTextStream(stderr) << "Unable to open the host semaphores\n";
		return 1;
	}

	QSharedMemory shared(sharedKey);
	if (!shared.attach())
	{
		QTextStream(stderr) << "Unable to attach the shared audio segment\n";
		return 1;
	}
	if (shared.size() < static_cast<int>(sizeof(lmms::vst3::SharedAudio)))
	{
		QTextStream(stderr) << "Shared audio segment is too small\n";
		return 1;
	}
	auto* const audio = static_cast<lmms::vst3::SharedAudio*>(shared.data());

	QLocalSocket socket;
	socket.connectToServer(socketName);
	if (!socket.waitForConnected(5000))
	{
		QTextStream(stderr) << "Unable to connect to the DSP client\n";
		return 1;
	}

	QByteArray readBuffer;
	QJsonObject stateMessage;
	if (!readMessage(socket, stateMessage, readBuffer)
		|| stateMessage.value(QStringLiteral("type")).toString()
			!= QLatin1String("state"))
	{
		QTextStream(stderr) << "No initial plugin state received\n";
		return 1;
	}

	lmms::Vst3Host host;
	QString error;
	if (!host.load(bundlePath, classId, sampleRate, blockSize, &error))
	{
		QJsonObject message;
		message.insert(QStringLiteral("type"), QStringLiteral("error"));
		message.insert(QStringLiteral("message"), error);
		sendMessage(socket, message);
		return 1;
	}

	const auto component = QByteArray::fromBase64(
		stateMessage.value(QStringLiteral("component")).toString().toLatin1());
	const auto controller = QByteArray::fromBase64(
		stateMessage.value(QStringLiteral("controller")).toString().toLatin1());
	if (!component.isEmpty() || !controller.isEmpty())
	{
		host.setState(component, controller);
	}

	QJsonArray parameters;
	for (const auto& parameter : host.parameters())
	{
		QJsonObject object;
		object.insert(QStringLiteral("id"), static_cast<double>(parameter.id));
		object.insert(QStringLiteral("name"), parameter.name);
		object.insert(QStringLiteral("unit"), parameter.unit);
		object.insert(QStringLiteral("stepCount"), parameter.stepCount);
		object.insert(QStringLiteral("flags"), parameter.flags);
		object.insert(QStringLiteral("default"), parameter.defaultNormalised);
		parameters.append(object);
	}
	QJsonObject readyMessage;
	readyMessage.insert(QStringLiteral("type"), QStringLiteral("ready"));
	readyMessage.insert(QStringLiteral("latency"), host.latencySamples());
	readyMessage.insert(QStringLiteral("parameters"), parameters);
	sendMessage(socket, readyMessage);

	signal(SIGTERM, stopHandler);
	signal(SIGINT, stopHandler);

	std::thread audioThread([&host, audio, blockSize] {
		while (!g_stop)
		{
			if (sem_wait(g_request) != 0)
			{
				break;
			}
			if (g_stop || audio->stop != 0)
			{
				break;
			}
			int frames = audio->nframes;
			frames = std::clamp(frames, 1, lmms::vst3::maxBlockSize);
			host.setTempo(audio->tempo > 0.0 ? audio->tempo : 120.0);
			host.setPlaying(audio->playing != 0);
			for (int i = 0; i < std::min(audio->parameterCount,
					lmms::vst3::maxParameterEvents); ++i)
			{
				const auto& parameter = audio->parameters[i];
				host.queueParameter(parameter.id, parameter.value,
					parameter.sampleOffset);
			}
			for (int i = 0; i < std::min(audio->midiCount,
					lmms::vst3::maxMidiEvents); ++i)
			{
				const auto& source = audio->midi[i];
				lmms::Vst3Host::MidiEvent event;
				event.type = static_cast<lmms::Vst3Host::MidiType>(
					std::clamp(source.type, 0, 6));
				event.sampleOffset = source.sampleOffset;
				event.channel = source.channel;
				event.data1 = source.data1;
				event.data2 = source.data2;
				event.value = source.value;
				host.queueMidi(event);
			}
			host.process(audio->inputLeft, audio->inputRight,
				audio->outputLeft, audio->outputRight, frames);
			sem_post(g_done);
		}
	});

	QObject::connect(&socket, &QLocalSocket::readyRead, [&] {
		readBuffer += socket.readAll();
		while (true)
		{
			const int newline = readBuffer.indexOf('\n');
			if (newline < 0)
			{
				break;
			}
			const auto document = QJsonDocument::fromJson(readBuffer.left(newline));
			readBuffer.remove(0, newline + 1);
			if (!document.isObject())
			{
				continue;
			}
			const auto message = document.object();
			const auto type = message.value(QStringLiteral("type")).toString();
			if (type == QLatin1String("close"))
			{
				g_stop = true;
				audio->stop = 1;
				sem_post(g_request);
				QCoreApplication::quit();
			}
			else if (type == QLatin1String("parameter"))
			{
				host.setParameterNormalised(
					static_cast<uint32_t>(message.value(QStringLiteral("id")).toDouble()),
					message.value(QStringLiteral("value")).toDouble());
			}
			else if (type == QLatin1String("program"))
			{
				host.setProgram(message.value(QStringLiteral("index")).toInt());
			}
			else if (type == QLatin1String("unit"))
			{
				host.selectUnit(message.value(QStringLiteral("unit")).toInt());
			}
			else if (type == QLatin1String("state"))
			{
				const auto comp = QByteArray::fromBase64(
					message.value(QStringLiteral("component")).toString().toLatin1());
				const auto ctrl = QByteArray::fromBase64(
					message.value(QStringLiteral("controller")).toString().toLatin1());
				host.setState(comp, ctrl);
			}
		}
	});
	QObject::connect(&socket, &QLocalSocket::disconnected, &application, [&] {
		g_stop = true;
		audio->stop = 1;
		sem_post(g_request);
		QCoreApplication::quit();
	});

	const int result = application.exec();
	g_stop = true;
	audio->stop = 1;
	sem_post(g_request);
	if (audioThread.joinable())
	{
		audioThread.join();
	}
	sem_close(g_request);
	sem_close(g_done);
	return result;
}
