/*
 * Vst3InstrumentView.cpp - view for the VST3 instrument host
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

#include "Vst3InstrumentView.h"

#include <QComboBox>
#include <QVBoxLayout>

#include "AutomatableModel.h"
#include "HostedPluginPanel.h"
#include "Vst3Host.h"
#include "Vst3Instrument.h"

namespace lmms::gui
{

Vst3InstrumentView::Vst3InstrumentView(Instrument* instrument, QWidget* parent) :
	InstrumentView(instrument, parent),
	m_instrument(dynamic_cast<Vst3Instrument*>(instrument))
{
	auto* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	HostedPluginActions actions;
	actions.name = [this] {
		auto* host = m_instrument != nullptr ? m_instrument->host() : nullptr;
		return host != nullptr ? host->name() : QString();
	};
	actions.vendor = [this] {
		auto* host = m_instrument != nullptr ? m_instrument->host() : nullptr;
		return host != nullptr ? host->vendor() : QString();
	};
	actions.location = [this] {
		return m_instrument != nullptr ? m_instrument->file() : QString();
	};
	actions.hasEditor = [this] {
		return m_instrument != nullptr && m_instrument->hasEditor();
	};
	actions.showEditor = [this] {
		if (m_instrument != nullptr) { m_instrument->showEditor(this); }
	};
	actions.hideEditor = [this] {
		if (m_instrument != nullptr) { m_instrument->hideEditor(); }
	};
	actions.isEditorVisible = [this] {
		return m_instrument != nullptr && m_instrument->isEditorVisible();
	};
	actions.canReload = [this] {
		return m_instrument != nullptr && !m_instrument->file().isEmpty();
	};
	actions.reload = [this] {
		if (m_instrument != nullptr)
		{
			if (m_panel != nullptr) { m_panel->clearParameters(); }
			m_instrument->reloadPlugin();
		}
	};
	actions.presetCount = [this] {
		auto* host = m_instrument != nullptr ? m_instrument->host() : nullptr;
		return host != nullptr ? host->programCount() : 0;
	};
	actions.presetName = [this](int index) {
		auto* host = m_instrument != nullptr ? m_instrument->host() : nullptr;
		if (host == nullptr || index < 0 || index >= host->programCount())
		{
			return QString();
		}
		return host->programName(index);
	};
	actions.currentPreset = [this] {
		auto* host = m_instrument != nullptr ? m_instrument->host() : nullptr;
		return host != nullptr ? host->currentProgram() : -1;
	};
	actions.selectPreset = [this](int index) {
		if (m_instrument != nullptr) { m_instrument->setProgram(index); }
	};
	actions.canOpenPlugin = [] { return false; };
	actions.hasHelp = [] { return false; };

	m_panel = new HostedPluginPanel(actions, [this] {
		std::vector<HostedParameterEntry> entries;
		if (m_instrument == nullptr)
		{
			return entries;
		}
		const auto& models = m_instrument->parameterModels();
		for (int index = 0; index < models.count(); ++index)
		{
			auto* model = models.model(index);
			if (model == nullptr)
			{
				continue;
			}
			const auto& info = models.parameter(index);
			entries.push_back({info.name, info.unit, model,
				model->isAutomatedOrControlled()});
		}
		return entries;
	}, this);
	layout->addWidget(m_panel);
	layout->addStretch();
	m_panel->refresh();
}




void Vst3InstrumentView::modelChanged()
{
	if (m_panel != nullptr) { m_panel->refresh(); }
}

} // namespace lmms::gui
