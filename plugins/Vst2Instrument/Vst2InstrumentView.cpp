/*
 * Vst2InstrumentView.cpp - view for the VST2 instrument host
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

#include "Vst2InstrumentView.h"

#include <QDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QVBoxLayout>

#include "AudioEngine.h"
#include "ConfigManager.h"
#include "Engine.h"
#include "FileDialog.h"
#include "HostedPluginPanel.h"
#include "Vst2HostedInstrument.h"
#include "Vst2PluginHost.h"
#include "VstPlugin.h"

namespace lmms::gui
{

Vst2InstrumentView::Vst2InstrumentView(Instrument* instrument, QWidget* parent) :
	InstrumentView(instrument, parent),
	m_instrument(dynamic_cast<Vst2HostedInstrument*>(instrument))
{
	auto* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	HostedPluginActions actions;
	actions.name = [this] {
		auto* p = plugin();
		return p != nullptr ? p->name() : QString();
	};
	actions.vendor = [this] {
		auto* p = plugin();
		return p != nullptr ? p->vendorString() : QString();
	};
	actions.location = [this] {
		return m_instrument != nullptr ? m_instrument->file() : QString();
	};
	actions.hasEditor = [this] {
		auto* p = plugin();
		return p != nullptr && p->hasEditorSupport();
	};
	actions.showEditor = [this] {
		if (m_instrument != nullptr) { m_instrument->showEditor(); }
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
		auto* host = m_instrument != nullptr ? m_instrument->vst2Host() : nullptr;
		return host != nullptr ? host->programCount() : 0;
	};
	actions.presetName = [this](int index) {
		auto* host = m_instrument != nullptr ? m_instrument->vst2Host() : nullptr;
		return host != nullptr ? host->programName(index) : QString();
	};
	actions.currentPreset = [this] {
		auto* host = m_instrument != nullptr ? m_instrument->vst2Host() : nullptr;
		return host != nullptr ? host->currentProgram() : -1;
	};
	actions.selectPreset = [this](int index) {
		auto* host = m_instrument != nullptr ? m_instrument->vst2Host() : nullptr;
		if (host != nullptr) { host->setProgram(index); }
	};
	actions.canOpenPlugin = [] { return true; };
	actions.openPlugin = [this](QWidget* parent) { openPluginFile(parent); };
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




VstPlugin* Vst2InstrumentView::plugin() const
{
	return m_instrument != nullptr ? m_instrument->nativePlugin() : nullptr;
}




void Vst2InstrumentView::modelChanged()
{
	if (m_panel != nullptr) { m_panel->refresh(); }
}




void Vst2InstrumentView::openPluginFile(QWidget* parent)
{
	if (m_instrument == nullptr)
	{
		return;
	}

	FileDialog dialog(parent != nullptr ? parent : this,
		tr("Open VST2 plugin"));
	QStringList types;
#if defined(LMMS_BUILD_WIN32)
	types << tr("VST2 files (*.dll)");
#elif defined(LMMS_BUILD_LINUX)
#	if defined(LMMS_HAVE_VST_32) || defined(LMMS_HAVE_VST_64)
	types << tr("All VST2 files (*.dll *.so)")
		<< tr("Windows VST2 files (*.dll)");
#	endif
	types << tr("Linux VST2 files (*.so)");
#endif
	dialog.setNameFilters(types);

	const QString current = m_instrument->file();
	if (!current.isEmpty())
	{
		dialog.setDirectory(QFileInfo(current).absolutePath());
		dialog.selectFile(QFileInfo(current).fileName());
	}
	else
	{
		dialog.setDirectory(ConfigManager::inst()->vstDir());
	}

	if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty())
	{
		return;
	}

	if (m_panel != nullptr) { m_panel->clearParameters(); }
	if (auto* engine = Engine::audioEngine())
	{
		engine->requestChangeInModel();
	}
	const bool loaded = m_instrument->openFile(dialog.selectedFiles().first());
	if (auto* engine = Engine::audioEngine())
	{
		engine->doneChangeInModel();
	}

	if (loaded)
	{
		if (m_instrument != nullptr) { m_instrument->showEditor(); }
	}
	if (m_panel != nullptr) { m_panel->refresh(); }
	if (!loaded)
	{
		QMessageBox::warning(this, tr("VST2 plugin"),
			tr("The selected VST2 plugin could not be loaded."));
	}
}

} // namespace lmms::gui
