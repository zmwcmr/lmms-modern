/*
 * ValidationTest.cpp - regression tests for project-file / model validation
 *
 * Copyright (c) 2026 LMMS Developers
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

#include <limits>

#include <QDomDocument>
#include <QtTest>

#include "AutomatableModel.h"
#include "ControllerConnection.h"
#include "DataFile.h"
#include "Engine.h"
#include "Mixer.h"
#include "Note.h"
#include "SampleBuffer.h"
#include "TimePos.h"

class ValidationTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase()
	{
		lmms::Engine::init(true);
	}

	void cleanupTestCase()
	{
		lmms::Engine::destroy();
	}

	void deeplyNestedProjectIsRejected()
	{
		QByteArray xml("<?xml version=\"1.0\"?>"
			"<lmms-project version=\"1.0\" type=\"song\"><head/><song>"
			"<trackcontainer type=\"song\">");
		for (int i = 0; i < 5000; ++i) { xml += "<x>"; }
		for (int i = 0; i < 5000; ++i) { xml += "</x>"; }
		xml += "</trackcontainer></song></lmms-project>";

		lmms::DataFile file(xml);
		QVERIFY(file.head().isNull());
		QVERIFY(file.documentElement().isNull());
	}

	void nonFiniteModelValuesFallBackToDefault()
	{
		lmms::FloatModel model(0.5f, 0.0f, 1.0f, 0.01f);
		model.setValue(std::numeric_limits<float>::quiet_NaN());
		QCOMPARE(model.value(), 0.5f);
		model.setValue(std::numeric_limits<float>::infinity());
		QCOMPARE(model.value(), 0.5f);
		model.setValue(-std::numeric_limits<float>::infinity());
		QCOMPARE(model.value(), 0.5f);
	}

	void zeroMeterStillYieldsPositiveTicksPerBar()
	{
		const lmms::TimeSig sig(0, 0);
		QVERIFY(lmms::TimePos::ticksPerBar(sig) > 0);
		lmms::TimePos pos(0);
		QVERIFY(pos.ticksPerBeat(sig) > 0);
	}

	void noteKeyIsClampedInConstructor()
	{
		lmms::Note note(lmms::TimePos(1), lmms::TimePos(0), lmms::NumKeys, 100);
		QCOMPARE(note.key(), lmms::NumKeys - 1);
	}

	void noteKeyIsClampedWhenLoaded()
	{
		QDomDocument doc;
		auto element = doc.createElement("note");
		element.setAttribute("key", "1000000");

		lmms::Note note;
		note.restoreState(element);
		QCOMPARE(note.key(), lmms::NumKeys - 1);
	}

	void invalidMixerChannelIsIgnored()
	{
		QDomDocument doc;
		auto mixer = doc.createElement("mixer");
		auto channel = doc.createElement("mixerchannel");
		channel.setAttribute("num", "-1");
		mixer.appendChild(channel);

		lmms::Engine::mixer()->loadSettings(mixer);
		QVERIFY(lmms::Engine::mixer()->numChannels() >= 1);
	}

	void invalidSampleRateFallsBack()
	{
		lmms::SampleBuffer buffer(
			std::vector<lmms::SampleFrame>(1), 0);
		QVERIFY(buffer.sampleRate() > 0);

		lmms::SampleBuffer negativeRate(
			std::vector<lmms::SampleFrame>(1), -44100);
		QVERIFY(negativeRate.sampleRate() > 0);
	}

	void nullControllerIsHandled()
	{
		lmms::ControllerConnection connection(nullptr);
		connection.setController(nullptr);
		QVERIFY(connection.getController() != nullptr);
	}
};

QTEST_GUILESS_MAIN(ValidationTest)
#include "ValidationTest.moc"
