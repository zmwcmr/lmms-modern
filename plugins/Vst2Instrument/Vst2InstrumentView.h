/*
 * Vst2InstrumentView.h - view for the VST2 instrument host
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

#ifndef LMMS_GUI_VST2_INSTRUMENT_VIEW_H
#define LMMS_GUI_VST2_INSTRUMENT_VIEW_H

#include "InstrumentView.h"

namespace lmms
{

class Vst2HostedInstrument;
class VstPlugin;

namespace gui
{

class HostedPluginPanel;

class Vst2InstrumentView : public InstrumentView
{
	Q_OBJECT
public:
	Vst2InstrumentView(Instrument* instrument, QWidget* parent);
	~Vst2InstrumentView() override = default;

	QSize sizeHint() const override { return QSize(380, 360); }
	QSize minimumSizeHint() const override { return QSize(320, 240); }

private:
	void modelChanged() override;
	bool isResizable() const override { return true; }

	VstPlugin* plugin() const;
	void openPluginFile(QWidget* parent);

	Vst2HostedInstrument* m_instrument = nullptr;
	HostedPluginPanel* m_panel = nullptr;
};

} // namespace gui

} // namespace lmms

#endif // LMMS_GUI_VST2_INSTRUMENT_VIEW_H
