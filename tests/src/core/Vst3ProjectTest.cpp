/*
 * Vst3ProjectTest.cpp - VST3 project save/load round-trip test
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
#include <cstring>
#include <initializer_list>
#include <utility>

#include <QtTest>

#include <QDomDocument>
#include <QDomElement>
#include <QFile>
#include <QTemporaryDir>

#include "Engine.h"
#include "Instrument.h"
#include "InstrumentTrack.h"
#include "Song.h"
#include "Track.h"

namespace
{

constexpr float kTestGain = 0.8f;

QDomElement makeInstrumentTrackElement(QDomDocument& document)
{
	QDomElement track = document.createElement(QStringLiteral("track"));
	track.setAttribute(QStringLiteral("type"),
		static_cast<int>(lmms::Track::Type::Instrument));

	QDomElement instrument = document.createElement(QStringLiteral("instrument"));
	instrument.setAttribute(QStringLiteral("name"),
		QStringLiteral("vst3instrument"));

	QDomElement plugin = document.createElement(QStringLiteral("vst3instrument"));
	plugin.setAttribute(QStringLiteral("file"),
		QStringLiteral(VST3_FAKE_BUNDLE));
	plugin.setAttribute(QStringLiteral("class"),
		QStringLiteral(VST3_FAKE_CLASS));

	QDomElement key = document.createElement(QStringLiteral("key"));
	for (const auto& [name, value] : std::initializer_list<std::pair<const char*, const char*>>{
			{"file", VST3_FAKE_BUNDLE},
			{"class", VST3_FAKE_CLASS}})
	{
		QDomElement attribute = document.createElement(QStringLiteral("attribute"));
		attribute.setAttribute(QStringLiteral("name"), QString::fromLatin1(name));
		attribute.setAttribute(QStringLiteral("value"), QString::fromLatin1(value));
		key.appendChild(attribute);
	}
	plugin.appendChild(key);

	instrument.appendChild(plugin);
	track.appendChild(instrument);
	return track;
}

QDomElement saveInstrumentTrack(lmms::InstrumentTrack* track,
	QDomDocument& document)
{
	document = QDomDocument();
	QDomElement element = document.createElement(QStringLiteral("track"));
	document.appendChild(element);
	track->saveTrackSpecificSettings(document, element, false);
	return element;
}

QDomElement savedVst3Element(const QDomElement& track)
{
	return track.firstChildElement(QStringLiteral("instrument"))
		.firstChildElement(QStringLiteral("vst3instrument"));
}

bool readStateGain(const QDomElement& track, float& gain)
{
	const QDomElement plugin = savedVst3Element(track);
	if (plugin.isNull()
		|| !plugin.hasAttribute(QStringLiteral("componentstate")))
	{
		return false;
	}
	const QByteArray state = QByteArray::fromBase64(
		plugin.attribute(QStringLiteral("componentstate")).toLatin1());
	if (state.size() != static_cast<int>(sizeof(gain)))
	{
		return false;
	}
	std::memcpy(&gain, state.constData(), sizeof(gain));
	return true;
}

QString encodeGain(float gain)
{
	const QByteArray state(reinterpret_cast<const char*>(&gain), sizeof(gain));
	return QString::fromLatin1(state.toBase64());
}

} // namespace

class Vst3ProjectTest : public QObject
{
	Q_OBJECT
private slots:
	void initTestCase()
	{
		// Only expose the VST3 plugin and its base library to the plugin
		// loader; scanning the full build directory would load unrelated
		// plugins and slow the test down.
		static QTemporaryDir pluginDirectory;
		QVERIFY(pluginDirectory.isValid());
		const QDir source(VST3_PLUGIN_DIR);
		for (const auto& name : {
			QStringLiteral("libvst3instrument.so"),
			QStringLiteral("libvst3base.so")})
		{
			QVERIFY2(QFile::link(source.filePath(name),
				pluginDirectory.filePath(name)), qPrintable(name));
		}
		qputenv("LMMS_PLUGIN_DIR", pluginDirectory.path().toLocal8Bit());
		// Keep the test in-process and independent of the helper executable.
		qputenv("LMMS_VST3_REMOTE_DSP", "0");
		lmms::Engine::init(true);
	}

	void cleanupTestCase()
	{
		if (lmms::Engine::getSong() != nullptr)
		{
			lmms::Engine::getSong()->clearProject();
		}
		lmms::Engine::destroy();
	}

	void componentStateSurvivesProjectRoundTrip()
	{
		using namespace lmms;

		Song* const song = Engine::getSong();
		QVERIFY(song != nullptr);
		song->clearProject();

		auto* const track = new InstrumentTrack(song);

		QDomDocument document;
		const QDomElement trackElement = makeInstrumentTrackElement(document);
		track->loadTrackSpecificSettings(trackElement);

		QVERIFY(track->instrument() != nullptr);
		QCOMPARE(track->instrument()->nodeName(), QStringLiteral("vst3instrument"));

		// Replace the fake plugin state with a known gain and load it through
		// the same path used when opening an LMMS project.
		QDomDocument savedDocument;
		QDomElement savedTrack = saveInstrumentTrack(track, savedDocument);
		QDomElement savedPlugin = savedVst3Element(savedTrack);
		QVERIFY(!savedPlugin.isNull());
		QVERIFY(savedPlugin.hasAttribute(QStringLiteral("componentstate")));
		QVERIFY(savedPlugin.hasAttribute(QStringLiteral("controllerstate")));

		savedPlugin.setAttribute(QStringLiteral("componentstate"),
			encodeGain(kTestGain));
		savedPlugin.setAttribute(QStringLiteral("controllerstate"),
			encodeGain(kTestGain));

		track->loadTrackSpecificSettings(savedTrack);
		QCOMPARE(track->instrument()->nodeName(), QStringLiteral("vst3instrument"));

		QDomDocument resavedDocument;
		const QDomElement resavedTrack = saveInstrumentTrack(track, resavedDocument);
		float restoredGain = 0.0f;
		QVERIFY(readStateGain(resavedTrack, restoredGain));
		QVERIFY(std::abs(restoredGain - kTestGain) < 0.001f);

		// Now save and load a complete project and check the state again.
		QTemporaryDir directory;
		QVERIFY(directory.isValid());
		const QString projectFile = directory.filePath(QStringLiteral("vst3-state.mmp"));
		QVERIFY(song->saveProjectFile(projectFile));
		QVERIFY(QFile::exists(projectFile));

		song->loadProject(projectFile);

		InstrumentTrack* loadedTrack = nullptr;
		for (Track* const candidate : song->tracks())
		{
			auto* const instrumentTrack = dynamic_cast<InstrumentTrack*>(candidate);
			if (instrumentTrack != nullptr && instrumentTrack->instrument() != nullptr
				&& instrumentTrack->instrument()->nodeName()
					== QStringLiteral("vst3instrument"))
			{
				loadedTrack = instrumentTrack;
				break;
			}
		}
		QVERIFY(loadedTrack != nullptr);

		QDomDocument loadedDocument;
		const QDomElement loadedTrackElement = saveInstrumentTrack(
			loadedTrack, loadedDocument);
		QVERIFY(readStateGain(loadedTrackElement, restoredGain));
		QVERIFY(std::abs(restoredGain - kTestGain) < 0.001f);
	}
};

QTEST_MAIN(Vst3ProjectTest)
#include "Vst3ProjectTest.moc"
