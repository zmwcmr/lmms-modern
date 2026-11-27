/*
 * HostedEffectTest.cpp - tests for the generic effect wrapper
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

#include <vector>

#include "AutomatableModel.h"
#include "Engine.h"
#include "FakePluginHost.h"
#include "HostedEffect.h"
#include "Plugin.h"
#include "SampleFrame.h"

using namespace lmms;

class HostedEffectTest : public QObject
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

	void processesAndControlsParameters()
	{
		static const Plugin::Descriptor descriptor{
			"hostedeffecttest", "Hosted Effect Test", "test", "LMMS", 1,
			Plugin::Type::Effect, nullptr, ""
		};

		auto host = std::make_unique<FakePluginHost>();
		auto* const hostPtr = host.get();
		HostedEffect effect(&descriptor, nullptr, nullptr, std::move(host));
		effect.initializeModels();

		constexpr f_cnt_t frames = 64;
		std::vector<SampleFrame> buffer(
			static_cast<std::size_t>(frames), SampleFrame(1.0f));

		effect.processImpl(buffer.data(), frames);
		for (f_cnt_t frame = 0; frame < frames; ++frame)
		{
			QCOMPARE(buffer[static_cast<std::size_t>(frame)][0], 0.5f);
			QCOMPARE(buffer[static_cast<std::size_t>(frame)][1], 0.5f);
		}

		QCOMPARE(effect.controls()->controlCount(), 2);
		QCOMPARE(effect.latency(), 42);

		effect.hostedControls().parameterModels().model(0)->setValue(0.75f);
		effect.hostedControls().parameterModels().queueChanges(frames);
		QCOMPARE(hostPtr->m_calls.size(), 1u);
		QCOMPARE(hostPtr->m_calls[0].id, 7u);
	}

	void roundTripsState()
	{
		static const Plugin::Descriptor descriptor{
			"hostedeffecttest", "Hosted Effect Test", "test", "LMMS", 1,
			Plugin::Type::Effect, nullptr, ""
		};

		auto host = std::make_unique<FakePluginHost>();
		auto* const hostPtr = host.get();
		HostedEffect effect(&descriptor, nullptr, nullptr, std::move(host));
		effect.initializeModels();
		effect.hostedControls().parameterModels().model(0)->setValue(0.25f);

		QDomDocument document;
		QDomElement root = document.createElement(QStringLiteral("root"));
		document.appendChild(root);
		effect.hostedControls().saveSettings(document, root);
		QVERIFY(root.hasAttribute(QStringLiteral("param7")));
		QVERIFY(root.hasAttribute(QStringLiteral("hoststate")));

		effect.hostedControls().parameterModels().model(0)->setValue(0.75f);
		hostPtr->m_values[7] = 0.25;
		effect.hostedControls().loadSettings(root);
		QCOMPARE(effect.hostedControls().parameterModels().model(0)->value(),
			0.25f);
	}
};

QTEST_GUILESS_MAIN(HostedEffectTest)

#include "HostedEffectTest.moc"
