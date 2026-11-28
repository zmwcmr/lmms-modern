/*
 * EffectSelectDialog.cpp - dialog to choose effect plugin
 *
 * Copyright (c) 2006-2009 Tobias Doerffel <tobydox/at/users.sourceforge.net>
 * Copyright (c) 2023 Lost Robot <r94231/at/gmail.com>
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

#include "EffectSelectDialog.h"
#include "DummyEffect.h"
#include "EffectChain.h"
#include "embed.h"
#include "FontHelper.h"
#include "PluginFactory.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QTableView>
#include <QVBoxLayout>


namespace lmms::gui
{

void EffectSelectDialog::loadPlugins()
{
	m_effectKeys.clear();
	m_sourceModel.clear();
	m_currentSelection = EffectKey();

	// Query effects
	EffectKeyList subPluginEffectKeys;
	for (const auto desc : getPluginFactory()->descriptors(Plugin::Type::Effect))
	{
		if (desc->subPluginFeatures)
		{
			desc->subPluginFeatures->listSubPluginKeys(desc, subPluginEffectKeys);
		}
		else
		{
			m_effectKeys << EffectKey(desc, desc->name);
		}
	}
	m_effectKeys += subPluginEffectKeys;

	// Fill the source model
	m_sourceModel.setHorizontalHeaderItem(0, new QStandardItem(tr("Name")));
	m_sourceModel.setHorizontalHeaderItem(1, new QStandardItem(tr("Type")));
	int row = 0;
	for (const auto& key : m_effectKeys)
	{
		QString name;
		QString type;
		if (key.desc->subPluginFeatures)
		{
			name = key.displayName();
			type = key.desc->displayName;
		}
		else
		{
			name = key.desc->displayName;
			type = "LMMS";
		}
		m_sourceModel.setItem(row, 0, new QStandardItem(name));
		m_sourceModel.setItem(row, 1, new QStandardItem(type));
		++row;
	}
}


EffectSelectDialog::EffectSelectDialog(QWidget* parent) :
	QDialog(parent),
	m_effectKeys(),
	m_currentSelection(),
	m_sourceModel(),
	m_model(),
	m_descriptionWidget(nullptr),
	m_pluginList(new QTableView(this)),
	m_scrollArea(new QScrollArea(this))
{
	setWindowTitle(tr("Add effect"));
	// The plugin list and description need more room as the UI font grows.
	resize(scaledFontSize(640), scaledFontSize(480));

	setWindowIcon(embed::getIconPixmap("setup_audio"));

	loadPlugins();

	// Setup filtering
	m_model.setSourceModel(&m_sourceModel);
	m_model.setFilterCaseSensitivity(Qt::CaseInsensitive);

	QHBoxLayout* mainLayout = new QHBoxLayout(this);

	QVBoxLayout* leftSectionLayout = new QVBoxLayout();

	QList<QPushButton*> categoryButtons;
	QStringList buttonLabels = { tr("All"), "LMMS", "LADSPA", "LV2", "VST" };
	QStringList buttonSearchString = { "", "LMMS", "LADSPA", "LV2", "VST" };

	const QFont filterFont = bodyFont(font());
	int filterButtonWidth = 50;
	for (const auto& label : buttonLabels)
	{
		filterButtonWidth = qMax(filterButtonWidth,
			QFontMetrics(filterFont).horizontalAdvance(label) + 16);
	}

	for (int i = 0; i < buttonLabels.size(); ++i)
	{
		const QString& label = buttonLabels[i];
		const QString& searchString = buttonSearchString[i];

		QPushButton* button = new QPushButton(label, this);
		// Use the normal dialog-button styling and body font, not a separate
		// compact tile style. The buttons still line up in one column.
		button->setFont(filterFont);
		button->setFixedWidth(filterButtonWidth);
		button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		leftSectionLayout->addWidget(button, 0, Qt::AlignLeft);
		categoryButtons << button;

		connect(button, &QPushButton::clicked, this, [this, searchString] {
			m_model.setEffectTypeFilter(searchString);
			updateSelection();
		});
	}

	leftSectionLayout->addStretch();// Add stretch to the button layout to push buttons to the top
	mainLayout->addLayout(leftSectionLayout);

	m_filterEdit = new QLineEdit(this);
	connect(m_filterEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
		m_model.setFilterRegularExpression(QRegularExpression(text, QRegularExpression::CaseInsensitiveOption));
	});
	connect(m_filterEdit, &QLineEdit::textChanged, this, &EffectSelectDialog::updateSelection);
	m_filterEdit->setFocus();
	m_filterEdit->setFocusPolicy(Qt::StrongFocus);
	m_filterEdit->setPlaceholderText(tr("Search"));
	m_filterEdit->setClearButtonEnabled(true);
	m_filterEdit->addAction(embed::getIconPixmap("zoom"), QLineEdit::LeadingPosition);

	m_pluginList->setModel(&m_model);
	// QTableView does not reliably inherit the application font change in this
	// dialog, so set the body tier explicitly for rows and headers.
	const QFont listFont = bodyFont(m_pluginList->font());
	m_pluginList->setFont(listFont);
	m_pluginList->horizontalHeader()->setFont(listFont);
	m_pluginList->verticalHeader()->setFont(listFont);
	m_pluginList->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_pluginList->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_pluginList->setSelectionMode(QAbstractItemView::SingleSelection);
	m_pluginList->setSortingEnabled(true);
	m_pluginList->sortByColumn(0, Qt::AscendingOrder);  // Initial sort by column 0 (Name)
	m_pluginList->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	m_pluginList->verticalHeader()->hide();
	m_pluginList->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
	m_pluginList->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	m_pluginList->setAccessibleName(tr("Effect list"));
	m_pluginList->setAccessibleDescription(tr("Use the up and down arrow keys to select an effect, then press Enter."));

	// Scroll Area
	m_scrollArea->setWidgetResizable(true);
	QWidget* scrollAreaWidgetContents = new QWidget(m_scrollArea);
	scrollAreaWidgetContents->setObjectName("scrollAreaWidgetContents");
	m_scrollArea->setWidget(scrollAreaWidgetContents);
	m_scrollArea->setMaximumHeight(180);
	m_scrollArea->setFocusPolicy(Qt::NoFocus);

	// Button Box
	QDialogButtonBox* buttonBox = new QDialogButtonBox(Qt::Horizontal, this);
	buttonBox->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
	connect(buttonBox, &QDialogButtonBox::accepted, this, &EffectSelectDialog::acceptSelection);
	connect(buttonBox, &QDialogButtonBox::rejected, this, &EffectSelectDialog::reject);

	QVBoxLayout* rightSectionLayout = new QVBoxLayout();
	rightSectionLayout->addWidget(m_filterEdit);

	// Keep the plugin list and its action row in one container so the
	// Rescan button's right edge follows the list's right edge.
	auto* listContainer = new QWidget(this);
	auto* listLayout = new QVBoxLayout(listContainer);
	listLayout->setContentsMargins(0, 0, 0, 0);
	listLayout->setSpacing(0);
	listLayout->addWidget(m_pluginList);
	rightSectionLayout->addWidget(listContainer);
	// "Rescan" is an action, not a plugin category. Put it below the plugin
	// list, aligned to the right, instead of mixing it into the filter tiles.
	bool canRescan = false;
	for (const auto desc : getPluginFactory()->descriptors(Plugin::Type::Effect))
	{
		if (desc->subPluginFeatures && desc->subPluginFeatures->canRescan())
		{
			canRescan = true;
			break;
		}
	}
	QPushButton* rescanButton = nullptr;
	if (canRescan)
	{
		auto* rescanLayout = new QHBoxLayout();
		rescanLayout->setContentsMargins(0, 0, 0, 0);
		rescanLayout->addStretch();
		rescanButton = new QPushButton(tr("Rescan"), this);
		rescanButton->setFont(bodyFont(rescanButton->font()));
		const int rescanWidth = qMax(50, rescanButton->fontMetrics().horizontalAdvance(tr("Rescan")) + 16);
		rescanButton->setFixedWidth(rescanWidth);
		rescanButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		rescanButton->setToolTip(tr("Rescan VST plugins"));
		rescanLayout->addWidget(rescanButton);
		listLayout->addLayout(rescanLayout);
		QTimer::singleShot(0, this, [this, rescanLayout] {
			const int viewportInset = m_pluginList->verticalScrollBar()->width() + m_pluginList->frameWidth();
			rescanLayout->setContentsMargins(0, 0, viewportInset, 0);
		});
		connect(rescanButton, &QPushButton::clicked, this, [this] {
			QApplication::setOverrideCursor(Qt::WaitCursor);
			for (const auto desc : getPluginFactory()->descriptors(Plugin::Type::Effect))
			{
				if (auto* features = desc->subPluginFeatures;
					features && features->canRescan())
				{
					features->rescan();
				}
			}
			QApplication::restoreOverrideCursor();
			loadPlugins();
			// Clearing the source model removes its columns, so Qt discards the
			// per-column resize modes when the columns are created again.
			auto* header = m_pluginList->horizontalHeader();
			header->setSectionResizeMode(0, QHeaderView::Stretch);
			header->setSectionResizeMode(1, QHeaderView::ResizeToContents);
			m_pluginList->sortByColumn(0, Qt::AscendingOrder);
			updateSelection();
		});
	}

	rightSectionLayout->addWidget(m_scrollArea);
	rightSectionLayout->addWidget(buttonBox);
	mainLayout->addLayout(rightSectionLayout);

	setLayout(mainLayout);

	auto selectionModel = new QItemSelectionModel(&m_model);
	m_pluginList->setSelectionModel(selectionModel);
	connect(selectionModel, &QItemSelectionModel::currentRowChanged,
			this, &EffectSelectDialog::rowChanged);

	connect(m_pluginList, &QTableView::doubleClicked,
			this, &EffectSelectDialog::acceptSelection);
	connect(m_pluginList, &QTableView::activated,
			this, &EffectSelectDialog::acceptSelection);

	// Keep the search field first, then move through the list, categories and
	// dialog buttons in visual order for keyboard users.
	QWidget* tabPrevious = m_filterEdit;
	setTabOrder(tabPrevious, m_pluginList);
	tabPrevious = m_pluginList;
	for (auto* button : categoryButtons)
	{
		setTabOrder(tabPrevious, button);
		tabPrevious = button;
	}
	if (rescanButton)
	{
		setTabOrder(tabPrevious, rescanButton);
		tabPrevious = rescanButton;
	}
	auto* okButton = buttonBox->button(QDialogButtonBox::Ok);
	auto* cancelButton = buttonBox->button(QDialogButtonBox::Cancel);
	if (okButton)
	{
		setTabOrder(tabPrevious, okButton);
		tabPrevious = okButton;
	}
	if (cancelButton)
	{
		setTabOrder(tabPrevious, cancelButton);
		setTabOrder(cancelButton, m_filterEdit);
	}
	
	setModal(true);
	installEventFilter(this);

	updateSelection();
	show();
	QTimer::singleShot(0, this, [this] {
		m_pluginList->horizontalHeader()->setFont(bodyFont(m_pluginList->font()));
	});
}


Effect* EffectSelectDialog::instantiateSelectedPlugin(EffectChain* parent)
{
	Effect* result = nullptr;
	if (!m_currentSelection.name.isEmpty() && m_currentSelection.desc)
	{
		result = Effect::instantiate(m_currentSelection.desc->name, parent, &m_currentSelection);
	}
	if (!result)
	{
		result = new DummyEffect(parent, QDomElement());
	}
	return result;
}

void EffectSelectDialog::acceptSelection()
{
	if (m_currentSelection.isValid())
	{
		accept();
	}
}

void EffectSelectDialog::rowChanged(const QModelIndex& idx, const QModelIndex&)
{
	delete m_descriptionWidget;
	m_descriptionWidget = nullptr;

	if (m_model.mapToSource(idx).row() < 0)
	{
		// Invalidate current selection
		m_currentSelection = Plugin::Descriptor::SubPluginFeatures::Key();
	}
	else
	{
		m_currentSelection = m_effectKeys[m_model.mapToSource(idx).row()];
	}
	if (m_currentSelection.desc)
	{
		m_descriptionWidget = new QWidget;

		auto hbox = new QHBoxLayout(m_descriptionWidget);
		hbox->setAlignment(Qt::AlignTop);

		Plugin::Descriptor const& descriptor = *(m_currentSelection.desc);

		const PixmapLoader* pixLoa = m_currentSelection.logo();
		if (pixLoa)
		{
			auto logoLabel = new QLabel(m_descriptionWidget);
			logoLabel->setPixmap(pixLoa->pixmap());
			logoLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
			logoLabel->setMaximumSize(64, 64);

			hbox->addWidget(logoLabel);
		}

		auto textualInfoWidget = new QWidget(m_descriptionWidget);
		textualInfoWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		hbox->addWidget(textualInfoWidget);

		auto textWidgetLayout = new QVBoxLayout(textualInfoWidget);
		textWidgetLayout->setContentsMargins(4, 4, 4, 4);
		textWidgetLayout->setSpacing(8);

		if (m_currentSelection.desc->subPluginFeatures)
		{
			auto subWidget = new QWidget(textualInfoWidget);
			auto subLayout = new QVBoxLayout(subWidget);
			subLayout->setContentsMargins(0, 0, 0, 0);
			subLayout->setSpacing(8);

			m_currentSelection.desc->subPluginFeatures->
				fillDescriptionWidget(subWidget, &m_currentSelection);
			for (QWidget* w : subWidget->findChildren<QWidget*>())
			{
				if (w->parent() == subWidget)
				{
					subLayout->addWidget(w);
					subLayout->setAlignment(w, QFlags<Qt::AlignmentFlag>(Qt::AlignTop | Qt::AlignLeft));
				}
			}
			textWidgetLayout->addWidget(subWidget);
		}
		else
		{
			// HACK: Markup inside translation strings due to RTL not being handled correctly.
			// Move the markup out of the translation strings and into the QString template as soon as RTL layout works properly
			auto labelText = QString{
				"<p>%1%2</p>"
				"<p>%3%4</p>"
				"<p>%5%6</p>"
			}.arg(
				tr("<b>Name: </b>"), descriptor.displayName,
				tr("<b>Author: </b>"), QString::fromUtf8(descriptor.author)
					.replace("/dot/", ".").replace("/at/", "@").toHtmlEscaped(),
				tr("<b>Description: </b>"), qApp->translate("PluginBrowser", descriptor.description)
					.toHtmlEscaped()
			);
		
			auto label = new QLabel(labelText, m_descriptionWidget);
			label->setWordWrap(true);
			textWidgetLayout->addWidget(label);
		}

		m_scrollArea->setWidget(m_descriptionWidget);
		m_descriptionWidget->show();
	}
}

void EffectSelectDialog::updateSelection()
{
	// No valid selection anymore due to changed filter?
	if (m_pluginList->selectionModel()->selection().size() <= 0)
	{
		// Then select our first item
		m_pluginList->selectionModel()->
			select(m_model.index(0, 0), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
		rowChanged(m_model.index(0, 0), QModelIndex());
	}
}

bool EffectSelectDialog::eventFilter(QObject *obj, QEvent *event)
{
	if (obj == this && event->type() == QEvent::KeyPress)
	{
		QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
		if (keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down)
		{
			QItemSelectionModel* selectionModel = m_pluginList->selectionModel();
			int currentRow = selectionModel->currentIndex().row();
			int newRow = (keyEvent->key() == Qt::Key_Up) ? currentRow - 1 : currentRow + 1;
			int rowCount = m_pluginList->model()->rowCount();
			newRow = qBound(0, newRow, rowCount - 1);
			
			selectionModel->setCurrentIndex(m_pluginList->model()->index(newRow, 0),
				QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
			m_pluginList->scrollTo(m_pluginList->model()->index(newRow, 0));
			return true;
		}
	}

	return QDialog::eventFilter(obj, event);
}

} // namespace lmms::gui
