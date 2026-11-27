/*
 * GuiApplication.cpp
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

#include "GuiApplication.h"

#include "lmmsversion.h"

#include "LmmsStyle.h"
#include "LmmsPalette.h"

#include "AutomationEditor.h"
#include "ConfigManager.h"
#include "ControllerRackView.h"
#include "DropDownInteraction.h"
#include "MixerView.h"
#include "MainWindow.h"
#include "MicrotunerConfig.h"
#include "PatternEditor.h"
#include "PianoRoll.h"
#include "ProjectNotes.h"
#include "SongEditor.h"

#include <QAbstractButton>
#include <QApplication>
#include <QDebug>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QFontDatabase>
#include <QtGlobal>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSplashScreen>
#include <QSocketNotifier>

#include "embed.h"

#ifdef LMMS_BUILD_WIN32
#include <io.h>
#include <stdio.h>
#include <windows.h>
#else
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace lmms
{


namespace gui
{

namespace
{

//! Copies a widget's tooltip into its accessible name so that screen readers
//! can announce icon-only controls. Qt emits ToolTipChange whenever
//! QWidget::setToolTip() is called, i.e. while the UI is being constructed.
class AccessibleNameSync : public QObject
{
public:
	explicit AccessibleNameSync(QObject* parent = nullptr) : QObject(parent) {}

protected:
	bool eventFilter(QObject* watched, QEvent* event) override
	{
		if (event->type() == QEvent::ToolTipChange)
		{
			if (auto* widget = qobject_cast<QWidget*>(watched))
			{
				if (widget->accessibleName().isEmpty() && !widget->toolTip().isEmpty())
				{
					widget->setAccessibleName(widget->toolTip());
				}
			}
		}
		return QObject::eventFilter(watched, event);
	}
};


//! Shows a button's tooltip in the main status bar when the button has no
//! explicit status tip. This gives the many icon buttons in the main and
//! editor toolbars a hint without editing every construction site.
class ButtonStatusTipFallback : public QObject
{
public:
	explicit ButtonStatusTipFallback(QObject* parent = nullptr) : QObject(parent) {}

protected:
	bool eventFilter(QObject* watched, QEvent* event) override
	{
		const auto type = event->type();
		if (type != QEvent::Enter && type != QEvent::Leave)
		{
			return QObject::eventFilter(watched, event);
		}

		auto* button = qobject_cast<QAbstractButton*>(watched);
		if (button == nullptr || !button->statusTip().isEmpty() || button->toolTip().isEmpty())
		{
			return QObject::eventFilter(watched, event);
		}

		auto* gui = GuiApplication::instance();
		auto* mainWindow = gui != nullptr ? gui->mainWindow() : nullptr;
		if (mainWindow == nullptr || !mainWindow->isAncestorOf(button))
		{
			return QObject::eventFilter(watched, event);
		}

		if (type == QEvent::Enter)
		{
			mainWindow->showStatusHint(button->toolTip());
		}
		else
		{
			mainWindow->clearStatusHint(button->toolTip());
		}
		return QObject::eventFilter(watched, event);
	}
};


//! Qt's standard message boxes and input dialogs take their icons from the
//! platform icon theme, which is usually a different visual style than LMMS'
//! flat artwork. Replace the dialog icon and the standard button icons with
//! LMMS' own icons when the dialog is shown.
class StandardDialogIconSync : public QObject
{
public:
	explicit StandardDialogIconSync(QObject* parent = nullptr) : QObject(parent) {}

protected:
	bool eventFilter(QObject* watched, QEvent* event) override
	{
		if (event->type() == QEvent::Show)
		{
			if (auto* box = qobject_cast<QMessageBox*>(watched))
			{
				applyToMessageBox(*box);
			}
			else if (auto* dialog = qobject_cast<QInputDialog*>(watched))
			{
				applyToInputDialog(*dialog);
			}
		}
		return QObject::eventFilter(watched, event);
	}

private:
	static void setButtonIcon(QAbstractButton* button, const char* icon)
	{
		if (button == nullptr) { return; }
		constexpr int iconSize = 14;
		button->setIconSize(QSize(iconSize, iconSize));
		button->setIcon(QIcon(embed::getIconPixmap(icon, iconSize, iconSize)));
	}

	static void applyToMessageBox(QMessageBox& box)
	{
		const char* dialogIcon = nullptr;
		switch (box.icon())
		{
		case QMessageBox::Question:
			dialogIcon = "help";
			break;
		case QMessageBox::Warning:
		case QMessageBox::Critical:
			dialogIcon = "error";
			break;
		case QMessageBox::Information:
			dialogIcon = "info";
			break;
		default:
			break;
		}
		if (dialogIcon != nullptr)
		{
			box.setIconPixmap(embed::getIconPixmap(dialogIcon, 48, 48));
		}

		setButtonIcon(box.button(QMessageBox::Ok), "apply");
		setButtonIcon(box.button(QMessageBox::Yes), "apply");
		setButtonIcon(box.button(QMessageBox::YesToAll), "apply");
		setButtonIcon(box.button(QMessageBox::Save), "project_save");
		setButtonIcon(box.button(QMessageBox::SaveAll), "project_save");
		setButtonIcon(box.button(QMessageBox::Apply), "apply");
		setButtonIcon(box.button(QMessageBox::Reset), "reload");
		setButtonIcon(box.button(QMessageBox::Retry), "reload");
		setButtonIcon(box.button(QMessageBox::Discard), "discard");
		setButtonIcon(box.button(QMessageBox::Cancel), "cancel");
		setButtonIcon(box.button(QMessageBox::Close), "close");
		setButtonIcon(box.button(QMessageBox::No), "cancel");
		setButtonIcon(box.button(QMessageBox::NoToAll), "cancel");
		setButtonIcon(box.button(QMessageBox::Abort), "cancel");
		setButtonIcon(box.button(QMessageBox::Ignore), "discard");
		setButtonIcon(box.button(QMessageBox::Help), "help");
	}

	static void applyToInputDialog(QInputDialog& dialog)
	{
		const auto* buttonBox = dialog.findChild<QDialogButtonBox*>();
		if (buttonBox == nullptr) { return; }

		setButtonIcon(buttonBox->button(QDialogButtonBox::Ok), "apply");
		setButtonIcon(buttonBox->button(QDialogButtonBox::Cancel), "cancel");
		setButtonIcon(buttonBox->button(QDialogButtonBox::Help), "help");
	}
};

} // namespace


GuiApplication* getGUI()
{
	return GuiApplication::instance();
}


GuiApplication* GuiApplication::s_instance = nullptr;

GuiApplication* GuiApplication::instance()
{
	return s_instance;
}

bool GuiApplication::isWayland()
{
	return QGuiApplication::platformName().contains("wayland");
}



GuiApplication::GuiApplication()
{
	// Immediately register our SIGINT handler
	createSocketNotifier();

	// Init style and palette
	QFontDatabase::addApplicationFont(QStringLiteral(":/LMMS-LFO-Display-Bold.ttf"));
	QFontDatabase::addApplicationFont(QStringLiteral(":/LMMS-Send-Sans-Bold.ttf"));
	QDir::addSearchPath("artwork", ConfigManager::inst()->themeDir());
	QDir::addSearchPath("artwork", ConfigManager::inst()->defaultThemeDir());
	QDir::addSearchPath("artwork", ":/artwork");

	auto lmmsstyle = new LmmsStyle();
	QApplication::setStyle(lmmsstyle);

	auto lmmspal = new LmmsPalette(nullptr, lmmsstyle);
	auto lpal = new QPalette(lmmspal->palette());

	QApplication::setPalette( *lpal );
	LmmsStyle::s_palette = lpal;

	// Give every widget that only has a tooltip an accessible name as well,
	// so that screen readers can identify icon-only controls.
	qApp->installEventFilter(new AccessibleNameSync(this));
	// Standard dialogs otherwise use the platform icon theme, which does not
	// match LMMS' flat artwork.
	qApp->installEventFilter(new StandardDialogIconSync(this));
	qApp->installEventFilter(new DropDownInteraction(this));

	// Prompt the user to create the LMMS working directory (e.g.
	// ~/Documents/lmms) if it doesn't exist. This is done after the style is
	// applied so that the dialog is themed like the rest of the application.
	if ( !ConfigManager::inst()->hasWorkingDir() &&
		QMessageBox::question( nullptr,
				tr( "Working directory" ),
				tr( "The LMMS working directory %1 does not "
				"exist. Create it now? You can change the directory "
				"later via Edit -> Settings." ).arg( ConfigManager::inst()->workingDir() ),
					QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes ) == QMessageBox::Yes)
	{
		ConfigManager::inst()->createWorkingDir();
	}

#ifdef LMMS_BUILD_APPLE
	QApplication::setAttribute(Qt::AA_DontShowIconsInMenus, true);
#endif

	// Show splash screen
	QSplashScreen splashScreen( embed::getIconPixmap( "splash" ) );
	splashScreen.setFixedSize(embed::logicalSize(splashScreen.pixmap()));
	splashScreen.show();

	QHBoxLayout layout;
	layout.setAlignment(Qt::AlignBottom);
	splashScreen.setLayout(&layout);

	// Create a left-aligned label for loading progress 
	// & a right-aligned label for version info
	QLabel loadingProgressLabel;
	m_loadingProgressLabel = &loadingProgressLabel;
	QLabel versionLabel(MainWindow::tr( "Version %1" ).arg( LMMS_VERSION ));

	loadingProgressLabel.setAlignment(Qt::AlignLeft);
	versionLabel.setAlignment(Qt::AlignRight);

	layout.addWidget(&loadingProgressLabel);
	layout.addWidget(&versionLabel);

	// may have long gaps between future frames, so force update now
	splashScreen.update();
	qApp->processEvents();

	connect(Engine::inst(), SIGNAL(initProgress(const QString&)), 
		this, SLOT(displayInitProgress(const QString&)));

	// Init central engine which handles all components of LMMS
	Engine::init(false);

	s_instance = this;

	displayInitProgress(tr("Preparing UI"));

	m_mainWindow = new MainWindow;
	// Status tips on menu actions are handled by Qt; buttons only have
	// tooltips, so fall back to those while the status bar is visible.
	qApp->installEventFilter(new ButtonStatusTipFallback(this));
	connect(m_mainWindow, SIGNAL(destroyed(QObject*)), this, SLOT(childDestroyed(QObject*)));
	connect(m_mainWindow, SIGNAL(initProgress(const QString&)), 
		this, SLOT(displayInitProgress(const QString&)));

	displayInitProgress(tr("Preparing song editor"));
	m_songEditor = new SongEditorWindow(Engine::getSong());
	connect(m_songEditor, SIGNAL(destroyed(QObject*)), this, SLOT(childDestroyed(QObject*)));

	displayInitProgress(tr("Preparing mixer"));
	m_mixerView = new MixerView(Engine::mixer());
	connect(m_mixerView, SIGNAL(destroyed(QObject*)), this, SLOT(childDestroyed(QObject*)));

	displayInitProgress(tr("Preparing controller rack"));
	m_controllerRackView = new ControllerRackView;
	connect(m_controllerRackView, SIGNAL(destroyed(QObject*)), this, SLOT(childDestroyed(QObject*)));

	displayInitProgress(tr("Preparing project notes"));
	m_projectNotes = new ProjectNotes;
	connect(m_projectNotes, SIGNAL(destroyed(QObject*)), this, SLOT(childDestroyed(QObject*)));

	displayInitProgress(tr("Preparing microtuner"));
	m_microtunerConfig = new MicrotunerConfig;
	connect(m_microtunerConfig, SIGNAL(destroyed(QObject*)), this, SLOT(childDestroyed(QObject*)));

	displayInitProgress(tr("Preparing pattern editor"));
	m_patternEditor = new PatternEditorWindow(Engine::patternStore());
	connect(m_patternEditor, SIGNAL(destroyed(QObject*)), this, SLOT(childDestroyed(QObject*)));

	displayInitProgress(tr("Preparing piano roll"));
	m_pianoRoll = new PianoRollWindow();
	connect(m_pianoRoll, SIGNAL(destroyed(QObject*)), this, SLOT(childDestroyed(QObject*)));

	displayInitProgress(tr("Preparing automation editor"));
	m_automationEditor = new AutomationEditorWindow;
	connect(m_automationEditor, SIGNAL(destroyed(QObject*)), this, SLOT(childDestroyed(QObject*)));

	splashScreen.finish(m_mainWindow);
	m_mainWindow->finalize();

	m_loadingProgressLabel = nullptr;
}

GuiApplication::~GuiApplication()
{
	s_instance = nullptr;
}


void GuiApplication::displayInitProgress(const QString &msg)
{
	Q_ASSERT(m_loadingProgressLabel != nullptr);
	
	m_loadingProgressLabel->setText(msg);
	// must force a UI update and process events, as there may be long gaps between processEvents() calls during init
	m_loadingProgressLabel->repaint();
	qApp->processEvents();
}

void GuiApplication::childDestroyed(QObject *obj)
{
	// when any object that can be reached via getGUI()->mainWindow(), getGUI()->mixerView(), etc
	//   is destroyed, ensure that their accessor functions will return null instead of a garbage pointer.
	if (obj == m_mainWindow)
	{
		m_mainWindow = nullptr;
	}
	else if (obj == m_mixerView)
	{
		m_mixerView = nullptr;
	}
	else if (obj == m_songEditor)
	{
		m_songEditor = nullptr;
	}
	else if (obj == m_automationEditor)
	{
		m_automationEditor = nullptr;
	}
	else if (obj == m_patternEditor)
	{
		m_patternEditor = nullptr;
	}
	else if (obj == m_pianoRoll)
	{
		m_pianoRoll = nullptr;
	}
	else if (obj == m_projectNotes)
	{
		m_projectNotes = nullptr;
	}
	else if (obj == m_microtunerConfig)
	{
		m_microtunerConfig = nullptr;
	}
	else if (obj == m_controllerRackView)
	{
		m_controllerRackView = nullptr;
	}
}

void GuiApplication::sigintHandler(int)
{
#ifdef LMMS_BUILD_WIN32
	char message[] = "Sorry, SIGINT is unhandled on this platform\n";
	std::ignore = _write(_fileno(stderr), message, sizeof(message));
#else
	char a = 1;
	std::ignore = ::write(s_sigintFd[0], &a, sizeof(a));
#endif
}

// Create our unix signal notifiers
void GuiApplication::createSocketNotifier()
{
#ifdef LMMS_BUILD_WIN32
	// no-op
#else
	if (::socketpair(AF_UNIX, SOCK_STREAM, 0, s_sigintFd))
	{
		qFatal("Couldn't create SIGINT socketpair");
		return;
	}

	// Listen on the file descriptor for SIGINT
	m_sigintNotifier = new QSocketNotifier(s_sigintFd[1], QSocketNotifier::Read, this);
	connect(m_sigintNotifier, SIGNAL(activated(QSocketDescriptor)), this, SLOT(sigintOccurred()), Qt::QueuedConnection);
#endif
}

// Handle the shutdown event
void GuiApplication::sigintOccurred()
{
	m_sigintNotifier->setEnabled(false);
	qDebug() << "Shutting down...";
	// cleanup, etc
	qApp->exit(3);
	m_sigintNotifier->setEnabled(true);
}

#ifdef LMMS_BUILD_WIN32
QFont GuiApplication::getWin32SystemFont()
{
	auto metrics = NONCLIENTMETRICSW{ .cbSize = sizeof(NONCLIENTMETRICSW) };
	SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), &metrics, 0);
	int pointSize = static_cast<int>(metrics.lfMessageFont.lfHeight);
	if ( pointSize < 0 )
	{
		// height is in pixels, convert to points
		HDC hDC = GetDC( nullptr );
		pointSize = MulDiv(std::abs(pointSize), 72, GetDeviceCaps(hDC, LOGPIXELSY));
		ReleaseDC( nullptr, hDC );
	}

	return QFont{QString::fromWCharArray(metrics.lfMessageFont.lfFaceName), pointSize};
}
#endif


} // namespace gui

} // namespace lmms
