/*
 * Vst3PluginHostTest.cpp - tests for the generic VST3 PluginHost adapter
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

#include <cmath>
#include <vector>

#include "PluginHost.h"
#include "Vst3Host.h"
#include "Vst3PluginHost.h"

using namespace lmms;

class Vst3PluginHostTest : public QObject
{
	Q_OBJECT

private slots:
	void loadsProcessesAndAutomates()
	{
		Vst3Host host;
		Vst3PluginHost adapter(host);

		PluginLoadRequest request;
		request.path = QStringLiteral(VST3_FAKE_BUNDLE);
		request.classId = QStringLiteral(VST3_FAKE_CLASS);
		request.sampleRate = 48000.0;
		request.maxBlockSize = 512;
		request.isInstrument = true;

		QString error;
		QVERIFY2(adapter.load(request, &error), qPrintable(error));
		QVERIFY(adapter.isLoaded());
		QCOMPARE(adapter.format(), PluginFormat::Vst3);

		// A VST3 instrument has no audio input bus.
		QVERIFY(adapter.inputBuses().empty());
		const auto outputBuses = adapter.outputBuses();
		QCOMPARE(outputBuses.size(), 1u);
		QCOMPARE(outputBuses[0].channels, 2);
		QCOMPARE(outputBuses[0].role, PluginBusRole::Main);

		const auto& parameters = adapter.parameters();
		QCOMPARE(parameters.size(), 1);
		QCOMPARE(parameters[0].id, 1u);
		QVERIFY(parameters[0].automatable);

		QVERIFY(adapter.setParameter(1, 0.75));
		QVERIFY(std::abs(adapter.parameterValue(1) - 0.75) < 0.01);

		constexpr int frames = 64;
		std::vector<float> outL(frames, 0.0f);
		std::vector<float> outR(frames, 0.0f);
		float* outputs[2] = { outL.data(), outR.data() };

		PluginProcessBlock block;
		block.inputs = nullptr;
		block.outputs = outputs;
		block.inputChannels = 0;
		block.outputChannels = 2;
		block.frames = frames;

		QVERIFY(adapter.process(block));
		bool nonZero = false;
		for (int frame = 0; frame < frames; ++frame)
		{
			QCOMPARE(outL[frame], outR[frame]);
			nonZero = nonZero || std::abs(outL[frame]) > 0.001f;
		}
		QVERIFY(nonZero);
	}

	void reportsEffectBuses()
	{
		Vst3Host host;
		Vst3PluginHost adapter(host);

		PluginLoadRequest request;
		request.path = QStringLiteral(VST3_FAKE_BUNDLE);
		request.classId = QStringLiteral(VST3_FAKE_EFFECT_CLASS);
		request.sampleRate = 48000.0;
		request.maxBlockSize = 512;

		QString error;
		QVERIFY2(adapter.load(request, &error), qPrintable(error));
		const auto inputs = adapter.inputBuses();
		QCOMPARE(inputs.size(), 1u);
		QCOMPARE(inputs[0].channels, 2);
		QCOMPARE(inputs[0].role, PluginBusRole::Main);
		const auto outputs = adapter.outputBuses();
		QCOMPARE(outputs.size(), 1u);
		QCOMPARE(outputs[0].channels, 2);
		QVERIFY(!adapter.capabilities().multipleBuses);
	}

	void stateRoundTrip()
	{
		Vst3Host host;
		Vst3PluginHost adapter(host);

		PluginLoadRequest request;
		request.path = QStringLiteral(VST3_FAKE_BUNDLE);
		request.classId = QStringLiteral(VST3_FAKE_CLASS);
		request.sampleRate = 48000.0;
		request.maxBlockSize = 512;

		QString error;
		QVERIFY2(adapter.load(request, &error), qPrintable(error));
		QVERIFY(adapter.setParameter(1, 0.75));

		const auto saved = adapter.saveState(&error);
		QVERIFY2(!saved.primary.isEmpty(), qPrintable(error));

		QVERIFY(adapter.setParameter(1, 0.25));
		QVERIFY2(adapter.loadState(saved, &error), qPrintable(error));
		QVERIFY(std::abs(adapter.parameterValue(1) - 0.75) < 0.01);
	}

	void editorIsDeferred()
	{
		Vst3Host host;
		Vst3PluginHost adapter(host);

		PluginLoadRequest request;
		request.path = QStringLiteral(VST3_FAKE_BUNDLE);
		request.classId = QStringLiteral(VST3_FAKE_CLASS);
		request.sampleRate = 48000.0;
		request.maxBlockSize = 512;

		QString error;
		QVERIFY2(adapter.load(request, &error), qPrintable(error));
		const auto editor = adapter.createEditor(0, &error);
		QCOMPARE(editor.kind, PluginEditorHandle::Kind::Unavailable);
		QVERIFY(!editor.error.isEmpty());

		adapter.unload();
		QVERIFY(!adapter.isLoaded());
		QVERIFY(adapter.parameters().empty());
	}
};

QTEST_MAIN(Vst3PluginHostTest)

#include "Vst3PluginHostTest.moc"
