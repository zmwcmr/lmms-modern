/*
 * sidechain.c - LV2 plugin with a main and a sidechain stereo input
 *
 * Copyright (c) 2026 LMMS contributors
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#include <stdlib.h>

#include <lv2/core/lv2.h>

#define SIDECHAIN_URI "http://lmms.io/lv2/fake-sidechain"

typedef struct
{
	const float* mainLeft;
	const float* mainRight;
	const float* sideLeft;
	const float* sideRight;
	float* outLeft;
	float* outRight;
} Sidechain;

static LV2_Handle instantiate(const LV2_Descriptor* descriptor, double rate,
	const char* bundlePath, const LV2_Feature* const* features)
{
	(void)descriptor;
	(void)rate;
	(void)bundlePath;
	(void)features;
	return (LV2_Handle)calloc(1, sizeof(Sidechain));
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data)
{
	Sidechain* plugin = (Sidechain*)instance;
	switch (port)
	{
		case 0: plugin->mainLeft = (const float*)data; break;
		case 1: plugin->mainRight = (const float*)data; break;
		case 2: plugin->sideLeft = (const float*)data; break;
		case 3: plugin->sideRight = (const float*)data; break;
		case 4: plugin->outLeft = (float*)data; break;
		case 5: plugin->outRight = (float*)data; break;
	}
}

static void activate(LV2_Handle instance) { (void)instance; }
static void deactivate(LV2_Handle instance) { (void)instance; }

static void run(LV2_Handle instance, uint32_t frames)
{
	Sidechain* plugin = (Sidechain*)instance;
	for (uint32_t i = 0; i < frames; ++i)
	{
		const float side = plugin->sideLeft != NULL
			? 0.5f * (plugin->sideLeft[i] + plugin->sideRight[i]) : 0.0f;
		const float gain = 1.0f + side;
		plugin->outLeft[i] = plugin->mainLeft[i] * gain;
		plugin->outRight[i] = plugin->mainRight[i] * gain;
	}
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
	SIDECHAIN_URI, instantiate, connect_port, activate, run, deactivate,
	cleanup, extension_data
};

LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
	return index == 0 ? &descriptor : NULL;
}
