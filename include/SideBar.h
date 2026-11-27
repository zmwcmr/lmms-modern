/*
 * SideBar.h - side-bar in LMMS' MainWindow
 *
 * Copyright (c) 2004-2009 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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

#ifndef LMMS_GUI_SIDE_BAR_H
#define LMMS_GUI_SIDE_BAR_H

#include <QMap>
#include <QButtonGroup>
#include <QElapsedTimer>
#include <QPixmap>
#include <QPointer>
#include <QTimer>
#include <QToolBar>
#include <QSplitter>
#include <QVariantAnimation>

class QToolButton;
class QMdiArea;
class QWindow;

namespace lmms::gui
{

class SideBarWidget;
class SlidingSideBarPanel;

class SideBarWorkspace : public QWidget
{
	Q_OBJECT

public:
	SideBarWorkspace(QSplitter* splitter, QMdiArea* workspace);
	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;
	void preparePreview();
	void discardPreparedPreview();
	void freeze();
	void thaw();

signals:
	void previewInvalidated();

protected:
	bool eventFilter(QObject* watched, QEvent* event) override;
	void paintEvent(QPaintEvent* event) override;
	void resizeEvent(QResizeEvent*) override;

private:
	bool canClipWorkspace() const;
	QMdiArea* m_workspace;
	QWidget* m_preview = nullptr;
	QPixmap m_preparedSnapshot;
	bool m_layoutDeferred = false;
};

class SideBarFrameClock : public QTimer
{
	Q_OBJECT
public:
	SideBarFrameClock();
	void acquire(const QWidget* widget);
	void release();

signals:
	void frame();

protected:
	bool eventFilter(QObject* watched, QEvent* event) override;

private:
	void requestFrame();
	void advanceFrame();

	QPointer<QWindow> m_window;
	QElapsedTimer m_lastFrame;
	int m_interval = 16;
	int m_animationCount = 0;
};

class SideBarAnimation : public QVariantAnimation
{
public:
	explicit SideBarAnimation(SideBarFrameClock& clock);
	~SideBarAnimation() override { stopUpdates(); }
	void animate(const QWidget* widget, int from, int to, int duration, QEasingCurve::Type easing);
	void stopUpdates();
	bool isUpdating() const { return m_updating; }

private:
	SideBarFrameClock& m_clock;
	QElapsedTimer m_elapsed;
	bool m_updating = false;
};

class SideBarSplitter : public QSplitter
{
public:
	using QSplitter::QSplitter;
	using QSplitter::moveSplitter;
	SideBarWorkspace* workspace() const { return m_workspace; }
	void setWorkspace(SideBarWorkspace* workspace) { m_workspace = workspace; }

private:
	SideBarWorkspace* m_workspace = nullptr;
};


class SideBar : public QToolBar
{
	Q_OBJECT
public:
	SideBar( Qt::Orientation _orientation, QWidget * _parent );
	~SideBar() override;

	void appendTab( SideBarWidget * _sbw );

protected:
	bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
	void toggleButton( QAbstractButton * _btn );


private:
	void schedulePreview(int delay);
	void schedulePreviewRefresh();

	QButtonGroup m_btnGroup;
	using ButtonMap = QMap<QToolButton*, SideBarWidget*>;
	ButtonMap m_widgets;
	SideBarFrameClock m_frameClock;
	SideBarAnimation m_slideAnimation;
	QTimer m_previewTimer;
	SlidingSideBarPanel* m_panel = nullptr;
	int m_openWidth = 225;
	bool m_opening = false;

} ;

} // namespace lmms::gui

#endif // LMMS_GUI_SIDE_BAR_H
