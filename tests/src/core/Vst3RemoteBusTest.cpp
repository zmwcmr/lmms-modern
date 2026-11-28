/*
 * Vst3RemoteBusTest.cpp - remote VST3 DSP bus layout and sidechain
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

#include <QtTest>

#include <cmath>
#include <vector>

#include "PluginHost.h"
#include "Vst3PluginHost.h"
#include "Vst3RemoteHostClient.h"

using namespace lmms;

class Vst3RemoteBusTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase()
	{
		qputenv("LMMS_VST3_HOST",
			QByteArrayLiteral(LMMS_VST3_HOST_EXECUTABLE));
	}

	void sidechainOverRemoteBus()
	{
		Vst3RemoteHostClient host;
		Vst3PluginHost adapter(host);

		PluginLoadRequest request;
		request.path = QStringLiteral(VST3_FAKE_BUNDLE);
		request.classId = QStringLiteral(VST3_FAKE_EFFECT_CLASS);
		request.sampleRate = 48000.0;
		request.maxBlockSize = 512;

		QString error;
		QVERIFY2(adapter.load(request, &error), qPrintable(error));
		const auto inputs = adapter.inputBuses();
		QCOMPARE(inputs.size(), 2u);
		QCOMPARE(inputs[1].role, PluginBusRole::Sidechain);
		
		constexpr int frames = 64;
		std::vector<float> mainL(frames, 1.0f);
		std::vector<float> mainR(frames, 1.0f);
		std::vector<float> sideL(frames, 0.5f);
		std::vector<float> sideR(frames, 0.5f);
		std::vector<float> outL(frames, 0.0f);
		std::vector<float> outR(frames, 0.0f);
		float* main[2] = { mainL.data(), mainR.data() };
		float* side[2] = { sideL.data(), sideR.data() };
		float* output[2] = { outL.data(), outR.data() };
		PluginBusBlock inputBuses[2] = {
			PluginBusBlock{ main, 2 },
			PluginBusBlock{ side, 2 },
		};
		PluginProcessBlock block;
		block.inputs = main;
		block.outputs = output;
		block.inputChannels = 2;
		block.outputChannels = 2;
		block.frames = frames;
		block.inputBuses = inputBuses;
		block.inputBusCount = 2;

		QVERIFY(adapter.process(block));
		for (int frame = 2; frame < frames; ++frame)
		{
			QCOMPARE(outL[frame], 0.75f);
			QCOMPARE(outR[frame], 0.75f);
		}
		adapter.unload();
	}
};

QTEST_GUILESS_MAIN(Vst3RemoteBusTest)

#include "Vst3RemoteBusTest.moc"
