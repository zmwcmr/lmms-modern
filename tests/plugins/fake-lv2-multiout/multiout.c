/*
 * multiout.c - LV2 plugin with two stereo output pairs
 */

#include <stdlib.h>

#include <lv2/core/lv2.h>

#define MULTIOUT_URI "http://lmms.io/lv2/fake-multiout"

typedef struct
{
	const float* in[2];
	float* out[4];
} MultiOut;

static LV2_Handle instantiate(const LV2_Descriptor*, double, const char*,
	const LV2_Feature* const*)
{
	return (LV2_Handle)calloc(1, sizeof(MultiOut));
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data)
{
	MultiOut* plugin = (MultiOut*)instance;
	if (port < 2) { plugin->in[port] = (const float*)data; }
	else if (port < 6) { plugin->out[port - 2] = (float*)data; }
}

static void activate(LV2_Handle) {}
static void deactivate(LV2_Handle) {}

static void run(LV2_Handle instance, uint32_t frames)
{
	MultiOut* plugin = (MultiOut*)instance;
	for (uint32_t frame = 0; frame < frames; ++frame)
	{
		const float left = plugin->in[0][frame];
		const float right = plugin->in[1][frame];
		plugin->out[0][frame] = left;
		plugin->out[1][frame] = right;
		plugin->out[2][frame] = 0.5f * left;
		plugin->out[3][frame] = 0.5f * right;
	}
}

static void cleanup(LV2_Handle instance) { free(instance); }
static const void* extension_data(const char*) { return NULL; }

static const LV2_Descriptor descriptor = {
	MULTIOUT_URI, instantiate, connect_port, activate, run, deactivate,
	cleanup, extension_data
};

LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
	return index == 0 ? &descriptor : NULL;
}
