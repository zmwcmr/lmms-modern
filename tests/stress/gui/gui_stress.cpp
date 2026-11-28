/*
 * gui_stress.cpp - one-scenario-per-process GUI stress driver
 *
 * Run with QT_QPA_PLATFORM=offscreen. The Python driver starts a fresh
 * process for every scenario so the GuiApplication singleton cannot leak
 * state between scenarios.
 */

#include <cstdio>
#include <cstdlib>
#include <string>

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QList>
#include <QPointer>
#include <QSize>
#include <QStringList>
#include <QTimer>

#include <QtTest>

#include "AutomationEditor.h"
#include "ConfigManager.h"
#include "Effect.h"
#include "EffectChain.h"
#include "Engine.h"
#include "GuiApplication.h"
#include "MainWindow.h"
#include "Mixer.h"
#include "MixerView.h"
#include "PatternEditor.h"
#include "PatternStore.h"
#include "PianoRoll.h"
#include "PluginView.h"
#include "ProjectJournal.h"
#include "Song.h"
#include "SongEditor.h"
#include "TimePos.h"
#include "Track.h"

using namespace lmms;
using namespace lmms::gui;

namespace
{

int g_failures = 0;

void check(bool condition, const char* message)
{
	if (!condition)
	{
		++g_failures;
		std::fprintf(stderr, "FAIL: %s\n", message);
	}
}

void installModalWatchdog()
{
	auto* timer = new QTimer(qApp);
	QObject::connect(timer, &QTimer::timeout, []() {
		if (QWidget* modal = QApplication::activeModalWidget())
		{
			std::fprintf(
				stderr,
				"modal dialog auto-closed: %s / %s\n",
				modal->metaObject()->className(),
				modal->windowTitle().toUtf8().constData());
			modal->close();
		}
	});
	timer->start(100);
}

void createAndDeleteTracks()
{
	auto* song = Engine::getSong();
	const Track::Type types[] = {
		Track::Type::Instrument,
		Track::Type::Sample,
		Track::Type::Automation,
		Track::Type::Pattern,
	};
	for (Track::Type type : types)
	{
		TrackContainer* container = type == Track::Type::Pattern
			? static_cast<TrackContainer*>(Engine::patternStore())
			: static_cast<TrackContainer*>(song);
		Track* track = Track::create(type, container);
		check(track != nullptr, "track creation");
		QTest::qWait(20);
		delete track;
		QTest::qWait(20);
	}
}

void openAndCloseEditors()
{
	auto* gui = getGUI();
	check(gui != nullptr, "GuiApplication");
	if (gui == nullptr) { return; }

	QWidget* editors[] = {
		gui->songEditor(),
		gui->patternEditor(),
		gui->pianoRoll(),
		gui->automationEditor(),
		gui->mixerView(),
	};
	for (QWidget* editor : editors)
	{
		if (editor == nullptr) { continue; }
		editor->show();
		QTest::qWait(30);
		editor->resize(900, 600);
		QTest::qWait(30);
		editor->hide();
	}
}

void undoRedoStorm()
{
	for (int i = 0; i < 200; ++i)
	{
		Engine::projectJournal()->undo();
		Engine::projectJournal()->redo();
	}
	QTest::qWait(50);
}

void loadPlaySave(const QString& project, const QString& output)
{
	if (project.isEmpty() || !QFileInfo::exists(project)) { return; }
	auto* song = Engine::getSong();
	song->loadProject(project);
	check(!song->isEmpty(), "project loaded");
	if (song->isEmpty()) { return; }
	song->playSong();
	QTest::qWait(500);
	song->stop();
	QTest::qWait(100);
	check(song->saveProjectFile(output), "project saved");
}

void mixerEffects()
{
	auto* mixer = Engine::mixer();
	check(mixer != nullptr, "mixer");
	if (mixer == nullptr) { return; }
	const int index = mixer->createChannel();
	MixerChannel* channel = mixer->mixerChannel(index);
	check(channel != nullptr, "mixer channel");
	if (channel == nullptr) { return; }

	Effect* effect = Effect::instantiate("amplifier", &channel->m_fxChain, nullptr);
	check(effect != nullptr, "effect instantiate");
	if (effect == nullptr) { return; }
	channel->m_fxChain.appendEffect(effect);
	QTest::qWait(50);

	gui::PluginView* view = effect->createView(nullptr);
	if (view != nullptr)
	{
		view->show();
		QTest::qWait(30);
		view->hide();
		delete view;
	}
	channel->m_fxChain.removeEffect(effect);
	delete effect;
	QTest::qWait(30);
}

void resizeStorm()
{
	auto* gui = getGUI();
	if (gui == nullptr) { return; }
	QWidget* window = gui->mainWindow();
	const QSize sizes[] = {QSize(800, 600), QSize(1280, 800), QSize(1024, 700)};
	for (const QSize& size : sizes)
	{
		window->resize(size);
		QTest::qWait(30);
	}
}

void keyboardSmoke()
{
	auto* gui = getGUI();
	if (gui == nullptr) { return; }
	QWidget* window = gui->mainWindow();
	const Qt::Key keys[] = {
		Qt::Key_Space, Qt::Key_Left, Qt::Key_Right,
		Qt::Key_Up, Qt::Key_Down, Qt::Key_Home, Qt::Key_End,
	};
	for (Qt::Key key : keys)
	{
		QTest::keyClick(window, key);
		QTest::qWait(20);
	}
}

void randomSafeActions()
{
	auto* gui = getGUI();
	if (gui == nullptr) { return; }
	const QStringList blocked = {
		"quit", "exit", "open", "save", "export", "delete", "remove",
		"clear", "reset", "uninstall", "settings", "preferences",
	};
	QList<QPointer<QAction>> actions;
	for (QAction* action : gui->mainWindow()->findChildren<QAction*>())
	{
		actions.append(action);
	}
	int triggered = 0;
	for (const QPointer<QAction>& guarded : actions)
	{
		QAction* action = guarded.data();
		if (action == nullptr || !action->isEnabled()) { continue; }
		const QString text = action->text().toLower();
		bool safe = !text.isEmpty();
		for (const QString& word : blocked)
		{
			if (text.contains(word)) { safe = false; break; }
		}
		if (!safe) { continue; }
		action->trigger();
		QTest::qWait(10);
		if (++triggered >= 100) { break; }
	}
	check(triggered > 0, "at least one safe action triggered");
}

} // namespace

int main(int argc, char** argv)
{
	QApplication app(argc, argv);
	installModalWatchdog();

	QString scenario;
	QString project;
	QString output;
	for (int i = 1; i < argc; ++i)
	{
		const QString arg = QString::fromLocal8Bit(argv[i]);
		if (arg == "--scenario" && i + 1 < argc) { scenario = argv[++i]; }
		else if (arg == "--project" && i + 1 < argc) { project = argv[++i]; }
		else if (arg == "--output" && i + 1 < argc) { output = argv[++i]; }
	}
	if (scenario.isEmpty())
	{
		std::fprintf(stderr, "usage: %s --scenario <name> [--project in.mmp] [--output out.mmp]\n", argv[0]);
		return 2;
	}
	if (output.isEmpty())
	{
		output = QDir::tempPath() + "/lmms-gui-stress-output.mmp";
	}

	const QString home = qEnvironmentVariable("LMMS_STRESS_HOME",
		QDir::tempPath() + "/lmms-gui-stress");
	QDir().mkpath(home + "/lmms");
	ConfigManager* config = ConfigManager::inst();
	config->setWorkingDir(home + "/lmms/");
	config->setValue("app", "configured", "1");
	config->setValue("audioengine", "audiodev", "Dummy (no sound output)");
	config->setValue("audioengine", "mididev", "Dummy");

	auto* gui = new GuiApplication;
	check(gui->mainWindow() != nullptr, "main window");
	if (gui->mainWindow() == nullptr)
	{
		std::fflush(nullptr);
		std::_Exit(1);
	}
	gui->mainWindow()->show();
	QTest::qWait(150);

	if (scenario == "startup_quit")
	{
		QTest::qWait(200);
	}
	else if (scenario == "track_crud")
	{
		createAndDeleteTracks();
	}
	else if (scenario == "editors")
	{
		openAndCloseEditors();
	}
	else if (scenario == "undo_redo")
	{
		undoRedoStorm();
	}
	else if (scenario == "load_play_save")
	{
		loadPlaySave(project, output);
	}
	else if (scenario == "mixer_effects")
	{
		mixerEffects();
	}
	else if (scenario == "resize")
	{
		resizeStorm();
	}
	else if (scenario == "keyboard")
	{
		keyboardSmoke();
	}
	else if (scenario == "random_actions")
	{
		randomSafeActions();
	}
	else
	{
		std::fprintf(stderr, "unknown scenario: %s\n", scenario.toUtf8().constData());
		return 2;
	}

	QTest::qWait(100);
	std::printf("scenario=%s failures=%d\n", scenario.toUtf8().constData(), g_failures);
	std::fflush(nullptr);
	// The process intentionally skips static destruction and QApplication
	// teardown: one scenario runs per process and the GUI/engine singletons
	// are not designed to be destroyed in an arbitrary order. This helper is
	// for crash/hang detection; leak checking is done by gui_smoke.py with a
	// real process exit.
	std::_Exit(g_failures == 0 ? 0 : 1);
}
