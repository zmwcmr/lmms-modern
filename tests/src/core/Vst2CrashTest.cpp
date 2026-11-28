/*
 * Vst2CrashTest.cpp - VST2 child crash containment
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

#include <vector>

#include "Engine.h"
#include "PluginHost.h"
#include "Vst2PluginHost.h"

using namespace lmms;

class Vst2CrashTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase()
	{
		Engine::init(true);
	}

	void cleanupTestCase()
	{
		Engine::destroy();
	}

	void crashIsContained()
	{
		Vst2PluginHost host;

		PluginLoadRequest request;
		request.path = QStringLiteral(LMMS_FAKE_CRASH_VST);
		request.sampleRate = 48000.0;
		request.maxBlockSize = 256;

		QString error;
		QVERIFY2(host.load(request, &error), qPrintable(error));
		QVERIFY(host.isLoaded());

		constexpr int frames = 256;
		std::vector<float> outL(frames, 0.0f);
		std::vector<float> outR(frames, 0.0f);
		float* outputs[2] = { outL.data(), outR.data() };
		PluginProcessBlock block;
		block.inputs = nullptr;
		block.outputs = outputs;
		block.inputChannels = 0;
		block.outputChannels = 2;
		block.frames = frames;

		// The child may report the block done before the abort is observed,
		// so only require that it dies and the host stays alive.
		host.process(block);
		QTest::qWait(200);
		QVERIFY(!host.process(block));
	}
};

QTEST_GUILESS_MAIN(Vst2CrashTest)

#include "Vst2CrashTest.moc"
