/*
 * Lv2PortTest.cpp - regression tests for LV2 port handling
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

#include <array>

#include <QtTest>

#include "Engine.h"
#include "Lv2Manager.h"
#include "Lv2Proc.h"
#include "SampleFrame.h"

using namespace lmms;

class Lv2PortTest : public QObject
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

	// sfizz declares lv2:enumeration on several ports but provides no scale
	// points. Creating the Lv2Proc used to index an empty scale point map.
	void enumPortsWithoutScalePoints()
	{
		const QString uri = QStringLiteral("http://sfztools.github.io/sfizz");
		if (!Engine::getLv2Manager()->getPlugin(uri))
		{
			QSKIP("sfizz LV2 is not installed");
		}

		Model parent(nullptr);
		Lv2Proc proc(&parent, uri);
		QVERIFY(proc.portNum() > 0);
	}

	// OB-Xd marks its only audio outputs as connectionOptional. The plugin
	// used to be left without an output port and crashed on the first block.
	void optionalAudioOutputs()
	{
		const QString uri = QStringLiteral("https://www.discodsp.com/obxd/");
		if (!Engine::getLv2Manager()->getPlugin(uri))
		{
			QSKIP("OB-Xd LV2 is not installed");
		}

		Model parent(nullptr);
		Lv2Proc proc(&parent, uri);

		std::array<SampleFrame, 256> buffer{};
		proc.copyBuffersToCore(buffer.data(), buffer.size());
		for (const SampleFrame& frame : buffer)
		{
			QCOMPARE(frame[0], 0.f);
			QCOMPARE(frame[1], 0.f);
		}
	}
};

QTEST_GUILESS_MAIN(Lv2PortTest)
#include "Lv2PortTest.moc"
