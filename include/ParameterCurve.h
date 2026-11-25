/*
 * ParameterCurve.h - compress per-frame parameter automation into points
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

#ifndef LMMS_PARAMETER_CURVE_H
#define LMMS_PARAMETER_CURVE_H

#include <algorithm>
#include <cmath>
#include <vector>

namespace lmms
{

//! A parameter value that takes effect at a frame within one audio block.
struct ParameterEvent
{
	int frame;
	float value;
};

/**
 * Compress a per-frame parameter curve into a bounded list of points.
 *
 * A point is emitted at frame 0, whenever the value changes by at least
 * (maxValue - minValue) / 4096, and at the last frame when it differs from
 * the previous point. Points closer than minSpacing frames are coalesced.
 * If the result exceeds maxEvents, the endpoints are kept and the remaining
 * points are spread evenly over the curve.
 */
inline std::vector<ParameterEvent> compressParameterCurve(
	const float* values, int length, float minValue, float maxValue,
	int maxEvents = 32, int minSpacing = 4)
{
	std::vector<ParameterEvent> events;
	if (values == nullptr || length <= 0 || maxEvents <= 0)
	{
		return events;
	}

	minSpacing = std::max(1, minSpacing);
	const float range = std::max(0.0f, maxValue - minValue);
	const float threshold = std::max(range / 4096.0f, 1.0e-6f);

	events.push_back({0, values[0]});
	int lastFrame = 0;
	float lastValue = values[0];

	for (int frame = 1; frame < length; ++frame)
	{
		if (frame - lastFrame < minSpacing)
		{
			continue;
		}
		const float value = values[frame];
		if (std::abs(value - lastValue) < threshold)
		{
			continue;
		}
		events.push_back({frame, value});
		lastFrame = frame;
		lastValue = value;
	}

	// Keep the final value so the next block starts from the right state.
	if (lastFrame != length - 1 && values[length - 1] != lastValue)
	{
		events.push_back({length - 1, values[length - 1]});
	}

	if (static_cast<int>(events.size()) <= maxEvents)
	{
		return events;
	}

	if (maxEvents == 1)
	{
		return {events.front()};
	}

	// Decimate while keeping the endpoints. This bounds IPC traffic and the
	// number of sub-blocks per period.
	std::vector<ParameterEvent> reduced;
	reduced.reserve(maxEvents);
	reduced.push_back(events.front());
	for (int i = 1; i < maxEvents - 1; ++i)
	{
		const auto index = static_cast<std::size_t>(i)
			* (events.size() - 1) / static_cast<std::size_t>(maxEvents - 1);
		if (events[index].frame != reduced.back().frame)
		{
			reduced.push_back(events[index]);
		}
	}
	if (events.back().frame != reduced.back().frame)
	{
		reduced.push_back(events.back());
	}
	return reduced;
}

} // namespace lmms

#endif // LMMS_PARAMETER_CURVE_H
