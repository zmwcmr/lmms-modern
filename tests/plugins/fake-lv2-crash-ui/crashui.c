/*
 * crashui.c - LV2 plugin whose X11 UI aborts during instantiate
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
#include <string.h>

#include <lv2/core/lv2.h>
#include <lv2/ui/ui.h>

#define CRASH_UI_PLUGIN_URI "http://lmms.io/lv2/fake-crash-ui"
#define CRASH_UI_URI CRASH_UI_PLUGIN_URI "#ui"

typedef struct
{
	const float* in;
	float* out;
} UiCrashPlugin;

static LV2_Handle instantiate(const LV2_Descriptor* descriptor, double rate,
	const char* bundlePath, const LV2_Feature* const* features)
{
	(void)descriptor;
	(void)rate;
	(void)bundlePath;
	(void)features;
	return calloc(1, sizeof(UiCrashPlugin));
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data)
{
	UiCrashPlugin* plugin = (UiCrashPlugin*)instance;
	if (port == 0) { plugin->in = (const float*)data; }
	else if (port == 1) { plugin->out = (float*)data; }
}

static void activate(LV2_Handle instance) { (void)instance; }
static void deactivate(LV2_Handle instance) { (void)instance; }

static void run(LV2_Handle instance, uint32_t frames)
{
	UiCrashPlugin* plugin = (UiCrashPlugin*)instance;
	for (uint32_t i = 0; i < frames; ++i) { plugin->out[i] = plugin->in[i]; }
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
	CRASH_UI_PLUGIN_URI, instantiate, connect_port, activate, run, deactivate,
	cleanup, extension_data
};

LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
	return index == 0 ? &descriptor : NULL;
}

/* --- the crashing UI --------------------------------------------------- */

typedef struct { } UiCrash;

static LV2UI_Handle ui_instantiate(const LV2UI_Descriptor* descriptor,
	const char* pluginUri, const char* bundlePath, LV2UI_Write_Function write,
	LV2UI_Controller controller, LV2UI_Widget* widget,
	const LV2_Feature* const* features)
{
	(void)descriptor;
	(void)pluginUri;
	(void)bundlePath;
	(void)write;
	(void)controller;
	(void)widget;
	(void)features;
	abort();
}

static void ui_cleanup(LV2UI_Handle handle) { (void)handle; }
static void ui_port_event(LV2UI_Handle handle, uint32_t port, uint32_t size,
	uint32_t protocol, const void* buffer)
{
	(void)handle;
	(void)port;
	(void)size;
	(void)protocol;
	(void)buffer;
}

static const void* ui_extension_data(const char* uri)
{
	(void)uri;
	return NULL;
}

static const LV2UI_Descriptor uiDescriptor = {
	CRASH_UI_URI, ui_instantiate, ui_cleanup, ui_port_event, ui_extension_data
};

LV2_SYMBOL_EXPORT const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index)
{
	return index == 0 ? &uiDescriptor : NULL;
}
