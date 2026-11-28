/*
 * fake-crash-vst.cpp - minimal VST2 instrument that aborts in processReplacing
 *
 * Used to verify that the VST2 host process crash is contained.
 */

#include <aeffectx.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace
{

AEffect g_effect{};

void processReplacing(AEffect*, float**, float**, int32_t)
{
	std::abort();
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
AEffect* VSTPluginMain(audioMasterCallback)
{
	std::memset(&g_effect, 0, sizeof(g_effect));
	g_effect.magic = kEffectMagic;
	g_effect.dispatcher = dispatcher;
	g_effect.processReplacing = processReplacing;
	g_effect.numPrograms = 0;
	g_effect.numParams = 0;
	g_effect.numInputs = 0;
	g_effect.numOutputs = 2;
	g_effect.flags = effFlagsCanReplacing | effFlagsIsSynth;
	g_effect.uniqueID = 0x43524153; // 'CRAS'
	return &g_effect;
}
