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

#include <QLabel>
#include <QVBoxLayout>

#include "Vst3EffectControls.h"

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

	if (controls->parameterNames().isEmpty())
	{
		layout->addWidget(new QLabel(tr("No automatable parameters reported."), this));
	}
	else
	{
		layout->addWidget(new QLabel(tr("Parameters: %1")
			.arg(controls->parameterNames().size()), this));

		QStringList visibleParameters;
		for (const auto& parameter : controls->parameterNames())
		{
			visibleParameters.push_back(parameter);
			if (visibleParameters.size() >= 32)
			{
				break;
			}
		}
		auto* parameterLabel = new QLabel(visibleParameters.join(QStringLiteral("\n")), this);
		parameterLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
		layout->addWidget(parameterLabel);
	}

	layout->addWidget(new QLabel(
		tr("Parameter automation and the native editor follow in later phases."), this));
	layout->setSizeConstraint(QLayout::SetFixedSize);
}

} // namespace lmms::gui
