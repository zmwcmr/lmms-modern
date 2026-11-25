/*
 * AudioBusHandle.h - ThreadableJob between PlayHandle and MixerChannel
 *
 * Copyright (c) 2005-2014 Tobias Doerffel <tobydox/at/users.sourceforge.net>
 * Copyright (c) 2025 Johannes Lorenz <jlsf2013$users.sourceforge.net, $=@>
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

#ifndef LMMS_AUDIO_BUS_HANDLE_H
#define LMMS_AUDIO_BUS_HANDLE_H

#include <array>
#include <atomic>
#include <memory>
#include <vector>

#include <QString>
#include <QMutex>

#include "AudioBuffer.h"
#include "PlayHandle.h"

namespace lmms
{

class EffectChain;
class FloatModel;
class BoolModel;

/**
	@brief Job between @ref PlayHandle and @ref MixerChannel

	A @ref ThreadableJob class located at the exit point of each @ref PlayHandle into a @ref MixerChannel
	(or into an audio device, in case of AudioJack, but this is not supported in AudioBusHandle yet).
	It contains an optional @ref EffectChain which is e.g. visualized in the
	@ref gui::InstrumentTrackWindow or @ref gui::SampleTrackWindow.
	For processing, it adds all input play handles into an internal buffer,
	processes the @ref EffectChain (if existing) on that buffer
	and finally merges the buffer into its @ref MixerChannel.
*/
class AudioBusHandle : public ThreadableJob
{
public:
	AudioBusHandle(const QString& name, bool hasEffectChain = true,
		FloatModel* volumeModel = nullptr, FloatModel* panningModel = nullptr,
		BoolModel* mutedModel = nullptr);
	virtual ~AudioBusHandle();

	// indicate whether JACK & Co should provide output-buffer at ext. port
	bool extOutputEnabled() const { return m_extOutputEnabled; }
	void setExtOutputEnabled(bool enabled);

	// next mixer-channel after this audio-bus-handle
	// (-1 = none  0 = master)
	mix_ch_t nextMixerChannel() const { return m_nextMixerChannel; }
	void setNextMixerChannel(const mix_ch_t chnl) { m_nextMixerChannel = chnl; }

	const QString& name() const { return m_name; }
	void setName(const QString& newName);

	//! Processing latency of the source and the track effect chain
	f_cnt_t latency() const;
	//! Latency of the instrument feeding this bus, in samples
	void setSourceLatency(f_cnt_t latency) { m_sourceLatency = latency; }
	//! Additional delay needed to align this bus with its mixer channel
	void setDelayFrames(f_cnt_t delay);

	EffectChain* effects() { return m_effects.get(); }
	bool processEffects();

	// ThreadableJob stuff
	void doProcessing() override;
	bool requiresProcessing() const override { return true; }

	void addPlayHandle(PlayHandle* handle);
	void removePlayHandle(PlayHandle* handle);

	//! @returns true if the processing outputted corrupted audio (infs/nans).
	bool isCorrupted() const { return m_corrupted.load(std::memory_order_relaxed); }

private:
	static constexpr f_cnt_t MaxDelayFrames = 8192;

	void applyDelay(f_cnt_t frames);

	volatile bool m_bufferUsage;

	AudioBuffer m_buffer;

	bool m_extOutputEnabled;
	mix_ch_t m_nextMixerChannel;

	QString m_name;

	std::unique_ptr<EffectChain> m_effects;

	PlayHandleList m_playHandles;
	// Snapshot of m_playHandles used during processing. This avoids holding
	// m_playHandleLock while a PlayHandle may add sub-notes to the same list.
	PlayHandleList m_processingPlayHandles;
	QMutex m_playHandleLock;

	FloatModel* m_volumeModel;
	FloatModel* m_panningModel;
	BoolModel* m_mutedModel;
	
	std::atomic<bool> m_corrupted = false;

	std::atomic<f_cnt_t> m_sourceLatency{0};
	std::atomic<f_cnt_t> m_delayFrames{0};
	std::array<std::vector<float>, 2> m_delayBuffer;
	f_cnt_t m_delayWriteIndex = 0;

	friend class AudioEngine;
	friend class AudioEngineWorkerThread;
};

} // namespace lmms

#endif // LMMS_AUDIO_BUS_HANDLE_H
