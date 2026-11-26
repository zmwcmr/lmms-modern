/*
 * Lv2HostClient.h - client side of the LV2 host child process
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

#ifndef LMMS_LV2_HOST_CLIENT_H
#define LMMS_LV2_HOST_CLIENT_H

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

#include <vector>

#include "LmmsTypes.h"
#include "Lv2HostProtocol.h"
#include "RemotePluginBase.h"
#include "SharedMemory.h"
#include "lmms_export.h"

namespace lmms
{

//! Port metadata reported by the child.
struct Lv2HostPort
{
	int index = 0;
	QString symbol;
	QString name;
	//! "audio", "control", "atom", "midi" or "other"
	QString kind;
	bool isInput = true;
	float defaultValue = 0.0f;
	float minValue = 0.0f;
	float maxValue = 0.0f;
};

/*! Host-side handle for one `lmms-lv2-host` child process.
 *
 *  The class owns the control FIFOs, the shared audio segment and the process
 *  lifecycle. It is deliberately independent of `Lv2Proc` so the transport can
 *  be tested without an audio engine.
 *
 *  Only the scaffold protocol is implemented so far: the child echoes the
 *  input block. Plugin instantiation, ports, atom events and the UI move into
 *  the child in later steps.
 */
class LMMS_EXPORT Lv2HostClient : public QObject, public RemotePluginBase
{
	Q_OBJECT
public:
	explicit Lv2HostClient(QObject* parent = nullptr);
	~Lv2HostClient() override;

	/*! Start the child.
	 *
	 *  @param hostExecutable absolute path, or a file name looked up in the
	 *         plugin directory (`LMMS_PLUGIN_DIR` / `plugins:` search path)
	 *  @param pluginUri      plugin URI, or `--echo` for the scaffold mode
	 *  @param sampleRate     sample rate passed to the child
	 *  @param framesPerBlock block size passed to the child
	 *  @param timeoutMs      how long to wait for the child to become ready
	 */
	bool start(const QString& hostExecutable, const QString& pluginUri,
		sample_rate_t sampleRate, f_cnt_t framesPerBlock, int timeoutMs = 10000);

	//! Ask the child to quit and reap the process.
	void stop();

	bool isRunning() const;
	bool failed() const { return m_failed; }

	//! Human readable reason for the last failure, empty on success.
	QString errorString() const { return m_error; }

	sample_rate_t sampleRate() const { return m_sampleRate; }
	f_cnt_t framesPerBlock() const { return m_framesPerBlock; }

	//! Ports reported by the child, in plugin order.
	const std::vector<Lv2HostPort>& ports() const { return m_ports; }

	//! Send a control-port value to the child. Applied at the next block.
	bool setControlValue(int portIndex, float value);

	//! Queue MIDI events for the next processBlock() call.
	void setMidiEvents(std::vector<lv2host::MidiEventData> events);

	//! MIDI events produced by the last processBlock() call.
	const std::vector<lv2host::MidiEventData>& midiOutput() const { return m_midiOutput; }

	//! Latency reported by the child in frames, 0 when unknown.
	int latency() const { return m_latency; }

	//! Ask the child to create and show its own X11 UI window.
	bool showUi(float scale = 1.0f);
	void hideUi();
	void destroyUi();
	bool isUiOpen() const { return m_uiOpen; }

	//! Process pending messages without waiting for an audio block.
	void poll();

	//! Save the plugin state into @a directory (files written by the child).
	bool saveState(const QString& directory, int timeoutMs = 10000);
	//! Restore the plugin state from @a directory.
	bool restoreState(const QString& directory, int timeoutMs = 10000);
	//! Restore a factory preset by URI.
	bool loadPreset(const QString& presetUri, int timeoutMs = 10000);

	//! Control values reported by the child after a state/preset restore.
	const std::vector<float>& controlValues() const { return m_controlSnapshot; }

	//! Process ID of the child, 0 when not running (used by tests).
	qint64 processId() const { return m_process.processId(); }

	/*! Process one stereo block.
	 *
	 *  @a in and @a out point to lv2host::AudioSegment::channels planar
	 *  buffers of @a frames samples each. A null array is treated as silence.
	 *  Returns false when the child is not usable; the output is then silenced.
	 */
	bool processBlock(const float* const* in, float* const* out, f_cnt_t frames);

	bool processMessage(const message& m) override;

signals:
	//! Emitted when the child died unexpectedly.
	void crashed();
	//! Emitted when the child's UI window was closed or could not be created.
	void uiClosed();

private:
	bool waitForReply(int messageId, int timeoutMs);
	void markFailed(const QString& reason);
	void clearAudioSegment();
	void silenceOutput(float* const* out, f_cnt_t frames) const;

	QProcess m_process;
	SharedMemory<float[]> m_audio;
	std::vector<Lv2HostPort> m_ports;
	std::vector<lv2host::MidiEventData> m_pendingMidi;
	std::vector<lv2host::MidiEventData> m_midiOutput;
	std::vector<float> m_controlSnapshot;
	int m_lastStateResult = -1;
	int m_latency = 0;
	sample_rate_t m_sampleRate = 0;
	f_cnt_t m_framesPerBlock = 0;
	bool m_failed = true;
	bool m_stopping = false;
	bool m_uiOpen = false;
	int m_lastExitCode = 0;
	int m_lastExitStatus = 0;
	QString m_error;
#ifndef SYNC_WITH_SHM_FIFO
	int m_serverSocket = -1;
	QString m_socketFile;
#endif
};

} // namespace lmms

#endif // LMMS_LV2_HOST_CLIENT_H
