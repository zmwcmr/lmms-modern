/*
 * Editor.cpp - implementation of Editor class
 *
 * Copyright (c) 2014 Lukas W <lukaswhl/at/gmail.com>
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

#include "Editor.h"

#include <QAction>
#include <QKeyEvent>
#include <QShortcut>

#include "EditorShortcuts.h"
#include "GuiApplication.h"
#include "MainWindow.h"
#include "Song.h"
#include "embed.h"

namespace lmms::gui
{

namespace
{
const TransportShortcuts& configuredTransportShortcuts()
{
	static const auto shortcuts = transportShortcuts();
	return shortcuts;
}
}


void Editor::setPauseIcon(bool displayPauseIcon)
{
	// If we're playing, show a pause icon
	if (displayPauseIcon)
		m_playAction->setIcon(embed::getIconPixmap("pause"));
	else
		m_playAction->setIcon(embed::getIconPixmap("play"));
}

DropToolBar * Editor::addDropToolBarToTop(QString const & windowTitle)
{
	return addDropToolBar(Qt::TopToolBarArea, windowTitle);
}

DropToolBar * Editor::addDropToolBar(Qt::ToolBarArea whereToAdd, QString const & windowTitle)
{
	return addDropToolBar(this, whereToAdd, windowTitle);
}

DropToolBar * Editor::addDropToolBar(QWidget * parent, Qt::ToolBarArea whereToAdd, QString const & windowTitle)
{
	auto toolBar = new DropToolBar(parent);
	addToolBar(whereToAdd, toolBar);
	toolBar->setMovable(false);
	toolBar->setFloatable(false);
	toolBar->setContextMenuPolicy(Qt::PreventContextMenu);
	toolBar->setWindowTitle(windowTitle);

	return toolBar;
}

void Editor::togglePlayStop()
{
	s_lastPlayedEditor = this;
	if (Engine::getSong()->isPlaying())
		stop();
	else
		play();
}

void Editor::togglePause()
{
	s_lastPlayedEditor = this;
	Engine::getSong()->togglePause();
}

void Editor::toggleMaximize()
{
	isMaximized() ? showNormal() : showMaximized();
}

Editor::Editor(bool record, bool stepRecord) :
	QMainWindow(),
	m_toolBar(new DropToolBar(this)),
	m_playAction(nullptr),
	m_recordAction(nullptr),
	m_recordAccompanyAction(nullptr),
	m_toggleStepRecordingAction(nullptr),
	m_stopAction(nullptr)
{
	m_toolBar = addDropToolBarToTop(tr("Transport controls"));

	auto addButton = [this](QAction* action, QString objectName) {
		m_toolBar->addAction(action);
		m_toolBar->widgetForAction(action)->setObjectName(objectName);
	};

	// Set up play and record actions
	m_playAction = new QAction(embed::getIconPixmap("play"), tr("Play"), this);
	m_stopAction = new QAction(embed::getIconPixmap("stop"), tr("Stop"), this);
	setTransportToolTips(m_playAction->text(), m_stopAction->text());

	m_recordAction = new QAction(embed::getIconPixmap("record"), tr("Record"), this);
	m_recordAccompanyAction = new QAction(embed::getIconPixmap("record_accompany"), tr("Record while playing"), this);
	m_toggleStepRecordingAction = new QAction(embed::getIconPixmap("record_step_off"), tr("Toggle Step Recording"), this);

	// Set up connections
	connect(m_playAction, SIGNAL(triggered()), this, SLOT(play()));
	connect(m_recordAction, SIGNAL(triggered()), this, SLOT(record()));
	connect(m_recordAccompanyAction, SIGNAL(triggered()), this, SLOT(recordAccompany()));
	connect(m_toggleStepRecordingAction, SIGNAL(triggered()), this, SLOT(toggleStepRecording()));
	connect(m_stopAction, SIGNAL(triggered()), this, SLOT(stop()));
	new QShortcut(windowLayoutShortcuts().value(QStringLiteral("layout_maximize_editor")), this, SLOT(toggleMaximize()));

	// Add actions to toolbar
	addButton(m_playAction, "playButton");
	if (record)
	{
		addButton(m_recordAction, "recordButton");
		addButton(m_recordAccompanyAction, "recordAccompanyButton");
	}
	if(stepRecord)
	{
		addButton(m_toggleStepRecordingAction, "stepRecordButton");
	}
	addButton(m_stopAction, "stopButton");
}

QAction *Editor::playAction() const
{
	return m_playAction;
}

void Editor::setTransportToolTips(const QString& play, const QString& stop)
{
	const auto& shortcuts = configuredTransportShortcuts();
	const auto playStopKey = shortcuts.playStop.toString(QKeySequence::NativeText);
	const auto pauseKey = shortcuts.pause.toString(QKeySequence::NativeText);
	m_playAction->setToolTip(tr("%1 (%2)\nPause (%3)").arg(play, playStopKey, pauseKey));
	m_stopAction->setToolTip(tr("%1 (%2)").arg(stop, playStopKey));
}

bool Editor::handleTransportKey(QKeyEvent* event, Editor* target)
{
	using Action = TransportShortcuts::Action;
	const auto action = configuredTransportShortcuts().match(*event);
	if (action == Action::None) { return false; }
	if (target && action == Action::PlayStop) { target->togglePlayStop(); }
	if (target && action == Action::Pause) { target->togglePause(); }
	event->accept();
	return true;
}

void Editor::keyPressEvent(QKeyEvent* ke)
{
	if (!handleTransportKey(ke, this)) { ke->ignore(); }
}

DropToolBar::DropToolBar(QWidget* parent) : QToolBar(parent)
{
	setAcceptDrops(true);
}

void DropToolBar::dragEnterEvent(QDragEnterEvent* event)
{
	dragEntered(event);
}

void DropToolBar::dropEvent(QDropEvent* event)
{
	dropped(event);
}



} // namespace lmms::gui
