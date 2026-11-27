/*
 * crash.c - LV2 plugin that aborts in run(), for host crash tests
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

#define CRASH_URI "http://lmms.io/lv2/fake-crash-dsp"

typedef struct
{
	const float* in;
	float* out;
	unsigned long runs;
} Crash;

static LV2_Handle instantiate(const LV2_Descriptor* descriptor, double rate,
	const char* bundlePath, const LV2_Feature* const* features)
{
	(void)descriptor;
	(void)rate;
	(void)bundlePath;
	(void)features;
	return (LV2_Handle)calloc(1, sizeof(Crash));
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data)
{
	Crash* crash = (Crash*)instance;
	if (port == 0) { crash->in = (const float*)data; }
	else if (port == 1) { crash->out = (float*)data; }
}

static void activate(LV2_Handle instance) { (void)instance; }
static void deactivate(LV2_Handle instance) { (void)instance; }

static void run(LV2_Handle instance, uint32_t frames)
{
	(void)instance;
	(void)frames;
	abort();
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
	CRASH_URI, instantiate, connect_port, activate, run, deactivate, cleanup,
	extension_data
};

LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
	return index == 0 ? &descriptor : NULL;
}
