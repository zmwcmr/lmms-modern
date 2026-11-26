/*
 * Vst3RemoteHostClient.h - client for the out-of-process DSP host
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

#ifndef LMMS_VST3_REMOTE_HOST_CLIENT_H
#define LMMS_VST3_REMOTE_HOST_CLIENT_H

#include <atomic>
#include <mutex>
#include <vector>

#include <QObject>
#include <QString>

#include "Vst3Host.h"
#include "vst3base_export.h"

class QLocalServer;
class QLocalSocket;
class QProcess;
class QSharedMemory;

namespace lmms
{

namespace vst3
{
struct SharedAudio;
}

/**
 * Starts `lmms-vst3-host` and exchanges stereo audio, MIDI and parameter
 * changes through a shared memory block. The control socket carries state and
 * lifecycle messages.
 */
class VST3BASE_EXPORT Vst3RemoteHostClient : public QObject
{
public:
	Vst3RemoteHostClient();
	~Vst3RemoteHostClient() override;

	bool start(const QString& bundlePath,
		const QString& classId,
		double sampleRate,
		int maxBlockSize,
		const QByteArray& componentState,
		const QByteArray& controllerState,
		QString* error = nullptr);
	void stop();
	bool isRunning() const;

	int latencySamples() const { return m_latency; }
	const std::vector<Vst3Host::Parameter>& parameters() const
	{
		return m_parameters;
	}

	void queueParameter(uint32_t id, double value, int sampleOffset = 0);
	bool setProgram(int index);
	bool setUnit(int unitId);
	void queueMidi(const Vst3Host::MidiEvent& event);
	void setTempo(double bpm);
	void setPlaying(bool playing);
	void process(const float* leftIn,
		const float* rightIn,
		float* leftOut,
		float* rightOut,
		int frames);

	static QString hostExecutable();

private:
	void sendMessage(const QJsonObject& message);

	QSharedMemory* m_shared = nullptr;
	vst3::SharedAudio* m_audio = nullptr;
	QLocalServer* m_server = nullptr;
	QLocalSocket* m_socket = nullptr;
	QProcess* m_process = nullptr;
	void* m_request = nullptr;
	void* m_done = nullptr;
	QString m_key;
	QString m_requestName;
	QString m_doneName;
	QByteArray m_readBuffer;
	std::vector<Vst3Host::Parameter> m_parameters;
	int m_latency = 0;
	std::atomic<bool> m_running{false};

	mutable std::mutex m_queueMutex;
	std::vector<Vst3Host::MidiEvent> m_pendingMidi;
	struct PendingParameter
	{
		uint32_t id;
		double value;
		int sampleOffset;
	};
	std::vector<PendingParameter> m_pendingParameters;
	double m_tempo = 120.0;
	bool m_playing = false;
};

} // namespace lmms

#endif // LMMS_VST3_REMOTE_HOST_CLIENT_H
