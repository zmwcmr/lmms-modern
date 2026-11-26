/*
 * Lv2HostClient.cpp - client side of the LV2 host child process
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

#include "Lv2HostClient.h"

#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QThread>
#include <QUuid>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <exception>

#ifndef SYNC_WITH_SHM_FIFO
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace lmms
{

Lv2HostClient::Lv2HostClient(QObject* parent) :
	QObject(parent),
#ifdef SYNC_WITH_SHM_FIFO
	RemotePluginBase(new shmFifo(), new shmFifo()),
#else
	RemotePluginBase(),
#endif
	m_process()
{
	m_process.setProcessChannelMode(QProcess::ForwardedChannels);

	// Writing to a child that has just died must not raise SIGPIPE and take
	// the host down with it.
#ifndef LMMS_BUILD_WIN32
	static const bool sigpipeIgnored = []
	{
		std::signal(SIGPIPE, SIG_IGN);
		return true;
	}();
	Q_UNUSED(sigpipeIgnored)
#endif
}




Lv2HostClient::~Lv2HostClient()
{
	stop();
}




bool Lv2HostClient::start(const QString& hostExecutable, const QString& pluginUri,
	sample_rate_t sampleRate, f_cnt_t framesPerBlock, int timeoutMs)
{
	stop();
	m_error.clear();
	m_failed = true;

	if (sampleRate == 0 || framesPerBlock == 0 || timeoutMs <= 0)
	{
		markFailed(tr("Invalid sample rate, block size or timeout for the LV2 host"));
		return false;
	}

#ifdef SYNC_WITH_SHM_FIFO
	// stop() invalidated the previous transport, so start from fresh FIFOs.
	reset(new shmFifo(), new shmFifo());
#else
	// The socket transport keeps its invalid flag, clear it for the new child.
	clearInvalid();
#endif

	QString exec = hostExecutable.isEmpty()
		? QStringLiteral("lmms-lv2-host") : hostExecutable;
	QString resolved = exec;
	if (!QFileInfo(exec).isAbsolute() || !QFileInfo::exists(exec))
	{
		resolved = QFileInfo(QDir(QStringLiteral("plugins:")), exec).absoluteFilePath();
	}
	if (const char* env = std::getenv("LMMS_PLUGIN_DIR");
		env != nullptr && *env != '\0' && !QFileInfo(exec).isAbsolute())
	{
		resolved = QFileInfo(QDir(QString::fromLocal8Bit(env)), exec).absoluteFilePath();
	}
#ifdef LMMS_BUILD_WIN32
	if (!resolved.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive))
	{
		resolved += QStringLiteral(".exe");
	}
#endif
	if (!QFileInfo::exists(resolved))
	{
		markFailed(tr("LV2 host '%1' was not found").arg(resolved));
		return false;
	}

	m_sampleRate = sampleRate;
	m_framesPerBlock = framesPerBlock;

	try
	{
		m_audio.create(lv2host::AudioSegment::floatCount(
			static_cast<std::size_t>(framesPerBlock)));
	}
	catch (const std::exception& error)
	{
		markFailed(tr("Could not allocate the LV2 audio segment: %1")
			.arg(QString::fromLocal8Bit(error.what())));
		return false;
	}

	QStringList args;
#ifdef SYNC_WITH_SHM_FIFO
	args = QStringList{
		QString::fromStdString(out()->shmKey()),
		QString::fromStdString(in()->shmKey()),
		pluginUri,
	};
#else
	m_socketFile = QDir::tempPath() + QDir::separator()
		+ QStringLiteral("lmms-lv2-host-")
		+ QUuid::createUuid().toString(QUuid::WithoutBraces);

	struct sockaddr_un address = {};
	address.sun_family = AF_LOCAL;
	const QByteArray path = QFile::encodeName(m_socketFile);
	if (static_cast<std::size_t>(path.size()) >= sizeof(address.sun_path))
	{
		markFailed(tr("Socket path for the LV2 host is too long"));
		return false;
	}
	std::memcpy(address.sun_path, path.constData(),
		static_cast<std::size_t>(path.size()) + 1);

	m_serverSocket = ::socket(PF_LOCAL, SOCK_STREAM, 0);
	if (m_serverSocket == -1)
	{
		markFailed(tr("Could not create the LV2 host socket"));
		return false;
	}
	::unlink(address.sun_path);
	if (::bind(m_serverSocket, reinterpret_cast<struct sockaddr*>(&address),
			sizeof(address)) == -1
		|| ::listen(m_serverSocket, 1) == -1)
	{
		markFailed(tr("Could not listen on the LV2 host socket"));
		return false;
	}
	args = QStringList{ m_socketFile, pluginUri };
#endif

	m_process.start(resolved, args);
	if (!m_process.waitForStarted(timeoutMs))
	{
		markFailed(tr("Could not start the LV2 host '%1'").arg(resolved));
		stop();
		return false;
	}

#ifndef SYNC_WITH_SHM_FIFO
	struct pollfd pollin = { m_serverSocket, POLLIN, 0 };
	switch (::poll(&pollin, 1, timeoutMs))
	{
		case -1:
			markFailed(tr("Unexpected error while waiting for the LV2 host"));
			stop();
			return false;
		case 0:
			markFailed(tr("The LV2 host did not connect"));
			stop();
			return false;
		default:
			m_socket = ::accept(m_serverSocket, nullptr, nullptr);
			if (m_socket == -1)
			{
				markFailed(tr("Could not accept the LV2 host connection"));
				stop();
				return false;
			}
	}
#endif

	sendMessage(message(IdBufferSizeInformation).addInt(static_cast<int>(framesPerBlock)));
	sendMessage(message(IdSampleRateInformation).addInt(static_cast<int>(sampleRate)));
	sendMessage(message(IdChangeSharedMemoryKey).addString(m_audio.key()));

	if (!waitForReply(lv2host::IdReady, timeoutMs))
	{
		if (m_error.isEmpty())
		{
			markFailed(tr("The LV2 host did not become ready"));
		}
		stop();
		return false;
	}

	m_failed = false;
	return true;
}




void Lv2HostClient::stop()
{
	m_stopping = true;

	if (m_process.state() != QProcess::NotRunning)
	{
		sendMessage(message(IdQuit));

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

#ifndef SYNC_WITH_SHM_FIFO
	if (m_socket != -1)
	{
		::close(m_socket);
		m_socket = -1;
	}
	if (m_serverSocket != -1)
	{
		::close(m_serverSocket);
		m_serverSocket = -1;
	}
	if (!m_socketFile.isEmpty())
	{
		::unlink(QFile::encodeName(m_socketFile).constData());
		m_socketFile.clear();
	}
#endif

	m_stopping = false;
	m_failed = true;
	clearAudioSegment();
}




bool Lv2HostClient::isRunning() const
{
	return !m_failed && m_process.state() != QProcess::NotRunning;
}




bool Lv2HostClient::processBlock(const float* const* in, float* const* out, f_cnt_t frames)
{
	if (m_failed || !m_audio || frames == 0 || frames != m_framesPerBlock)
	{
		silenceOutput(out, frames);
		return false;
	}

	float* const data = m_audio.get();
	float* const input = data + lv2host::AudioSegment::inputOffset();
	float* const output = data + lv2host::AudioSegment::outputOffset(
		static_cast<std::size_t>(frames));
	const auto channelSamples = static_cast<std::size_t>(frames);
	const std::size_t planeBytes = channelSamples * sizeof(float);

	for (int ch = 0; ch < lv2host::AudioSegment::channels; ++ch)
	{
		if (in != nullptr && in[ch] != nullptr)
		{
			std::memcpy(input + ch * channelSamples, in[ch], planeBytes);
		}
		else
		{
			std::memset(input + ch * channelSamples, 0, planeBytes);
		}
		std::memset(output + ch * channelSamples, 0, planeBytes);
	}

	sendMessage(message(IdStartProcessing));

	if (!waitForReply(IdProcessingDone, 5000))
	{
		silenceOutput(out, frames);
		return false;
	}

	for (int ch = 0; ch < lv2host::AudioSegment::channels; ++ch)
	{
		if (out != nullptr && out[ch] != nullptr)
		{
			std::memcpy(out[ch], output + ch * channelSamples, planeBytes);
		}
	}
	return true;
}




bool Lv2HostClient::processMessage(const message& m)
{
	if (m.id == IdDebugMessage)
	{
		qWarning() << "LV2 host:" << m.getQString(0);
	}
	return false;
}




bool Lv2HostClient::waitForReply(int messageId, int timeoutMs)
{
	QElapsedTimer timer;
	timer.start();

	while (timer.elapsed() < timeoutMs)
	{
		if (isInvalid())
		{
			if (m_error.isEmpty())
			{
				markFailed(tr("The LV2 host process died"));
			}
			return false;
		}

		while (messagesLeft() && !isInvalid())
		{
			const auto m = fetchAndProcessNextMessage();
			if (m.id == IdUndefined)
			{
				break;
			}
			if (m.id == messageId)
			{
				return true;
			}
			if (m.id == lv2host::IdFailed)
			{
				const QString reason = m.getQString(0);
				markFailed(reason.isEmpty()
					? tr("The LV2 host reported a failure") : reason);
				return false;
			}
		}

		if (isInvalid())
		{
			if (m_error.isEmpty())
			{
				markFailed(tr("The LV2 host process died"));
			}
			return false;
		}

		if (m_process.state() == QProcess::NotRunning)
		{
			if (!m_stopping)
			{
				emit crashed();
			}
			if (m_error.isEmpty())
			{
				markFailed(tr("The LV2 host process is not running"));
			}
			return false;
		}

		QThread::msleep(1);
	}

	return false;
}




void Lv2HostClient::markFailed(const QString& reason)
{
	m_failed = true;
	if (m_error.isEmpty())
	{
		m_error = reason;
	}
	qWarning() << "Lv2HostClient:" << reason;
	invalidate();
}




void Lv2HostClient::clearAudioSegment()
{
	m_audio.detach();
	m_framesPerBlock = 0;
	m_sampleRate = 0;
}




void Lv2HostClient::silenceOutput(float* const* out, f_cnt_t frames) const
{
	if (out == nullptr || frames == 0)
	{
		return;
	}
	for (int ch = 0; ch < lv2host::AudioSegment::channels; ++ch)
	{
		if (out[ch] != nullptr)
		{
			std::memset(out[ch], 0, static_cast<std::size_t>(frames) * sizeof(float));
		}
	}
}

} // namespace lmms
