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
#include <map>
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
class QTimer;

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
class VST3BASE_EXPORT Vst3RemoteHostClient : public QObject, public Vst3HostBase
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

	bool load(const QString& bundlePath,
		const QString& classId,
		double sampleRate,
		int maxBlockSize,
		QString* error = nullptr) override;
	void unload() override;
	bool isLoaded() const override;
	bool isInstrument() const override;
	QString name() const override { return m_name; }
	QString vendor() const override { return m_vendor; }
	QString version() const override { return m_version; }
	QString bundlePath() const override { return m_bundlePath; }
	QString classId() const override { return m_classId; }

	std::vector<BusInfo> inputBuses() const override;
	std::vector<BusInfo> outputBuses() const override;

	const std::vector<Parameter>& parameters() const override
	{
		return m_parameters;
	}
	bool setParameterNormalised(uint32_t id, double value) override;
	double parameterNormalised(uint32_t id) const override;
	int programCount() const override;
	QString programName(int index) const override;
	int currentProgram() const override;
	bool setProgram(int index) override;
	const std::vector<Unit>& units() const override { return m_units; }
	int currentUnit() const override { return m_currentUnit; }
	bool selectUnit(int unitId) override;

	void queueParameter(uint32_t id, double value, int sampleOffset = 0) override;
	void queueMidi(const MidiEvent& event) override;
	void setTempo(double bpm) override;
	void setPlaying(bool playing) override;
	void process(const float* leftIn,
		const float* rightIn,
		float* leftOut,
		float* rightOut,
		int frames) override;

	int latencySamples() const override { return m_latency; }

	bool hasEditor() const override { return m_hasEditor; }
	bool openEditor(void*, QString* = nullptr) override { return false; }
	void closeEditor() override {}
	bool isEditorOpen() const override { return false; }
	bool editorSize(int&, int&) const override { return false; }
	bool canResizeEditor() const override { return false; }
	void resizeEditor(int, int) override {}
	void setEditorContentScale(float) override {}
	void focusEditor(bool) override {}
	void setEditorResizeCallback(std::function<void(int, int)>) override {}

	QByteArray componentState() const override;
	QByteArray controllerState() const override;
	bool setState(const QByteArray& componentState,
		const QByteArray& controllerState) override;

	static QString hostExecutable();

	qint64 hostProcessId() const;
	std::function<void()> hostCrashed;
	std::function<void()> hostRestarted;

private:
	void sendMessage(const QJsonObject& message);
	bool requestState();
	void onWatchdog();
	void startWatchdog();

	QSharedMemory* m_shared = nullptr;
	vst3::SharedAudio* m_audio = nullptr;
	QLocalServer* m_server = nullptr;
	QLocalSocket* m_socket = nullptr;
	QProcess* m_process = nullptr;
	void* m_request = nullptr;
	void* m_done = nullptr;
	QTimer* m_watchdog = nullptr;
	std::mutex m_transportMutex;
	QString m_key;
	QString m_requestName;
	QString m_doneName;
	double m_sampleRate = 44100.0;
	int m_blockSize = 512;
	QByteArray m_readBuffer;
	std::vector<Parameter> m_parameters;
	std::map<uint32_t, double> m_parameterValues;
	std::vector<QString> m_programNames;
	std::vector<Unit> m_units;
	int m_currentProgram = -1;
	int m_currentUnit = -1;
	int m_latency = 0;
	QString m_name;
	QString m_vendor;
	QString m_version;
	QString m_bundlePath;
	QString m_classId;
	bool m_isInstrument = false;
	bool m_hasEditor = false;
	std::atomic<bool> m_running{false};
	bool m_started = false;
	bool m_stopping = false;
	bool m_autoRestart = true;
	mutable QByteArray m_stateComponent;
	mutable QByteArray m_stateController;
	mutable bool m_stateValid = false;

	mutable std::mutex m_queueMutex;
	std::vector<MidiEvent> m_pendingMidi;
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
