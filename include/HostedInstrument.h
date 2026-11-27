/*
 * HostedInstrument.h - generic Instrument wrapper for a PluginHost
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

#ifndef LMMS_HOSTED_INSTRUMENT_H
#define LMMS_HOSTED_INSTRUMENT_H

#include <memory>
#include <vector>

#include "HostedPluginModels.h"
#include "Instrument.h"
#include "PluginHost.h"
#include "lmms_export.h"

namespace lmms
{

/*! Instrument wrapper that owns a PluginHost.
 *
 *  Format plugins subclass this, create the host in their constructor and
 *  call initializeModels() after loading. The base class handles the audio
 *  block, MIDI forwarding, latency, parameter models and a generic state
 *  representation; subclasses override the XML methods when they must keep a
 *  legacy project format (Vestige, VST3, LV2).
 */
class LMMS_EXPORT HostedInstrument : public Instrument
{
public:
	HostedInstrument(InstrumentTrack* instrumentTrack,
		const Descriptor* descriptor,
		const Descriptor::SubPluginFeatures::Key* key,
		std::unique_ptr<PluginHost> host,
		Flags flags = Flag::NoFlags);
	~HostedInstrument() override;

	void play(SampleFrame* workingBuffer) override;
	bool handleMidiEvent(const MidiEvent& event,
		const TimePos& time = TimePos(), f_cnt_t offset = 0) override;

	void saveSettings(QDomDocument& doc, QDomElement& parent) override;
	void loadSettings(const QDomElement& parent) override;

	f_cnt_t latency() const override;
	QString nodeName() const override;
	gui::PluginView* instantiateView(QWidget*) override { return nullptr; }

	//! Build the parameter models; call after the host is loaded.
	void initializeModels();

	PluginHost* host() const { return m_host.get(); }
	HostedPluginModels& parameterModels() { return m_models; }
	const HostedPluginModels& parameterModels() const { return m_models; }

protected:
	void saveHostState(QDomDocument& doc, QDomElement& parent) const;
	void loadHostState(const QDomElement& parent);

private:
	std::unique_ptr<PluginHost> m_host;
	HostedPluginModels m_models;
	std::vector<float> m_outputLeft;
	std::vector<float> m_outputRight;
	double m_lastTempo = -1.0;
	bool m_lastPlaying = false;
	bool m_transportKnown = false;
};

} // namespace lmms

#endif // LMMS_HOSTED_INSTRUMENT_H
