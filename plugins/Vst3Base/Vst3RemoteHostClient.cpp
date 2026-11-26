/*
 * Vst3RemoteHostClient.cpp - client for the out-of-process DSP host
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

#include "Vst3RemoteHostClient.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <chrono>
#include <cmath>
#include <cstring>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QProcess>
#include <QSharedMemory>

#include <fcntl.h>
#include <semaphore.h>
#include <time.h>

#include "Vst3SharedAudio.h"

namespace lmms
{

Vst3RemoteHostClient::Vst3RemoteHostClient() = default;

Vst3RemoteHostClient::~Vst3RemoteHostClient()
{
	stop();
}

bool Vst3RemoteHostClient::start(const QString& bundlePath,
	const QString& classId,
	double sampleRate,
	int maxBlockSize,
	const QByteArray& componentState,
	const QByteArray& controllerState,
	QString* error)
{
	stop();

	const auto host = hostExecutable();
	if (host.isEmpty())
	{
		if (error != nullptr)
		{
			*error = tr("lmms-vst3-host was not found");
		}
		return false;
	}

	const auto unique = QStringLiteral("%1-%2")
		.arg(QCoreApplication::applicationPid())
		.arg(reinterpret_cast<quintptr>(this));
	m_key = QStringLiteral("lmms-vst3-dsp-") + unique;
	m_requestName = QStringLiteral("/lmms-vst3-req-") + unique;
	m_doneName = QStringLiteral("/lmms-vst3-done-") + unique;

	m_shared = new QSharedMemory(m_key, this);
	if (!m_shared->create(sizeof(vst3::SharedAudio)))
	{
		if (error != nullptr)
		{
			*error = tr("Unable to create the shared audio segment: %1")
				.arg(m_shared->errorString());
		}
		stop();
		return false;
	}
	m_audio = static_cast<vst3::SharedAudio*>(m_shared->data());

	sem_unlink(m_requestName.toUtf8().constData());
	sem_unlink(m_doneName.toUtf8().constData());
	m_request = sem_open(m_requestName.toUtf8().constData(), O_CREAT | O_EXCL,
		0600, 0);
	m_done = sem_open(m_doneName.toUtf8().constData(), O_CREAT | O_EXCL, 0600, 0);
	if (m_request == SEM_FAILED || m_done == SEM_FAILED)
	{
		if (error != nullptr)
		{
			*error = tr("Unable to create the host semaphores");
		}
		stop();
		return false;
	}

	m_server = new QLocalServer(this);
	if (!m_server->listen(m_key))
	{
		if (error != nullptr)
		{
			*error = tr("Unable to create the DSP host socket");
		}
		stop();
		return false;
	}

	m_process = new QProcess(this);
	m_process->setProgram(host);
	m_process->setArguments({
		m_key,
		m_key,
		m_requestName,
		m_doneName,
		bundlePath,
		classId,
		QString::number(sampleRate),
		QString::number(std::max(1, maxBlockSize)),
	});
	m_process->start();
	if (!m_process->waitForStarted(5000))
	{
		if (error != nullptr)
		{
			*error = tr("Unable to start lmms-vst3-host");
		}
		stop();
		return false;
	}

	if (!m_server->waitForNewConnection(5000))
	{
		if (error != nullptr)
		{
			*error = tr("The DSP host did not connect");
		}
		stop();
		return false;
	}
	m_socket = m_server->nextPendingConnection();
	if (m_socket == nullptr)
	{
		if (error != nullptr)
		{
			*error = tr("The DSP host socket is invalid");
		}
		stop();
		return false;
	}

	QJsonObject state;
	state.insert(QStringLiteral("type"), QStringLiteral("state"));
	state.insert(QStringLiteral("component"),
		QString::fromLatin1(componentState.toBase64()));
	state.insert(QStringLiteral("controller"),
		QString::fromLatin1(controllerState.toBase64()));
	sendMessage(state);

	QJsonObject ready;
	while (true)
	{
		const int newline = m_readBuffer.indexOf('\n');
		if (newline >= 0)
		{
			const auto document = QJsonDocument::fromJson(
				m_readBuffer.left(newline));
			m_readBuffer.remove(0, newline + 1);
			if (document.isObject())
			{
				ready = document.object();
				break;
			}
			continue;
		}
		if (!m_socket->waitForReadyRead(10000))
		{
			if (error != nullptr)
			{
				*error = tr("The DSP host did not become ready");
			}
			stop();
			return false;
		}
		m_readBuffer += m_socket->readAll();
	}

	if (ready.value(QStringLiteral("type")).toString() == QLatin1String("error"))
	{
		if (error != nullptr)
		{
			*error = ready.value(QStringLiteral("message")).toString();
		}
		stop();
		return false;
	}

	m_latency = ready.value(QStringLiteral("latency")).toInt();
	m_parameters.clear();
	for (const auto& value : ready.value(QStringLiteral("parameters")).toArray())
	{
		const auto object = value.toObject();
		Vst3Host::Parameter parameter;
		parameter.id = static_cast<uint32_t>(
			object.value(QStringLiteral("id")).toDouble());
		parameter.name = object.value(QStringLiteral("name")).toString();
		parameter.unit = object.value(QStringLiteral("unit")).toString();
		parameter.stepCount = object.value(QStringLiteral("stepCount")).toInt();
		parameter.flags = object.value(QStringLiteral("flags")).toInt();
		parameter.defaultNormalised = object.value(QStringLiteral("default")).toDouble();
		parameter.automatable = (parameter.flags & 1) != 0;
		parameter.readOnly = (parameter.flags & 2) != 0;
		parameter.hidden = (parameter.flags & 16) != 0;
		m_parameters.push_back(std::move(parameter));
	}

	m_running = true;
	return true;
}

void Vst3RemoteHostClient::stop()
{
	m_running = false;
	if (m_audio != nullptr)
	{
		m_audio->stop = 1;
	}
	if (m_request != nullptr)
	{
		sem_post(static_cast<sem_t*>(m_request));
	}
	if (m_socket != nullptr)
	{
		QJsonObject message;
		message.insert(QStringLiteral("type"), QStringLiteral("close"));
		sendMessage(message);
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
	if (m_request != nullptr)
	{
		sem_close(static_cast<sem_t*>(m_request));
		sem_unlink(m_requestName.toUtf8().constData());
		m_request = nullptr;
	}
	if (m_done != nullptr)
	{
		sem_close(static_cast<sem_t*>(m_done));
		sem_unlink(m_doneName.toUtf8().constData());
		m_done = nullptr;
	}
	if (m_shared != nullptr)
	{
		m_shared->detach();
		delete m_shared;
		m_shared = nullptr;
		m_audio = nullptr;
	}
	m_parameters.clear();
	m_readBuffer.clear();
	m_latency = 0;
}

bool Vst3RemoteHostClient::isRunning() const
{
	return m_running;
}

void Vst3RemoteHostClient::queueParameter(
	uint32_t id, double value, int sampleOffset)
{
	std::lock_guard<std::mutex> lock(m_queueMutex);
	m_pendingParameters.push_back({id, value, sampleOffset});
}

bool Vst3RemoteHostClient::setProgram(int index)
{
	if (!m_running)
	{
		return false;
	}
	QJsonObject message;
	message.insert(QStringLiteral("type"), QStringLiteral("program"));
	message.insert(QStringLiteral("index"), index);
	sendMessage(message);
	return true;
}

bool Vst3RemoteHostClient::setUnit(int unitId)
{
	if (!m_running)
	{
		return false;
	}
	QJsonObject message;
	message.insert(QStringLiteral("type"), QStringLiteral("unit"));
	message.insert(QStringLiteral("unit"), unitId);
	sendMessage(message);
	return true;
}

void Vst3RemoteHostClient::queueMidi(const Vst3Host::MidiEvent& event)
{
	std::lock_guard<std::mutex> lock(m_queueMutex);
	m_pendingMidi.push_back(event);
}

void Vst3RemoteHostClient::setTempo(double bpm)
{
	std::lock_guard<std::mutex> lock(m_queueMutex);
	m_tempo = bpm > 0.0 ? bpm : 120.0;
}

void Vst3RemoteHostClient::setPlaying(bool playing)
{
	std::lock_guard<std::mutex> lock(m_queueMutex);
	m_playing = playing;
}

void Vst3RemoteHostClient::process(const float* leftIn,
	const float* rightIn,
	float* leftOut,
	float* rightOut,
	int frames)
{
	if (frames <= 0)
	{
		return;
	}
	if (leftOut != nullptr)
	{
		std::memset(leftOut, 0, sizeof(float) * static_cast<size_t>(frames));
	}
	if (rightOut != nullptr)
	{
		std::memset(rightOut, 0, sizeof(float) * static_cast<size_t>(frames));
	}
	if (!m_running || m_audio == nullptr || m_request == nullptr || m_done == nullptr)
	{
		return;
	}

	frames = std::min(frames, vst3::maxBlockSize);
	{
		std::lock_guard<std::mutex> lock(m_queueMutex);
		m_audio->nframes = frames;
		m_audio->tempo = m_tempo;
		m_audio->playing = m_playing ? 1 : 0;
		for (int frame = 0; frame < frames; ++frame)
		{
			m_audio->inputLeft[frame] = leftIn != nullptr ? leftIn[frame] : 0.0f;
			m_audio->inputRight[frame] = rightIn != nullptr ? rightIn[frame] : 0.0f;
			m_audio->outputLeft[frame] = 0.0f;
			m_audio->outputRight[frame] = 0.0f;
		}
		const int midiCount = std::min(static_cast<int>(m_pendingMidi.size()),
			vst3::maxMidiEvents);
		m_audio->midiCount = midiCount;
		for (int index = 0; index < midiCount; ++index)
		{
			const auto& source = m_pendingMidi[static_cast<size_t>(index)];
			auto& target = m_audio->midi[index];
			target.type = static_cast<int32_t>(source.type);
			target.channel = source.channel;
			target.data1 = source.data1;
			target.data2 = source.data2;
			target.sampleOffset = source.sampleOffset;
			target.value = source.value;
		}
		const int parameterCount = std::min(
			static_cast<int>(m_pendingParameters.size()),
			vst3::maxParameterEvents);
		m_audio->parameterCount = parameterCount;
		for (int index = 0; index < parameterCount; ++index)
		{
			const auto& source = m_pendingParameters[static_cast<size_t>(index)];
			auto& target = m_audio->parameters[index];
			target.id = source.id;
			target.value = source.value;
			target.sampleOffset = source.sampleOffset;
		}
		m_pendingMidi.clear();
		m_pendingParameters.clear();
	}

	sem_post(static_cast<sem_t*>(m_request));
	timespec timeout = {};
	clock_gettime(CLOCK_REALTIME, &timeout);
	timeout.tv_sec += 2;
	while (sem_timedwait(static_cast<sem_t*>(m_done), &timeout) != 0)
	{
		if (errno != EINTR)
		{
			m_running = false;
			return;
		}
	}

	if (leftOut != nullptr)
	{
		std::memcpy(leftOut, m_audio->outputLeft,
			sizeof(float) * static_cast<size_t>(frames));
	}
	if (rightOut != nullptr)
	{
		std::memcpy(rightOut, m_audio->outputRight,
			sizeof(float) * static_cast<size_t>(frames));
	}
}

QString Vst3RemoteHostClient::hostExecutable()
{
	const auto name = QStringLiteral("lmms-vst3-host");
	const char* env = std::getenv("LMMS_VST3_HOST");
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

void Vst3RemoteHostClient::sendMessage(const QJsonObject& message)
{
	if (m_socket != nullptr && m_socket->state() == QLocalSocket::ConnectedState)
	{
		m_socket->write(QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n');
		m_socket->flush();
	}
}

} // namespace lmms
