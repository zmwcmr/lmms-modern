/*
 * Vst3EffectControlDialog.cpp - control dialog for the VST3 effect host
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

#include "Vst3EffectControlDialog.h"

#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>

#include "AutomatableModel.h"
#include "Knob.h"
#include "Vst3EffectControls.h"
#include "Vst3EditorWidget.h"
#include "Vst3EditorWindow.h"
#include "Vst3Host.h"

namespace lmms::gui
{

Vst3EffectControlDialog::Vst3EffectControlDialog(Vst3EffectControls* controls) :
	EffectControlDialog(controls)
{
	auto* layout = new QVBoxLayout(this);

	const auto name = controls->pluginName().isEmpty()
		? tr("No VST3 plugin loaded")
		: controls->pluginName();
	layout->addWidget(new QLabel(tr("Plugin: %1").arg(name), this));

	if (!controls->pluginVendor().isEmpty())
	{
		layout->addWidget(new QLabel(tr("Vendor: %1").arg(controls->pluginVendor()), this));
	}

	const auto& models = controls->parameterModels();
	if (models.count() == 0)
	{
		layout->addWidget(new QLabel(tr("No automatable parameters reported."), this));
	}
	else
	{
		layout->addWidget(new QLabel(tr("Parameters: %1")
			.arg(models.count()), this));

		auto* scrollArea = new QScrollArea(this);
		scrollArea->setWidgetResizable(true);
		auto* parameterWidget = new QWidget(scrollArea);
		auto* grid = new QGridLayout(parameterWidget);
		const int shown = std::min(models.count(), 64);
		for (int index = 0; index < shown; ++index)
		{
			auto* const parameter = models.model(index);
			if (parameter == nullptr)
			{
				continue;
			}
			const auto& info = models.parameter(index);
			auto* knob = new Knob(KnobType::Bright26, info.name, parameterWidget);
			knob->setModel(parameter);
			knob->setHintText(info.name, info.unit);
			grid->addWidget(knob, index / 4, index % 4, Qt::AlignHCenter);
		}
		if (models.count() > shown)
		{
			grid->addWidget(new QLabel(tr("Showing the first %1 parameters.")
				.arg(shown), parameterWidget), (shown + 3) / 4, 0, 1, 4);
		}
		scrollArea->setWidget(parameterWidget);
		scrollArea->setMinimumSize(420, 300);
		layout->addWidget(scrollArea);
	}

	if (auto* host = controls->host(); host != nullptr && host->programCount() > 1)
	{
		layout->addWidget(new QLabel(tr("Program"), this));
		auto* programBox = new QComboBox(this);
		for (int index = 0; index < host->programCount(); ++index)
		{
			programBox->addItem(host->programName(index));
		}
		programBox->setCurrentIndex(std::max(0, host->currentProgram()));
		connect(programBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, [controls](int index) { controls->setProgram(index); });
		layout->addWidget(programBox);
	}

	if (auto* host = controls->host(); host != nullptr && host->units().size() > 1)
	{
		layout->addWidget(new QLabel(tr("Unit"), this));
		auto* unitBox = new QComboBox(this);
		for (const auto& unit : host->units())
		{
			unitBox->addItem(unit.name, unit.id);
		}
		unitBox->setCurrentIndex(std::max(0,
			unitBox->findData(host->currentUnit())));
		connect(unitBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, [controls, unitBox](int index) {
				controls->setUnit(unitBox->itemData(index).toInt());
			});
		layout->addWidget(unitBox);
	}

	if (auto* host = controls->host(); host != nullptr && host->hasEditor())
	{
		auto* editorButton = new QPushButton(tr("Open editor"), this);
		editorButton->setEnabled(Vst3EditorWidget::isSupported());
		if (!Vst3EditorWidget::isSupported())
		{
			editorButton->setToolTip(
				tr("The VST3 editor requires the X11 (xcb) Qt platform."));
		}
		connect(editorButton, &QPushButton::clicked, this, [this, controls] {
			if (m_editorWindow != nullptr)
			{
				m_editorWindow->raiseEditor();
				m_editorWindow->activateEditor();
				return;
			}
			m_editorWindow = Vst3EditorWindow::open(
				controls->host(), this, controls->pluginName());
		});
		layout->addWidget(editorButton);
	}
	else
	{
		layout->addWidget(new QLabel(tr("This VST3 plugin has no editor."), this));
	}

	layout->setSizeConstraint(QLayout::SetFixedSize);
}

} // namespace lmms::gui
