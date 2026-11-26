/*
 * Vst3AutomationTest.cpp - VST3 parameter automation save/load test
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
#include <vector>

#include <QDomDocument>
#include <QDomElement>
#include <QtTest>

#include "AutomationClip.h"
#include "AutomationTrack.h"
#include "Engine.h"
#include "Model.h"
#include "TimePos.h"
#include "Song.h"
#include "Vst3Host.h"
#include "Vst3ParameterModels.h"

class Vst3AutomationTest : public QObject
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

	void parameterAutomationRoundTrip()
	{
		lmms::Vst3Host host;
		QString error;
		QVERIFY2(host.load(QStringLiteral(VST3_FAKE_BUNDLE),
			QStringLiteral(VST3_FAKE_CLASS), 48000.0, 512, &error),
			qPrintable(error));
		QCOMPARE(host.parameters().size(), size_t(1));

		lmms::Model parent(nullptr);
		lmms::Vst3ParameterModels models;
		models.initialize(&parent, &host);
		QCOMPARE(models.count(), 1);
		auto* const model = models.model(0);
		QVERIFY(model != nullptr);

		lmms::AutomationTrack automationTrack(lmms::Engine::getSong());
		lmms::AutomationClip clip(&automationTrack);
		QVERIFY(clip.addObject(model));
		clip.putValue(lmms::TimePos(0), 0.75f);
		model->setValue(0.75f, true);
		QVERIFY(model->isAutomated());

		QDomDocument document;
		QDomElement parentElement = document.createElement(QStringLiteral("root"));
		document.appendChild(parentElement);
		models.saveSettings(document, parentElement);
		QVERIFY(!parentElement.firstChildElement(QStringLiteral("vst3p1")).isNull());

		model->setValue(0.2f);
		models.loadSettings(parentElement);
		QCOMPARE(model->value(), 0.75f);

		lmms::Vst3ParameterModels manualModels;
		manualModels.initialize(&parent, &host);
		QCOMPARE(manualModels.count(), 1);
		auto* const manualModel = manualModels.model(0);
		QVERIFY(manualModel != nullptr);
		manualModel->setValue(0.75f);
		manualModels.queueChanges(64);
		std::vector<float> left(64);
		std::vector<float> right(64);
		host.process(nullptr, nullptr, left.data(), right.data(), 64);
		QVERIFY(std::abs(host.parameterNormalised(1) - 0.75) < 0.01);
	}
};

QTEST_MAIN(Vst3AutomationTest)
#include "Vst3AutomationTest.moc"
