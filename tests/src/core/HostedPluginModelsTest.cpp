/*
 * HostedPluginModelsTest.cpp - tests for the generic parameter bridge
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

#include "AutomatableModel.h"
#include "Engine.h"
#include "FakePluginHost.h"
#include "HostedPluginModels.h"
#include "Model.h"

using namespace lmms;

class HostedPluginModelsTest : public QObject
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

	void createsModels()
	{
		Model parent(nullptr);
		FakePluginHost host;
		HostedPluginModels models;
		models.initialize(&parent, &host);

		QCOMPARE(models.count(), 2);
		QCOMPARE(models.settingPrefix(), QStringLiteral("param"));
		QCOMPARE(models.model(0)->value(), 0.5f);
		QCOMPARE(models.model(1)->value(), 25.0f);
		QCOMPARE(models.model(1)->maxValue(), 100.0f);
		QCOMPARE(models.parameter(0).id, 7u);
	}

	void queuesChanges()
	{
		Model parent(nullptr);
		FakePluginHost host;
		HostedPluginModels models;
		models.initialize(&parent, &host);

		models.model(0)->setValue(0.75f);
		models.queueChanges(64);
		QCOMPARE(host.m_calls.size(), 1u);
		QCOMPARE(host.m_calls[0].id, 7u);
		QCOMPARE(host.m_calls[0].value, 0.75);
		QCOMPARE(host.m_calls[0].sampleOffset, 0);
	}

	void refreshesFromHost()
	{
		Model parent(nullptr);
		FakePluginHost host;
		HostedPluginModels models;
		models.initialize(&parent, &host);

		host.m_values[7] = 0.25;
		models.refreshFromHost();
		QCOMPARE(models.model(0)->value(), 0.25f);
	}

	void roundTripsXml()
	{
		Model parent(nullptr);
		FakePluginHost host;
		HostedPluginModels models;
		models.initialize(&parent, &host);

		QDomDocument document;
		QDomElement root = document.createElement(QStringLiteral("root"));
		document.appendChild(root);
		models.saveSettings(document, root);
		QVERIFY(root.hasAttribute(QStringLiteral("param7")));
		QVERIFY(root.hasAttribute(QStringLiteral("param9")));

		models.model(0)->setValue(0.1f);
		models.loadSettings(root);
		QCOMPARE(models.model(0)->value(), 0.5f);
		QCOMPARE(models.model(1)->value(), 25.0f);
	}
};

QTEST_GUILESS_MAIN(HostedPluginModelsTest)

#include "HostedPluginModelsTest.moc"
