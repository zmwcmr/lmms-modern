/*
 * Lv2Host.cpp - child process that hosts an LV2 plugin instance
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

/*
 * Scaffold for the isolated LV2 host (P6.4, first step).
 *
 * The plugin itself is not instantiated yet: the child echoes the input block
 * so the transport, the shared audio segment, the block handshake and the
 * crash containment can be tested without any LV2 code. The next step replaces
 * the echo in Lv2HostProcess::processBlock() with a real lilv instance.
 */

#include <csignal>
#include <cstdio>
#include <cstring>
#include <exception>
#include <string>

#include <condition_variable>
#include <chrono>
#include <mutex>
#include <thread>

#include "LmmsTypes.h"
#include "Lv2HostProtocol.h"
#include "RemotePluginBase.h"
#include "SharedMemory.h"

#ifdef LMMS_BUILD_LINUX
#include <sys/prctl.h>
#include <unistd.h>
#endif

#ifndef SYNC_WITH_SHM_FIFO
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace
{

using namespace lmms;

/*! Kills the child when its parent disappears without sending IdQuit.
 *
 *  PR_SET_PDEATHSIG covers SIGKILL of the host, but not a reparented process
 *  on every platform, so the parent ID is also polled. This mirrors the VST2
 *  remote host.
 */
#ifdef LMMS_BUILD_LINUX
class ParentWatcher
{
public:
	ParentWatcher()
	{
		m_thread = std::thread([this]
		{
			std::unique_lock lock{m_mutex};
			while (!m_cv.wait_for(lock, std::chrono::milliseconds(500),
				[this] { return m_stop; }))
			{
				if (::getppid() == 1)
				{
					::kill(::getpid(), SIGHUP);
					break;
				}
			}
		});
	}

	~ParentWatcher()
	{
		{
			const std::lock_guard lock{m_mutex};
			m_stop = true;
		}
		m_cv.notify_all();
		m_thread.join();
	}

private:
	bool m_stop = false;
	std::mutex m_mutex;
	std::condition_variable m_cv;
	std::thread m_thread;
};
#endif // LMMS_BUILD_LINUX




class Lv2HostProcess : public RemotePluginBase
{
public:
#ifdef SYNC_WITH_SHM_FIFO
	Lv2HostProcess(shmFifo* in, shmFifo* out, std::string pluginUri) :
		RemotePluginBase(in, out),
		m_pluginUri(std::move(pluginUri))
	{
	}
#else
	explicit Lv2HostProcess(std::string pluginUri) :
		RemotePluginBase(),
		m_pluginUri(std::move(pluginUri))
	{
	}

	void setSocket(int socket)
	{
		m_socket = socket;
	}
#endif

	int run();
	bool processMessage(const message& m) override;

private:
	void processBlock();

	SharedMemory<float[]> m_audio;
	f_cnt_t m_frames = 0;
	sample_rate_t m_sampleRate = 0;
	std::string m_pluginUri;
};




int Lv2HostProcess::run()
{
	for (;;)
	{
		const auto m = receiveMessage();
		if (m.id == IdQuit || m.id == IdUndefined)
		{
			break;
		}
		processMessage(m);
	}
	return 0;
}




bool Lv2HostProcess::processMessage(const message& m)
{
	switch (m.id)
	{
		case IdBufferSizeInformation:
			m_frames = static_cast<f_cnt_t>(m.getInt());
			return true;

		case IdSampleRateInformation:
			m_sampleRate = static_cast<sample_rate_t>(m.getInt());
			return true;

		case IdChangeSharedMemoryKey:
			try
			{
				m_audio.attach(m.getString(0));
			}
			catch (const std::exception& error)
			{
				sendMessage(message(lv2host::IdFailed).addString(error.what()));
				return false;
			}
			sendMessage(message(lv2host::IdReady));
			return true;

		case IdStartProcessing:
			processBlock();
			sendMessage(message(IdProcessingDone));
			return true;

		default:
			return false;
	}
}




void Lv2HostProcess::processBlock()
{
	if (!m_audio || m_frames == 0)
	{
		return;
	}

	const auto frames = static_cast<std::size_t>(m_frames);
	float* const input = m_audio.get() + lv2host::AudioSegment::inputOffset();
	float* const output = m_audio.get() + lv2host::AudioSegment::outputOffset(frames);

	// Scaffold: echo the input. The real instance replaces this in the next
	// step, with one float plane per LV2 audio port.
	std::memcpy(output, input,
		lv2host::AudioSegment::channels * frames * sizeof(float));
}




#ifndef SYNC_WITH_SHM_FIFO
int connectToHost(const char* path)
{
	struct sockaddr_un address = {};
	address.sun_family = AF_LOCAL;
	std::strncpy(address.sun_path, path, sizeof(address.sun_path) - 1);

	const int fd = ::socket(PF_LOCAL, SOCK_STREAM, 0);
	if (fd == -1)
	{
		return -1;
	}
	if (::connect(fd, reinterpret_cast<struct sockaddr*>(&address),
			sizeof(address)) == -1)
	{
		::close(fd);
		return -1;
	}
	return fd;
}
#endif

} // namespace




int main(int argc, char** argv)
{
#ifdef LMMS_BUILD_LINUX
	// If LMMS is killed with SIGKILL, the kernel terminates this process as
	// well instead of leaving an orphan behind.
	prctl(PR_SET_PDEATHSIG, SIGTERM);
	if (getppid() == 1)
	{
		return 1;
	}
#endif

	// A dead host must not kill the child through SIGPIPE before it notices
	// the broken connection.
#ifndef LMMS_BUILD_WIN32
	std::signal(SIGPIPE, SIG_IGN);
#endif

#ifdef SYNC_WITH_SHM_FIFO
	if (argc < 4)
	{
		std::fprintf(stderr,
			"usage: %s <in-shm-key> <out-shm-key> <plugin-uri>\n", argv[0]);
		return 1;
	}
	Lv2HostProcess host{new shmFifo(argv[1]), new shmFifo(argv[2]), argv[3]};
#else
	if (argc < 3)
	{
		std::fprintf(stderr, "usage: %s <socket-path> <plugin-uri>\n", argv[0]);
		return 1;
	}
	const int fd = connectToHost(argv[1]);
	if (fd == -1)
	{
		std::fprintf(stderr, "could not connect to the LMMS LV2 host socket\n");
		return 1;
	}
	Lv2HostProcess host{argv[2]};
	host.setSocket(fd);
#endif

#ifdef LMMS_BUILD_LINUX
	const ParentWatcher parentWatcher;
#endif
	return host.run();
}
