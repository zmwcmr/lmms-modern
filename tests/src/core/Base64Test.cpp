/*
 * Base64Test.cpp - tests for the base64 helpers
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

#include "base64.h"

class Base64Test : public QObject
{
	Q_OBJECT

private slots:
	void decodeExactFloatCount()
	{
		QByteArray raw("abcdefgh");
		float* data = nullptr;
		int size = 0;
		lmms::base64::decode(QString::fromLatin1(raw.toBase64()), &data, &size);
		QCOMPARE(size, 8);
		QVERIFY(data != nullptr);
		QCOMPARE(QByteArray(reinterpret_cast<const char*>(data), size), raw);
		delete[] data;
	}

	void decodeTruncatesPartialElement()
	{
		// Five bytes cannot be represented by a whole number of floats.
		// The decoder must not copy the trailing byte into a 4-byte
		// allocation.
		QByteArray raw("abcde");
		float* data = nullptr;
		int size = 0;
		lmms::base64::decode(QString::fromLatin1(raw.toBase64()), &data, &size);
		QCOMPARE(size, 4);
		QVERIFY(data != nullptr);
		QCOMPARE(QByteArray(reinterpret_cast<const char*>(data), size), raw.left(4));
		delete[] data;
	}

	void decodeEmpty()
	{
		float* data = nullptr;
		int size = -1;
		lmms::base64::decode(QString(), &data, &size);
		QCOMPARE(size, 0);
		QVERIFY(data != nullptr);
		delete[] data;
	}
};

QTEST_MAIN(Base64Test)

#include "Base64Test.moc"
