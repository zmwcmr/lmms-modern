/*
 * fake-midi-vst.cpp - minimal VST2 instrument for host integration tests
 *
 * Emits a fixed note-on/note-off sequence from processReplacing(). The
 * plugin has no editor and no parameters; it exists to exercise the host's
 * MIDI output and routing paths.
 */

#include <aeffectx.h>

#include <cstdint>
#include <cstring>

namespace
{

audioMasterCallback g_host = nullptr;
AEffect g_effect{};
VstTimeInfo g_timeInfo{};
int g_blocks = 0;

intptr_t VST_CALL_CONV hostCallback(AEffect*, int32_t opcode, int32_t,
	intptr_t, void* ptr, float)
{
	switch (opcode)
	{
	case audioMasterVersion:
		return 2400;
	case audioMasterGetSampleRate:
		return 44100;
	case audioMasterGetBlockSize:
		return 512;
	case audioMasterGetCurrentProcessLevel:
		return 2;
	case audioMasterGetVendorString:
		if (ptr) { std::strcpy(static_cast<char*>(ptr), "LMMS tests"); }
		return 1;
	case audioMasterGetProductString:
		if (ptr) { std::strcpy(static_cast<char*>(ptr), "Fake MIDI VST"); }
		return 1;
	case audioMasterGetVendorVersion:
		return 1;
	case audioMasterCanDo:
		return 1;
	case audioMasterGetTime:
		g_timeInfo.sampleRate = 44100.0;
		g_timeInfo.tempo = 120.0;
		g_timeInfo.timeSigNumerator = 4;
		g_timeInfo.timeSigDenominator = 4;
		g_timeInfo.flags = kVstTempoValid | kVstTimeSigValid;
		return reinterpret_cast<intptr_t>(&g_timeInfo);
	default:
		return 0;
	}
}

void processReplacing(AEffect*, float** /*inputs*/, float** outputs, int32_t frames)
{
	if (outputs)
	{
		for (int ch = 0; ch < 2; ++ch)
		{
			if (outputs[ch])
			{
				std::memset(outputs[ch], 0, sizeof(float) * frames);
			}
		}
	}

	++g_blocks;
	if (g_blocks != 2 && g_blocks != 20)
	{
		return;
	}

	VstMidiEvent event{};
	event.type = kVstMidiType;
	event.byteSize = sizeof(VstMidiEvent);
	event.deltaFrames = 0;
	event.midiData[0] = static_cast<char>(g_blocks == 2 ? 0x90 : 0x80);
	event.midiData[1] = 60;
	event.midiData[2] = 100;
	event.midiData[3] = 0;

	VstEvents events{};
	events.numEvents = 1;
	events.events[0] = reinterpret_cast<VstEvent*>(&event);
	if (g_host)
	{
		g_host(&g_effect, audioMasterProcessEvents, 0, 0, &events, 0.0f);
	}
}

intptr_t VST_CALL_CONV dispatcher(AEffect*, int32_t opcode, int32_t,
	intptr_t, void*, float)
{
	switch (opcode)
	{
	case effOpen:
	case effSetSampleRate:
	case effSetBlockSize:
	case effMainsChanged:
	case effEditClose:
		return 0;
	case effGetVstVersion:
		return 2400;
	default:
		return 0;
	}
}

} // namespace

extern "C" __attribute__((visibility("default")))
AEffect* VSTPluginMain(audioMasterCallback host)
{
	g_host = host;
	g_blocks = 0;
	std::memset(&g_effect, 0, sizeof(g_effect));
	g_effect.magic = kEffectMagic;
	g_effect.dispatcher = dispatcher;
	g_effect.processReplacing = processReplacing;
	g_effect.numPrograms = 0;
	g_effect.numParams = 0;
	g_effect.numInputs = 0;
	g_effect.numOutputs = 2;
	g_effect.flags = effFlagsCanReplacing | effFlagsIsSynth;
	g_effect.uniqueID = 0x4D494449; // 'MIDI'
	return &g_effect;
}
