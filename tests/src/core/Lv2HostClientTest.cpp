/*
 * Lv2HostClientTest.cpp - tests for the LV2 host child transport
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

#include <QElapsedTimer>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>

#include <lilv/lilv.h>

#include <cmath>
#include <memory>
#include <vector>

#include "Lv2HostClient.h"
#include "Lv2HostProtocol.h"
#include "Lv2UridMap.h"

using namespace lmms;

namespace
{

constexpr f_cnt_t BlockSize = 256;
constexpr const char* GainUri = "http://lmms.io/lv2/fake-gain";

void fillRamp(std::vector<float>& buffer, float scale)
{
	for (std::size_t i = 0; i < buffer.size(); ++i)
	{
		buffer[i] = scale * static_cast<float>(i) / static_cast<float>(buffer.size());
	}
}

/*! In-process reference instance of the fake gain plugin. */
struct Reference
{
	LilvWorld* world = nullptr;
	LilvInstance* instance = nullptr;
	std::unique_ptr<UridMap> m_urids;
	float gain = 0.5f;

	~Reference()
	{
		if (instance != nullptr) { lilv_instance_free(instance); }
		if (world != nullptr) { lilv_world_free(world); }
	}

	bool create()
	{
		world = lilv_world_new();
		lilv_world_load_all(world);
		LilvNode* uri = lilv_new_uri(world, GainUri);
		const LilvPlugins* plugins = lilv_world_get_all_plugins(world);
		const LilvPlugin* plugin = nullptr;
		// lilv_plugins_get_by_uri crashes on an empty plugin set in lilv 0.28.
		LILV_FOREACH(plugins, it, plugins)
		{
			const LilvPlugin* candidate = lilv_plugins_get(plugins, it);
			if (lilv_node_equals(lilv_plugin_get_uri(candidate), uri))
			{
				plugin = candidate;
				break;
			}
		}
		lilv_node_free(uri);
		if (plugin == nullptr) { return false; }
		m_urids = std::make_unique<UridMap>();
		LV2_Feature mapFeature{ LV2_URID__map, m_urids->mapFeature() };
		LV2_Feature unmapFeature{ LV2_URID__unmap, m_urids->unmapFeature() };
		const LV2_Feature* features[] = { &mapFeature, &unmapFeature, nullptr };
		instance = lilv_plugin_instantiate(plugin, 44100, features);
		return instance != nullptr;
	}
};

} // namespace

class Lv2HostClientTest : public QObject
{
	Q_OBJECT

private slots:
	void echoesInputBlock();
	void survivesChildKill();
	void matchesInProcessGain();
	void forwardsMidiEvents();
	void stateRoundTrip();
};




void Lv2HostClientTest::echoesInputBlock()
{
	Lv2HostClient client;
	QVERIFY2(client.start(QStringLiteral(LMMS_LV2_HOST_EXECUTABLE),
		QStringLiteral("--echo"), 44100, BlockSize, 15000),
		qPrintable(client.errorString()));
	QVERIFY(client.isRunning());
	QCOMPARE(client.framesPerBlock(), BlockSize);

	std::vector<float> inLeft(BlockSize);
	std::vector<float> inRight(BlockSize);
	std::vector<float> outLeft(BlockSize, -1.0f);
	std::vector<float> outRight(BlockSize, -1.0f);
	fillRamp(inLeft, 1.0f);
	fillRamp(inRight, -2.0f);

	const float* in[2] = { inLeft.data(), inRight.data() };
	float* out[2] = { outLeft.data(), outRight.data() };

	QVERIFY(client.processBlock(in, out, BlockSize));

	for (f_cnt_t i = 0; i < BlockSize; ++i)
	{
		QCOMPARE(outLeft[i], inLeft[i]);
		QCOMPARE(outRight[i], inRight[i]);
	}

	client.stop();
	QVERIFY(!client.isRunning());
}




void Lv2HostClientTest::survivesChildKill()
{
#ifndef Q_OS_UNIX
	QSKIP("The crash test needs POSIX signals");
#endif
	Lv2HostClient client;
	QVERIFY2(client.start(QStringLiteral(LMMS_LV2_HOST_EXECUTABLE),
		QStringLiteral("--echo"), 44100, BlockSize, 15000),
		qPrintable(client.errorString()));

	const qint64 pid = client.processId();
	QVERIFY(pid > 0);
	QCOMPARE(QProcess::execute(QStringLiteral("kill"),
		{ QStringLiteral("-9"), QString::number(pid) }), 0);

	// The block must fail quickly instead of hanging on a dead child.
	std::vector<float> buffer(BlockSize);
	const float* in[2] = { buffer.data(), buffer.data() };
	std::vector<float> outLeft(BlockSize, 0.5f);
	std::vector<float> outRight(BlockSize, 0.5f);
	float* out[2] = { outLeft.data(), outRight.data() };

	QElapsedTimer timer;
	timer.start();
	QVERIFY(!client.processBlock(in, out, BlockSize));
	QVERIFY2(timer.elapsed() < 6000, "processBlock() blocked on a dead child");

	// A failed block must not leak the previous content.
	QCOMPARE(outLeft[0], 0.0f);
	QCOMPARE(outRight[BlockSize - 1], 0.0f);

	QTRY_VERIFY_WITH_TIMEOUT(!client.isRunning(), 5000);
	QVERIFY(!client.errorString().isEmpty());
	client.stop();
}


void Lv2HostClientTest::matchesInProcessGain()
{
	qputenv("LV2_PATH",
		QFileInfo(QString::fromUtf8(LMMS_FAKE_LV2_GAIN_BUNDLE))
			.absolutePath().toUtf8());

	Reference reference;
	QVERIFY2(reference.create(), "could not load the fake gain plugin in-process");

	std::vector<float> inLeft(BlockSize);
	std::vector<float> inRight(BlockSize);
	fillRamp(inLeft, 1.0f);
	fillRamp(inRight, -0.5f);

	std::vector<float> refLeft(BlockSize, 0.0f);
	std::vector<float> refRight(BlockSize, 0.0f);
	lilv_instance_connect_port(reference.instance, 0, inLeft.data());
	lilv_instance_connect_port(reference.instance, 1, inRight.data());
	lilv_instance_connect_port(reference.instance, 2, refLeft.data());
	lilv_instance_connect_port(reference.instance, 3, refRight.data());
	lilv_instance_connect_port(reference.instance, 4, &reference.gain);
	lilv_instance_activate(reference.instance);
	lilv_instance_run(reference.instance, BlockSize);

	Lv2HostClient client;
	QVERIFY2(client.start(QStringLiteral(LMMS_LV2_HOST_EXECUTABLE),
		QString::fromUtf8(GainUri), 44100, BlockSize, 15000),
		qPrintable(client.errorString()));

	// The child reports the five ports of the fake plugin.
	const auto& ports = client.ports();
	QCOMPARE(ports.size(), std::size_t{5});
	int gainPort = -1;
	for (const auto& port : ports)
	{
		if (port.symbol == QStringLiteral("gain"))
		{
			gainPort = port.index;
			QCOMPARE(port.kind, QStringLiteral("control"));
			QVERIFY(port.isInput);
			QCOMPARE(port.defaultValue, 0.5f);
			QCOMPARE(port.minValue, 0.0f);
			QCOMPARE(port.maxValue, 1.0f);
		}
	}
	QVERIFY2(gainPort >= 0, "the gain control port was not reported");

	std::vector<float> outLeft(BlockSize);
	std::vector<float> outRight(BlockSize);
	const float* in[2] = { inLeft.data(), inRight.data() };
	float* out[2] = { outLeft.data(), outRight.data() };

	QVERIFY(client.processBlock(in, out, BlockSize));
	for (f_cnt_t i = 0; i < BlockSize; ++i)
	{
		QVERIFY(std::abs(outLeft[i] - refLeft[i]) < 1e-6f);
		QVERIFY(std::abs(outRight[i] - refRight[i]) < 1e-6f);
	}

	// A control change must reach the plugin at the next block.
	QVERIFY(client.setControlValue(gainPort, 0.25f));
	reference.gain = 0.25f;
	lilv_instance_run(reference.instance, BlockSize);
	QVERIFY(client.processBlock(in, out, BlockSize));
	for (f_cnt_t i = 0; i < BlockSize; ++i)
	{
		QVERIFY(std::abs(outLeft[i] - refLeft[i]) < 1e-6f);
		QVERIFY(std::abs(outRight[i] - refRight[i]) < 1e-6f);
	}

	client.stop();
}


void Lv2HostClientTest::forwardsMidiEvents()
{
	qputenv("LV2_PATH",
		QFileInfo(QString::fromUtf8(LMMS_FAKE_LV2_MIDI_BUNDLE))
			.absolutePath().toUtf8());

	Lv2HostClient client;
	QVERIFY2(client.start(QStringLiteral(LMMS_LV2_HOST_EXECUTABLE),
		QStringLiteral("http://lmms.io/lv2/fake-midi"), 44100, BlockSize, 15000),
		qPrintable(client.errorString()));
	QCOMPARE(client.ports().size(), std::size_t{6});

	client.setMidiEvents({
		lv2host::MidiEventData{32, {0x90, 60, 100}},
		lv2host::MidiEventData{192, {0x80, 60, 0}},
	});

	std::vector<float> buffer(BlockSize, 0.0f);
	const float* in[2] = { buffer.data(), buffer.data() };
	float* out[2] = { buffer.data(), buffer.data() };
	QVERIFY(client.processBlock(in, out, BlockSize));

	const auto& output = client.midiOutput();
	QCOMPARE(output.size(), std::size_t{2});
	QCOMPARE(output[0].frame, 32u);
	QCOMPARE(output[1].frame, 192u);
	QVERIFY(output[0].data == std::vector<uint8_t>({ 0x90, 60, 100 }));
	QVERIFY(output[1].data == std::vector<uint8_t>({ 0x80, 60, 0 }));

	// Events belong to one block; the next block must not repeat them.
	client.setMidiEvents({});
	QVERIFY(client.processBlock(in, out, BlockSize));
	QVERIFY(client.midiOutput().empty());

	client.stop();
}


void Lv2HostClientTest::stateRoundTrip()
{
	qputenv("LV2_PATH",
		QFileInfo(QString::fromUtf8(LMMS_FAKE_LV2_GAIN_BUNDLE))
			.absolutePath().toUtf8());

	QTemporaryDir stateDir;
	QVERIFY(stateDir.isValid());

	{
		Lv2HostClient client;
		QVERIFY2(client.start(QStringLiteral(LMMS_LV2_HOST_EXECUTABLE),
			QString::fromUtf8(GainUri), 44100, BlockSize, 15000),
			qPrintable(client.errorString()));

		int gainPort = -1;
		for (const auto& port : client.ports())
		{
			if (port.symbol == QStringLiteral("gain")) { gainPort = port.index; }
		}
		QVERIFY(gainPort >= 0);
		QVERIFY(client.setControlValue(gainPort, 0.25f));

		std::vector<float> buffer(BlockSize, 0.0f);
		const float* in[2] = { buffer.data(), buffer.data() };
		float* out[2] = { buffer.data(), buffer.data() };
		QVERIFY(client.processBlock(in, out, BlockSize));

		QVERIFY2(client.saveState(stateDir.path()),
			"could not save the plugin state in the child");
	}

	{
		Lv2HostClient client;
		QVERIFY2(client.start(QStringLiteral(LMMS_LV2_HOST_EXECUTABLE),
			QString::fromUtf8(GainUri), 44100, BlockSize, 15000),
			qPrintable(client.errorString()));

		QVERIFY2(client.restoreState(stateDir.path()),
			"could not restore the plugin state in the child");

		int gainPort = -1;
		for (const auto& port : client.ports())
		{
			if (port.symbol == QStringLiteral("gain")) { gainPort = port.index; }
		}
		QVERIFY(gainPort >= 0);
		QVERIFY(client.controlValues().size() > static_cast<std::size_t>(gainPort));
		QCOMPARE(client.controlValues()[gainPort], 0.25f);

		std::vector<float> in(BlockSize);
		std::vector<float> out(BlockSize, 0.0f);
		fillRamp(in, 1.0f);
		const float* inPtr[2] = { in.data(), in.data() };
		float* outPtr[2] = { out.data(), out.data() };
		QVERIFY(client.processBlock(inPtr, outPtr, BlockSize));
		for (f_cnt_t i = 0; i < BlockSize; ++i)
		{
			QVERIFY(std::abs(out[i] - in[i] * 0.25f) < 1e-6f);
		}
	}
}


QTEST_GUILESS_MAIN(Lv2HostClientTest)
#include "Lv2HostClientTest.moc"
