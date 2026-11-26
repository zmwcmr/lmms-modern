/*
 * Vst3SharedAudio.h - shared memory layout for the DSP host
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
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef LMMS_VST3_SHARED_AUDIO_H
#define LMMS_VST3_SHARED_AUDIO_H

#include <cstdint>

namespace lmms::vst3
{

inline constexpr uint32_t sharedMagic = 0x56334453; // "V3DS"
inline constexpr uint32_t sharedVersion = 1;
inline constexpr int maxBlockSize = 4096;
inline constexpr int maxMidiEvents = 128;
inline constexpr int maxParameterEvents = 64;

struct SharedMidiEvent
{
	int32_t type = 0;
	int32_t channel = 0;
	int32_t data1 = 0;
	int32_t data2 = 0;
	int32_t sampleOffset = 0;
	float value = 0.0f;
};

struct SharedParameterEvent
{
	uint32_t id = 0;
	int32_t sampleOffset = 0;
	double value = 0.0;
};

struct SharedAudio
{
	uint32_t magic = sharedMagic;
	uint32_t version = sharedVersion;
	int32_t nframes = 0;
	int32_t stop = 0;
	double tempo = 120.0;
	int32_t playing = 0;

	int32_t midiCount = 0;
	SharedMidiEvent midi[maxMidiEvents] = {};
	int32_t parameterCount = 0;
	SharedParameterEvent parameters[maxParameterEvents] = {};

	float inputLeft[maxBlockSize] = {};
	float inputRight[maxBlockSize] = {};
	float outputLeft[maxBlockSize] = {};
	float outputRight[maxBlockSize] = {};
};

} // namespace lmms::vst3

#endif // LMMS_VST3_SHARED_AUDIO_H
