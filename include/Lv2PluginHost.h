/*
 * Lv2PluginHost.h - PluginHost adapter for LV2
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

#ifndef LMMS_LV2_PLUGIN_HOST_H
#define LMMS_LV2_PLUGIN_HOST_H

#include "lmmsconfig.h"

#ifdef LMMS_HAVE_LV2

#include <deque>
#include <memory>
#include <vector>

#include "Lv2Proc.h"
#include "PluginHost.h"
#include "lmms_export.h"

namespace lmms
{

/*! PluginHost adapter for one LV2 plugin instance.
 *
 *  The adapter owns the Lv2Proc, so it can create and destroy the plugin
 *  instance without touching the models of a surrounding effect or
 *  instrument. Parameter IDs are LV2 port indices. The state blob is an XML
 *  fragment containing the model values plus the plugin's state extension.
 *
 *  The editor is not available through this adapter yet: LV2 UIs need the
 *  GUI layer and are unified in the editor-host stage.
 */
class LMMS_EXPORT Lv2PluginHost : public PluginHost
{
public:
	//! @param parent model that receives the parameter models; must outlive
	//!        the adapter
	explicit Lv2PluginHost(Model* parent);
	~Lv2PluginHost() override;

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

	int programCount() const override;
	int currentProgram() const override { return m_currentProgram; }
	QString programName(int index) const override;
	bool setProgram(int index) override;

	PluginStateBlob saveState(QString* error = nullptr) override;
	bool loadState(const PluginStateBlob& state,
		QString* error = nullptr) override;

private:
	void refreshParameters();
	Lv2Ports::Control* controlAt(uint32_t id) const;
	void copyInput(const PluginProcessBlock& block);
	void copyOutput(const PluginProcessBlock& block) const;

	Model* m_parent;
	std::unique_ptr<Lv2Proc> m_proc;
	std::vector<PluginParameterInfo> m_parameters;
	//! Control port lookup indexed by the same port index used in the IDs.
	std::vector<Lv2Ports::Control*> m_controls;
	//! Keeps SysEx payloads alive until the next process() call.
	std::deque<QByteArray> m_sysexStorage;
	int m_currentProgram = -1;
};

} // namespace lmms

#endif // LMMS_HAVE_LV2

#endif // LMMS_LV2_PLUGIN_HOST_H
