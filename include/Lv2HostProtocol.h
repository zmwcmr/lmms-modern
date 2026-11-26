/*
 * Lv2HostProtocol.h - messages and shared layout for the LV2 host child
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

#ifndef LMMS_LV2_HOST_PROTOCOL_H
#define LMMS_LV2_HOST_PROTOCOL_H

#include <cstddef>

#include "RemotePluginBase.h"

namespace lmms::lv2host
{

/*! Messages that are specific to the LV2 host child.
 *
 *  The transport itself (shmFifo pairs, block handshake, sample rate and
 *  buffer size notifications, IdQuit) is shared with the VST2 remote host and
 *  defined in RemotePluginBase.h. Only the IDs below are new.
 */
enum MessageId : int
{
	//! child -> host: the audio segment is attached and the instance is ready
	IdReady = IdUserBase,
	//! child -> host: instantiation or segment attach failed (followed by text)
	IdFailed,
};

/*! Layout of the stereo planar audio segment.
 *
 *  The host creates a float array with floatCount(frames) elements. The first
 *  two planes hold the input, the next two planes hold the output. LV2 audio
 *  ports are planar as well, so no interleaving happens on either side.
 */
struct AudioSegment
{
	static constexpr int channels = 2;

	static constexpr std::size_t floatCount(std::size_t frames)
	{
		return 2 * channels * frames;
	}

	static constexpr std::size_t inputOffset()
	{
		return 0;
	}

	static constexpr std::size_t outputOffset(std::size_t frames)
	{
		return static_cast<std::size_t>(channels) * frames;
	}
};

} // namespace lmms::lv2host

#endif // LMMS_LV2_HOST_PROTOCOL_H
