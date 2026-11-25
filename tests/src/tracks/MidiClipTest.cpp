/*
 * MidiClipTest.cpp - tests for MIDI clip (pattern) serialization and lengths
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
#include "InstrumentTrack.h"
#include "MidiClip.h"
#include "Note.h"
#include "PatternStore.h"
#include "TimePos.h"

class MidiClipTest : public QObject
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

	//! A melody clip in the pattern editor is as long as its notes, and older
	//! projects without a stored length must not be truncated (#8575).
	void melodyClipLengthFollowsNotes()
	{
		using namespace lmms;

		InstrumentTrack track(Engine::patternStore());
		MidiClip clip(&track);

		clip.addNote(Note(TimePos(4, 0)), false);
		QCOMPARE(clip.length(), TimePos(4, 0));

		QDomDocument doc;
		QDomElement clipElement = doc.createElement("midiclip");
		clip.saveSettings(doc, clipElement);
		QCOMPARE(clipElement.attribute("steps").toInt(), 4 * TimePos::stepsPerBar());

		// Projects saved by 1.2 do not store the length, and a melody clip did
		// not need to extend the step count. Simulate such a clip.
		clipElement.removeAttribute("len");
		clipElement.setAttribute("steps", TimePos::stepsPerBar());

		MidiClip loadedClip(&track);
		loadedClip.loadSettings(clipElement);

		QCOMPARE(loadedClip.length(), TimePos(4, 0));

		QDomDocument resavedDoc;
		QDomElement resavedElement = resavedDoc.createElement("midiclip");
		loadedClip.saveSettings(resavedDoc, resavedElement);
		QCOMPARE(resavedElement.attribute("steps").toInt(), 4 * TimePos::stepsPerBar());
	}
};

QTEST_MAIN(MidiClipTest)

#include "MidiClipTest.moc"
