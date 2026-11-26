/*
 * ConfigUpgradeTest.cpp - regression tests for configuration upgrades
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

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "ConfigManager.h"

class ConfigUpgradeTest : public QObject
{
	Q_OBJECT

private slots:
	void audioEngineSettingsSurviveUpgrade()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const auto path = dir.filePath("lmms.conf");
		QFile file(path);
		QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
		file.write(R"cfg(<?xml version="1.0"?>
<lmms version="1.3.0-alpha.2.266" configversion="0">
  <app configured="1"/>
  <audioengine audiodev="Dummy (no sound output)" mididev="Dummy"
               samplerate="48000" framesperaudiobuffer="512" hqaudio="1"/>
</lmms>
)cfg" );
		file.close();

		auto* config = lmms::ConfigManager::inst();
		config->loadConfigFile(path);

		// Upgrading an old configuration must not overwrite the current
		// audio settings with the empty default of the legacy "mixer"
		// section.
		QCOMPARE(config->value("audioengine", "audiodev"),
			QString("Dummy (no sound output)"));
		QCOMPARE(config->value("audioengine", "mididev"), QString("Dummy"));
		QCOMPARE(config->value("audioengine", "samplerate"), QString("48000"));
		QCOMPARE(config->value("audioengine", "framesperaudiobuffer"), QString("512"));
		QCOMPARE(config->value("audioengine", "hqaudio"), QString("1"));
	}

	void legacyMixerSettingsAreMigrated()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const auto path = dir.filePath("lmms.conf");
		QFile file(path);
		QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
		file.write(R"cfg(<?xml version="1.0"?>
<lmms version="1.3.0-alpha.2.266" configversion="0">
  <mixer audiodev="PulseAudio" mididev="Dummy" samplerate="44100"/>
</lmms>
)cfg" );
		file.close();

		auto* config = lmms::ConfigManager::inst();
		config->loadConfigFile(path);

		QCOMPARE(config->value("audioengine", "audiodev"), QString("PulseAudio"));
		QCOMPARE(config->value("audioengine", "mididev"), QString("Dummy"));
		QCOMPARE(config->value("audioengine", "samplerate"), QString("44100"));
	}

	void legacyDisplaySettingIsRenamed()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const auto path = dir.filePath("lmms.conf");
		QFile file(path);
		QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
		file.write(R"cfg(<?xml version="1.0"?>
<lmms version="1.3.0-alpha.2.266" configversion="0">
  <app displaydbv="1"/>
</lmms>
)cfg" );
		file.close();

		auto* config = lmms::ConfigManager::inst();
		config->loadConfigFile(path);

		QCOMPARE(config->value("app", "displaydbfs"), QString("1"));
		QVERIFY(config->value("app", "displaydbv").isEmpty());
	}

	void legacyMidiDeviceIsRenamed()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const auto path = dir.filePath("lmms.conf");
		QFile file(path);
		QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
		file.write(R"cfg(<?xml version="1.0"?>
<lmms version="1.3.0-alpha.2.266" configversion="0">
  <MidiAlsaRaw Device="hw:1"/>
</lmms>
)cfg" );
		file.close();

		auto* config = lmms::ConfigManager::inst();
		config->loadConfigFile(path);

		QCOMPARE(config->value("MidiAlsaRaw", "device"), QString("hw:1"));
		QVERIFY(config->value("MidiAlsaRaw", "Device").isEmpty());
	}
};

QTEST_MAIN(ConfigUpgradeTest)

#include "ConfigUpgradeTest.moc"
