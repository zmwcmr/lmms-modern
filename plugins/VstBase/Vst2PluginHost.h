/*
 * Vst2PluginHost.h - PluginHost adapter for VST2
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

#ifndef LMMS_VST2_PLUGIN_HOST_H
#define LMMS_VST2_PLUGIN_HOST_H

#include <memory>
#include <vector>

#include <QStringList>

#include "PluginHost.h"
#include "SampleFrame.h"
#include "vstbase_export.h"

namespace lmms
{

class VstPlugin;

/*! PluginHost adapter for the remote VST2 host.
 *
 *  The adapter owns one VstPlugin, which in turn owns the platform-specific
 *  host process (native 64-bit, 32-bit Wine, ...). Parameter IDs are the flat
 *  VST program parameter indices. The values are cached from the parameter
 *  dump so the audio thread never performs a synchronous plugin round-trip;
 *  changes made inside the plugin UI are picked up on the next state or
 *  program refresh.
 */
class VSTBASE_EXPORT Vst2PluginHost : public PluginHost
{
public:
	Vst2PluginHost();
	~Vst2PluginHost() override;

	PluginFormat format() const override;
	PluginHostCapabilities capabilities() const override;

	bool load(const PluginLoadRequest& request,
		QString* error = nullptr) override;
	void unload() override;
	bool isLoaded() const override;

	const std::vector<PluginParameterInfo>& parameters() const override
	{
		return m_parameters;
	}
	bool setParameter(uint32_t id, double value,
		int sampleOffset = 0) override;
	double parameterValue(uint32_t id) const override;

	void sendMidi(const PluginMidiEvent& event) override;
	bool process(const PluginProcessBlock& block) override;

	int latencySamples() const override;
	void setTempo(double bpm) override;

	int programCount() const override;
	int currentProgram() const override;
	QString programName(int index) const override;
	bool setProgram(int index) override;

	PluginStateBlob saveState(QString* error = nullptr) override;
	bool loadState(const PluginStateBlob& state,
		QString* error = nullptr) override;

private:
	void rebuildParameters();
	void refreshValues();
	void ensureBuffers(int frames);

	std::unique_ptr<VstPlugin> m_plugin;
	std::vector<PluginParameterInfo> m_parameters;
	std::vector<double> m_values;
	QStringList m_programNames;
	std::vector<SampleFrame> m_inputFrames;
	std::vector<SampleFrame> m_outputFrames;
};

} // namespace lmms

#endif // LMMS_VST2_PLUGIN_HOST_H
