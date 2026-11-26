/*
 * Lv2HostClientTest.cpp - tests for the LV2 host child transport
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

#include <QElapsedTimer>
#include <QProcess>

#include <vector>

#include "Lv2HostClient.h"
#include "Lv2HostProtocol.h"

using namespace lmms;

namespace
{

constexpr f_cnt_t BlockSize = 256;

void fillRamp(std::vector<float>& buffer, float scale)
{
	for (std::size_t i = 0; i < buffer.size(); ++i)
	{
		buffer[i] = scale * static_cast<float>(i) / static_cast<float>(buffer.size());
	}
}

} // namespace

class Lv2HostClientTest : public QObject
{
	Q_OBJECT

private slots:
	void echoesInputBlock();
	void survivesChildKill();
};




void Lv2HostClientTest::echoesInputBlock()
{
	Lv2HostClient client;
	QVERIFY2(client.start(QStringLiteral(LMMS_LV2_HOST_EXECUTABLE),
		QStringLiteral("--echo"), 44100, BlockSize, 15000),
		qPrintable(client.errorString()));
	QVERIFY(client.isRunning());
	QCOMPARE(client.framesPerBlock(), BlockSize);

	std::vector<float> inLeft(BlockSize);
	std::vector<float> inRight(BlockSize);
	std::vector<float> outLeft(BlockSize, -1.0f);
	std::vector<float> outRight(BlockSize, -1.0f);
	fillRamp(inLeft, 1.0f);
	fillRamp(inRight, -2.0f);

	const float* in[2] = { inLeft.data(), inRight.data() };
	float* out[2] = { outLeft.data(), outRight.data() };

	QVERIFY(client.processBlock(in, out, BlockSize));

	for (f_cnt_t i = 0; i < BlockSize; ++i)
	{
		QCOMPARE(outLeft[i], inLeft[i]);
		QCOMPARE(outRight[i], inRight[i]);
	}

	client.stop();
	QVERIFY(!client.isRunning());
}




void Lv2HostClientTest::survivesChildKill()
{
#ifndef Q_OS_UNIX
	QSKIP("The crash test needs POSIX signals");
#endif
	Lv2HostClient client;
	QVERIFY2(client.start(QStringLiteral(LMMS_LV2_HOST_EXECUTABLE),
		QStringLiteral("--echo"), 44100, BlockSize, 15000),
		qPrintable(client.errorString()));

	const qint64 pid = client.processId();
	QVERIFY(pid > 0);
	QCOMPARE(QProcess::execute(QStringLiteral("kill"),
		{ QStringLiteral("-9"), QString::number(pid) }), 0);

	// The block must fail quickly instead of hanging on a dead child.
	std::vector<float> buffer(BlockSize);
	const float* in[2] = { buffer.data(), buffer.data() };
	std::vector<float> outLeft(BlockSize, 0.5f);
	std::vector<float> outRight(BlockSize, 0.5f);
	float* out[2] = { outLeft.data(), outRight.data() };

	QElapsedTimer timer;
	timer.start();
	QVERIFY(!client.processBlock(in, out, BlockSize));
	QVERIFY2(timer.elapsed() < 6000, "processBlock() blocked on a dead child");

	// A failed block must not leak the previous content.
	QCOMPARE(outLeft[0], 0.0f);
	QCOMPARE(outRight[BlockSize - 1], 0.0f);

	QTRY_VERIFY_WITH_TIMEOUT(!client.isRunning(), 5000);
	QVERIFY(!client.errorString().isEmpty());
	client.stop();
}


QTEST_GUILESS_MAIN(Lv2HostClientTest)
#include "Lv2HostClientTest.moc"
