/*
 * midi.c - MIDI echo plugin for the LV2 host tests
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
#include <string.h>

#include <lv2/atom/atom.h>
#include <lv2/core/lv2.h>
#include <lv2/midi/midi.h>
#include <lv2/urid/urid.h>

#define MIDI_URI "http://lmms.io/lv2/fake-midi"

enum
{
	PORT_IN_L = 0,
	PORT_IN_R = 1,
	PORT_OUT_L = 2,
	PORT_OUT_R = 3,
	PORT_MIDI_IN = 4,
	PORT_MIDI_OUT = 5
};

typedef struct
{
	const float* in[2];
	float* out[2];
	LV2_Atom_Sequence* midiIn;
	LV2_Atom_Sequence* midiOut;
	const LV2_URID_Map* map;
	LV2_URID sequenceType;
	LV2_URID midiType;
} MidiEcho;

static LV2_Handle instantiate(const LV2_Descriptor* descriptor, double rate,
	const char* bundlePath, const LV2_Feature* const* features)
{
	(void)descriptor;
	(void)rate;
	(void)bundlePath;

	MidiEcho* plugin = (MidiEcho*)calloc(1, sizeof(MidiEcho));
	if (plugin == NULL)
	{
		return NULL;
	}
	for (int i = 0; features != NULL && features[i] != NULL; ++i)
	{
		if (strcmp(features[i]->URI, LV2_URID__map) == 0)
		{
			plugin->map = (const LV2_URID_Map*)features[i]->data;
		}
	}
	if (plugin->map == NULL)
	{
		free(plugin);
		return NULL;
	}
	plugin->sequenceType =
		plugin->map->map(plugin->map->handle, LV2_ATOM__Sequence);
	plugin->midiType =
		plugin->map->map(plugin->map->handle, LV2_MIDI__MidiEvent);
	return plugin;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data)
{
	MidiEcho* plugin = (MidiEcho*)instance;
	switch (port)
	{
		case PORT_IN_L: plugin->in[0] = (const float*)data; break;
		case PORT_IN_R: plugin->in[1] = (const float*)data; break;
		case PORT_OUT_L: plugin->out[0] = (float*)data; break;
		case PORT_OUT_R: plugin->out[1] = (float*)data; break;
		case PORT_MIDI_IN: plugin->midiIn = (LV2_Atom_Sequence*)data; break;
		case PORT_MIDI_OUT: plugin->midiOut = (LV2_Atom_Sequence*)data; break;
		default: break;
	}
}

static void activate(LV2_Handle instance)
{
	(void)instance;
}

static void deactivate(LV2_Handle instance)
{
	(void)instance;
}

static void run(LV2_Handle instance, uint32_t frames)
{
	MidiEcho* plugin = (MidiEcho*)instance;
	for (int channel = 0; channel < 2; ++channel)
	{
		memcpy(plugin->out[channel], plugin->in[channel],
			frames * sizeof(float));
	}
	if (plugin->midiIn == NULL || plugin->midiOut == NULL)
	{
		return;
	}

	plugin->midiOut->atom.type = plugin->sequenceType;
	plugin->midiOut->atom.size = sizeof(LV2_Atom_Sequence_Body);
	plugin->midiOut->body.unit = 0;
	plugin->midiOut->body.pad = 0;

	const uint8_t* cursor = (const uint8_t*)plugin->midiIn
		+ sizeof(LV2_Atom) + sizeof(LV2_Atom_Sequence_Body);
	const uint8_t* end = cursor
		+ (plugin->midiIn->atom.size - sizeof(LV2_Atom_Sequence_Body));
	uint8_t* out = (uint8_t*)plugin->midiOut
		+ sizeof(LV2_Atom) + sizeof(LV2_Atom_Sequence_Body);

	while (cursor + sizeof(LV2_Atom_Event) <= end)
	{
		const LV2_Atom_Event* event = (const LV2_Atom_Event*)cursor;
		const size_t total = sizeof(LV2_Atom_Event) + event->body.size;
		if (cursor + total > end)
		{
			break;
		}
		if (event->body.type == plugin->midiType)
		{
			memcpy(out, event, total);
			out += total;
			plugin->midiOut->atom.size += (uint32_t)total;
		}
		cursor += total;
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
	MIDI_URI,
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
