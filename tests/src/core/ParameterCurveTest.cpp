/*
 * ParameterCurveTest.cpp - tests for parameter curve compression
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

#include <vector>

#include "ParameterCurve.h"

using namespace lmms;

class ParameterCurveTest : public QObject
{
	Q_OBJECT

private slots:
	void stepKeepsExactFrame()
	{
		std::vector<float> values(256, 0.0f);
		std::fill(values.begin() + 64, values.end(), 1.0f);

		const auto events = compressParameterCurve(
			values.data(), static_cast<int>(values.size()), 0.0f, 1.0f);

		QCOMPARE(events.size(), 2);
		QCOMPARE(events[0].frame, 0);
		QCOMPARE(events[0].value, 0.0f);
		QCOMPARE(events[1].frame, 64);
		QCOMPARE(events[1].value, 1.0f);
	}

	void rampIsBoundedAndKeepsEndpoints()
	{
		std::vector<float> values(256);
		for (std::size_t i = 0; i < values.size(); ++i)
		{
			values[i] = static_cast<float>(i) / 255.0f;
		}

		const auto events = compressParameterCurve(
			values.data(), static_cast<int>(values.size()), 0.0f, 1.0f);

		QVERIFY(events.size() <= 32);
		QCOMPARE(events.front().frame, 0);
		QCOMPARE(events.front().value, 0.0f);
		QCOMPARE(events.back().frame, 255);
		QCOMPARE(events.back().value, 1.0f);
		for (std::size_t i = 1; i < events.size(); ++i)
		{
			QVERIFY(events[i].frame > events[i - 1].frame);
			QVERIFY(events[i].value >= events[i - 1].value);
		}
	}

	void constantCurveHasOnePoint()
	{
		std::vector<float> values(128, 0.25f);
		const auto events = compressParameterCurve(
			values.data(), static_cast<int>(values.size()), 0.0f, 1.0f);

		QCOMPARE(events.size(), 1);
		QCOMPARE(events[0].frame, 0);
		QCOMPARE(events[0].value, 0.25f);
	}

	void fastChangesAreBounded()
	{
		std::vector<float> values(128);
		for (std::size_t i = 0; i < values.size(); ++i)
		{
			values[i] = (i % 2 == 0) ? 0.0f : 1.0f;
		}

		const auto events = compressParameterCurve(
			values.data(), static_cast<int>(values.size()), 0.0f, 1.0f, 32, 8);

		QVERIFY(events.size() <= 32);
		for (std::size_t i = 1; i < events.size(); ++i)
		{
			QVERIFY(events[i].frame > events[i - 1].frame);
		}
	}

	void minSpacingCoalescesFastChanges()
	{
		std::vector<float> values(32);
		for (std::size_t i = 0; i < values.size(); ++i)
		{
			values[i] = (i % 2 == 0) ? 0.0f : 1.0f;
		}

		const auto events = compressParameterCurve(
			values.data(), static_cast<int>(values.size()), 0.0f, 1.0f, 32, 8);

		QVERIFY(events.size() <= 32);
		// The last point may be closer than minSpacing so the final value is
		// preserved for the next block.
		for (std::size_t i = 1; i + 1 < events.size(); ++i)
		{
			QVERIFY(events[i].frame - events[i - 1].frame >= 8);
		}
	}

	void emptyInputReturnsNothing()
	{
		QVERIFY(compressParameterCurve(nullptr, 10, 0.0f, 1.0f).empty());
		const float value = 0.5f;
		QVERIFY(compressParameterCurve(&value, 0, 0.0f, 1.0f).empty());
		QVERIFY(compressParameterCurve(&value, 1, 0.0f, 1.0f, 0).empty());
	}
};

QTEST_GUILESS_MAIN(ParameterCurveTest)
#include "ParameterCurveTest.moc"
