/*
 * HostedInstrumentTest.cpp - tests for the generic instrument wrapper
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

#include <QDomDocument>
#include <QDomElement>

#include <vector>

#include "AudioEngine.h"
#include "AutomatableModel.h"
#include "Engine.h"
#include "FakePluginHost.h"
#include "HostedInstrument.h"
#include "MidiEvent.h"
#include "Plugin.h"
#include "SampleFrame.h"

using namespace lmms;

class HostedInstrumentTest : public QObject
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

	void playsAndForwardsMidi()
	{
		static const Plugin::Descriptor descriptor{
			"hostedtest", "Hosted Test", "test", "LMMS", 1,
			Plugin::Type::Instrument, nullptr, ""
		};

		auto host = std::make_unique<FakePluginHost>();
		auto* const hostPtr = host.get();
		HostedInstrument instrument(nullptr, &descriptor, nullptr,
			std::move(host),
			Instrument::Flag::IsSingleStreamed | Instrument::Flag::IsMidiBased);
		instrument.initializeModels();

		const f_cnt_t frames = Engine::audioEngine()->framesPerPeriod();
		std::vector<SampleFrame> buffer(static_cast<std::size_t>(frames));
		instrument.play(buffer.data());

		for (f_cnt_t frame = 0; frame < frames; ++frame)
		{
			QCOMPARE(buffer[static_cast<std::size_t>(frame)][0], 0.5f);
			QCOMPARE(buffer[static_cast<std::size_t>(frame)][1], 0.5f);
		}

		QVERIFY(instrument.handleMidiEvent(
			MidiEvent(MidiNoteOn, 0, 60, 100), TimePos(0), 0));
		QCOMPARE(hostPtr->m_midi.size(), 1u);
		QCOMPARE(hostPtr->m_midi[0].type, PluginMidiEvent::Type::NoteOn);
		QCOMPARE(hostPtr->m_midi[0].data1, 60);
		QVERIFY(std::abs(hostPtr->m_midi[0].value - 100.0f / 127.0f) < 1e-4f);

		QCOMPARE(instrument.latency(), 42);
		QCOMPARE(instrument.nodeName(), QStringLiteral("hostedinstrument"));
	}

	void roundTripsState()
	{
		static const Plugin::Descriptor descriptor{
			"hostedtest", "Hosted Test", "test", "LMMS", 1,
			Plugin::Type::Instrument, nullptr, ""
		};

		auto host = std::make_unique<FakePluginHost>();
		auto* const hostPtr = host.get();
		HostedInstrument instrument(nullptr, &descriptor, nullptr,
			std::move(host));
		instrument.initializeModels();
		instrument.parameterModels().model(0)->setValue(0.25f);

		QDomDocument document;
		QDomElement root = document.createElement(QStringLiteral("root"));
		document.appendChild(root);
		instrument.saveSettings(document, root);
		QVERIFY(root.hasAttribute(QStringLiteral("hoststate")));

		instrument.parameterModels().model(0)->setValue(0.75f);
		hostPtr->m_values[7] = 0.25;
		instrument.loadSettings(root);
		QCOMPARE(instrument.parameterModels().model(0)->value(), 0.25f);
	}
};

QTEST_GUILESS_MAIN(HostedInstrumentTest)

#include "HostedInstrumentTest.moc"
