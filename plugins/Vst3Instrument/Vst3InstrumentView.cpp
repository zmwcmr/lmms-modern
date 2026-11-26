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
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>

#include "AutomatableModel.h"
#include "Instrument.h"
#include "Knob.h"
#include "Vst3EditorWidget.h"
#include "Vst3EditorWindow.h"
#include "Vst3Host.h"
#include "Vst3Instrument.h"

namespace lmms::gui
{

Vst3InstrumentView::Vst3InstrumentView(Instrument* instrument, QWidget* parent) :
	InstrumentViewFixedSize(instrument, parent)
{
	auto* layout = new QVBoxLayout(this);

	auto* const vst3Instrument = dynamic_cast<Vst3Instrument*>(instrument);
	if (vst3Instrument == nullptr || vst3Instrument->host() == nullptr)
	{
		layout->addWidget(new QLabel(tr("No VST3 instrument loaded."), this));
		return;
	}

	auto* const host = vst3Instrument->host();
	layout->addWidget(new QLabel(tr("Plugin: %1").arg(host->name()), this));
	if (!host->vendor().isEmpty())
	{
		layout->addWidget(new QLabel(tr("Vendor: %1").arg(host->vendor()), this));
	}
	const auto& models = vst3Instrument->parameterModels();
	layout->addWidget(new QLabel(tr("Automatable parameters: %1")
		.arg(models.count()), this));

	if (models.count() > 0)
	{
		auto* scrollArea = new QScrollArea(this);
		scrollArea->setWidgetResizable(true);
		auto* parameterWidget = new QWidget(scrollArea);
		auto* grid = new QGridLayout(parameterWidget);
		const int shown = std::min(models.count(), 48);
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
		scrollArea->setMinimumSize(420, 260);
		layout->addWidget(scrollArea);
	}

	if (host->programCount() > 1)
	{
		layout->addWidget(new QLabel(tr("Program"), this));
		auto* programBox = new QComboBox(this);
		for (int index = 0; index < host->programCount(); ++index)
		{
			programBox->addItem(host->programName(index));
		}
		programBox->setCurrentIndex(std::max(0, host->currentProgram()));
		connect(programBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, [vst3Instrument](int index) { vst3Instrument->setProgram(index); });
		layout->addWidget(programBox);
	}

	if (host->units().size() > 1)
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
			this, [vst3Instrument, unitBox](int index) {
				vst3Instrument->setUnit(unitBox->itemData(index).toInt());
			});
		layout->addWidget(unitBox);
	}

	if (host->hasEditor())
	{
		auto* editorButton = new QPushButton(tr("Open editor"), this);
		editorButton->setEnabled(Vst3EditorWidget::isSupported());
		if (!Vst3EditorWidget::isSupported())
		{
			editorButton->setToolTip(
				tr("The VST3 editor requires the X11 (xcb) Qt platform."));
		}
		connect(editorButton, &QPushButton::clicked, this,
			[this, host, vst3Instrument] {
			if (m_editorWindow != nullptr)
			{
				m_editorWindow->raiseEditor();
				m_editorWindow->activateEditor();
				return;
			}
			m_editorWindow = Vst3EditorWindow::open(host, this, host->name(),
				[vst3Instrument](uint32_t id, double value) {
					vst3Instrument->setParameterValue(id, value);
				});
		});
		layout->addWidget(editorButton);
	}
	else
	{
		layout->addWidget(new QLabel(tr("This VST3 plugin has no editor."), this));
	}

	layout->addWidget(new QLabel(
		tr("MIDI input is active."), this));
	layout->setSizeConstraint(QLayout::SetFixedSize);
}

void Vst3InstrumentView::modelChanged()
{
}

} // namespace lmms::gui
