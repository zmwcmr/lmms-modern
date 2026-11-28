/*
 * PluginScanCacheTest.cpp - shared scanner cache tests
 *
 * Copyright (c) 2026 LMMS contributors
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#include <QtTest>

#include <QTemporaryDir>

#include "PluginScanCache.h"

using namespace lmms;

namespace
{

PluginScanEntry makeEntry()
{
	PluginScanEntry entry;
	entry.format = PluginScanFormat::Vst3;
	entry.path = QStringLiteral("/plugins/Test.vst3");
	entry.classId = QStringLiteral("abcdef");
	entry.uniqueId = QStringLiteral("unique");
	entry.name = QStringLiteral("Test");
	entry.vendor = QStringLiteral("LMMS");
	entry.version = QStringLiteral("1.0");
	entry.mtimeMs = 1234;
	entry.size = 5678;
	entry.hash = QStringLiteral("hash");
	entry.isInstrument = true;
	entry.classes.push_back({ QStringLiteral("abcdef"),
		QStringLiteral("Test"), true });
	entry.parameters.push_back({ 7, QStringLiteral("Gain"), 0.0, 1.0, 0.5, 0 });
	return entry;
}

} // namespace

class PluginScanCacheTest : public QObject
{
	Q_OBJECT

private slots:
	void roundTrips()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const auto path = dir.filePath(QStringLiteral("scan.json"));

		PluginScanCache cache(PluginScanFormat::Vst3);
		auto entry = makeEntry();
		cache.upsert(entry);
		QVERIFY(cache.save(path));

		PluginScanCache loaded(PluginScanFormat::Vst3);
		QVERIFY(loaded.load(path));
		const auto* found = loaded.find(entry.path, entry.classId);
		QVERIFY(found != nullptr);
		QCOMPARE(found->name, entry.name);
		QCOMPARE(found->mtimeMs, entry.mtimeMs);
		QCOMPARE(found->classes.size(), 1u);
		QCOMPARE(found->parameters.size(), 1u);
		QCOMPARE(found->parameters[0].id, 7u);
		QVERIFY(loaded.isUpToDate(entry.path, entry.mtimeMs, entry.size,
			entry.classId));
		QVERIFY(!loaded.isUpToDate(entry.path, entry.mtimeMs + 1,
			entry.size, entry.classId));
	}

	void rejectsWrongFormatAndVersion()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const auto path = dir.filePath(QStringLiteral("scan.json"));

		PluginScanCache cache(PluginScanFormat::Vst2);
		cache.upsert(makeEntry());
		QVERIFY(cache.save(path));

		PluginScanCache wrong(PluginScanFormat::Vst3);
		QVERIFY(!wrong.load(path));
		QVERIFY(wrong.entries().empty());

		QFile file(path);
		QVERIFY(file.open(QIODevice::ReadOnly));
		auto text = file.readAll();
		file.close();
		text.replace("\"version\":1", "\"version\":99");
		QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
		file.write(text);
		file.close();
		QVERIFY(!wrong.load(path));
	}

	void upsertsAndRemoves()
	{
		PluginScanCache cache(PluginScanFormat::Vst3);
		auto entry = makeEntry();
		cache.upsert(entry);
		entry.name = QStringLiteral("Updated");
		cache.upsert(entry);
		QCOMPARE(cache.entries().size(), 1u);
		QCOMPARE(cache.find(entry.path, entry.classId)->name,
			QStringLiteral("Updated"));
		QVERIFY(cache.remove(entry.path, entry.classId));
		QVERIFY(cache.entries().empty());
		QVERIFY(!cache.remove(entry.path, entry.classId));
	}
};

QTEST_GUILESS_MAIN(PluginScanCacheTest)

#include "PluginScanCacheTest.moc"
