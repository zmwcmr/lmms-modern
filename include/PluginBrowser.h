/*
 * PluginBrowser.h - include file for PluginBrowser
 *
 * Copyright (c) 2005-2009 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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

#ifndef LMMS_GUI_PLUGIN_BROWSER_H
#define LMMS_GUI_PLUGIN_BROWSER_H

#include "SideBarWidget.h"
#include "Plugin.h"

class QLineEdit;
class QTreeWidget;

namespace lmms::gui
{

using PluginKey = Plugin::Descriptor::SubPluginFeatures::Key;

class PluginBrowser : public SideBarWidget
{
	Q_OBJECT
public:
	PluginBrowser( QWidget * _parent );
	~PluginBrowser() override = default;

private slots:
	void onFilterChanged( const QString & filter );
	void reloadPlugins();

private:
	void addPlugins();
	void rescanPlugins();
	void updateRootVisibility( int index );
	void updateRootVisibilities();
	void openInNewInstrumentTrack( const PluginKey & key );

	QWidget * m_view;
	QTreeWidget * m_descTree;
	QLineEdit * m_searchBar;
};


} // namespace lmms::gui

#endif // LMMS_GUI_PLUGIN_BROWSER_H
