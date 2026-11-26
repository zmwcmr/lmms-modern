/*
 * DelayLine.h - small fixed-size delay line for PDC
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

#ifndef LMMS_DELAY_LINE_H
#define LMMS_DELAY_LINE_H

#include <algorithm>
#include <array>
#include <atomic>
#include <vector>

#include "AudioBufferView.h"
#include "LmmsTypes.h"

namespace lmms
{

/**
 * Fixed-size stereo delay line used by the mixer's latency compensation.
 * The buffer is allocated once; changing the delay does not allocate or free.
 */
class DelayLine
{
public:
	static constexpr f_cnt_t maxFrames = 8192;

	DelayLine()
	{
		for (auto& buffer : m_buffer)
		{
			buffer.assign(maxFrames, 0.0f);
		}
	}

	void setDelayFrames(f_cnt_t delay)
	{
		m_delayFrames.store(
			std::clamp<f_cnt_t>(delay, 0, maxFrames - 1),
			std::memory_order_relaxed);
	}

	f_cnt_t delayFrames() const
	{
		return m_delayFrames.load(std::memory_order_relaxed);
	}

	void process(PlanarBufferView<float> buffers, f_cnt_t frames)
	{
		const f_cnt_t delay = delayFrames();
		if (delay <= 0 || frames <= 0)
		{
			return;
		}

		const auto channels = std::min<ch_cnt_t>(
			static_cast<ch_cnt_t>(buffers.channels()), m_buffer.size());
		for (f_cnt_t frame = 0; frame < frames; ++frame)
		{
			const f_cnt_t writeIndex = (m_writeIndex + frame) % maxFrames;
			const f_cnt_t readIndex = (writeIndex + maxFrames - delay) % maxFrames;
			for (ch_cnt_t channel = 0; channel < channels; ++channel)
			{
				m_buffer[channel][writeIndex] = buffers.sample(channel, frame);
				buffers.sample(channel, frame) = m_buffer[channel][readIndex];
			}
		}
		m_writeIndex = (m_writeIndex + frames) % maxFrames;
	}

private:
	std::atomic<f_cnt_t> m_delayFrames{0};
	std::array<std::vector<float>, 2> m_buffer;
	f_cnt_t m_writeIndex = 0;
};

} // namespace lmms

#endif // LMMS_DELAY_LINE_H
