/*
 * host_test.cpp - command line smoke test for Vst3Host
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
#include <QFile>
#include <QStringList>
#include <QTextStream>

#include "Vst3Host.h"

namespace
{

void printUsage()
{
	QTextStream(stderr)
		<< "Usage: vst3-host-test <bundle.vst3> <class-id> [seconds] [--effect]\n";
}

lmms::Vst3Host::MidiEvent noteEvent(
	lmms::Vst3Host::MidiType type, int note, float velocity)
{
	lmms::Vst3Host::MidiEvent event;
	event.type = type;
	event.channel = 0;
	event.data1 = note;
	event.value = velocity;
	return event;
}

} // namespace

int main(int argc, char* argv[])
{
	QCoreApplication application(argc, argv);
	const auto arguments = application.arguments();
	if (arguments.size() < 3)
	{
		printUsage();
		return 2;
	}

	const auto bundlePath = arguments.at(1);
	const auto classId = arguments.at(2);
	const double seconds = arguments.size() > 3
		? arguments.at(3).toDouble()
		: 2.0;
	const bool effect = arguments.contains(QStringLiteral("--effect"));

	constexpr double sampleRate = 48000.0;
	constexpr int blockSize = 512;

	lmms::Vst3Host host;
	QString error;
	if (!host.load(bundlePath, classId, sampleRate, blockSize, &error))
	{
		QTextStream(stderr) << "Load failed: " << error << '\n';
		return 1;
	}

	QTextStream(stdout) << "name=" << host.name()
		<< " vendor=" << host.vendor()
		<< " version=" << host.version()
		<< " instrument=" << host.isInstrument()
		<< " parameters=" << host.parameters().size()
		<< " programs=" << host.programCount()
		<< " current-program=" << host.currentProgram()
		<< " units=" << host.units().size()
		<< " current-unit=" << host.currentUnit()
		<< " latency=" << host.latencySamples() << '\n';
	if (host.programCount() > 0)
	{
		QTextStream(stdout) << "program-0=" << host.programName(0) << '\n';
	}
	if (host.programCount() > 5)
	{
		host.setProgram(5);
		QTextStream(stdout) << "selected-program=" << host.currentProgram()
			<< '/' << host.programName(5) << '\n';
	}

	for (const auto& parameter : host.parameters())
	{
		if (parameter.automatable && !parameter.readOnly && !parameter.hidden)
		{
			QTextStream(stdout) << "first-parameter=" << parameter.name
				<< " default=" << host.parameterNormalised(parameter.id) << '\n';
			host.setParameterNormalised(parameter.id, 0.75);
			break;
		}
	}

	const auto componentState = host.componentState();
	const auto controllerState = host.controllerState();
	QTextStream(stdout) << "state-sizes=" << componentState.size()
		<< '/' << controllerState.size() << '\n';
	if (!componentState.isEmpty() || !controllerState.isEmpty())
	{
		if (!host.setState(componentState, controllerState))
		{
			QTextStream(stderr) << "State round-trip failed\n";
		}
	}

	std::vector<float> inputLeft(blockSize, 0.0f);
	std::vector<float> inputRight(blockSize, 0.0f);
	std::vector<float> outputLeft(blockSize, 0.0f);
	std::vector<float> outputRight(blockSize, 0.0f);

	QFile rawFile(QStringLiteral("/tmp/vst3-host-test.raw"));
	if (!rawFile.open(QFile::WriteOnly))
	{
		QTextStream(stderr) << "Cannot write /tmp/vst3-host-test.raw\n";
	}

	const int totalFrames = static_cast<int>(seconds * sampleRate);
	double sumSquares = 0.0;
	float peak = 0.0f;
	int sampleCount = 0;
	bool noteOnSent = false;
	bool noteOffSent = false;

	for (int position = 0; position < totalFrames; position += blockSize)
	{
		const int frames = std::min(blockSize, totalFrames - position);
		if (!effect && !noteOnSent)
		{
			host.queueMidi(noteEvent(lmms::Vst3Host::MidiType::NoteOn, 60, 0.8f));
			noteOnSent = true;
		}
		if (!effect && !noteOffSent && position >= totalFrames / 2)
		{
			host.queueMidi(noteEvent(lmms::Vst3Host::MidiType::NoteOff, 60, 0.0f));
			noteOffSent = true;
		}

		if (effect)
		{
			for (int frame = 0; frame < frames; ++frame)
			{
				const auto phase = 2.0 * M_PI * 440.0
					* static_cast<double>(position + frame) / sampleRate;
				inputLeft[frame] = 0.2f * static_cast<float>(std::sin(phase));
				inputRight[frame] = inputLeft[frame];
			}
		}

		host.process(effect ? inputLeft.data() : nullptr,
			effect ? inputRight.data() : nullptr,
			outputLeft.data(),
			outputRight.data(),
			frames);

		for (int frame = 0; frame < frames; ++frame)
		{
			const auto left = outputLeft[frame];
			const auto right = outputRight[frame];
			peak = std::max({peak, std::abs(left), std::abs(right)});
			sumSquares += left * left + right * right;
			sampleCount += 2;
		}

		if (rawFile.isOpen())
		{
			for (int frame = 0; frame < frames; ++frame)
			{
				const float frameData[2] = {outputLeft[frame], outputRight[frame]};
				rawFile.write(reinterpret_cast<const char*>(frameData),
					sizeof(frameData));
			}
		}
	}

	rawFile.close();
	const double rms = sampleCount > 0 ? std::sqrt(sumSquares / sampleCount) : 0.0;
	QTextStream(stdout) << "peak=" << peak << " rms=" << rms << '\n';
	return peak > 0.0f ? 0 : 3;
}
