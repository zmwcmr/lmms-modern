/*
 * Vst3Instrument.h - VST3 instrument host
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

#ifndef LMMS_VST3_INSTRUMENT_H
#define LMMS_VST3_INSTRUMENT_H

#include <memory>
#include <vector>

#include <QMutex>

#include "HostedInstrument.h"
#include "Instrument.h"
#include "Vst3ParameterModels.h"

namespace lmms
{

class Vst3HostBase;
class Vst3RemoteHostClient;
class InstrumentTrack;

class Vst3Instrument : public HostedInstrument
{
	Q_OBJECT
public:
	Vst3Instrument(InstrumentTrack* instrumentTrack,
		const Plugin::Descriptor::SubPluginFeatures::Key* key);
	~Vst3Instrument() override;

	void play(SampleFrame* workingBuffer) override;
	bool handleMidiEvent(const MidiEvent& event,
		const TimePos& time = TimePos(), f_cnt_t offset = 0) override;

	void saveSettings(QDomDocument& doc, QDomElement& parent) override;
	void loadSettings(const QDomElement& parent) override;
	QString nodeName() const override;

	gui::PluginView* instantiateView(QWidget* parent) override;

	Vst3HostBase* host() const { return m_host.get(); }
	QMutex& hostMutex() { return m_mutex; }
	const Vst3ParameterModels& parameterModels() const { return m_parameterModels; }
	bool setProgram(int index);
	bool setUnit(int unitId);
	void setParameterValue(uint32_t id, double value);

private:
	bool openPlugin(const QString& file, const QString& classId);
	void closePlugin();

	std::unique_ptr<Vst3HostBase> m_host;
		QMutex m_mutex;
	QString m_file;
	QString m_classId;
	Vst3ParameterModels m_parameterModels;
};

} // namespace lmms

#endif // LMMS_VST3_INSTRUMENT_H
