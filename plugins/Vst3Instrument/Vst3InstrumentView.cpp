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

#include <QGridLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>

#include "AutomatableModel.h"
#include "Instrument.h"
#include "Knob.h"
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

	layout->addWidget(new QLabel(
		tr("MIDI input is active. The native editor follows in a later phase."),
		this));
	layout->setSizeConstraint(QLayout::SetFixedSize);
}

void Vst3InstrumentView::modelChanged()
{
}

} // namespace lmms::gui
