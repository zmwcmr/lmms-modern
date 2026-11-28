/*
 * TimePosTest.cpp - tests for tick/bar/beat arithmetic
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

#include <cmath>

#include <QtTest>

#include "TimePos.h"

using namespace lmms;

class TimePosTest : public QObject
{
	Q_OBJECT

private slots:
	void constructorConvertsBarsAndTicks()
	{
		const auto ticksPerBar = TimePos::ticksPerBar();
		QCOMPARE(TimePos(0, 0).getTicks(), tick_t(0));
		QCOMPARE(TimePos(1, 0).getTicks(), ticksPerBar);
		QCOMPARE(TimePos(2, 10).getTicks(), 2 * ticksPerBar + 10);
		QCOMPARE(TimePos(TimePos::stepPosition(4)).getTicks(), tick_t(4 * 12));
	}

	void barRounding()
	{
		const auto ticksPerBar = TimePos::ticksPerBar();
		QCOMPARE(TimePos(0).getBar(), bar_t(0));
		QCOMPARE(TimePos(1).getBar(), bar_t(0));
		QCOMPARE(TimePos(ticksPerBar).getBar(), bar_t(1));
		QCOMPARE(TimePos(ticksPerBar + 1).getBar(), bar_t(1));

		QCOMPARE(TimePos(0).nextFullBar(), bar_t(0));
		QCOMPARE(TimePos(1).nextFullBar(), bar_t(1));
		QCOMPARE(TimePos(ticksPerBar).nextFullBar(), bar_t(1));
		QCOMPARE(TimePos(ticksPerBar + 1).nextFullBar(), bar_t(2));
		QCOMPARE(TimePos(ticksPerBar).toAbsoluteBar().getTicks(), ticksPerBar);
	}

	void timeSignatureArithmetic()
	{
		const TimeSig fourFour(4, 4);
		QCOMPARE(TimePos::ticksPerBar(fourFour), tick_t(192));
		QCOMPARE(TimePos::ticksPerBar(TimeSig(3, 4)), tick_t(144));
		QCOMPARE(TimePos::ticksPerBar(TimeSig(6, 8)), tick_t(144));
		QCOMPARE(TimePos::ticksPerBar(TimeSig(7, 16)), tick_t(84));

		const TimeSig sixEight(6, 8);
		const TimePos pos(200);
		QCOMPARE(pos.getTickWithinBar(sixEight), tick_t(200 % 144));
		QCOMPARE(pos.getBeatWithinBar(sixEight), tick_t((200 % 144) / 24));
		QCOMPARE(pos.getTickWithinBeat(sixEight), tick_t((200 % 144) % 24));
	}

	void timeSignatureValuesAreClamped()
	{
		// Malformed projects used to produce a zero ticks-per-bar value,
		// which made callers divide by zero or loop forever.
		for (const auto& sig : {
				 TimeSig(0, 0),
				 TimeSig(0, 4),
				 TimeSig(4, 0),
				 TimeSig(-4, -4),
				 TimeSig(1000, 1000),
				 TimeSig(-1, 1000),
			 })
		{
			QVERIFY2(TimePos::ticksPerBar(sig) >= 1,
				qPrintable(QString("numerator=%1 denominator=%2")
					.arg(sig.numerator())
					.arg(sig.denominator())));
		}
		QCOMPARE(TimePos::ticksPerBar(TimeSig(1000, 1000)), tick_t(192));
		QCOMPARE(TimePos::ticksPerBar(TimeSig(0, 0)), tick_t(192));
	}

	void quantizeSnapsToBarsAndHalves()
	{
		QCOMPARE(TimePos(0).quantize(1.0f).getTicks(), tick_t(0));
		QCOMPARE(TimePos(191).quantize(1.0f).getTicks(), tick_t(192));
		QCOMPARE(TimePos(191).quantize(1.0f, true).getTicks(), tick_t(0));
		QCOMPARE(TimePos(96).quantize(0.5f).getTicks(), tick_t(96));
		QCOMPARE(TimePos(47).quantize(0.5f).getTicks(), tick_t(0));
		QCOMPARE(TimePos(48).quantize(0.5f).getTicks(), tick_t(96));
		// Exactly half-way, rounded towards the right.
		QCOMPARE(TimePos(48).quantize(0.5f, true).getTicks(), tick_t(0));
		// Negative positions must not snap before the lower bound.
		QCOMPARE(TimePos(-48).quantize(0.5f).getTicks(), tick_t(0));
		QCOMPARE(TimePos(-49).quantize(0.5f).getTicks(), tick_t(-96));
	}

	void frameConversion()
	{
		QCOMPARE(TimePos(192).frames(1.0f), f_cnt_t(192));
		QCOMPARE(TimePos(192).frames(0.5f), f_cnt_t(96));
		QCOMPARE(TimePos::fromFrames(441, 0.5f).getTicks(), tick_t(882));
		QCOMPARE(TimePos::fromFrames(0, 0.5f).getTicks(), tick_t(0));
	}

	void milliseconds()
	{
		// 192 ticks are one bar of 4/4 at 120 BPM, i.e. two seconds.
		QCOMPARE(TimePos::ticksToMilliseconds(tick_t(192), bpm_t(120)), 2000.0);
		QCOMPARE(TimePos::ticksToMilliseconds(tick_t(96), bpm_t(120)), 1000.0);
		QCOMPARE(TimePos(192).getTimeInMilliseconds(bpm_t(180)), 4000.0 / 3.0);
	}

	void ticksPerBarCanBeOverridden()
	{
		const auto oldTicksPerBar = TimePos::ticksPerBar();
		TimePos::setTicksPerBar(0);
		const auto clamped = TimePos::ticksPerBar();
		const auto steps = TimePos::stepsPerBar();
		const auto step = TimePos::stepPosition(1).getTicks();
		TimePos::setTicksPerBar(oldTicksPerBar);

		QCOMPARE(clamped, tick_t(1));
		QCOMPARE(steps, 1);
		QCOMPARE(step, tick_t(1));

		TimePos::setTicksPerBar(96);
		const auto overridden = TimePos::ticksPerBar();
		const auto overriddenSteps = TimePos::stepsPerBar();
		const auto overriddenStep = TimePos::stepPosition(4).getTicks();
		TimePos::setTicksPerBar(oldTicksPerBar);

		QCOMPARE(overridden, tick_t(96));
		QCOMPARE(overriddenSteps, 8);
		QCOMPARE(overriddenStep, tick_t(48));
	}
};

QTEST_MAIN(TimePosTest)

#include "TimePosTest.moc"
