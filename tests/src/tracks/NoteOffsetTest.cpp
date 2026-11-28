/*
 * NoteOffsetTest.cpp - guard rails for note start offsets
 *
 * Copyright (c) 2026 LMMS developers
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

#include "AudioEngine.h"
#include "Engine.h"
#include "InstrumentTrack.h"
#include "Note.h"
#include "NotePlayHandle.h"
#include "Song.h"
#include "TimePos.h"

using namespace lmms;

class NoteOffsetTest : public QObject
{
	Q_OBJECT

private:
	InstrumentTrack* m_track = nullptr;

private slots:
	void initTestCase()
	{
		Engine::init(true);
		// The dummy device otherwise processes notes concurrently with the
		// assertions below.
		Engine::audioEngine()->audioDev()->stopProcessing();
		NotePlayHandleManager::init();
		m_track = new InstrumentTrack(Engine::getSong());
	}

	void cleanupTestCase()
	{
		delete m_track;
		m_track = nullptr;
		Engine::destroy();
	}

	void framesForFirstPeriodRespectOffset()
	{
		const auto framesPerPeriod = Engine::audioEngine()->framesPerPeriod();
		const auto offset = framesPerPeriod / 4;
		auto* handle = NotePlayHandleManager::acquire(
			m_track, offset, 1000000, Note(TimePos(192), TimePos(0), 60, 100, 0));
		QVERIFY(handle != nullptr);
		QCOMPARE(handle->framesLeftForCurrentPeriod(), framesPerPeriod - offset);
		NotePlayHandleManager::release(handle);
	}

	void framesAreBoundedByNoteLength()
	{
		auto* handle = NotePlayHandleManager::acquire(
			m_track, 0, 5, Note(TimePos(192), TimePos(0), 60, 100, 0));
		QVERIFY(handle != nullptr);
		QCOMPARE(handle->framesLeftForCurrentPeriod(), f_cnt_t(5));
		NotePlayHandleManager::release(handle);
	}

	void offsetAtPeriodBoundaryProducesNoFrames()
	{
		const auto framesPerPeriod = Engine::audioEngine()->framesPerPeriod();
		auto* handle = NotePlayHandleManager::acquire(
			m_track, framesPerPeriod, 1000000,
			Note(TimePos(192), TimePos(0), 60, 100, 0));
		QVERIFY(handle != nullptr);
		QCOMPARE(handle->framesLeftForCurrentPeriod(), f_cnt_t(0));
		NotePlayHandleManager::release(handle);
	}

	void oversizedOffsetIsAdvancedWithoutPlayback()
	{
		const auto framesPerPeriod = Engine::audioEngine()->framesPerPeriod();
		const auto offset = framesPerPeriod + 7;
		auto* handle = NotePlayHandleManager::acquire(
			m_track, offset, 1000, Note(TimePos(192), TimePos(0), 60, 100, 0));
		QVERIFY(handle != nullptr);

		// The first call must only carry the offset into the next period;
		// it must not render into the (null) working buffer.
		handle->play(nullptr);
		QCOMPARE(handle->offset(), f_cnt_t(7));
		QCOMPARE(handle->totalFramesPlayed(), f_cnt_t(0));
		QVERIFY(handle->framesLeftForCurrentPeriod() <=
			Engine::audioEngine()->framesPerPeriod());

		NotePlayHandleManager::release(handle);
	}

	void zeroOffsetUsesFullPeriod()
	{
		auto* handle = NotePlayHandleManager::acquire(
			m_track, 0, 1000000, Note(TimePos(192), TimePos(0), 60, 100, 0));
		QVERIFY(handle != nullptr);
		QCOMPARE(handle->noteOffset(), f_cnt_t(0));
		QCOMPARE(handle->framesLeftForCurrentPeriod(),
			Engine::audioEngine()->framesPerPeriod());
		NotePlayHandleManager::release(handle);
	}

	void noteOffsetIsExposedOnlyForTheFirstPeriod()
	{
		const auto offset = Engine::audioEngine()->framesPerPeriod() / 3;
		auto* handle = NotePlayHandleManager::acquire(
			m_track, offset, 1000, Note(TimePos(192), TimePos(0), 60, 100, 0));
		QVERIFY(handle != nullptr);
		QCOMPARE(handle->noteOffset(), offset);
		QCOMPARE(handle->totalFramesPlayed(), f_cnt_t(0));
		NotePlayHandleManager::release(handle);
	}
};

QTEST_MAIN(NoteOffsetTest)

#include "NoteOffsetTest.moc"
