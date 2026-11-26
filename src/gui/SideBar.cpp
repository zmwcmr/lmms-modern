/*
 * SideBar.cpp - side-bar in LMMS' MainWindow
 *
 * Copyright (c) 2004-2014 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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

#include <QStyleOptionToolButton>
#include <QStylePainter>
#include <QSplitter>
#include <QEvent>
#include <QGuiApplication>
#include <QMdiArea>
#include <QPaintEvent>
#include <QPushButton>
#include <QScreen>
#include <QSignalBlocker>
#include <QToolButton>
#include <QWindow>
#include <algorithm>

#include "LmmsStyle.h"
#include "SideBar.h"
#include "SideBarWidget.h"

namespace lmms::gui
{

namespace
{
int slideDuration(const QWidget* widget)
{
	return LmmsStyle::uiAnimationDuration(widget, 240);
}

QEasingCurve::Type slideEasing(bool opening)
{
	return opening ? QEasingCurve::OutQuart : QEasingCurve::OutCubic;
}

QBrush workspaceBackground(const QMdiArea* workspace)
{
	const auto background = workspace->background();
	return background.style() == Qt::NoBrush ? workspace->palette().brush(QPalette::Window) : background;
}

class WorkspacePreview : public QWidget
{
public:
	WorkspacePreview(QWidget* parent, QPixmap snapshot, QBrush background)
		: QWidget(parent), m_snapshot(std::move(snapshot)), m_background(std::move(background))
	{
		setAttribute(Qt::WA_OpaquePaintEvent);
		setAttribute(Qt::WA_StaticContents);
		setAttribute(Qt::WA_TransparentForMouseEvents);
	}

protected:
	void paintEvent(QPaintEvent* event) override
	{
		QPainter painter(this);
		const QSize imageSize = m_snapshot.size() / m_snapshot.devicePixelRatio();
		const QRect imageRect(QPoint{}, imageSize);
		if (event->region().intersects(imageRect))
		{
			painter.drawPixmap(0, 0, m_snapshot);
		}
		const auto exposed = event->region().subtracted(imageRect);
		for (const auto& rect : exposed)
		{
			painter.fillRect(rect, m_background);
		}
	}

private:
	QPixmap m_snapshot;
	QBrush m_background;
};
}

SideBarWorkspace::SideBarWorkspace(QSplitter* splitter, QMdiArea* workspace)
	: QWidget(splitter), m_workspace(workspace)
{
	setSizePolicy(workspace->sizePolicy());
	setAttribute(Qt::WA_StaticContents);
	m_workspace->setParent(this);
	m_workspace->show();
}

QSize SideBarWorkspace::sizeHint() const { return m_workspace->sizeHint(); }
QSize SideBarWorkspace::minimumSizeHint() const { return m_workspace->minimumSizeHint(); }

bool SideBarWorkspace::canClipWorkspace() const
{
	if (m_workspace->internalWinId()) { return false; }
	for (const auto* child : m_workspace->findChildren<QWidget*>())
	{
		if (child->isVisible() && (child->internalWinId() || child->testAttribute(Qt::WA_NativeWindow))) { return false; }
	}
	return true;
}

void SideBarWorkspace::preparePreview()
{
	if (m_layoutDeferred || !m_preparedSnapshot.isNull() || !m_workspace->isVisible()
		|| !m_workspace->updatesEnabled() || !canClipWorkspace()) { return; }
	m_preparedSnapshot = m_workspace->grab();
	if (m_preparedSnapshot.isNull()) { return; }
	QCoreApplication::instance()->installEventFilter(this);
}

void SideBarWorkspace::discardPreparedPreview()
{
	if (m_preparedSnapshot.isNull()) { return; }
	QCoreApplication::instance()->removeEventFilter(this);
	m_preparedSnapshot = QPixmap{};
}

bool SideBarWorkspace::eventFilter(QObject* watched, QEvent* event)
{
	switch (event->type())
	{
	case QEvent::Paint:
	case QEvent::Move:
	case QEvent::Resize:
	case QEvent::Show:
	case QEvent::Hide:
	case QEvent::StyleChange:
	case QEvent::PaletteChange:
	case QEvent::WinIdChange:
		if (auto* widget = qobject_cast<QWidget*>(watched);
			widget && (widget == m_workspace || m_workspace->isAncestorOf(widget)))
		{
			discardPreparedPreview();
		}
		break;
	default: break;
	}
	return QWidget::eventFilter(watched, event);
}

void SideBarWorkspace::freeze()
{
	if (m_layoutDeferred || !m_workspace->isVisible() || !m_workspace->updatesEnabled()) { return; }
	if (m_preparedSnapshot.isNull())
	{
		m_layoutDeferred = canClipWorkspace();
		return;
	}
	if (m_preparedSnapshot.devicePixelRatio() != m_workspace->devicePixelRatioF())
	{
		discardPreparedPreview();
		m_layoutDeferred = canClipWorkspace();
		return;
	}
	m_layoutDeferred = true;
	auto snapshot = m_preparedSnapshot;
	discardPreparedPreview();
	m_preview = new WorkspacePreview(this, std::move(snapshot), workspaceBackground(m_workspace));
	m_preview->setGeometry(rect());
	m_workspace->setUpdatesEnabled(false);
	m_preview->show();
	m_preview->raise();
}

void SideBarWorkspace::thaw()
{
	if (!m_layoutDeferred) { return; }
	m_layoutDeferred = false;
	m_workspace->setGeometry(rect());
	if (!m_preview) { return; }
	m_workspace->setUpdatesEnabled(true);
	delete m_preview;
	m_preview = nullptr;
}

void SideBarWorkspace::paintEvent(QPaintEvent* event)
{
	if (!m_layoutDeferred || m_preview) { return; }
	const auto exposed = event->region().subtracted(m_workspace->geometry());
	if (exposed.isEmpty()) { return; }
	QPainter painter(this);
	const auto background = workspaceBackground(m_workspace);
	for (const auto& rect : exposed) { painter.fillRect(rect, background); }
}

void SideBarWorkspace::resizeEvent(QResizeEvent*)
{
	if (m_preview) { m_preview->setGeometry(rect()); }
	else if (!m_layoutDeferred) { m_workspace->setGeometry(rect()); }
}

SideBarFrameClock::SideBarFrameClock()
{
	setTimerType(Qt::PreciseTimer);
	setSingleShot(true);
	connect(this, &QTimer::timeout, this, &SideBarFrameClock::advanceFrame);
}

void SideBarFrameClock::acquire(const QWidget* widget)
{
	if (++m_animationCount != 1) { return; }
	auto* window = widget->window()->windowHandle();
	const auto* screen = window ? window->screen() : QGuiApplication::primaryScreen();
	const qreal rate = screen && screen->refreshRate() > 0 ? screen->refreshRate() : 60.0;
	m_interval = qMax(1, static_cast<int>(1000.0 / rate));
	if (window && QGuiApplication::platformName().startsWith("wayland")
		&& qEnvironmentVariableIntValue("LMMS_SIDEBAR_FRAME_SYNC") > 0)
	{
		m_window = window;
		window->installEventFilter(this);
	}
	m_lastFrame.start();
	requestFrame();
}

void SideBarFrameClock::release()
{
	if (--m_animationCount != 0) { return; }
	stop();
	if (m_window) { m_window->removeEventFilter(this); }
	m_window.clear();
}

void SideBarFrameClock::requestFrame()
{
	if (!m_animationCount || isActive()) { return; }
	if (m_window && m_window->isExposed())
	{
		start(2 * m_interval);
		m_window->requestUpdate();
	}
	else { start(m_interval); }
}

bool SideBarFrameClock::eventFilter(QObject* watched, QEvent* event)
{
	if (watched == m_window && event->type() == QEvent::UpdateRequest && isActive())
	{
		const auto remaining = m_interval - m_lastFrame.elapsed();
		if (remaining > 1) { start(static_cast<int>(remaining)); }
		else { advanceFrame(); }
	}
	return QTimer::eventFilter(watched, event);
}

void SideBarFrameClock::advanceFrame()
{
	stop();
	if (!m_animationCount) { return; }
	m_lastFrame.restart();
	emit frame();
	if (m_window) { QTimer::singleShot(0, this, &SideBarFrameClock::requestFrame); }
	else { requestFrame(); }
}

SideBarAnimation::SideBarAnimation(SideBarFrameClock& clock) : m_clock(clock)
{
	connect(&m_clock, &SideBarFrameClock::frame, this, [this] {
		if (!m_updating) { return; }
		const int time = static_cast<int>(std::min<qint64>(m_elapsed.elapsed(), duration()));
		setCurrentTime(time);
		if (time == duration())
		{
			stopUpdates();
			emit finished();
		}
	});
}

void SideBarAnimation::stopUpdates()
{
	if (!m_updating) { return; }
	m_updating = false;
	m_clock.release();
}

void SideBarAnimation::animate(const QWidget* widget, int from, int to, int duration,
	QEasingCurve::Type easing)
{
	stopUpdates();
	{
		const QSignalBlocker blocker(this);
		setDuration(duration);
		setEasingCurve(easing);
		setStartValue(from);
		setEndValue(to);
		setCurrentTime(0);
	}
	if (duration == 0 || from == to)
	{
		emit valueChanged(to);
		emit finished();
		return;
	}

	m_elapsed.start();
	m_updating = true;
	m_clock.acquire(widget);
}

class SlidingSideBarPanel : public QWidget
{
public:
	static constexpr int MinimumWidth = 200;

	explicit SlidingSideBarPanel(QWidget* parent) : QWidget(parent) {}

	void addPage(SideBarWidget* page)
	{
		page->setParent(this);
		page->hide();
	}

	void showPage(SideBarWidget* page)
	{
		if (page == m_content) { return; }
		if (m_content) { m_content->hide(); }
		m_content = page;
		positionContent();
		m_content->show();
	}

	void setSlideWidth(int visibleWidth, int contentWidth)
	{
		if (m_contentWidth != contentWidth)
		{
			m_contentWidth = contentWidth;
			positionContent();
		}
		if (width() == visibleWidth) { return; }
		auto* splitter = static_cast<SideBarSplitter*>(parentWidget());
		const int panelIndex = splitter->indexOf(this);
		const bool left = panelIndex < splitter->count() - 1;
		const int handleIndex = left ? splitter->count() - 1 : panelIndex;
		const int position = left ? visibleWidth
			: splitter->contentsRect().width() - splitter->handle(handleIndex)->width() - visibleWidth;
		splitter->moveSplitter(position, handleIndex);
	}

	void beginSlide() { setMinimumWidth(0); }

	void finishSlide()
	{
		m_contentWidth = 0;
		setMinimumWidth(MinimumWidth);
		positionContent();
	}

	void closeSlide()
	{
		hide();
		static_cast<SideBarSplitter*>(parentWidget())->refresh();
	}

protected:
	void resizeEvent(QResizeEvent*) override { positionContent(); }

private:
	void positionContent()
	{
		if (!m_content) { return; }
		const int contentWidth = std::max(width(), m_contentWidth);
		const auto* splitter = qobject_cast<QSplitter*>(parentWidget());
		const bool left = !splitter || splitter->indexOf(this) < splitter->count() - 1;
		m_content->setGeometry(left ? width() - contentWidth : 0, 0, contentWidth, height());
	}

	SideBarWidget* m_content = nullptr;
	int m_contentWidth = 0;
};

// internal helper class allowing to create QToolButtons with
// vertical orientation
class SideBarButton : public QToolButton
{
public:
	SideBarButton( Qt::Orientation _orientation, QWidget * _parent, SideBarFrameClock& clock ) :
		QToolButton( _parent ),
		m_orientation( _orientation ),
		m_labelAnimation(clock)
	{
		connect(&m_labelAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
			if (m_orientation == Qt::Vertical) { setMaximumHeight(value.toInt()); }
			else { setMaximumWidth(value.toInt()); }
		});
		connect(&m_labelAnimation, &QVariantAnimation::finished, this, [this] {
			setToolButtonStyle(m_expanded ? Qt::ToolButtonTextBesideIcon : Qt::ToolButtonIconOnly);
			if (m_orientation == Qt::Vertical)
			{
				setMaximumHeight(QWIDGETSIZE_MAX);
			}
			else
			{
				setMaximumWidth(QWIDGETSIZE_MAX);
			}
		});
	}

	~SideBarButton() override = default;

	void stopAnimation() { m_labelAnimation.stopUpdates(); }

	void setExpanded(bool expanded)
	{
		if (m_expanded == expanded) { return; }
		m_expanded = expanded;
		m_labelAnimation.stopUpdates();
		const bool vertical = m_orientation == Qt::Vertical;
		const int start = vertical ? height() : width();
		if (vertical) { setMaximumHeight(start); }
		else { setMaximumWidth(start); }
		const auto finalStyle = expanded ? Qt::ToolButtonTextBesideIcon : Qt::ToolButtonIconOnly;
		setToolButtonStyle(finalStyle);
		const int end = vertical ? sizeHint().height() : sizeHint().width();
		if (start != end)
		{
			setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		}
		m_animationIconWidth = iconSlotSize().width();
		m_animationLabelExtent = vertical ? sizeHint().height() : sizeHint().width();
		m_labelAnimation.animate(this, start, end, slideDuration(this), slideEasing(expanded));
	}

	Qt::Orientation orientation() const
	{
		return m_orientation;
	}

	QSize sizeHint() const override
	{
		QSize s = QToolButton::sizeHint();
		s.rwidth() += ButtonPadding;
		if( orientation() == Qt::Horizontal )
		{
			return s;
		}
		const QSize iconSlot = iconSlotSize();
		return QSize( iconSlot.height(), qMax( s.width(), iconSlot.width() ) );
	}


protected:
	void paintEvent( QPaintEvent * ) override
	{
		QStylePainter p( this );
		QStyleOptionToolButton opt;
		initStyleOption( &opt );
		if( orientation() == Qt::Vertical )
		{
			p.rotate( 270 );
			p.translate( -height(), 0 );
			opt.rect = QRect( 0, 0, height(), width() );

			auto background = opt;
			background.icon = QIcon();
			background.text.clear();
			p.drawComplexControl( QStyle::CC_ToolButton, background );

			// Reserve the same icon slot at the top whether or not the title is visible.
			auto icon = opt;
			icon.toolButtonStyle = Qt::ToolButtonIconOnly;
			icon.text.clear();
			icon.state &= ~(QStyle::State_Sunken | QStyle::State_On);
			const bool animating = m_labelAnimation.isUpdating();
			const int iconWidth = qMin(height(), animating ? m_animationIconWidth : iconSlotSize().width());
			p.save();
			p.translate( height() - iconWidth, 0 );
			icon.rect = QRect( 0, 0, iconWidth, width() );
			p.drawControl( QStyle::CE_ToolButtonLabel, icon );
			p.restore();

			if( opt.toolButtonStyle == Qt::ToolButtonTextBesideIcon )
			{
				opt.toolButtonStyle = Qt::ToolButtonTextOnly;
				opt.icon = QIcon();
				const int labelExtent = animating ? m_animationLabelExtent : height();
				const int textWidth = qMax(0, labelExtent - iconWidth);
				opt.rect.setRect(height() - iconWidth - textWidth, 0, textWidth, width());
				p.drawControl( QStyle::CE_ToolButtonLabel, opt );
			}
			return;
		}
		p.drawComplexControl( QStyle::CC_ToolButton, opt );
	}


private:
	QSize iconSlotSize() const
	{
		QStyleOptionToolButton option;
		initStyleOption( &option );
		option.toolButtonStyle = Qt::ToolButtonIconOnly;
		option.text.clear();
		option.state &= ~(QStyle::State_Sunken | QStyle::State_On);
		const QSize iconContents = option.iconSize + QSize( 2, 2 );
		QSize size = style()->sizeFromContents( QStyle::CT_ToolButton, &option, iconContents, this );
		size.rwidth() += ButtonPadding;
		return size;
	}

	static constexpr int ButtonPadding = 8;
	Qt::Orientation m_orientation;
	SideBarAnimation m_labelAnimation;
	bool m_expanded = false;
	int m_animationIconWidth = 0;
	int m_animationLabelExtent = 0;

} ;


SideBar::SideBar( Qt::Orientation _orientation, QWidget * _parent ) :
	QToolBar( _parent ),
	m_btnGroup( this ),
	m_slideAnimation(m_frameClock)
{
	setOrientation( _orientation );
	setIconSize( QSize( 16, 16 ) );

	m_btnGroup.setExclusive( false );
	connect( &m_btnGroup, SIGNAL(buttonClicked(QAbstractButton*)),
				this, SLOT(toggleButton(QAbstractButton*)));

	installEventFilter(this);
	m_previewTimer.setSingleShot(true);
	connect(&m_previewTimer, &QTimer::timeout, this, [this] {
		if (!m_panel || m_slideAnimation.isUpdating() || (!underMouse() && !m_panel->underMouse())
			|| QGuiApplication::mouseButtons() != Qt::NoButton || slideDuration(this) == 0) { return; }
		auto* splitter = static_cast<SideBarSplitter*>(m_panel->parentWidget());
		if (auto* workspace = splitter->workspace()) { workspace->preparePreview(); }
	});

	connect(&m_slideAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
		m_panel->setSlideWidth(value.toInt(), m_openWidth);
	});
	connect(&m_slideAnimation, &QVariantAnimation::finished, this, [this] {
		auto* splitter = static_cast<SideBarSplitter*>(m_panel->parentWidget());
		if (m_opening) { m_panel->finishSlide(); }
		else { m_panel->closeSlide(); }
		splitter->workspace()->thaw();
		schedulePreview(90);
	});
}

SideBar::~SideBar()
{
	for (auto* tab : m_widgets.keys()) { static_cast<SideBarButton*>(tab)->stopAnimation(); }
}

bool SideBar::eventFilter(QObject* watched, QEvent* event)
{
	if (event->type() == QEvent::Enter) { schedulePreview(24); }
	else if (event->type() == QEvent::Leave && (watched == this || watched == m_panel))
	{
		QTimer::singleShot(0, this, [this] {
			if (!m_panel || underMouse() || m_panel->underMouse()) { return; }
			m_previewTimer.stop();
			auto* splitter = static_cast<SideBarSplitter*>(m_panel->parentWidget());
			if (auto* workspace = splitter->workspace()) { workspace->discardPreparedPreview(); }
		});
	}
	return QToolBar::eventFilter(watched, event);
}

void SideBar::schedulePreview(int delay)
{
	if (!m_panel || (!underMouse() && !m_panel->underMouse()) || slideDuration(this) <= 0) { return; }
	m_previewTimer.start(delay);
}



void SideBar::appendTab( SideBarWidget *widget )
{
	auto button = new SideBarButton(orientation(), this, m_frameClock);
	button->setText( " " + widget->title() );
	button->setIcon( widget->icon() );
	button->setLayoutDirection( Qt::RightToLeft );
	button->setCheckable( true );
	if (!m_panel)
	{
		m_panel = new SlidingSideBarPanel(widget->parentWidget());
		m_panel->hide();
		m_panel->installEventFilter(this);
	}
	m_panel->addPage(widget);
	m_widgets[button] = widget;
	m_btnGroup.addButton( button );
	addWidget( button );
	button->installEventFilter(this);
	widget->closeButton()->installEventFilter(this);

	button->setToolTip(widget->title());

	connect(widget, &SideBarWidget::closeButtonClicked,
		[=]() { button->click(); });
}




void SideBar::toggleButton( QAbstractButton * button )
{
	auto* selected = m_widgets.value(qobject_cast<QToolButton*>(button));
	if (!selected) { return; }
	m_previewTimer.stop();
	const bool wasAnimating = m_slideAnimation.isUpdating();
	m_slideAnimation.stopUpdates();
	const bool panelVisible = !m_panel->isHidden();
	const int currentWidth = panelVisible ? m_panel->width() : 0;
	if (panelVisible && !wasAnimating)
	{
		m_openWidth = std::max(currentWidth, SlidingSideBarPanel::MinimumWidth);
	}

	m_opening = button->isChecked();
	const int targetWidth = m_opening ? m_openWidth : 0;
	const int duration = slideDuration(this);
	auto* workspace = static_cast<SideBarSplitter*>(m_panel->parentWidget())->workspace();
	const bool animate = duration > 0 && currentWidth != targetWidth;
	if (animate) { workspace->freeze(); }

	for (auto* tab : m_widgets.keys())
	{
		const bool checked = tab == button && m_opening;
		tab->setChecked(checked);
		static_cast<SideBarButton*>(tab)->setExpanded(checked);
	}

	if (m_opening) { m_panel->showPage(selected); }
	if (!animate)
	{
		if (m_opening)
		{
			if (!panelVisible || currentWidth != targetWidth)
			{
				m_panel->show();
				m_panel->setSlideWidth(targetWidth, m_openWidth);
			}
			m_panel->finishSlide();
		}
		else
		{
			m_panel->closeSlide();
		}
		workspace->thaw();
		schedulePreview(90);
		return;
	}
	m_panel->beginSlide();
	m_panel->show();
	m_panel->setSlideWidth(currentWidth, m_openWidth);
	m_slideAnimation.animate(this, currentWidth, targetWidth, duration, slideEasing(m_opening));
}


} // namespace lmms::gui
