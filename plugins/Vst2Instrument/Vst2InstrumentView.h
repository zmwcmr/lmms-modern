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

class QComboBox;
class QGridLayout;
class QLabel;
class QPushButton;
class QWidget;

namespace lmms
{

class Vst2HostedInstrument;

namespace gui
{

class Vst2InstrumentView : public InstrumentView
{
	Q_OBJECT
public:
	Vst2InstrumentView(Instrument* instrument, QWidget* parent);
	~Vst2InstrumentView() override = default;

	QSize sizeHint() const override { return QSize(380, 600); }
	QSize minimumSizeHint() const override { return QSize(320, 320); }

private:
	void modelChanged() override;
	bool isResizable() const override { return true; }

	void rebuild();
	void clearParameters();
	void rebuildParameters();
	void openPluginFile();
	void updateGuiButton();

	Vst2HostedInstrument* m_instrument = nullptr;
	QLabel* m_pluginLabel = nullptr;
	QLabel* m_vendorLabel = nullptr;
	QLabel* m_fileLabel = nullptr;
	QPushButton* m_openButton = nullptr;
	QPushButton* m_guiButton = nullptr;
	QComboBox* m_programBox = nullptr;
	QWidget* m_parameterPanel = nullptr;
	QGridLayout* m_parameterGrid = nullptr;
};

} // namespace gui

} // namespace lmms

#endif // LMMS_GUI_VST2_INSTRUMENT_VIEW_H
