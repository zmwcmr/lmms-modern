/*
 * gain.c - deterministic stereo gain plugin for the LV2 host tests
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

#include <stdlib.h>

#include <lv2/core/lv2.h>

#define GAIN_URI "http://lmms.io/lv2/fake-gain"

enum
{
	PORT_IN_L = 0,
	PORT_IN_R = 1,
	PORT_OUT_L = 2,
	PORT_OUT_R = 3,
	PORT_GAIN = 4
};

typedef struct
{
	const float* in[2];
	float* out[2];
	float* gain;
} Gain;

static LV2_Handle instantiate(const LV2_Descriptor* descriptor, double rate,
	const char* bundlePath, const LV2_Feature* const* features)
{
	(void)descriptor;
	(void)rate;
	(void)bundlePath;
	(void)features;
	return calloc(1, sizeof(Gain));
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data)
{
	Gain* gain = (Gain*)instance;
	switch (port)
	{
		case PORT_IN_L: gain->in[0] = (const float*)data; break;
		case PORT_IN_R: gain->in[1] = (const float*)data; break;
		case PORT_OUT_L: gain->out[0] = (float*)data; break;
		case PORT_OUT_R: gain->out[1] = (float*)data; break;
		case PORT_GAIN: gain->gain = (float*)data; break;
		default: break;
	}
}

static void activate(LV2_Handle instance)
{
	(void)instance;
}

static void run(LV2_Handle instance, uint32_t frames)
{
	Gain* gain = (Gain*)instance;
	const float value = gain->gain != NULL ? *gain->gain : 0.0f;
	for (int channel = 0; channel < 2; ++channel)
	{
		for (uint32_t frame = 0; frame < frames; ++frame)
		{
			gain->out[channel][frame] = gain->in[channel][frame] * value;
		}
	}
}

static void deactivate(LV2_Handle instance)
{
	(void)instance;
}

static void cleanup(LV2_Handle instance)
{
	free(instance);
}

static const void* extension_data(const char* uri)
{
	(void)uri;
	return NULL;
}

static const LV2_Descriptor descriptor = {
	GAIN_URI,
	instantiate,
	connect_port,
	activate,
	run,
	deactivate,
	cleanup,
	extension_data
};

LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
	return index == 0 ? &descriptor : NULL;
}
