/*
 * Lv2PresetTest.cpp - tests for LV2 factory presets
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

#include "AutomatableModel.h"
#include "Engine.h"
#include "Lv2Manager.h"
#include "Lv2Proc.h"

using namespace lmms;

class Lv2PresetTest : public QObject
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

	void testFactoryPresets()
	{
		const char* uri = "http://drobilla.net/plugins/mda/Piano";
		if (!Engine::getLv2Manager()->getPlugin(QString::fromUtf8(uri)))
		{
			QSKIP("mda Piano is not installed");
		}

		Model parent(nullptr);
		Lv2Proc proc(&parent, uri);
		QVERIFY2(!proc.presets().empty(), "no factory presets found");

		float before = -1.0f;
		proc.foreach_model([&before](const std::string& name,
			const ModelGroup::ModelInfo& info)
			{
				if (name == "env_decay")
				{
					before = info.m_model->value<float>();
				}
			});
		QVERIFY(before >= 0.0f);

		std::size_t presetIndex = proc.presets().size();
		for (std::size_t i = 0; i < proc.presets().size(); ++i)
		{
			if (proc.presets()[i].label.contains("Compressed Piano"))
			{
				presetIndex = i;
				break;
			}
		}
		QVERIFY(presetIndex < proc.presets().size());
		QVERIFY(proc.loadPreset(presetIndex));

		float after = -1.0f;
		proc.foreach_model([&after](const std::string& name,
			const ModelGroup::ModelInfo& info)
			{
				if (name == "env_decay")
				{
					after = info.m_model->value<float>();
				}
			});
		QVERIFY2(after >= 0.0f, "env_decay model missing");
		QVERIFY2(qAbs(after - 0.902f) < 0.002f,
			qPrintable(QString("env_decay = %1").arg(after)));
		QVERIFY(qAbs(after - before) > 0.001f);
	}
};

QTEST_GUILESS_MAIN(Lv2PresetTest)
#include "Lv2PresetTest.moc"
