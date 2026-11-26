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
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

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
	//! child -> host: the instance is ready and the audio segment is attached
	IdReady = IdUserBase,
	//! child -> host: instantiation or segment attach failed (followed by text)
	IdFailed,
	//! child -> host: port metadata. index, symbol, name, kind, input, def, min, max
	IdPortInfo,
	//! host -> child: control value. index, value
	IdControlValue,
	//! child -> host: plugin latency in frames (only sent on change)
	IdLatency,
	//! host -> child: binary list of MIDI events for the next block
	IdMidiEvents,
	//! child -> host: binary list of MIDI events produced by the last block
	IdMidiOut,
};

//! One MIDI event with a block-relative frame offset.
struct MidiEventData
{
	uint32_t frame = 0;
	std::vector<uint8_t> data;
};

/*! Encode a MIDI event list for the message channel.
 *
 *  Layout (host-endian, both sides run on the same machine):
 *  `uint32 count` followed by `uint32 frame`, `uint32 size`, `size` bytes for
 *  every event. The message transport is binary safe, so no base64 is needed.
 */
inline std::string encodeMidiEvents(const std::vector<MidiEventData>& events)
{
	std::string blob;
	blob.reserve(4 + events.size() * 8);
	const auto appendU32 = [&blob](uint32_t value) {
		blob.append(reinterpret_cast<const char*>(&value), sizeof(value));
	};
	appendU32(static_cast<uint32_t>(events.size()));
	for (const auto& event : events)
	{
		appendU32(event.frame);
		appendU32(static_cast<uint32_t>(event.data.size()));
		blob.append(reinterpret_cast<const char*>(event.data.data()), event.data.size());
	}
	return blob;
}

//! Decode a blob produced by encodeMidiEvents(); truncation stops decoding.
inline std::vector<MidiEventData> decodeMidiEvents(const std::string& blob)
{
	std::vector<MidiEventData> events;
	std::size_t offset = 0;
	const auto readU32 = [&blob, &offset](uint32_t& value) {
		if (offset + sizeof(value) > blob.size())
		{
			return false;
		}
		std::memcpy(&value, blob.data() + offset, sizeof(value));
		offset += sizeof(value);
		return true;
	};

	uint32_t count = 0;
	if (!readU32(count))
	{
		return events;
	}
	events.reserve(count);
	for (uint32_t i = 0; i < count; ++i)
	{
		MidiEventData event;
		uint32_t size = 0;
		if (!readU32(event.frame) || !readU32(size) || offset + size > blob.size())
		{
			break;
		}
		event.data.assign(
			reinterpret_cast<const uint8_t*>(blob.data() + offset),
			reinterpret_cast<const uint8_t*>(blob.data() + offset + size));
		offset += size;
		events.push_back(std::move(event));
	}
	return events;
}

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
