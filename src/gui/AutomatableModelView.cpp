/*
 * AutomatableModelView.cpp - implementation of AutomatableModelView
 *
 * Copyright (c) 2011-2014 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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

#include <algorithm>

#include <QApplication>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QScrollBar>

#include "AutomatableModelView.h"
#include "AutomationClip.h"
#include "AutomationClipView.h"
#include "ControllerConnectionDialog.h"
#include "ControllerConnection.h"
#include "DeprecationHelper.h"
#include "embed.h"
#include "Engine.h"
#include "GuiApplication.h"
#include "KeyboardShortcuts.h"
#include "MainWindow.h"
#include "PatternEditor.h"
#include "PatternStore.h"
#include "Song.h"
#include "SongEditor.h"
#include "StringPairDrag.h"
#include "TrackView.h"
#include "UiFeedback.h"
#include "Clipboard.h"

#include "AutomationEditor.h"


namespace lmms::gui
{

namespace
{
bool isBoundTo(AutomationClipView* view, const AutomatableModel* model)
{
	auto* clip = static_cast<AutomationClip*>(view->getClip());
	return std::ranges::any_of(clip->objects(), [model](const auto& object) { return object == model; });
}

QList<AutomationClipView*> automationClipViews(const AutomatableModel* model)
{
	auto* gui = getGUI();
	if (!gui || !model) { return {}; }

	QList<AutomationClipView*> matches;
	for (const auto* editor : {static_cast<QWidget*>(gui->songEditor()),
			static_cast<QWidget*>(gui->patternEditor())})
	{
		for (auto* view : editor->findChildren<AutomationClipView*>())
		{
			if (isBoundTo(view, model)) { matches.append(view); }
		}
	}
	return matches;
}

class AutomationClipPicker : public QObject
{
public:
	static void open(AutomatableModel* model)
	{
		static QPointer<AutomationClipPicker> active;
		if (active) { active->finish(); }

		const auto views = automationClipViews(model);
		if (views.isEmpty()) { return; }
		if (views.size() == 1)
		{
			views.front()->openInAutomationEditor();
			return;
		}

		active = new AutomationClipPicker(model, views);
	}

protected:
	bool eventFilter(QObject* object, QEvent* event) override
	{
		const auto type = event->type();
		if ((type == QEvent::ShortcutOverride || type == QEvent::KeyPress)
			&& static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape)
		{
			event->accept();
			if (type == QEvent::KeyPress) { finish(); }
			return true;
		}

		if (type != QEvent::MouseButtonPress && type != QEvent::MouseButtonRelease) { return false; }
		const auto* mouse = static_cast<QMouseEvent*>(event);
		if (mouse->button() != Qt::LeftButton) { return false; }

		auto* target = qobject_cast<QWidget*>(object);
		if (!target) { return false; }
		auto* clipWidget = target;
		while (clipWidget && !qobject_cast<ClipView*>(clipWidget)) { clipWidget = clipWidget->parentWidget(); }
		auto* view = qobject_cast<AutomationClipView*>(clipWidget);

		if (type == QEvent::MouseButtonPress)
		{
			m_pressed.clear();
			if (view && m_views.contains(view) && isBoundTo(view, m_model))
			{
				m_pressed = view;
				return true;
			}
			if (isTimelineNavigation(target)) { return false; }

			// An empty track cell normally creates a clip on mouse press.
			finish();
			return true;
		}
		if (!m_pressed) { return false; }

		const bool chosen = view == m_pressed && isBoundTo(view, m_model)
			&& view->rect().contains(view->mapFromGlobal(globalPosition(mouse)));
		m_pressed.clear();
		finish();
		if (chosen)
		{
			view->openInAutomationEditor();
		}
		return true;
	}

private:
	bool isTimelineNavigation(const QWidget* widget) const
	{
		const auto* songEditor = getGUI()->songEditor();
		const auto* patternEditor = getGUI()->patternEditor();
		return m_mainWindow->isTimelineEditorButton(widget) || patternEditor->isPatternSelector(widget)
			|| (qobject_cast<const QScrollBar*>(widget)
				&& (songEditor->isAncestorOf(widget) || patternEditor->isAncestorOf(widget)));
	}

	AutomationClipPicker(AutomatableModel* model, const QList<AutomationClipView*>& views) :
		QObject(getGUI()),
		m_mainWindow(getGUI()->mainWindow()),
		m_model(model),
		m_hint(AutomatableModel::tr("Click a highlighted automation clip. Click elsewhere or press Esc to cancel."))
	{
		auto* songEditor = getGUI()->songEditor();
		auto* patternEditor = getGUI()->patternEditor();
		AutomationClipView* firstSongClip = nullptr;
		AutomationClipView* firstPatternClip = nullptr;
		const auto playhead = Engine::getSong()->getPlayPos(Song::PlayMode::Song);
		for (auto* view : views)
		{
			m_views.append(view);
			UiFeedback::setPulsingHighlight(view, true);
			auto* editor = view->getTrackView()->trackContainerView();
			if (editor == patternEditor->m_editor)
			{
				if (!firstPatternClip || view->getClip()->startPosition().getBar() == Engine::patternStore()->currentPattern())
				{
					firstPatternClip = view;
				}
			}
			else if (!firstSongClip
				|| qAbs(view->getClip()->startPosition() - playhead) < qAbs(firstSongClip->getClip()->startPosition() - playhead))
			{
				firstSongClip = view;
			}
		}

		if (firstPatternClip)
		{
			Engine::patternStore()->setCurrentPattern(firstPatternClip->getClip()->startPosition().getBar());
			m_mainWindow->togglePatternEditorWin(true);
			patternEditor->m_editor->scrollToTrackView(firstPatternClip->getTrackView());
		}
		if (firstSongClip)
		{
			songEditor->parentWidget()->show();
			songEditor->show();
			songEditor->setFocus();
			songEditor->m_editor->revealClip(firstSongClip);
		}
		if (firstPatternClip && firstSongClip)
		{
			m_hint += QStringLiteral(" ") + AutomatableModel::tr("You can switch between the Song and Pattern Editors.");
		}
		m_mainWindow->showStatusHint(m_hint);
		connect(model, &QObject::destroyed, this, &AutomationClipPicker::finish);
		connect(Engine::getSong(), &Song::projectLoaded, this, &AutomationClipPicker::finish);
		qApp->installEventFilter(this);
	}

	void finish()
	{
		if (m_views.isEmpty()) { return; }
		qApp->removeEventFilter(this);
		for (const auto& view : m_views)
		{
			if (view) { UiFeedback::setPulsingHighlight(view, false); }
		}
		m_views.clear();
		if (m_mainWindow) { m_mainWindow->clearStatusHint(m_hint); }
		deleteLater();
	}

	QPointer<MainWindow> m_mainWindow;
	QPointer<AutomatableModel> m_model;
	QPointer<AutomationClipView> m_pressed;
	QList<QPointer<AutomationClipView>> m_views;
	QString m_hint;
};
}

static float floatFromClipboard(bool* ok=nullptr);

AutomatableModelView::AutomatableModelView( Model* model, QWidget* _this ) :
	ModelView( model, _this ),
	m_conversionFactor( 1.0 )
{
	widget()->setAcceptDrops( true );
	widget()->setCursor(Qt::PointingHandCursor);
}

void AutomatableModelView::addDefaultActions( QMenu* menu )
{
	AutomatableModel* model = modelUntyped();

	auto amvSlots = new AutomatableModelViewSlots(this, menu);

	menu->addAction( embed::getIconPixmap( "reload" ),
						AutomatableModel::tr( "&Reset (%1%2)" ).
							arg( model->initValue<float>() * m_conversionFactor ).
							arg( m_unit ),
						model, SLOT(reset()));

	menu->addSeparator();
	menu->addAction( embed::getIconPixmap( "edit_copy" ),
						AutomatableModel::tr( "&Copy value (%1%2)" ).
							arg( model->value<float>() * m_conversionFactor ).
							arg( m_unit ),
						amvSlots, SLOT(copyToClipboard()));

	bool canPaste = true;
	const float valueToPaste = floatFromClipboard(&canPaste);
	const QString pasteDesc = canPaste ?
					AutomatableModel::tr( "&Paste value (%1%2)").
						arg( valueToPaste ).
						arg( m_unit )
					: AutomatableModel::tr( "&Paste value");
	QAction* pasteAction = menu->addAction( embed::getIconPixmap( "edit_paste" ),
						pasteDesc, amvSlots, SLOT(pasteFromClipboard()));
	pasteAction->setEnabled(canPaste);

	if (!automationClipViews(model).isEmpty())
	{
		menu->addAction(embed::getIconPixmap("automation"),
			AutomatableModel::tr("Show in Automation Editor"), menu, [target = QPointer<AutomatableModel>(model)]
			{
				AutomationClipPicker::open(target);
			});
	}

	menu->addSeparator();

	if (model->isLinked())
	{
		menu->addAction(embed::getIconPixmap("edit_unlink"),
							AutomatableModel::tr("Remove all linked controls"),
							model, SLOT(unlink()));
		menu->addSeparator();
	}

	QString controllerTxt;
	if( model->controllerConnection() )
	{
		Controller* cont = model->controllerConnection()->getController();
		if( cont )
		{
			controllerTxt = AutomatableModel::tr( "Connected to %1" ).arg( cont->name() );
		}
		else
		{
			controllerTxt = AutomatableModel::tr( "Connected to controller" );
		}

		QMenu* contMenu = menu->addMenu( embed::getIconPixmap( "controller" ), controllerTxt );

		contMenu->addAction( embed::getIconPixmap( "controller" ),
								AutomatableModel::tr("Edit connection..."),
								amvSlots, SLOT(execConnectionDialog()));
		contMenu->addAction( embed::getIconPixmap( "cancel" ),
								AutomatableModel::tr("Remove connection"),
								amvSlots, SLOT(removeConnection()));
	}
	else
	{
		menu->addAction( embed::getIconPixmap( "controller" ),
							AutomatableModel::tr("Connect to controller..."),
							amvSlots, SLOT(execConnectionDialog()));
	}
}




void AutomatableModelView::setModel( Model* model, bool isOldModelValid )
{
	ModelView::setModel( model, isOldModelValid );
}




// Unsets the current model by setting a dummy empty model. The dummy model is marked as
// "defaultConstructed", so the next call to setModel will delete it.
void AutomatableModelView::unsetModel()
{
	if (dynamic_cast<FloatModelView*>(this))
	{
		setModel(new FloatModel(0, 0, 0, 1, nullptr, QString(), true));
	}
	else if (dynamic_cast<IntModelView*>(this))
	{
		setModel(new IntModel(0, 0, 0, nullptr, QString(), true));
	}
	else if (dynamic_cast<BoolModelView*>(this))
	{
		setModel(new BoolModel(false, nullptr, QString(), true));
	}
	else
	{
		ModelView::unsetModel();
	}
}




void AutomatableModelView::mousePressEvent( QMouseEvent* event )
{
	if (event->button() == Qt::LeftButton && event->modifiers() & KBD_COPY_MODIFIER)
	{
		new gui::StringPairDrag( "automatable_model", QString::number( modelUntyped()->id() ), QPixmap(), widget() );
		event->accept();
	}
	else if( event->button() == Qt::MiddleButton )
	{
		modelUntyped()->reset();
	}
}


void AutomatableModelView::setConversionFactor( float factor )
{
	if( factor != 0.0 )
	{
		m_conversionFactor = factor;
	}
}


float AutomatableModelView::getConversionFactor()
{
	return m_conversionFactor;
}


AutomatableModelViewSlots::AutomatableModelViewSlots( AutomatableModelView* amv, QObject* parent ) :
	QObject(),
	m_amv( amv )
{
	connect( parent, SIGNAL(destroyed()), this, SLOT(deleteLater()), Qt::QueuedConnection );
}




void AutomatableModelViewSlots::execConnectionDialog()
{
	// TODO[pg]: Display a dialog with list of controllers currently in the song
	// in addition to any system MIDI controllers
	AutomatableModel* m = m_amv->modelUntyped();

	m->displayName();
	gui::ControllerConnectionDialog d( getGUI()->mainWindow(), m );

	if( d.exec() == 1 )
	{
		// Actually chose something
		if( d.chosenController() )
		{
			// Update
			if( m->controllerConnection() )
			{
				m->controllerConnection()->setController( d.chosenController() );
			}
			// New
			else
			{
				auto cc = new ControllerConnection(d.chosenController());
				m->setControllerConnection( cc );
				//cc->setTargetName( m->displayName() );
			}
		}
		// no controller, so delete existing connection
		else
		{
			removeConnection();
		}
	}
}




void AutomatableModelViewSlots::removeConnection()
{
	AutomatableModel* m = m_amv->modelUntyped();

	if( m->controllerConnection() )
	{
		delete m->controllerConnection();
		m->setControllerConnection( nullptr );
	}
}




void AutomatableModelViewSlots::copyToClipboard()
{
	// For copyString() and MimeType enum class
	using namespace Clipboard;

	copyString( QString::number( m_amv->value<float>() * m_amv->getConversionFactor() ), MimeType::Default );
}

void AutomatableModelViewSlots::pasteFromClipboard()
{
	bool isNumber = false;
	const float number = floatFromClipboard(&isNumber);
	if (isNumber) {
		m_amv->modelUntyped()->setValue(number / m_amv->getConversionFactor());
	}
}

/// Attempt to parse a float from the clipboard
static float floatFromClipboard(bool* ok)
{
	// For getString() and MimeType enum class
	using namespace Clipboard;

	return getString( MimeType::Default ).toFloat(ok);
}


} // namespace lmms::gui
