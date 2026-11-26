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

#include <QLabel>
#include <QVBoxLayout>

#include "Instrument.h"
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
	layout->addWidget(new QLabel(tr("Parameters: %1")
		.arg(host->parameters().size()), this));
	layout->addWidget(new QLabel(
		tr("MIDI input is active. Parameter automation and the native editor follow later."),
		this));
	layout->setSizeConstraint(QLayout::SetFixedSize);
}

void Vst3InstrumentView::modelChanged()
{
}

} // namespace lmms::gui
