/*
 * X11EditorWindowTest.cpp - shared X11 editor window tests
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

#include "X11EditorWindow.h"

#if defined(Q_OS_LINUX)
#	include <X11/Xlib.h>
#endif

using namespace lmms;

class X11EditorWindowTest : public QObject
{
	Q_OBJECT

private slots:
	void opensResizesAndCloses()
	{
		if (!X11EditorWindow::isAvailable())
		{
			QSKIP("No X11/XWayland display available");
		}

		X11EditorWindow window;
		X11EditorWindow::Options options;
		options.title = QStringLiteral("LMMS editor window test");
		options.width = 320;
		options.height = 200;

		QVERIFY(window.open(options));
		QVERIFY(window.isOpen());
		QVERIFY(window.window() != 0);
		QVERIFY(window.display() != nullptr);
		QVERIFY(window.isVisible());

		window.resize(400, 300);
		QVERIFY(window.isOpen());

		const QSize clamped = window.clampToDisplay(100000, 100000);
		QVERIFY(clamped.width() > 0);
		QVERIFY(clamped.height() > 0);
		QVERIFY(clamped.width() <= window.displayWidth());
		QVERIFY(clamped.height() <= window.displayHeight());

		window.hide();
		QVERIFY(!window.isVisible());
		window.show();
		QVERIFY(window.isVisible());

		window.close();
		QVERIFY(!window.isOpen());
		QVERIFY(!window.isVisible());
	}

#if defined(Q_OS_LINUX)
	void closeRequestIsRepeatable()
	{
		if (!X11EditorWindow::isAvailable())
		{
			QSKIP("No X11/XWayland display available");
		}

		X11EditorWindow window;
		QVERIFY(window.open({}));
		auto* const display = static_cast<Display*>(window.display());
		QVERIFY(display != nullptr);

		int requests = 0;
		window.closeRequested = [&requests] { ++requests; };
		const auto sendDelete = [&] {
			XEvent event = {};
			event.xclient.type = ClientMessage;
			event.xclient.window = static_cast<::Window>(window.window());
			event.xclient.message_type =
				XInternAtom(display, "WM_PROTOCOLS", False);
			event.xclient.format = 32;
			event.xclient.data.l[0] =
				XInternAtom(display, "WM_DELETE_WINDOW", False);
			XSendEvent(display, static_cast<::Window>(window.window()),
				False, NoEventMask, &event);
			XFlush(display);
		};

		sendDelete();
		QTRY_COMPARE(requests, 1);
		sendDelete();
		QTRY_COMPARE(requests, 2);
		window.close();
	}
#endif

	void rejectsDoubleOpen()
	{
		if (!X11EditorWindow::isAvailable())
		{
			QSKIP("No X11/XWayland display available");
		}

		X11EditorWindow window;
		QVERIFY(window.open({}));
		QVERIFY(!window.open({}));
		window.close();
	}
};

QTEST_GUILESS_MAIN(X11EditorWindowTest)

#include "X11EditorWindowTest.moc"
