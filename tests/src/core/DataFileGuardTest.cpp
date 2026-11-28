/*
 * DataFileGuardTest.cpp - tests for project-file safety guards
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

#include <QByteArray>
#include <QtTest>

#include "DataFile.h"

using namespace lmms;

namespace
{

QByteArray project(const QByteArray& body, const QByteArray& type = "song")
{
	return QByteArray("<?xml version=\"1.0\"?>\n<lmms type=\"") + type
		+ "\" version=\"1.3.0\">\n" + body + "\n</lmms>\n";
}

} // namespace

class DataFileGuardTest : public QObject
{
	Q_OBJECT

private slots:
	void cleanProjectHasNoLocalPlugins()
	{
		DataFile file(project(
			"<head/>"
			"<song><trackcontainer type=\"song\">"
			"<track type=\"instrumenttrack\">"
			"<instrument name=\"vestige\"><vestige plugin=\"/usr/lib/vst/ok.so\"/></instrument>"
			"</track></trackcontainer></song>"));
		QVERIFY(!file.hasLocalPlugins());
	}

	void rejectsLocalPluginPath()
	{
		DataFile file(project(
			"<head/>"
			"<song><trackcontainer type=\"song\">"
			"<track type=\"instrumenttrack\">"
			"<instrument name=\"vestige\">"
			"<vestige plugin=\"local:/tmp/evil.so\"/>"
			"</instrument></track></trackcontainer></song>"));
		QVERIFY(file.hasLocalPlugins());
	}

	void rejectsCaseInsensitiveLocalPrefix()
	{
		DataFile file(project(
			"<head/>"
			"<song><trackcontainer type=\"song\">"
			"<track type=\"instrumenttrack\">"
			"<instrument name=\"vestige\">"
			"<vestige plugin=\"LOCAL:/tmp/evil.so\"/>"
			"</instrument></track></trackcontainer></song>"));
		QVERIFY(file.hasLocalPlugins());
	}

	void allowsLocalSampleAndAfpPaths()
	{
		DataFile file(project(
			"<head/>"
			"<song><trackcontainer type=\"song\">"
			"<track type=\"sampletrack\">"
			"<sampleclip src=\"local:/tmp/kick.wav\"/>"
			"</track>"
			"<track type=\"instrumenttrack\">"
			"<instrument name=\"audiofileprocessor\">"
			"<audiofileprocessor src=\"local:/tmp/snare.wav\"/>"
			"</instrument></track>"
			"</trackcontainer></song>"));
		QVERIFY(!file.hasLocalPlugins());
	}

	void stillChecksChildrenOfAllowedElements()
	{
		DataFile file(project(
			"<head/>"
			"<song><trackcontainer type=\"song\">"
			"<track type=\"sampletrack\">"
			"<sampleclip src=\"local:/tmp/kick.wav\">"
			"<vestige plugin=\"local:/tmp/evil.so\"/>"
			"</sampleclip></track>"
			"</trackcontainer></song>"));
		QVERIFY(file.hasLocalPlugins());
	}

	void validatesSongExtensions()
	{
		DataFile file(project("<head/><song/>"));
		QCOMPARE(file.type(), DataFile::Type::SongProject);
		QVERIFY(file.validate("mmp"));
		QVERIFY(file.validate("mmpz"));
		QVERIFY(!file.validate("wav"));
		QVERIFY(!file.validate("xpf"));
	}

	void validatesTemplateAndSettingsExtensions()
	{
		DataFile projectTemplate(project("<head/>", "songtemplate"));
		QCOMPARE(projectTemplate.type(), DataFile::Type::SongProjectTemplate);
		QVERIFY(projectTemplate.validate("mpt"));
		QVERIFY(!projectTemplate.validate("mmp"));

		DataFile trackSettings(project("<head/>", "instrumenttracksettings"));
		QCOMPARE(trackSettings.type(), DataFile::Type::InstrumentTrackSettings);
		QVERIFY(trackSettings.validate("xpf"));
		QVERIFY(trackSettings.validate("xml"));
		QVERIFY(!trackSettings.validate("mmp"));
	}

	void unknownTypeUsesExtensionAllowList()
	{
		DataFile file(project("<head/>", "unknown"));
		QCOMPARE(file.type(), DataFile::Type::Unknown);
		QVERIFY(file.validate("mmp"));
		QVERIFY(file.validate("mid"));
		QVERIFY(!file.validate("exe"));
		QVERIFY(!file.validate("sh"));
	}

	void rejectsDecompressionBomb()
	{
		// qCompress stores the expected uncompressed size in the first four
		// bytes. A claimed size above the 512 MB limit must be refused
		// before qUncompress allocates anything.
		QByteArray bomb;
		bomb.append(char(0xFF));
		bomb.append(char(0xFF));
		bomb.append(char(0xFF));
		bomb.append(char(0xFF));
		bomb.append(QByteArray("not really compressed data", 25));

		DataFile file(bomb);
		QVERIFY(file.documentElement().isNull());
	}
};

QTEST_MAIN(DataFileGuardTest)

#include "DataFileGuardTest.moc"
