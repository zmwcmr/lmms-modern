/*
 * HostedPluginPanel.h - shared chrome for hosted instrument views
 *
 * Copyright (c) 2026 LMMS contributors
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#ifndef LMMS_GUI_HOSTED_PLUGIN_PANEL_H
#define LMMS_GUI_HOSTED_PLUGIN_PANEL_H

#include <functional>
#include <memory>
#include <vector>

#include <QString>
#include <QWidget>
#include "lmms_export.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QVBoxLayout;

namespace lmms
{

class AutomatableModel;

namespace gui
{

class Control;

//! One parameter exposed by a hosted plugin for the shared parameter drawer.
struct HostedParameterEntry
{
	QString name;
	QString unit;
	AutomatableModel* model = nullptr;
	bool automated = false;
};

//! Format-specific actions used by the shared panel.
struct HostedPluginActions
{
	std::function<QString()> name;
	std::function<QString()> vendor;
	std::function<QString()> location;
	std::function<bool()> hasEditor;
	std::function<void()> showEditor;
	std::function<void()> hideEditor;
	std::function<bool()> isEditorVisible;
	std::function<bool()> canReload;
	std::function<void()> reload;
	std::function<int()> presetCount;
	std::function<QString(int)> presetName;
	std::function<int()> currentPreset;
	std::function<void(int)> selectPreset;
	std::function<bool()> canOpenPlugin;
	std::function<void(QWidget*)> openPlugin;
	std::function<bool()> hasHelp;
	std::function<void(bool)> showHelp;
};

using HostedParameterProvider =
	std::function<std::vector<HostedParameterEntry>()>;

/*! Shared information/toolbar/parameter drawer for VST2, VST3 and LV2.
 *
 *  The panel intentionally has no format knowledge. Views supply callbacks and
 *  a parameter provider; the plugin's own GUI is the primary editor, while the
 *  parameter drawer stays collapsed until the user asks for it.
 */
class LMMS_EXPORT HostedPluginPanel : public QWidget
{
	Q_OBJECT
public:
	HostedPluginPanel(HostedPluginActions actions,
		HostedParameterProvider parameters, QWidget* parent = nullptr);
	~HostedPluginPanel() override;

	void refresh();
	//! Drop parameter widgets before the instrument deletes their models.
	void clearParameters();
	void setParametersVisible(bool visible);
	bool parametersVisible() const;

private:
	struct ParameterRow
	{
		QWidget* widget = nullptr;
		QString name;
		bool automated = false;
	};

	void rebuildParameters();
	void filterParameters();
	void updatePresets();
	void updateInfo();

	HostedPluginActions m_actions;
	HostedParameterProvider m_parameterProvider;

	QLabel* m_title = nullptr;
	QLabel* m_vendor = nullptr;
	QPushButton* m_guiButton = nullptr;
	QPushButton* m_hideButton = nullptr;
	QPushButton* m_parametersButton = nullptr;
	QPushButton* m_reloadButton = nullptr;
	QPushButton* m_openButton = nullptr;
	QPushButton* m_helpButton = nullptr;
	QLabel* m_presetLabel = nullptr;
	QComboBox* m_presetBox = nullptr;
	QWidget* m_parametersWidget = nullptr;
	QLineEdit* m_search = nullptr;
	QCheckBox* m_automatedOnly = nullptr;
	QVBoxLayout* m_parameterLayout = nullptr;
	std::vector<ParameterRow> m_rows;
	std::vector<std::unique_ptr<Control>> m_controls;
};

} // namespace gui

} // namespace lmms

#endif // LMMS_GUI_HOSTED_PLUGIN_PANEL_H
