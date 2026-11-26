/*
 * remote_host_test.cpp - out-of-process DSP host smoke test
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
#include <cstdio>
#include <vector>

#include <QCoreApplication>
#include <QTextStream>

#include "Vst3RemoteHostClient.h"

int main(int argc, char* argv[])
{
	QCoreApplication application(argc, argv);
	const auto arguments = application.arguments();
	if (arguments.size() < 3)
	{
		QTextStream(stderr)
			<< "Usage: vst3-remote-host-test <bundle.vst3> <class-id>\n";
		return 2;
	}

	lmms::Vst3RemoteHostClient client;
	QString error;
	if (!client.start(arguments.at(1), arguments.at(2), 48000.0, 512,
			QByteArray(), QByteArray(), &error))
	{
		QTextStream(stderr) << "Start failed: " << error << '\n';
		return 1;
	}

	const auto componentState = client.componentState();
	const auto controllerState = client.controllerState();
	QTextStream(stdout) << "parameters=" << client.parameters().size()
		<< " latency=" << client.latencySamples()
		<< " state=" << componentState.size() << '/' << controllerState.size()
		<< '\n';
	if (!componentState.isEmpty() || !controllerState.isEmpty())
	{
		client.setState(componentState, controllerState);
	}

	constexpr int blockSize = 512;
	std::vector<float> outputLeft(blockSize);
	std::vector<float> outputRight(blockSize);
	float peak = 0.0f;
	double sumSquares = 0.0;
	int sampleCount = 0;
	constexpr int totalFrames = 48000;
	bool noteOn = false;
	bool noteOff = false;

	for (int position = 0; position < totalFrames; position += blockSize)
	{
		const int frames = std::min(blockSize, totalFrames - position);
		if (!noteOn)
		{
			lmms::Vst3Host::MidiEvent event;
			event.type = lmms::Vst3Host::MidiType::NoteOn;
			event.data1 = 60;
			event.value = 0.8f;
			client.queueMidi(event);
			noteOn = true;
		}
		if (!noteOff && position >= totalFrames / 2)
		{
			lmms::Vst3Host::MidiEvent event;
			event.type = lmms::Vst3Host::MidiType::NoteOff;
			event.data1 = 60;
			client.queueMidi(event);
			noteOff = true;
		}

		client.process(nullptr, nullptr, outputLeft.data(), outputRight.data(),
			frames);
		for (int frame = 0; frame < frames; ++frame)
		{
			peak = std::max({peak, std::abs(outputLeft[frame]),
				std::abs(outputRight[frame])});
			sumSquares += outputLeft[frame] * outputLeft[frame]
				+ outputRight[frame] * outputRight[frame];
			sampleCount += 2;
		}
	}

	client.stop();
	const double rms = sampleCount > 0 ? std::sqrt(sumSquares / sampleCount) : 0.0;
	QTextStream(stdout) << "peak=" << peak << " rms=" << rms << '\n';
	return peak > 0.0f ? 0 : 3;
}
