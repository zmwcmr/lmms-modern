/*
 * Vst3Host.h - VST3 host interface and in-process implementation
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
#include <functional>
#include <memory>
#include <vector>

#include <QByteArray>
#include <QString>

#include "vst3base_export.h"

namespace lmms
{

/**
 * Common interface for the in-process VST3 host and the out-of-process DSP
 * host client.
 */
class VST3BASE_EXPORT Vst3HostBase
{
public:
	struct Unit
	{
		int id = 0;
		int parentId = -1;
		QString name;
	};

	//! One audio bus as reported by the component.
	struct BusInfo
	{
		QString name;
		int channels = 0;
		bool isInput = true;
		bool isMain = true;
		bool active = false;
	};

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

	virtual ~Vst3HostBase() = default;

	virtual bool load(const QString& bundlePath,
		const QString& classId,
		double sampleRate,
		int maxBlockSize,
		QString* error = nullptr) = 0;
	virtual void unload() = 0;

	virtual bool isLoaded() const = 0;
	virtual bool isInstrument() const = 0;

	virtual QString name() const = 0;
	virtual QString vendor() const = 0;
	virtual QString version() const = 0;
	virtual QString bundlePath() const = 0;
	virtual QString classId() const = 0;

	virtual const std::vector<Parameter>& parameters() const = 0;

	//! Audio bus layout; the default is one active main stereo bus.
	virtual std::vector<BusInfo> inputBuses() const
	{
		return { BusInfo{ QStringLiteral("Main"), 2, true, true, true } };
	}
	virtual std::vector<BusInfo> outputBuses() const
	{
		return { BusInfo{ QStringLiteral("Main"), 2, false, true, true } };
	}
	virtual bool setParameterNormalised(uint32_t id, double value) = 0;
	virtual double parameterNormalised(uint32_t id) const = 0;

	virtual int programCount() const = 0;
	virtual QString programName(int index) const = 0;
	virtual int currentProgram() const = 0;
	virtual bool setProgram(int index) = 0;

	virtual const std::vector<Unit>& units() const = 0;
	virtual int currentUnit() const = 0;
	virtual bool selectUnit(int unitId) = 0;

	virtual void queueParameter(uint32_t id, double value, int sampleOffset) = 0;
	virtual void queueMidi(const MidiEvent& event) = 0;

	virtual void setTempo(double bpm) = 0;
	virtual void setPlaying(bool playing) = 0;

	virtual void process(const float* leftIn,
		const float* rightIn,
		float* leftOut,
		float* rightOut,
		int frames) = 0;

	virtual int latencySamples() const = 0;

	virtual bool hasEditor() const = 0;
	virtual bool openEditor(void* parentWindow, QString* error = nullptr) = 0;
	virtual void closeEditor() = 0;
	virtual bool isEditorOpen() const = 0;
	virtual bool editorSize(int& width, int& height) const = 0;
	virtual bool canResizeEditor() const = 0;
	virtual void resizeEditor(int width, int height) = 0;
	virtual void setEditorContentScale(float scale) = 0;
	virtual void focusEditor(bool focused) = 0;
	virtual void setEditorResizeCallback(std::function<void(int, int)> callback) = 0;

	virtual QByteArray componentState() const = 0;
	virtual QByteArray controllerState() const = 0;
	virtual bool setState(const QByteArray& componentState,
		const QByteArray& controllerState) = 0;
};

/**
 * Minimal in-process VST3 host. It owns the component, controller and
 * processor and exposes only the stereo main bus described in the plan.
 *
 * This class is not thread-safe. The owning plugin must serialize process()
 * and parameter/state access, normally with a mutex that is try-locked on
 * the audio thread.
 */
class VST3BASE_EXPORT Vst3Host : public Vst3HostBase
{
public:
	Vst3Host();
	~Vst3Host() override;

	Vst3Host(const Vst3Host&) = delete;
	Vst3Host& operator=(const Vst3Host&) = delete;

	bool load(const QString& bundlePath,
		const QString& classId,
		double sampleRate,
		int maxBlockSize,
		QString* error = nullptr) override;
	void unload() override;

	bool isLoaded() const override;
	bool isInstrument() const override;

	QString name() const override;
	QString vendor() const override;
	QString version() const override;
	QString bundlePath() const override;
	QString classId() const override;

	//! Called when the plugin itself changes a parameter (e.g. from its UI).
	void setParameterChangeCallback(std::function<void(uint32_t, double)> callback);

	const std::vector<Parameter>& parameters() const override;
	std::vector<BusInfo> inputBuses() const override;
	std::vector<BusInfo> outputBuses() const override;
	bool setParameterNormalised(uint32_t id, double value) override;
	double parameterNormalised(uint32_t id) const override;

	int programCount() const override;
	QString programName(int index) const override;
	int currentProgram() const override;
	bool setProgram(int index) override;

	const std::vector<Unit>& units() const override;
	int currentUnit() const override;
	bool selectUnit(int unitId) override;

	//! Queue a parameter change for the next process() call.
	void queueParameter(uint32_t id, double value, int sampleOffset) override;

	void queueMidi(const MidiEvent& event) override;
	void clearMidi();

	void setTempo(double bpm) override;
	void setPlaying(bool playing) override;

	//! Process one stereo block. Input pointers may be null for instruments.
	void process(const float* leftIn,
		const float* rightIn,
		float* leftOut,
		float* rightOut,
		int frames) override;

	int latencySamples() const override;

	//! True if the plugin exposes an IPlugView editor.
	bool hasEditor() const override;
	//! Create and attach the native editor to an X11 window id.
	bool openEditor(void* parentWindow, QString* error = nullptr) override;
	void closeEditor() override;
	bool isEditorOpen() const override;
	bool editorSize(int& width, int& height) const override;
	bool canResizeEditor() const override;
	void resizeEditor(int width, int height) override;
	void setEditorContentScale(float scale) override;
	void focusEditor(bool focused) override;
	void setEditorResizeCallback(std::function<void(int, int)> callback) override;

	QByteArray componentState() const override;
	QByteArray controllerState() const override;
	bool setState(const QByteArray& componentState,
		const QByteArray& controllerState) override;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};

} // namespace lmms

#endif // LMMS_VST3_HOST_H
