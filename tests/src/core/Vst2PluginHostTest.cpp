/*
 * Vst2PluginHostTest.cpp - contract test for the VST2 PluginHost adapter
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

#include "Engine.h"
#include "PluginHost.h"
#include "Vst2PluginHost.h"

using namespace lmms;

class Vst2PluginHostTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase()
	{
		m_pluginPath = qEnvironmentVariable("LMMS_TEST_VST2_PLUGIN");
		if (m_pluginPath.isEmpty())
		{
			QSKIP("set LMMS_TEST_VST2_PLUGIN to a VST2 .so to run this test");
		}
		Engine::init(true);
		m_initialized = true;
	}

	void cleanupTestCase()
	{
		if (m_initialized)
		{
			Engine::destroy();
		}
	}

	void loadProcessAndState()
	{
		Vst2PluginHost host;

		PluginLoadRequest request;
		request.path = m_pluginPath;
		request.sampleRate = 48000.0;
		request.maxBlockSize = 256;
		request.isInstrument = true;

		QString error;
		QVERIFY2(host.load(request, &error), qPrintable(error));
		QVERIFY(host.isLoaded());
		QCOMPARE(host.format(), PluginFormat::Vst2);
		QVERIFY(!host.parameters().empty());

		const uint32_t id = host.parameters().front().id;
		QVERIFY(host.setParameter(id, 0.5));
		QVERIFY(std::abs(host.parameterValue(id) - 0.5) < 0.001);

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
		QVERIFY(host.process(block));

		const auto saved = host.saveState(&error);
		QVERIFY2(!saved.primary.isEmpty(), qPrintable(error));
		QVERIFY2(host.loadState(saved, &error), qPrintable(error));

		host.unload();
		QVERIFY(!host.isLoaded());
	}

private:
	QString m_pluginPath;
	bool m_initialized = false;
};

QTEST_GUILESS_MAIN(Vst2PluginHostTest)

#include "Vst2PluginHostTest.moc"
