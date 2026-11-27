/*
 * Vst3PluginHost.h - PluginHost adapter for VST3
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

#ifndef LMMS_VST3_PLUGIN_HOST_H
#define LMMS_VST3_PLUGIN_HOST_H

#include <vector>

#include "PluginHost.h"
#include "Vst3Host.h"
#include "vst3base_export.h"

namespace lmms
{

/*! PluginHost adapter over any Vst3HostBase implementation.
 *
 *  Both the in-process Vst3Host and the out-of-process
 *  Vst3RemoteHostClient expose the same Vst3HostBase API, so this adapter
 *  covers both without owning the process itself.
 */
class VST3BASE_EXPORT Vst3PluginHost : public PluginHost
{
public:
	Vst3PluginHost();
	explicit Vst3PluginHost(Vst3HostBase& host);
	~Vst3PluginHost() override = default;

	//! Adopt a host created by the format wrapper; not owned.
	void adopt(Vst3HostBase* host);
	Vst3HostBase* nativeHost() const { return m_host; }

	PluginFormat format() const override;
	PluginHostCapabilities capabilities() const override;

	bool load(const PluginLoadRequest& request,
		QString* error = nullptr) override;
	void unload() override;
	bool isLoaded() const override;

	std::vector<PluginBusInfo> inputBuses() const override;
	std::vector<PluginBusInfo> outputBuses() const override;

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
	void setPlaying(bool playing) override;

	int programCount() const override;
	int currentProgram() const override;
	QString programName(int index) const override;
	bool setProgram(int index) override;

	PluginStateBlob saveState(QString* error = nullptr) override;
	bool loadState(const PluginStateBlob& state,
		QString* error = nullptr) override;

private:
	void refreshParameters();

	Vst3HostBase* m_host = nullptr;
	std::vector<PluginParameterInfo> m_parameters;
};

} // namespace lmms

#endif // LMMS_VST3_PLUGIN_HOST_H
