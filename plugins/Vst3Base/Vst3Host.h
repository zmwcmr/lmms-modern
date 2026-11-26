/*
 * Vst3Host.h - in-process VST3 host
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

#ifndef LMMS_VST3_HOST_H
#define LMMS_VST3_HOST_H

#include <cstdint>
#include <memory>
#include <vector>

#include <QByteArray>
#include <QString>

#include "vst3base_export.h"

namespace lmms
{

/**
 * Minimal in-process VST3 host. It owns the component, controller and
 * processor and exposes only the stereo main bus described in the plan.
 *
 * This class is not thread-safe. The owning plugin must serialize process()
 * and parameter/state access, normally with a mutex that is try-locked on
 * the audio thread.
 */
class VST3BASE_EXPORT Vst3Host
{
public:
	struct Parameter
	{
		uint32_t id = 0;
		QString name;
		QString unit;
		double defaultNormalised = 0.0;
		int stepCount = 0;
		int flags = 0;
		bool automatable = false;
		bool readOnly = false;
		bool hidden = false;
	};

	enum class MidiType
	{
		NoteOn,
		NoteOff,
		ControlChange,
		PitchBend,
		ChannelPressure,
		PolyPressure,
		ProgramChange,
	};

	struct MidiEvent
	{
		MidiType type = MidiType::NoteOn;
		int sampleOffset = 0;
		int channel = 0;
		int data1 = 0;
		int data2 = 0;
		float value = 0.0f;
	};

	Vst3Host();
	~Vst3Host();

	Vst3Host(const Vst3Host&) = delete;
	Vst3Host& operator=(const Vst3Host&) = delete;

	bool load(const QString& bundlePath,
		const QString& classId,
		double sampleRate,
		int maxBlockSize,
		QString* error = nullptr);
	void unload();

	bool isLoaded() const;
	bool isInstrument() const;

	QString name() const;
	QString vendor() const;
	QString version() const;

	const std::vector<Parameter>& parameters() const;
	bool setParameterNormalised(uint32_t id, double value);
	double parameterNormalised(uint32_t id) const;

	//! Queue a parameter change for the next process() call.
	void queueParameter(uint32_t id, double value, int sampleOffset);

	void queueMidi(const MidiEvent& event);
	void clearMidi();

	void setTempo(double bpm);
	void setPlaying(bool playing);

	//! Process one stereo block. Input pointers may be null for instruments.
	void process(const float* leftIn,
		const float* rightIn,
		float* leftOut,
		float* rightOut,
		int frames);

	int latencySamples() const;

	QByteArray componentState() const;
	QByteArray controllerState() const;
	bool setState(const QByteArray& componentState,
		const QByteArray& controllerState);

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};

} // namespace lmms

#endif // LMMS_VST3_HOST_H
