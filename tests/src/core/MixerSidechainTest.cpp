/*
 * MixerSidechainTest.cpp - tests for mixer sidechain routing
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
#include "Mixer.h"

using namespace lmms;

class MixerSidechainTest : public QObject
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

	void routesAndRejectsCycles()
	{
		Mixer& mixer = *Engine::mixer();
		const int first = mixer.createChannel();
		const int second = mixer.createChannel();

		QVERIFY(mixer.setChannelSidechain(first, second));
		QCOMPARE(mixer.channelSidechain(first), second);

		// The reverse edge would create a cycle.
		QVERIFY(!mixer.setChannelSidechain(second, first));
		QCOMPARE(mixer.channelSidechain(second), -1);

		// Self routing and the master channel as a source are rejected.
		QVERIFY(!mixer.setChannelSidechain(first, first));
		QVERIFY(!mixer.setChannelSidechain(first, 0));

		QVERIFY(mixer.setChannelSidechain(first, -1));
		QCOMPARE(mixer.channelSidechain(first), -1);
	}

	void routesMultiOutput()
	{
		Mixer& mixer = *Engine::mixer();
		const int first = mixer.createChannel();
		const int second = mixer.createChannel();

		QVERIFY(mixer.setChannelMultiOutput(first, second));
		QCOMPARE(mixer.channelMultiOutput(first), second);
		QVERIFY(!mixer.setChannelMultiOutput(second, first));
		QVERIFY(!mixer.setChannelMultiOutput(first, first));
		QVERIFY(!mixer.setChannelMultiOutput(0, second));
		QVERIFY(mixer.setChannelMultiOutput(first, -1));
		QCOMPARE(mixer.channelMultiOutput(first), -1);
	}

	void roundTripsXml()
	{
		Mixer& mixer = *Engine::mixer();
		const int first = mixer.createChannel();
		const int second = mixer.createChannel();
		QVERIFY(mixer.setChannelSidechain(first, second));
		QVERIFY(mixer.setChannelMultiOutput(first, second));

		QDomDocument document;
		QDomElement root = document.createElement(QStringLiteral("mixer"));
		document.appendChild(root);
		mixer.saveSettings(document, root);

		mixer.loadSettings(root);
		QCOMPARE(mixer.channelSidechain(first), second);
		QCOMPARE(mixer.channelMultiOutput(first), second);
	}
};

QTEST_GUILESS_MAIN(MixerSidechainTest)

#include "MixerSidechainTest.moc"
