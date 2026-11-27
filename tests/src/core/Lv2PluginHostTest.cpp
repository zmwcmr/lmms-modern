/*
 * Lv2PluginHostTest.cpp - tests for the generic LV2 PluginHost adapter
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

#include <QtTest>

#include <QFileInfo>

#include <vector>

#include "AudioEngine.h"
#include "Engine.h"
#include "Lv2PluginHost.h"
#include "Model.h"
#include "PluginHost.h"

using namespace lmms;

class Lv2PluginHostTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase()
	{
		qputenv("LV2_PATH",
			QFileInfo(QString::fromUtf8(LMMS_FAKE_LV2_GAIN_BUNDLE))
				.absolutePath().toUtf8());
		// The default run is deterministic and in-process. The remote
		// variant sets LMMS_LV2_TEST_REMOTE=1 and exercises the child host.
		if (qEnvironmentVariableIsEmpty("LMMS_LV2_TEST_REMOTE"))
		{
			qputenv("LMMS_LV2_INPROCESS", "1");
		}
		else
		{
			qunsetenv("LMMS_LV2_INPROCESS");
			qputenv("LMMS_LV2_REMOTE", "1");
		}
		Engine::init(true);
	}

	void cleanupTestCase()
	{
		Engine::destroy();
	}

	void loadsAndProcesses()
	{
		Model parent(nullptr);
		Lv2PluginHost host(&parent);

		PluginLoadRequest request;
		request.pluginUri = QStringLiteral("http://lmms.io/lv2/fake-gain");
		request.sampleRate = 44100.0;
		request.maxBlockSize = 256;

		QString error;
		QVERIFY2(host.load(request, &error), qPrintable(error));
		QVERIFY(host.isLoaded());
		QCOMPARE(host.format(), PluginFormat::Lv2);

		const auto& parameters = host.parameters();
		QCOMPARE(parameters.size(), 1);
		QCOMPARE(parameters[0].id, 4u);
		QCOMPARE(parameters[0].name, QStringLiteral("Gain"));
		QCOMPARE(parameters[0].min, 0.0);
		QCOMPARE(parameters[0].max, 1.0);
		QCOMPARE(parameters[0].defaultValue, 0.5);
		QCOMPARE(host.parameterValue(4), 0.5);

		const auto inputBuses = host.inputBuses();
		QCOMPARE(inputBuses.size(), 1u);
		QCOMPARE(inputBuses[0].channels, 2);
		QCOMPARE(inputBuses[0].role, PluginBusRole::Main);
		QVERIFY(inputBuses[0].isInput);
		const auto outputBuses = host.outputBuses();
		QCOMPARE(outputBuses.size(), 1u);
		QCOMPARE(outputBuses[0].channels, 2);
		QVERIFY(!outputBuses[0].isInput);

		QVERIFY(host.setParameter(4, 0.25));
		QCOMPARE(host.parameterValue(4), 0.25);

		// Lv2Proc allocates its port buffers for the engine block size, and
		// the isolated host only accepts that block size.
		const int frames = Engine::audioEngine()->framesPerPeriod();
		std::vector<float> inL(frames, 1.0f);
		std::vector<float> inR(frames, 1.0f);
		std::vector<float> outL(frames, 0.0f);
		std::vector<float> outR(frames, 0.0f);
		float* inputs[2] = { inL.data(), inR.data() };
		float* outputs[2] = { outL.data(), outR.data() };

		PluginProcessBlock block;
		block.inputs = inputs;
		block.outputs = outputs;
		block.inputChannels = 2;
		block.outputChannels = 2;
		block.frames = frames;

		QVERIFY(host.process(block));
		for (int frame = 0; frame < frames; ++frame)
		{
			QCOMPARE(outL[frame], 0.25f);
			QCOMPARE(outR[frame], 0.25f);
		}
	}

	void reportsSidechainBuses()
	{
		Model parent(nullptr);
		Lv2PluginHost host(&parent);

		PluginLoadRequest request;
		request.pluginUri = QStringLiteral(
			"http://lmms.io/lv2/fake-sidechain");
		request.sampleRate = 44100.0;
		request.maxBlockSize = 256;

		QString error;
		QVERIFY2(host.load(request, &error), qPrintable(error));

		const auto inputs = host.inputBuses();
		QCOMPARE(inputs.size(), 2u);
		QCOMPARE(inputs[0].channels, 2);
		QCOMPARE(inputs[0].role, PluginBusRole::Main);
		QVERIFY(inputs[0].isInput);
		QCOMPARE(inputs[1].channels, 2);
		QCOMPARE(inputs[1].role, PluginBusRole::Sidechain);
		QVERIFY(inputs[1].isInput);

		const auto outputs = host.outputBuses();
		QCOMPARE(outputs.size(), 1u);
		QCOMPARE(outputs[0].channels, 2);
		QCOMPARE(outputs[0].role, PluginBusRole::Main);
		QVERIFY(!outputs[0].isInput);
	}

	void stateRoundTrip()
	{
		Model parent(nullptr);
		Lv2PluginHost host(&parent);

		PluginLoadRequest request;
		request.pluginUri = QStringLiteral("http://lmms.io/lv2/fake-gain");
		request.sampleRate = 44100.0;
		request.maxBlockSize = 256;

		QString error;
		QVERIFY2(host.load(request, &error), qPrintable(error));
		QVERIFY(host.setParameter(4, 0.25));

		const auto saved = host.saveState(&error);
		QVERIFY2(!saved.primary.isEmpty(), qPrintable(error));

		QVERIFY(host.setParameter(4, 0.75));
		QCOMPARE(host.parameterValue(4), 0.75);

		QVERIFY2(host.loadState(saved, &error), qPrintable(error));
		QCOMPARE(host.parameterValue(4), 0.25);
	}

	void programsAndEditor()
	{
		Model parent(nullptr);
		Lv2PluginHost host(&parent);

		PluginLoadRequest request;
		request.pluginUri = QStringLiteral("http://lmms.io/lv2/fake-gain");

		QString error;
		QVERIFY2(host.load(request, &error), qPrintable(error));
		QCOMPARE(host.programCount(), 0);
		QVERIFY(!host.setProgram(0));

		const auto editor = host.createEditor(0, &error);
		QCOMPARE(editor.kind, PluginEditorHandle::Kind::Unavailable);
		QVERIFY(!editor.error.isEmpty());

		host.unload();
		QVERIFY(!host.isLoaded());
		QVERIFY(host.parameters().empty());
	}

	void rejectsUnknownPlugin()
	{
		Model parent(nullptr);
		Lv2PluginHost host(&parent);

		PluginLoadRequest request;
		request.pluginUri = QStringLiteral("http://lmms.io/lv2/does-not-exist");

		QString error;
		QVERIFY(!host.load(request, &error));
		QVERIFY(!error.isEmpty());
		QVERIFY(!host.isLoaded());
	}
};

QTEST_GUILESS_MAIN(Lv2PluginHostTest)

#include "Lv2PluginHostTest.moc"
