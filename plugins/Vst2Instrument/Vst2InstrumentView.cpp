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

#include <algorithm>

#include <QComboBox>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "AudioEngine.h"
#include "ConfigManager.h"
#include "EditorPlatform.h"
#include "Engine.h"
#include "FileDialog.h"
#include "Knob.h"
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

	m_pluginLabel = new QLabel(this);
	layout->addWidget(m_pluginLabel);
	m_vendorLabel = new QLabel(this);
	layout->addWidget(m_vendorLabel);
	m_fileLabel = new QLabel(this);
	m_fileLabel->setWordWrap(true);
	layout->addWidget(m_fileLabel);

	auto* buttonRow = new QHBoxLayout();
	m_openButton = new QPushButton(tr("Open plugin..."), this);
	buttonRow->addWidget(m_openButton);
	m_guiButton = new QPushButton(tr("Show/hide GUI"), this);
	buttonRow->addWidget(m_guiButton);
	buttonRow->addStretch();
	layout->addLayout(buttonRow);

	m_programBox = new QComboBox(this);
	layout->addWidget(m_programBox);

	auto* scrollArea = new QScrollArea(this);
	scrollArea->setWidgetResizable(true);
	m_parameterPanel = new QWidget(scrollArea);
	m_parameterGrid = new QGridLayout(m_parameterPanel);
	m_parameterGrid->setContentsMargins(0, 0, 0, 0);
	scrollArea->setWidget(m_parameterPanel);
	scrollArea->setMinimumSize(300, 260);
	layout->addWidget(scrollArea);

	connect(m_openButton, &QPushButton::clicked, this,
		&Vst2InstrumentView::openPluginFile);
	connect(m_guiButton, &QPushButton::clicked, this, [this] {
		auto* plugin = m_instrument != nullptr
			? m_instrument->nativePlugin() : nullptr;
		if (plugin != nullptr) { plugin->toggleUI(); }
	});
	connect(m_programBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, [this](int index) {
			if (m_instrument != nullptr) { m_instrument->setProgram(index); }
		});

	rebuild();
}




void Vst2InstrumentView::modelChanged()
{
	rebuild();
}




void Vst2InstrumentView::rebuild()
{
	auto* plugin = m_instrument != nullptr
		? m_instrument->nativePlugin() : nullptr;

	if (plugin == nullptr)
	{
		m_pluginLabel->setText(tr("No VST2 plugin loaded."));
		m_vendorLabel->clear();
		m_fileLabel->clear();
	}
	else
	{
		m_pluginLabel->setText(tr("Plugin: %1").arg(
			plugin->name().isEmpty() ? tr("(unnamed)") : plugin->name()));
		m_vendorLabel->setText(plugin->vendorString().isEmpty()
			? QString() : tr("Vendor: %1").arg(plugin->vendorString()));
		m_fileLabel->setText(tr("File: %1").arg(
			m_instrument->file()));
	}
	m_vendorLabel->setVisible(!m_vendorLabel->text().isEmpty());
	m_fileLabel->setVisible(!m_fileLabel->text().isEmpty());

	updateGuiButton();

	{
		QSignalBlocker blocker(m_programBox);
		m_programBox->clear();
		auto* host = m_instrument != nullptr ? m_instrument->vst2Host()
			: nullptr;
		if (host != nullptr && host->programCount() > 1)
		{
			for (int index = 0; index < host->programCount(); ++index)
			{
				m_programBox->addItem(host->programName(index));
			}
			m_programBox->setCurrentIndex(
				std::max(0, host->currentProgram()));
			m_programBox->setVisible(true);
		}
		else
		{
			m_programBox->setVisible(false);
		}
	}

	rebuildParameters();
}




void Vst2InstrumentView::updateGuiButton()
{
	auto* plugin = m_instrument != nullptr
		? m_instrument->nativePlugin() : nullptr;

	bool displayAvailable = true;
#if defined(LMMS_BUILD_LINUX)
	displayAvailable = EditorPlatform::preferredPresentation()
		== EditorPresentation::X11Window;
#endif

	m_guiButton->setEnabled(plugin != nullptr && displayAvailable);
	if (!displayAvailable)
	{
#if defined(LMMS_BUILD_LINUX)
		m_guiButton->setToolTip(EditorPlatform::unavailableReason());
#else
		m_guiButton->setToolTip(tr("No display available."));
#endif
	}
	else if (plugin != nullptr && !plugin->hasEditorSupport())
	{
		m_guiButton->setToolTip(tr(
			"This VST2 plugin reports no editor; Show/hide GUI may do nothing."));
	}
	else
	{
		m_guiButton->setToolTip(
			tr("Open or close the plugin's own window."));
	}
}




void Vst2InstrumentView::clearParameters()
{
	while (auto* item = m_parameterGrid->takeAt(0))
	{
		delete item->widget();
		delete item;
	}
}




void Vst2InstrumentView::rebuildParameters()
{
	clearParameters();

	const HostedPluginModels* models = m_instrument != nullptr
		? &m_instrument->parameterModels() : nullptr;
	const int count = models != nullptr ? models->count() : 0;
	const int shown = std::min(count, 64);
	constexpr int columns = 3;
	for (int index = 0; index < shown; ++index)
	{
		auto* const parameter = models->model(index);
		if (parameter == nullptr)
		{
			continue;
		}
		const auto& info = models->parameter(index);
		auto* knob = new Knob(KnobType::Bright26, info.name,
			m_parameterPanel);
		knob->setModel(parameter);
		knob->setHintText(info.name, info.unit);
		m_parameterGrid->addWidget(knob, index / columns, index % columns,
			Qt::AlignHCenter);
	}

	if (count == 0)
	{
		m_parameterGrid->addWidget(new QLabel(
			tr("No automatable parameters."), m_parameterPanel), 0, 0);
	}
	else if (count > shown)
	{
		m_parameterGrid->addWidget(new QLabel(
			tr("Showing the first %1 parameters.").arg(shown),
			m_parameterPanel), (shown + columns - 1) / columns, 0, 1,
			columns);
	}
}




void Vst2InstrumentView::openPluginFile()
{
	if (m_instrument == nullptr)
	{
		return;
	}

	FileDialog dialog(nullptr, tr("Open VST2 plugin"));
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

	// Destroy the old editor and knobs before the plugin instance and its
	// models they reference.
	m_guiButton->setChecked(false);
	clearParameters();
	if (auto* engine = Engine::audioEngine())
	{
		engine->requestChangeInModel();
	}
	const bool loaded = m_instrument->openFile(dialog.selectedFiles().first());
	if (auto* engine = Engine::audioEngine())
	{
		engine->doneChangeInModel();
	}

	rebuild();
	if (!loaded)
	{
		QMessageBox::warning(this, tr("VST2 plugin"),
			tr("The selected VST2 plugin could not be loaded."));
	}
}

} // namespace lmms::gui
