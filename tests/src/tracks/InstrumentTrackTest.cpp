/*
 * InstrumentTrackTest.cpp - tests for instrument track serialization
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

#include "Engine.h"
#include "Instrument.h"
#include "InstrumentTrack.h"
#include "Song.h"

class InstrumentTrackTest : public QObject
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

	//! Data of an instrument whose plugin is unavailable must survive saving
	//! the project (#8547).
	void unknownInstrumentDataSurvivesSave()
	{
		using namespace lmms;

		auto song = Engine::getSong();
		QVERIFY(song);

		InstrumentTrack track(song);

		QDomDocument doc;
		QDomElement trackElement = doc.createElement("track");
		trackElement.setAttribute("type", "instrumenttrack");

		QDomElement instrument = doc.createElement("instrument");
		instrument.setAttribute("name", "plugin-that-does-not-exist");

		QDomElement plugin = doc.createElement("plugin-that-does-not-exist");
		plugin.setAttribute("preserved", "yes");
		instrument.appendChild(plugin);
		instrument.appendChild(doc.createElement("key"));

		trackElement.appendChild(instrument);

		track.loadTrackSpecificSettings(trackElement);
		QCOMPARE(track.instrument()->nodeName(), QString("dummyinstrument"));

		// Saving must write back the original instrument element ...
		QDomDocument savedDoc;
		QDomElement savedTrack = savedDoc.createElement("track");
		track.saveTrackSpecificSettings(savedDoc, savedTrack, false);

		QDomElement savedInstrument = savedTrack.firstChildElement("instrument");
		QVERIFY(!savedInstrument.isNull());
		QCOMPARE(savedInstrument.attribute("name"), QString("plugin-that-does-not-exist"));

		QDomElement savedPlugin = savedInstrument.firstChildElement("plugin-that-does-not-exist");
		QVERIFY(!savedPlugin.isNull());
		QCOMPARE(savedPlugin.attribute("preserved"), QString("yes"));

		// ... and loading the saved project again must keep it as well.
		InstrumentTrack reloadedTrack(song);
		reloadedTrack.loadTrackSpecificSettings(savedTrack);

		QDomDocument resavedDoc;
		QDomElement resavedTrack = resavedDoc.createElement("track");
		reloadedTrack.saveTrackSpecificSettings(resavedDoc, resavedTrack, false);

		QDomElement resavedInstrument = resavedTrack.firstChildElement("instrument");
		QVERIFY(!resavedInstrument.isNull());
		QCOMPARE(resavedInstrument.attribute("name"), QString("plugin-that-does-not-exist"));
		QCOMPARE(resavedInstrument.firstChildElement("plugin-that-does-not-exist")
			.attribute("preserved"), QString("yes"));
	}
};

QTEST_MAIN(InstrumentTrackTest)

#include "InstrumentTrackTest.moc"
