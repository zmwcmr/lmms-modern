/*
 * DelayLineTest.cpp - tests for the mixer delay line
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

#include "AudioBuffer.h"
#include "DelayLine.h"

using namespace lmms;

class DelayLineTest : public QObject
{
	Q_OBJECT

private slots:
	void delayImpulse()
	{
		AudioBuffer buffer{512, 2};
		auto view = buffer.groupBuffers(0);
		view.sample(0, 0) = 1.0f;
		view.sample(1, 0) = 1.0f;

		DelayLine line;
		line.setDelayFrames(128);
		line.process(view, 512);

		for (f_cnt_t frame = 0; frame < 128; ++frame)
		{
			QCOMPARE(view.sample(0, frame), 0.0f);
			QCOMPARE(view.sample(1, frame), 0.0f);
		}
		QCOMPARE(view.sample(0, 128), 1.0f);
		QCOMPARE(view.sample(1, 128), 1.0f);
		for (f_cnt_t frame = 129; frame < 512; ++frame)
		{
			QCOMPARE(view.sample(0, frame), 0.0f);
		}
	}

	void delayAcrossBlocks()
	{
		AudioBuffer buffer{512, 2};
		DelayLine line;
		line.setDelayFrames(128);

		auto first = buffer.groupBuffers(0);
		first.sample(0, 0) = 1.0f;
		line.process(first, 512);

		auto second = buffer.groupBuffers(0);
		for (f_cnt_t frame = 0; frame < 512; ++frame)
		{
			second.sample(0, frame) = 0.0f;
			second.sample(1, frame) = 0.0f;
		}
		second.sample(0, 0) = 1.0f;
		line.process(second, 512);

		QCOMPARE(second.sample(0, 0), 0.0f);
		QCOMPARE(second.sample(0, 128), 1.0f);
	}

	void zeroDelayIsPassthrough()
	{
		AudioBuffer buffer{64, 2};
		auto view = buffer.groupBuffers(0);
		for (f_cnt_t frame = 0; frame < 64; ++frame)
		{
			view.sample(0, frame) = static_cast<float>(frame);
			view.sample(1, frame) = static_cast<float>(frame);
		}

		DelayLine line;
		line.setDelayFrames(0);
		line.process(view, 64);

		for (f_cnt_t frame = 0; frame < 64; ++frame)
		{
			QCOMPARE(view.sample(0, frame), static_cast<float>(frame));
		}
	}
};

QTEST_GUILESS_MAIN(DelayLineTest)
#include "DelayLineTest.moc"
