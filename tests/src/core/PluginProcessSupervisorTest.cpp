/*
 * PluginProcessSupervisorTest.cpp - tests for the shared plugin process
 * lifecycle
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

#include <QSignalSpy>

#include "PluginProcessSupervisor.h"

using namespace lmms;

class PluginProcessSupervisorTest : public QObject
{
	Q_OBJECT

private slots:
	void directStartsAndStops()
	{
		PluginProcessSupervisor supervisor(
			PluginProcessSupervisor::Mode::Direct);

		QVERIFY2(supervisor.start(QStringLiteral("sh"),
			QStringList{QStringLiteral("-c"), QStringLiteral("sleep 30")},
			5000), qPrintable(supervisor.errorString()));
		QVERIFY(supervisor.isRunning());
		QVERIFY(supervisor.processId() > 0);

		supervisor.stop();
		QVERIFY(!supervisor.isRunning());
	}

	void directMissingExecutableFails()
	{
		PluginProcessSupervisor supervisor(
			PluginProcessSupervisor::Mode::Direct);
		QVERIFY(!supervisor.start(QStringLiteral("/nonexistent/plugin-host"),
			QStringList{}, 2000));
		QVERIFY(!supervisor.isRunning());
	}

	void directCrashIsReported()
	{
		PluginProcessSupervisor supervisor(
			PluginProcessSupervisor::Mode::Direct);
		QSignalSpy crashed(&supervisor,
			&PluginProcessSupervisor::crashed);

		QVERIFY2(supervisor.start(QStringLiteral("sh"),
			QStringList{QStringLiteral("-c"), QStringLiteral("exit 3")},
			5000), qPrintable(supervisor.errorString()));
		QTRY_COMPARE_WITH_TIMEOUT(crashed.count(), 1, 5000);
		QCOMPARE(supervisor.exitCode(), 3);
		QVERIFY(!supervisor.isRunning());
	}

	void watcherCrashIsReported()
	{
		PluginProcessSupervisor supervisor(
			PluginProcessSupervisor::Mode::WatcherThread);
		QSignalSpy crashed(&supervisor,
			&PluginProcessSupervisor::crashed);

		QVERIFY(supervisor.start(QStringLiteral("sh"),
			QStringList{QStringLiteral("-c"), QStringLiteral("exit 7")}));
		QTRY_COMPARE_WITH_TIMEOUT(crashed.count(), 1, 5000);
		QCOMPARE(supervisor.exitCode(), 7);
		QVERIFY(!supervisor.isRunning());
	}

	void watcherStopIsNotACrash()
	{
		PluginProcessSupervisor supervisor(
			PluginProcessSupervisor::Mode::WatcherThread);
		QSignalSpy crashed(&supervisor,
			&PluginProcessSupervisor::crashed);

		QVERIFY(supervisor.start(QStringLiteral("sh"),
			QStringList{QStringLiteral("-c"), QStringLiteral("sleep 30")}));
		// Watcher mode starts the process on a helper thread, so the state
		// becomes visible asynchronously.
		QTRY_VERIFY_WITH_TIMEOUT(supervisor.isRunning(), 5000);

		supervisor.stop();
		QVERIFY(!supervisor.isRunning());
		QTest::qWait(300);
		QCOMPARE(crashed.count(), 0);
	}
};

QTEST_GUILESS_MAIN(PluginProcessSupervisorTest)

#include "PluginProcessSupervisorTest.moc"
