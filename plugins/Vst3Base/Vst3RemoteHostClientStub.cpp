/*
 * Vst3RemoteHostClientStub.cpp - non-POSIX fallback for the DSP host client
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

#include "Vst3RemoteHostClient.h"

#include <cstring>

#include <QJsonObject>

namespace lmms
{

Vst3RemoteHostClient::Vst3RemoteHostClient() = default;
Vst3RemoteHostClient::~Vst3RemoteHostClient() = default;

bool Vst3RemoteHostClient::start(const QString&, const QString&, double, int,
	const QByteArray&, const QByteArray&, QString* error)
{
	if (error != nullptr)
	{
		*error = tr("Out-of-process VST3 DSP is not supported on this platform");
	}
	return false;
}

void Vst3RemoteHostClient::stop() {}
bool Vst3RemoteHostClient::isRunning() const { return false; }

bool Vst3RemoteHostClient::load(const QString&, const QString&, double, int,
	QString* error)
{
	return start({}, {}, 0.0, 0, {}, {}, error);
}

void Vst3RemoteHostClient::unload() {}
bool Vst3RemoteHostClient::isLoaded() const { return false; }
bool Vst3RemoteHostClient::isInstrument() const { return false; }

bool Vst3RemoteHostClient::setParameterNormalised(uint32_t, double) { return false; }
double Vst3RemoteHostClient::parameterNormalised(uint32_t) const { return 0.0; }
int Vst3RemoteHostClient::programCount() const { return 0; }
QString Vst3RemoteHostClient::programName(int) const { return {}; }
int Vst3RemoteHostClient::currentProgram() const { return -1; }
bool Vst3RemoteHostClient::setProgram(int) { return false; }
bool Vst3RemoteHostClient::selectUnit(int) { return false; }

void Vst3RemoteHostClient::queueParameter(uint32_t, double, int) {}
void Vst3RemoteHostClient::queueMidi(const MidiEvent&) {}
void Vst3RemoteHostClient::setTempo(double) {}
void Vst3RemoteHostClient::setPlaying(bool) {}

void Vst3RemoteHostClient::process(const float*, const float*, float* leftOut,
	float* rightOut, int frames)
{
	if (leftOut != nullptr)
	{
		std::memset(leftOut, 0, sizeof(float) * static_cast<size_t>(frames));
	}
	if (rightOut != nullptr)
	{
		std::memset(rightOut, 0, sizeof(float) * static_cast<size_t>(frames));
	}
}


QByteArray Vst3RemoteHostClient::componentState() const { return {}; }
QByteArray Vst3RemoteHostClient::controllerState() const { return {}; }
bool Vst3RemoteHostClient::setState(const QByteArray&, const QByteArray&) { return false; }

QString Vst3RemoteHostClient::hostExecutable() { return {}; }
qint64 Vst3RemoteHostClient::hostProcessId() const { return 0; }

void Vst3RemoteHostClient::sendMessage(const QJsonObject&) {}
bool Vst3RemoteHostClient::requestState() { return false; }
void Vst3RemoteHostClient::onWatchdog() {}
void Vst3RemoteHostClient::startWatchdog() {}

} // namespace lmms
