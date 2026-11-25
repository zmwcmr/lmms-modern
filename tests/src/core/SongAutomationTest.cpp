/*
 * SongAutomationTest.cpp - tests for automation handling in Song
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

#include <memory>

#include <QtTest>

#include "AutomatableModel.h"
#include "AutomationClip.h"
#include "AutomationTrack.h"
#include "Engine.h"
#include "Song.h"
#include "TimePos.h"

class SongAutomationTest : public QObject
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

	//! Destroying an automated model must not leave a dangling pointer in
	//! Song::m_oldAutomatedValues (#8646).
	void destroyingAutomatedModelLeavesNoStalePointer()
	{
		using namespace lmms;

		auto song = Engine::getSong();
		QVERIFY(song);

		auto model = std::make_unique<FloatModel>(0.0f, 0.0f, 1.0f, 0.01f);

		{
			AutomationTrack track(song);
			AutomationClip clip(&track);
			clip.putValue(TimePos(0), 0.0f, false);
			clip.putValue(TimePos(4, 0), 1.0f, false);
			clip.movePosition(TimePos(0));
			clip.addObject(model.get());

			// Let the automation system cache the model for the next buffer.
			song->playSong();
			song->processNextBuffer();
			QVERIFY(song->automatedValuesAt(TimePos(0)).contains(model.get()));
		}
		// The clip is gone now, so it cannot unregister the model from Song
		// when it is destroyed below.

		// Deleting a model (e.g. by removing a mixer send) used to leave a
		// stale pointer behind. The next buffer then dereferenced freed memory.
		model.reset();
		song->processNextBuffer();

		// Also make sure stopping works after the model is gone.
		song->stop();
	}
};

QTEST_MAIN(SongAutomationTest)

#include "SongAutomationTest.moc"
