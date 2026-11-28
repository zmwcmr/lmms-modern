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
#include <QStyleOption>
#include <QTimer>
#include <QTreeWidget>

#include "embed.h"
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
class PluginBrowserTree : public QTreeWidget
{
public:
	explicit PluginBrowserTree(QWidget* parent) : QTreeWidget(parent)
	{
		setObjectName(QStringLiteral("pluginBrowserTree"));
	}

protected:
	void drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const override
	{
		QTreeWidget::drawBranches(painter, rect, index);
		if (!rootIsDecorated() || !index.isValid() || !model()->hasChildren(index)) { return; }
		painter->save();
		painter->setRenderHint(QPainter::Antialiasing);
		painter->setPen(Qt::NoPen);
		painter->setBrush(palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::Text));
		painter->translate(rect.right() - indentation() / 2.0 + 1, rect.center().y());
		if (isExpanded(index)) { painter->rotate(90); }
		QPainterPath arrow;
		arrow.moveTo(-2.0, -3.5);
		arrow.lineTo(2.5, 0.0);
		arrow.lineTo(-2.0, 3.5);
		arrow.closeSubpath();
		painter->drawPath(arrow);
		painter->restore();
	}
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
	m_descTree->setSelectionMode( QAbstractItemView::NoSelection );

	connect( m_searchBar, SIGNAL( textEdited( const QString& ) ),
			this, SLOT( onFilterChanged( const QString& ) ) );

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
	m_descTree->header()->setSectionResizeMode( QHeaderView::ResizeToContents );

	// Hide empty roots
	updateRootVisibilities();
}


void PluginBrowser::updateRootVisibility( int rootIndex )
{
	QTreeWidgetItem * root = m_descTree->topLevelItem( rootIndex );
	root->setHidden( !root->childCount() );
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
			auto descWidget = static_cast<PluginDescWidget*>(m_descTree->itemWidget(item, 0));
			if (descWidget->name().contains(filter, Qt::CaseInsensitive))
			{
				item->setHidden( false );
			}
			else
			{
				item->setHidden( true );
			}
		}
	}
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
	const auto addPlugin = [this](const auto& key, auto root)
	{
		const auto item = new QTreeWidgetItem();
		root->addChild(item);
		const auto widget = new PluginDescWidget(key, m_descTree);
		connect(widget, &PluginDescWidget::rescanRequested, this, [this] {
			QTimer::singleShot(0, this, &PluginBrowser::reloadPlugins);
		});
		m_descTree->setItemWidget(item, 0, widget);
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




PluginDescWidget::PluginDescWidget(const PluginKey &_pk,
							QWidget * _parent ) :
	QWidget( _parent ),
	m_pluginKey( _pk ),
	m_logo( _pk.logo()->pixmap() ),
	m_mouseOver( false )
{
	setFont(bodyFont(font()));
	setFixedHeight(qMax(DEFAULT_HEIGHT, QFontMetrics(bodyFont(font())).height() + 6));
	setMouseTracking( true );
	setCursor( Qt::PointingHandCursor );
	setToolTip(_pk.desc->subPluginFeatures
		? _pk.description()
		: tr(_pk.desc->description));
}




QString PluginDescWidget::name() const
{
	return m_pluginKey.displayName();
}




void PluginDescWidget::paintEvent( QPaintEvent * )
{

	QPainter p( this );

	// Paint everything according to the style sheet
	QStyleOption o;
	o.initFrom( this );
	style()->drawPrimitive( QStyle::PE_Widget, &o, &p, this );

	// Draw the rest
	const int s = 16 + ( 32 * ( qBound( 24, height(), 60 ) - 24 ) ) /
								( 60 - 24 );
	const qreal dpr = m_logo.devicePixelRatio();
	QPixmap logo = m_logo.scaled( QSize( s, s ) * dpr, Qt::KeepAspectRatio,
						Qt::SmoothTransformation );
	logo.setDevicePixelRatio( dpr );
	p.drawPixmap( 4, 4, logo );

	QFont f = p.font();
	if ( m_mouseOver )
	{
		f.setWeight(QFont::DemiBold);
	}

	p.setFont( f );
	const auto metrics = p.fontMetrics();
	const int baseline = (height() - metrics.height()) / 2 + metrics.ascent();
	p.drawText(10 + s, baseline, m_pluginKey.displayName());
}


#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
void PluginDescWidget::enterEvent(QEnterEvent* event)
#else
void PluginDescWidget::enterEvent(QEvent* event)
#endif
{
	m_mouseOver = true;

	QWidget::enterEvent(event);
}




void PluginDescWidget::leaveEvent( QEvent * _e )
{
	m_mouseOver = false;

	QWidget::leaveEvent( _e );
}




void PluginDescWidget::mousePressEvent( QMouseEvent * _me )
{
	Engine::setDndPluginKey(&m_pluginKey);
	if ( _me->button() == Qt::LeftButton )
	{
		new StringPairDrag("instrument",
			QString::fromUtf8(m_pluginKey.desc->name), m_logo, this);
		leaveEvent( _me );
	}
}


void PluginDescWidget::contextMenuEvent(QContextMenuEvent* e)
{
	QMenu contextMenu(this);
	contextMenu.addAction(
		tr("Send to new instrument track"),
		[=, this]{ openInNewInstrumentTrack(m_pluginKey.desc->name); }
	);

	if (auto* features = m_pluginKey.desc->subPluginFeatures;
		features && features->canRescan())
	{
		contextMenu.addAction(tr("Rescan plugins"), [this, features] {
			QApplication::setOverrideCursor(Qt::WaitCursor);
			features->rescan();
			QApplication::restoreOverrideCursor();
			emit rescanRequested();
		});
	}

	contextMenu.exec(e->globalPos());
}


void PluginDescWidget::openInNewInstrumentTrack(QString value)
{
	TrackContainer* tc = Engine::getSong();
	auto it = dynamic_cast<InstrumentTrack*>(Track::create(Track::Type::Instrument, tc));
	auto ilt = new InstrumentLoaderThread(this, it, value);
	ilt->start();
}


} // namespace lmms::gui
