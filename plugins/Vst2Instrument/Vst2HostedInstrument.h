/*
 * Vst2HostedInstrument.h - VST2 instrument host
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

#ifndef LMMS_VST2_INSTRUMENT_H
#define LMMS_VST2_INSTRUMENT_H

#include <QMutex>
#include <QString>

#include "HostedInstrument.h"

namespace lmms
{

class Vst2PluginHost;
class VstPlugin;

namespace gui
{
class PluginView;
}

/*! First-class VST2 instrument host.
 *
 *  This is the default VST2 entry point. It owns Vst2PluginHost, which in turn
 *  owns the remote VstPlugin process. VeSTige remains as a separate legacy
 *  implementation for older projects.
 */
class Vst2HostedInstrument : public HostedInstrument
{
	Q_OBJECT
public:
	Vst2HostedInstrument(InstrumentTrack* instrumentTrack,
		const Plugin::Descriptor::SubPluginFeatures::Key* key);
	~Vst2HostedInstrument() override = default;

	void play(SampleFrame* workingBuffer) override;
	bool handleMidiEvent(const MidiEvent& event,
		const TimePos& time = TimePos(), f_cnt_t offset = 0) override;

	void saveSettings(QDomDocument& doc, QDomElement& parent) override;
	void loadSettings(const QDomElement& parent) override;
	QString nodeName() const override;

	gui::PluginView* instantiateView(QWidget* parent) override;

	Vst2PluginHost* vst2Host() const;
	VstPlugin* nativePlugin() const;

	const QString& file() const { return m_file; }
	bool openFile(const QString& file);
	bool reloadPlugin();
	void requestShowEditor() override;
	void setProgram(int index);

private:
	bool openPlugin(const QString& file);
	void closePlugin();
	void connectPluginSignals(VstPlugin* plugin);

	QString m_file;
	QMutex m_mutex;
	bool m_updatingFromPlugin = false;
};

} // namespace lmms

#endif // LMMS_VST2_INSTRUMENT_H
