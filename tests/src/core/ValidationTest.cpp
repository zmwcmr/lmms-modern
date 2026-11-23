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

#include <QDomDocument>
#include <QtTest>

#include "ControllerConnection.h"
#include "Engine.h"
#include "Mixer.h"
#include "Note.h"
#include "SampleBuffer.h"

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
