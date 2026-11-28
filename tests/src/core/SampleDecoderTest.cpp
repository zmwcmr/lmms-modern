/*
 * SampleDecoderTest.cpp - tests for the shared sample decoder
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

#include <algorithm>
#include <cmath>

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#ifdef Q_OS_UNIX
#include <sys/stat.h>
#endif

#include "Engine.h"
#include "SampleDecoder.h"

using namespace lmms;

namespace
{

QByteArray makeWav(int channels, int sampleRate, const std::vector<qint16>& samples)
{
	Q_ASSERT(channels > 0);
	Q_ASSERT(samples.size() % static_cast<std::size_t>(channels) == 0);

	const auto dataSize = static_cast<quint32>(samples.size() * sizeof(qint16));
	QByteArray out;
	QDataStream stream(&out, QIODevice::WriteOnly);
	stream.setByteOrder(QDataStream::LittleEndian);

	stream.writeRawData("RIFF", 4);
	stream << quint32(36 + dataSize);
	stream.writeRawData("WAVE", 4);

	stream.writeRawData("fmt ", 4);
	stream << quint32(16);
	stream << quint16(1); // PCM
	stream << quint16(channels);
	stream << quint32(sampleRate);
	stream << quint32(sampleRate * channels * 2);
	stream << quint16(channels * 2);
	stream << quint16(16);

	stream.writeRawData("data", 4);
	stream << dataSize;
	for (const auto sample : samples)
	{
		stream << sample;
	}
	return out;
}

} // namespace

class SampleDecoderTest : public QObject
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

	void decodesStereoPcmWav()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const QString path = dir.filePath("stereo.wav");
		QFile file(path);
		QVERIFY(file.open(QIODevice::WriteOnly));
		const QByteArray wav = makeWav(
			2, 44100, {0, 32767, -32768, 0, 16384, -16384, 1, -1});
		QCOMPARE(file.write(wav), qint64(wav.size()));
		file.close();

		const auto result = SampleDecoder::decode(path);
		QVERIFY(result.has_value());
		QCOMPARE(result->sampleRate, 44100);
		QCOMPARE(result->data.size(), std::size_t(4));
		QVERIFY(std::abs(result->data[0].left() - 0.0f) < 1e-6f);
		QVERIFY(std::abs(result->data[0].right() - 1.0f) < 1e-4f);
		QVERIFY(std::abs(result->data[1].left() + 1.0f) < 1e-4f);
		QVERIFY(std::abs(result->data[2].left() - 0.5f) < 1e-4f);
		QVERIFY(std::abs(result->data[2].right() + 0.5f) < 1e-4f);
	}

	void upmixesMonoToStereo()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const QString path = dir.filePath("mono.wav");
		QFile file(path);
		QVERIFY(file.open(QIODevice::WriteOnly));
		const QByteArray wav = makeWav(1, 22050, {0, 16384, -16384});
		QCOMPARE(file.write(wav), qint64(wav.size()));
		file.close();

		const auto result = SampleDecoder::decode(path);
		QVERIFY(result.has_value());
		QCOMPARE(result->sampleRate, 22050);
		QCOMPARE(result->data.size(), std::size_t(3));
		for (const auto& frame : result->data)
		{
			QCOMPARE(frame.left(), frame.right());
		}
		QVERIFY(std::abs(result->data[1].left() - 0.5f) < 1e-4f);
	}

	void rejectsMissingFile()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		QVERIFY(!SampleDecoder::decode(dir.filePath("does-not-exist.wav")).has_value());
	}

	void rejectsCorruptFile()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const QString path = dir.filePath("corrupt.wav");
		QFile file(path);
		QVERIFY(file.open(QIODevice::WriteOnly));
		file.write(QByteArray("this is not an audio file at all", 32));
		file.close();
		QVERIFY(!SampleDecoder::decode(path).has_value());
	}

	void rejectsEmptyFile()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const QString path = dir.filePath("empty.wav");
		QFile file(path);
		QVERIFY(file.open(QIODevice::WriteOnly));
		file.close();
		QVERIFY(!SampleDecoder::decode(path).has_value());
	}

	void rejectsDeviceNodesAndFifos()
	{
#ifdef Q_OS_UNIX
		// Opening a FIFO or device for reading can block forever. The
		// decoder must reject anything that is not a regular file.
		QVERIFY(!SampleDecoder::decode("/dev/null").has_value());

		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const QByteArray fifo = QFile::encodeName(dir.filePath("sample.wav"));
		QCOMPARE(::mkfifo(fifo.constData(), 0600), 0);
		QVERIFY(!SampleDecoder::decode(QString::fromLocal8Bit(fifo)).has_value());
#endif
	}

	void advertisesSupportedTypes()
	{
		const auto& types = SampleDecoder::supportedAudioTypes();
		QVERIFY(!types.empty());
		const auto hasExtension = [&types](const std::string& extension) {
			return std::ranges::any_of(types, [&extension](const auto& type) {
				return type.extension == extension;
			});
		};
		QVERIFY(hasExtension("ds"));
		QVERIFY(hasExtension("wav"));
	}
};

QTEST_MAIN(SampleDecoderTest)

#include "SampleDecoderTest.moc"
