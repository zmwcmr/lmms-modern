/*
 * HostedPluginPanel.cpp - shared chrome for hosted instrument views
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

#include "HostedPluginPanel.h"

#include <algorithm>

#include <QCheckBox>
#include <QFont>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "AutomatableModel.h"
#include "Controls.h"
#include "Model.h"

namespace lmms::gui
{

HostedPluginPanel::~HostedPluginPanel() = default;




HostedPluginPanel::HostedPluginPanel(HostedPluginActions actions,
	HostedParameterProvider parameters, QWidget* parent) :
	QWidget(parent),
	m_actions(std::move(actions)),
	m_parameterProvider(std::move(parameters))
{
	setMinimumWidth(360);
	auto* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(4);

	auto* infoRow = new QHBoxLayout();
	m_title = new QLabel(this);
	QFont titleFont = m_title->font();
	titleFont.setBold(true);
	m_title->setFont(titleFont);
	infoRow->addWidget(m_title, 1);
	m_vendor = new QLabel(this);
	infoRow->addWidget(m_vendor);
	layout->addLayout(infoRow);

	auto* toolbar = new QHBoxLayout();
	m_guiButton = new QPushButton(tr("Show GUI"), this);
	m_guiButton->setToolTip(tr("Open or close the plugin's own window."));
	toolbar->addWidget(m_guiButton);
	m_parametersButton = new QPushButton(tr("Parameters"), this);
	m_parametersButton->setCheckable(true);
	m_parametersButton->setToolTip(tr("Show or hide the automation parameters"));
	toolbar->addWidget(m_parametersButton);
	m_reloadButton = new QPushButton(tr("Reload"), this);
	toolbar->addWidget(m_reloadButton);
	m_helpButton = new QPushButton(tr("Help"), this);
	m_helpButton->setCheckable(true);
	toolbar->addWidget(m_helpButton);
	toolbar->addStretch();
	layout->addLayout(toolbar);

	auto* presetRow = new QHBoxLayout();
	m_presetLabel = new QLabel(tr("Preset"), this);
	presetRow->addWidget(m_presetLabel);
	m_presetBox = new QComboBox(this);
	m_presetBox->setMinimumContentsLength(18);
	m_presetBox->setSizeAdjustPolicy(
		QComboBox::AdjustToMinimumContentsLengthWithIcon);
	m_presetBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	presetRow->addWidget(m_presetBox, 1);
	m_openButton = new QPushButton(tr("Open..."), this);
	m_openButton->setToolTip(tr("Open a plugin file"));
	presetRow->addWidget(m_openButton);
	layout->addLayout(presetRow);

	m_parametersWidget = new QWidget(this);
	auto* parameterLayout = new QVBoxLayout(m_parametersWidget);
	parameterLayout->setContentsMargins(0, 0, 0, 0);

	auto* filterRow = new QHBoxLayout();
	m_search = new QLineEdit(m_parametersWidget);
	m_search->setPlaceholderText(tr("Filter parameters"));
	filterRow->addWidget(m_search, 1);
	m_automatedOnly = new QCheckBox(tr("Automated only"), m_parametersWidget);
	filterRow->addWidget(m_automatedOnly);
	parameterLayout->addLayout(filterRow);

	auto* scrollArea = new QScrollArea(m_parametersWidget);
	scrollArea->setWidgetResizable(true);
	auto* scrollContent = new QWidget(scrollArea);
	m_parameterLayout = new QVBoxLayout(scrollContent);
	m_parameterLayout->setContentsMargins(0, 0, 0, 0);
	m_parameterLayout->setSpacing(2);
	scrollArea->setWidget(scrollContent);
	parameterLayout->addWidget(scrollArea, 1);
	m_parametersWidget->setVisible(false);
	layout->addWidget(m_parametersWidget, 1);

	connect(m_guiButton, &QPushButton::clicked, this, [this] {
		if (m_actions.toggleEditor) { m_actions.toggleEditor(); }
	});
	connect(m_parametersButton, &QPushButton::toggled, this,
		&HostedPluginPanel::setParametersVisible);
	connect(m_reloadButton, &QPushButton::clicked, this, [this] {
		if (m_actions.reload) { m_actions.reload(); }
		refresh();
	});
	connect(m_openButton, &QPushButton::clicked, this, [this] {
		if (m_actions.openPlugin) { m_actions.openPlugin(this); }
		refresh();
	});
	connect(m_helpButton, &QPushButton::toggled, this, [this](bool show) {
		if (m_actions.showHelp) { m_actions.showHelp(show); }
	});
	connect(m_presetBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, [this](int index) {
			if (index >= 0 && m_actions.selectPreset)
			{
				m_actions.selectPreset(index);
			}
		});
	connect(m_search, &QLineEdit::textChanged, this,
		&HostedPluginPanel::filterParameters);
	connect(m_automatedOnly, &QCheckBox::toggled, this,
		&HostedPluginPanel::filterParameters);

	refresh();
}




void HostedPluginPanel::refresh()
{
	updateInfo();
	updatePresets();
	if (m_parametersWidget->isVisible())
	{
		rebuildParameters();
	}
}




void HostedPluginPanel::updateInfo()
{
	const QString name = m_actions.name ? m_actions.name() : QString();
	const QString vendor = m_actions.vendor ? m_actions.vendor() : QString();
	const QString location = m_actions.location ? m_actions.location() : QString();

	m_title->setText(name.isEmpty() ? tr("No plugin loaded") : name);
	m_title->setToolTip(location);
	m_vendor->setText(vendor.isEmpty() ? QString() : tr("by %1").arg(vendor));
	m_vendor->setVisible(!vendor.isEmpty());

	const bool hasEditor = m_actions.hasEditor && m_actions.hasEditor();
	m_guiButton->setEnabled(hasEditor);
	m_guiButton->setToolTip(hasEditor
		? tr("Open or close the plugin's own window.")
		: tr("This plugin has no editor."));

	const bool canReload = m_actions.canReload && m_actions.canReload();
	m_reloadButton->setVisible(canReload);
	const bool canOpen = m_actions.canOpenPlugin && m_actions.canOpenPlugin();
	m_openButton->setVisible(canOpen);
	const bool hasHelp = m_actions.hasHelp && m_actions.hasHelp();
	m_helpButton->setVisible(hasHelp);
}




void HostedPluginPanel::updatePresets()
{
	const int count = m_actions.presetCount ? m_actions.presetCount() : 0;
	QSignalBlocker blocker(m_presetBox);
	m_presetBox->clear();
	m_presetBox->setVisible(count > 0);
	m_presetLabel->setVisible(count > 0);
	if (count <= 0)
	{
		return;
	}
	for (int index = 0; index < count; ++index)
	{
		m_presetBox->addItem(m_actions.presetName
			? m_actions.presetName(index) : QString::number(index + 1));
	}
	const int current = m_actions.currentPreset ? m_actions.currentPreset() : -1;
	m_presetBox->setCurrentIndex(std::max(0, current));
}




void HostedPluginPanel::setParametersVisible(bool visible)
{
	if (m_parametersButton->isChecked() != visible)
	{
		QSignalBlocker blocker(m_parametersButton);
		m_parametersButton->setChecked(visible);
	}
	m_parametersWidget->setVisible(visible);
	if (visible) { rebuildParameters(); }
}




bool HostedPluginPanel::parametersVisible() const
{
	return m_parametersWidget->isVisible();
}




void HostedPluginPanel::clearParameters()
{
	while (auto* item = m_parameterLayout->takeAt(0))
	{
		delete item->widget();
		delete item;
	}
	m_rows.clear();
	m_controls.clear();
}




void HostedPluginPanel::rebuildParameters()
{
	clearParameters();

	if (!m_parameterProvider)
	{
		return;
	}

	for (const auto& entry : m_parameterProvider())
	{
		if (entry.model == nullptr)
		{
			continue;
		}

		auto* row = new QWidget;
		auto* rowLayout = new QHBoxLayout(row);
		rowLayout->setContentsMargins(0, 0, 0, 0);

		auto* nameLabel = new QLabel(entry.name, row);
		nameLabel->setMinimumWidth(150);
		nameLabel->setMaximumWidth(220);
		nameLabel->setToolTip(entry.name);
		rowLayout->addWidget(nameLabel);

		std::unique_ptr<Control> control;
		if (auto* floatModel = dynamic_cast<FloatModel*>(entry.model))
		{
			Q_UNUSED(floatModel)
			control = std::make_unique<KnobControl>(QString(), row);
		}
		else if (dynamic_cast<ComboBoxModel*>(entry.model) != nullptr)
		{
			// ComboBoxModel derives from IntModel; test it first.
			control = std::make_unique<ComboControl>(row);
		}
		else if (auto* intModel = dynamic_cast<IntModel*>(entry.model))
		{
			const int digits = std::max(1,
				QString::number(static_cast<int>(intModel->maxValue())).size());
			control = std::make_unique<LcdControl>(digits, row);
		}
		else if (dynamic_cast<BoolModel*>(entry.model) != nullptr)
		{
			control = std::make_unique<CheckControl>(row);
		}
		if (!control)
		{
			delete row;
			continue;
		}
		control->setText(QString());
		control->setModel(entry.model);
		rowLayout->addWidget(control->topWidget(), 1);

		auto* valueLabel = new QLabel(
			entry.model->displayValue(entry.model->value<float>()), row);
		valueLabel->setMinimumWidth(70);
		valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		rowLayout->addWidget(valueLabel);
		QObject::connect(entry.model, &Model::dataChanged, valueLabel,
			[model = entry.model, valueLabel] {
				valueLabel->setText(model->displayValue(model->value<float>()));
			});

		auto* automatedLabel = new QLabel(entry.automated ? tr("A") : QString(),
			row);
		automatedLabel->setToolTip(tr("Parameter is automated or controlled"));
		automatedLabel->setMinimumWidth(12);
		rowLayout->addWidget(automatedLabel);

		m_parameterLayout->addWidget(row);
		m_rows.push_back({row, entry.name, entry.automated});
		m_controls.push_back(std::move(control));
	}

	m_parameterLayout->addStretch();
	filterParameters();
}




void HostedPluginPanel::filterParameters()
{
	const QString filter = m_search->text();
	const bool automatedOnly = m_automatedOnly->isChecked();
	for (const auto& row : m_rows)
	{
		const bool visible = (filter.isEmpty()
			|| row.name.contains(filter, Qt::CaseInsensitive))
			&& (!automatedOnly || row.automated);
		row.widget->setVisible(visible);
	}
}

} // namespace lmms::gui
