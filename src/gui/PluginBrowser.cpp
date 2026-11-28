/*
 * PluginBrowser.cpp - implementation of the plugin-browser
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

#include "PluginBrowser.h"

#include <QApplication>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QTreeWidget>

#include "embed.h"
#include "CompactItemDelegate.h"
#include "FontHelper.h"
#include "Engine.h"
#include "InstrumentTrack.h"
#include "Song.h"
#include "StringPairDrag.h"
#include "TrackContainerView.h"
#include "PluginFactory.h"

namespace lmms::gui
{

namespace
{
class PluginItem : public QTreeWidgetItem
{
public:
	explicit PluginItem(const PluginKey& key) : m_key(key)
	{
		setText(0, key.displayName());
		setIcon(0, key.logo()->pixmap());
		setToolTip(0, key.desc->subPluginFeatures
			? key.description()
			: PluginBrowser::tr(key.desc->description));
		setFlags(flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled);
	}

	const PluginKey& pluginKey() const { return m_key; }
	PluginKey& pluginKey() { return m_key; }

private:
	PluginKey m_key;
};


class PluginBrowserTree : public QTreeWidget
{
public:
	explicit PluginBrowserTree(QWidget* parent) : QTreeWidget(parent)
	{
		setObjectName(QStringLiteral("pluginBrowserTree"));
		setMouseTracking(true);
		setUniformRowHeights(true);
		setIconSize(QSize(icon::Small, icon::Small));
		setTextElideMode(Qt::ElideMiddle);
		setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		setItemDelegate(new CompactItemDelegate(this));
	}

protected:
	void drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const override
	{
		QTreeWidget::drawBranches(painter, rect, index);
		// CompactItemDelegate draws the selection pill around the content only;
		// keep the branch/indentation area on the plain background.
		painter->fillRect(rect, palette().color(QPalette::Base));
		if (!rootIsDecorated() || !index.isValid() || !model()->hasChildren(index)) { return; }
		painter->save();
		painter->setRenderHint(QPainter::Antialiasing);
		painter->setPen(Qt::NoPen);
		const auto arrowColor = palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::Text);
		painter->setBrush(arrowColor);

		const QPointF arrowCenter(rect.right() - indentation() / 2.0 + 1, rect.center().y());
		const QRectF arrowHoverRect(arrowCenter.x() - 8.0, arrowCenter.y() - 8.0, 16.0, 16.0);
		if (m_mousePos.x() >= 0 && arrowHoverRect.contains(QPointF(m_mousePos)))
		{
			painter->setBrush(QColor(60, 68, 78));
			painter->drawRoundedRect(arrowHoverRect, 3, 3);
			painter->setBrush(arrowColor);
		}

		painter->translate(arrowCenter);
		if (isExpanded(index)) { painter->rotate(90); }
		QPainterPath arrow;
		arrow.moveTo(-2.0, -3.5);
		arrow.lineTo(2.5, 0.0);
		arrow.lineTo(-2.0, 3.5);
		arrow.closeSubpath();
		painter->drawPath(arrow);
		painter->restore();
	}

	void mousePressEvent(QMouseEvent* event) override
	{
		QTreeWidget::mousePressEvent(event);
		m_pressItem = nullptr;
		if (event->button() == Qt::LeftButton)
		{
			m_pressPos = event->pos();
			m_pressItem = dynamic_cast<PluginItem*>(itemAt(event->pos()));
		}
	}

	void mouseMoveEvent(QMouseEvent* event) override
	{
		m_mousePos = event->pos();
		viewport()->update();
		if (m_pressItem && (event->buttons() & Qt::LeftButton)
			&& (event->pos() - m_pressPos).manhattanLength() >= QApplication::startDragDistance())
		{
			auto* item = m_pressItem;
			m_pressItem = nullptr;
			auto& key = item->pluginKey();
			Engine::setDndPluginKey(&key);
			new StringPairDrag(QStringLiteral("instrument"),
				QString::fromUtf8(key.desc->name), key.logo()->pixmap(), this);
			event->accept();
			return;
		}
		QTreeWidget::mouseMoveEvent(event);
	}

	void mouseDoubleClickEvent(QMouseEvent* event) override
	{
		if (event->button() == Qt::LeftButton)
		{
			const QModelIndex index = indexAt(QPoint(viewport()->width() - 1, event->pos().y()));
			if (index.isValid() && model()->hasChildren(index))
			{
				const QRect itemRect = visualRect(index);
				const QPointF arrowCenter(itemRect.left() - indentation() / 2.0, itemRect.center().y());
				if (QRectF(arrowCenter.x() - 8.0, arrowCenter.y() - 8.0, 16.0, 16.0).contains(QPointF(event->pos())))
				{
					setExpanded(index, !isExpanded(index));
					event->accept();
					return;
				}
			}
		}
		QTreeWidget::mouseDoubleClickEvent(event);
	}

	void mouseReleaseEvent(QMouseEvent* event) override
	{
		m_pressItem = nullptr;
		QTreeWidget::mouseReleaseEvent(event);
	}

	void leaveEvent(QEvent* event) override
	{
		m_mousePos = QPoint(-1, -1);
		viewport()->update();
		QTreeWidget::leaveEvent(event);
	}

	bool viewportEvent(QEvent* event) override
	{
		if (event->type() == QEvent::Leave)
		{
			m_mousePos = QPoint(-1, -1);
			viewport()->update();
		}
		return QTreeWidget::viewportEvent(event);
	}

private:
	QPoint m_pressPos;
	QPoint m_mousePos{-1, -1};
	PluginItem* m_pressItem = nullptr;
};
}

PluginBrowser::PluginBrowser( QWidget * _parent ) :
	SideBarWidget( tr( "Instrument plugins" ),
				embed::getIconPixmap( "plugins" ).transformed( QTransform().rotate( 90 ) ), _parent )
{
	setWindowTitle( tr( "Instrument browser" ) );
	m_view = new QWidget( contentParent() );
	//m_view->setFrameShape( QFrame::NoFrame );

	addContentWidget( m_view );

	auto view_layout = new QVBoxLayout(m_view);
	view_layout->setContentsMargins(5, 5, 5, 5);
	view_layout->setSpacing( 5 );


	auto hint = new QLabel( tr( "Drag an instrument "
					"into either the Song Editor, the "
					"Pattern Editor or an "
					"existing instrument track." ),
								m_view );
	hint->setWordWrap( true );
	hint->setFont(bodyFont(hint->font()));

	m_searchBar = new QLineEdit(m_view);
	m_searchBar->setFont(bodyFont(m_searchBar->font()));
	m_searchBar->setPlaceholderText(tr("Search"));
	m_searchBar->setMaxLength(64);
	m_searchBar->setClearButtonEnabled(true);
	m_searchBar->addAction(embed::getIconPixmap("zoom"), QLineEdit::LeadingPosition);

	m_descTree = new PluginBrowserTree(m_view);
	m_descTree->setFont(bodyFont(m_descTree->font()));
	m_descTree->setColumnCount( 1 );
	m_descTree->header()->setVisible( false );
	m_descTree->setIndentation(20);
	m_descTree->setSelectionMode( QAbstractItemView::SingleSelection );
	m_descTree->setSelectionBehavior( QAbstractItemView::SelectRows );
	m_descTree->setContextMenuPolicy( Qt::CustomContextMenu );

	connect( m_searchBar, SIGNAL( textEdited( const QString& ) ),
			this, SLOT( onFilterChanged( const QString& ) ) );

	connect( m_descTree, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
		auto* item = dynamic_cast<PluginItem*>(m_descTree->itemAt(pos));
		if (!item) { return; }

		const auto key = item->pluginKey();
		QMenu menu(m_descTree);
		menu.addAction(tr("Send to new instrument track"),
			[this, key] { openInNewInstrumentTrack(key); });
		if (auto* features = key.desc->subPluginFeatures; features && features->canRescan())
		{
			menu.addAction(tr("Rescan plugins"), [this, features] {
				QApplication::setOverrideCursor(Qt::WaitCursor);
				features->rescan();
				QApplication::restoreOverrideCursor();
				reloadPlugins();
			});
		}
		menu.exec(m_descTree->viewport()->mapToGlobal(pos));
	});

	view_layout->addWidget( hint );
	view_layout->addWidget( m_searchBar );

	bool canRescan = false;
	for (const auto desc : getPluginFactory()->descriptors(Plugin::Type::Instrument))
	{
		if (desc->subPluginFeatures && desc->subPluginFeatures->canRescan())
		{
			canRescan = true;
			break;
		}
	}
	if (canRescan)
	{
		auto* rescanButton = new QPushButton(tr("Rescan plugins"), m_view);
		rescanButton->setFont(bodyFont(rescanButton->font()));
		rescanButton->setToolTip(tr("Scan for newly installed VST plugins"));
		connect(rescanButton, &QPushButton::clicked,
			this, &PluginBrowser::rescanPlugins);
		view_layout->addWidget(rescanButton);
	}

	view_layout->addWidget( m_descTree );

	// Add plugins to the tree
	addPlugins();

	// Resize
	m_descTree->header()->setStretchLastSection(true);
	m_descTree->header()->setSectionResizeMode(QHeaderView::Stretch);

	// Hide empty roots
	updateRootVisibilities();
}


void PluginBrowser::updateRootVisibility( int rootIndex )
{
	QTreeWidgetItem * root = m_descTree->topLevelItem( rootIndex );
	if (!root) { return; }

	bool hasVisibleChild = false;
	for (int i = 0; i < root->childCount(); ++i)
	{
		if (!root->child(i)->isHidden())
		{
			hasVisibleChild = true;
			break;
		}
	}
	root->setHidden(!hasVisibleChild);
}


void PluginBrowser::updateRootVisibilities()
{
	int rootCount = m_descTree->topLevelItemCount();
	for (int rootIndex = 0; rootIndex < rootCount; ++rootIndex)
	{
		updateRootVisibility( rootIndex );
	}
}


void PluginBrowser::rescanPlugins()
{
	QApplication::setOverrideCursor(Qt::WaitCursor);
	for (const auto desc : getPluginFactory()->descriptors(Plugin::Type::Instrument))
	{
		if (auto* features = desc->subPluginFeatures;
			features && features->canRescan())
		{
			features->rescan();
		}
	}
	QApplication::restoreOverrideCursor();
	reloadPlugins();
}


void PluginBrowser::reloadPlugins()
{
	addPlugins();
	updateRootVisibilities();
	onFilterChanged(m_searchBar->text());
}


void PluginBrowser::onFilterChanged( const QString & filter )
{
	int rootCount = m_descTree->topLevelItemCount();
	for (int rootIndex = 0; rootIndex < rootCount; ++rootIndex)
	{
		QTreeWidgetItem * root = m_descTree->topLevelItem( rootIndex );

		int itemCount = root->childCount();
		for (int itemIndex = 0; itemIndex < itemCount; ++itemIndex)
		{
			QTreeWidgetItem * item = root->child( itemIndex );
			auto* pluginItem = dynamic_cast<PluginItem*>(item);
			if (pluginItem && pluginItem->text(0).contains(filter, Qt::CaseInsensitive))
			{
				item->setHidden( false );
			}
			else
			{
				item->setHidden( true );
			}
		}
	}
	updateRootVisibilities();
	if (!filter.isEmpty())
	{
		for (int rootIndex = 0; rootIndex < rootCount; ++rootIndex)
		{
			auto* root = m_descTree->topLevelItem(rootIndex);
			if (!root->isHidden()) { root->setExpanded(true); }
		}
	}
	m_descTree->viewport()->update();
}


void PluginBrowser::addPlugins()
{
	// Add a root node to the plugin tree with the specified `label` and return it
	const auto addRoot = [this](auto label)
	{
		const auto root = new QTreeWidgetItem();
		root->setText(0, label);
		root->setIcon(0, embed::getIconPixmap("folder"));
		m_descTree->addTopLevelItem(root);
		return root;
	};

	// Add the plugin identified by `key` to the tree under the root node `root`
	const auto addPlugin = [](const auto& key, auto root)
	{
		root->addChild(new PluginItem(key));
	};

	// Remove any existing plugins from the tree
	m_descTree->clear();

	// Fetch and sort all instrument plugin descriptors
	auto descs = getPluginFactory()->descriptors(Plugin::Type::Instrument);
	std::sort(descs.begin(), descs.end(),
		[](auto d1, auto d2)
		{
			return qstricmp(d1->displayName, d2->displayName) < 0;
		}
	);

	// Add a root node to the tree for native LMMS plugins
	const auto lmmsRoot = addRoot("LMMS");
	lmmsRoot->setExpanded(true);

	// Add all of the descriptors to the tree
	for (const auto desc : descs)
	{
		if (desc->subPluginFeatures)
		{
			// Fetch and sort all subplugins for this plugin descriptor
			auto subPluginKeys = Plugin::Descriptor::SubPluginFeatures::KeyList{};
			desc->subPluginFeatures->listSubPluginKeys(desc, subPluginKeys);
			std::sort(subPluginKeys.begin(), subPluginKeys.end(),
				[](const auto& l, const auto& r)
				{
					return QString::compare(l.displayName(), r.displayName(), Qt::CaseInsensitive) < 0;
				}
			);

			// Create a root node for this plugin and add the subplugins under it
			const auto root = addRoot(desc->displayName);
			for (const auto& key : subPluginKeys) { addPlugin(key, root); }
		}
		else
		{
			addPlugin(Plugin::Descriptor::SubPluginFeatures::Key(desc, desc->name), lmmsRoot);
		}
	}
}




void PluginBrowser::openInNewInstrumentTrack(const PluginKey& key)
{
	auto* trackContainer = Engine::getSong();
	auto* track = dynamic_cast<InstrumentTrack*>(Track::create(Track::Type::Instrument, trackContainer));
	auto* loader = new InstrumentLoaderThread(this, track, QString::fromUtf8(key.desc->name));
	loader->start();
}

} // namespace lmms::gui
