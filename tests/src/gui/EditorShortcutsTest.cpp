#include <QAction>
#include <QFile>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QTemporaryDir>
#include <QToolBar>
#include <QtTest>

#include "ConfigManager.h"
#include "Editor.h"
#include "EditorShortcuts.h"
#include "ToolButton.h"

using lmms::ConfigManager;
using lmms::gui::bindEditorShortcuts;
using lmms::gui::mainMenuShortcuts;
using lmms::gui::patternNavigationShortcuts;
using lmms::gui::TransportShortcuts;
using lmms::gui::transportShortcuts;
using lmms::gui::windowLayoutShortcuts;
using lmms::gui::windowToggleShortcuts;

class EditorShortcutsTest : public QObject
{
	Q_OBJECT

private slots:
	void cleanup()
	{
		for (const auto* id : {"piano_roll_draw", "piano_roll_erase", "automation_editor_draw", "window_song_editor",
			"file_save", "pattern_next", "transport_play_stop", "transport_pause", "layout_fullscreen",
			"layout_maximize_editor", "layout_detach_all", "layout_attach_all", "transport_record",
			"transport_record_accompany", "transport_record_step"})
		{
			ConfigManager::inst()->deleteValue("shortcuts", id);
		}
	}

	void readsAndPreservesPortableOverrides()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		QFile file(dir.filePath("lmmsrc.xml"));
		QVERIFY(file.open(QIODevice::WriteOnly));
		file.write(QStringLiteral("<lmms><paths workingdir=\"%1\"/><shortcuts piano_roll_draw=\"Alt+D\"/></lmms>")
			.arg(dir.path().toHtmlEscaped()).toUtf8());
		file.close();

		auto* config = ConfigManager::inst();
		config->loadConfigFile(file.fileName());
		QWidget editor;
		QToolBar toolbar(&editor);
		lmms::gui::ToolButtonAction draw(QIcon{}, QStringLiteral("Draw mode"), &editor);
		toolbar.addAction(&draw);
		bindEditorShortcuts(editor, {{"piano_roll_draw", &draw}});
		const auto sequence = QKeySequence::fromString("Alt+D", QKeySequence::PortableText);
		QCOMPARE(draw.shortcut(), sequence);
		QVERIFY(draw.toolTip().contains(sequence.toString(QKeySequence::NativeText)));
		QVERIFY(toolbar.widgetForAction(&draw));
		QCOMPARE(toolbar.widgetForAction(&draw)->toolTip(), draw.toolTip());
		config->saveConfigFile();
		QVERIFY(file.open(QIODevice::ReadOnly));
		QVERIFY(file.readAll().contains("piano_roll_draw=\"Alt+D\""));
	}

	void rejectsInvalidAndConflictingOverrides_data()
	{
		QTest::addColumn<QString>("overrideKey");
		QTest::newRow("unknown key") << QStringLiteral("Ctrl+NotAKey");
		QTest::newRow("duplicate") << QStringLiteral("Shift+E");
		QTest::newRow("prefix") << QStringLiteral("Shift+E, Shift+D");
	}

	void rejectsInvalidAndConflictingOverrides()
	{
		QFETCH(QString, overrideKey);
		ConfigManager::inst()->setValue("shortcuts", "piano_roll_draw", overrideKey);
		QWidget editor;
		QAction draw(&editor);
		QAction erase(&editor);
		bindEditorShortcuts(editor, {{"piano_roll_draw", &draw}, {"piano_roll_erase", &erase}});
		QCOMPARE(draw.shortcut(), QKeySequence("Shift+D"));
		QCOMPARE(erase.shortcut(), QKeySequence("Shift+E"));
	}

	void dispatchesSharedShortcutOnlyInFocusedEditor_data()
	{
		QTest::addColumn<bool>("useOverride");
		QTest::newRow("defaults") << false;
		QTest::newRow("overrides") << true;
	}

	void dispatchesSharedShortcutOnlyInFocusedEditor()
	{
		QFETCH(bool, useOverride);
		if (useOverride)
		{
			auto* config = ConfigManager::inst();
			config->setValue("shortcuts", "piano_roll_draw", "Alt+D");
			config->setValue("shortcuts", "automation_editor_draw", "Alt+D");
		}
		const auto modifier = useOverride ? Qt::AltModifier : Qt::ShiftModifier;
		QMdiArea workspace;
		auto* piano = new QWidget;
		auto* automation = new QWidget;
		auto* pianoWindow = workspace.addSubWindow(piano);
		auto* automationWindow = workspace.addSubWindow(automation);
		QWidget pianoChild(piano);
		QWidget automationChild(automation);
		pianoChild.setFocusPolicy(Qt::StrongFocus);
		automationChild.setFocusPolicy(Qt::StrongFocus);
		QAction pianoDraw(piano);
		QAction automationDraw(automation);
		QToolBar pianoTools(piano);
		QToolBar automationTools(automation);
		pianoTools.addAction(&pianoDraw);
		automationTools.addAction(&automationDraw);
		bindEditorShortcuts(*piano, {{"piano_roll_draw", &pianoDraw}});
		bindEditorShortcuts(*automation, {{"automation_editor_draw", &automationDraw}});
		QSignalSpy pianoTriggers(&pianoDraw, &QAction::triggered);
		QSignalSpy automationTriggers(&automationDraw, &QAction::triggered);
		workspace.show();
		QVERIFY(QTest::qWaitForWindowExposed(&workspace));
		QApplication::setActiveWindow(&workspace);

		workspace.setActiveSubWindow(pianoWindow);
		pianoChild.setFocus();
		QCOMPARE(QApplication::focusWidget(), &pianoChild);
		QTest::keyClick(&pianoChild, Qt::Key_D, modifier);
		QCOMPARE(pianoTriggers.count(), 1);
		QCOMPARE(automationTriggers.count(), 0);

		workspace.setActiveSubWindow(automationWindow);
		automationChild.setFocus();
		QCOMPARE(QApplication::focusWidget(), &automationChild);
		QTest::keyClick(&automationChild, Qt::Key_D, modifier);
		QCOMPARE(pianoTriggers.count(), 1);
		QCOMPARE(automationTriggers.count(), 1);
	}

	void resolvesWindowShortcuts_data()
	{
		QTest::addColumn<QString>("overrideKey");
		QTest::addColumn<QString>("expectedKey");
		QTest::newRow("default") << QString{} << QStringLiteral("Ctrl+1");
		QTest::newRow("override") << QStringLiteral("Alt+1") << QStringLiteral("Alt+1");
		QTest::newRow("another window") << QStringLiteral("Ctrl+2") << QStringLiteral("Ctrl+1");
		QTest::newRow("existing application action") << QStringLiteral("Ctrl+S, Alt+1") << QStringLiteral("Ctrl+1");
		QTest::newRow("configured editor action") << QStringLiteral("Alt+D") << QStringLiteral("Ctrl+1");
	}

	void resolvesWindowShortcuts()
	{
		QFETCH(QString, overrideKey);
		QFETCH(QString, expectedKey);
		auto* config = ConfigManager::inst();
		config->setValue("shortcuts", "window_song_editor", overrideKey);
		config->setValue("shortcuts", "piano_roll_draw", "Alt+D");
		const auto map = windowToggleShortcuts({QKeySequence("Ctrl+S")});
		QCOMPARE(map.size(), 7);
		QCOMPARE(map.value("window_song_editor"), QKeySequence(expectedKey));
		QCOMPARE(map.value("window_pattern_editor"), QKeySequence("Ctrl+2"));
	}

	void editorOverridesCannotTakeWindowDefaults()
	{
		ConfigManager::inst()->setValue("shortcuts", "piano_roll_draw", "Ctrl+1");
		QWidget editor;
		QAction draw(&editor);
		bindEditorShortcuts(editor, {{"piano_roll_draw", &draw}});
		QCOMPARE(draw.shortcut(), QKeySequence("Shift+D"));
	}

	void resolvesMenuShortcuts_data()
	{
		QTest::addColumn<QString>("overrideKey");
		QTest::addColumn<bool>("accepted");
		QTest::newRow("default") << QString{} << false;
		QTest::newRow("override") << QStringLiteral("Alt+9") << true;
		QTest::newRow("other menu action") << QKeySequence(QKeySequence::Open).toString(QKeySequence::PortableText) << false;
		QTest::newRow("window default") << QStringLiteral("Ctrl+1") << false;
		QTest::newRow("editor override") << QStringLiteral("Alt+D") << false;
		QTest::newRow("fullscreen prefix") << QStringLiteral("F11, Alt+9") << false;
	}

	void resolvesMenuShortcuts()
	{
		QFETCH(QString, overrideKey);
		QFETCH(bool, accepted);
		auto* config = ConfigManager::inst();
		config->setValue("shortcuts", "piano_roll_draw", "Alt+D");
		config->setValue("shortcuts", "file_save", overrideKey);
		const auto shortcuts = mainMenuShortcuts({QKeySequence(Qt::Key_F11)});
		QCOMPARE(shortcuts.size(), 11);
		QCOMPARE(shortcuts.value("file_new"), QKeySequence(QKeySequence::New));
		QCOMPARE(shortcuts.value("edit_undo"), QKeySequence(QKeySequence::Undo));
		QCOMPARE(shortcuts.value("edit_redo"), QKeySequence(QKeySequence::Redo));
		QCOMPARE(shortcuts.value("file_save"), accepted ? QKeySequence(overrideKey) : QKeySequence(QKeySequence::Save));
	}

	void menuAndWindowOverridesCannotShareAKey()
	{
		auto* config = ConfigManager::inst();
		config->setValue("shortcuts", "file_save", "Alt+9");
		config->setValue("shortcuts", "window_song_editor", "Alt+9");
		const auto menu = mainMenuShortcuts({});
		QCOMPARE(menu.value("file_save"), QKeySequence("Alt+9"));
		QCOMPARE(windowToggleShortcuts(menu.values()).value("window_song_editor"), QKeySequence("Ctrl+1"));
	}

	void editorOverridesCannotTakeMenuDefaults()
	{
		ConfigManager::inst()->setValue("shortcuts", "piano_roll_draw",
			QKeySequence(QKeySequence::Save).toString(QKeySequence::PortableText));
		QWidget editor;
		QAction draw(&editor);
		bindEditorShortcuts(editor, {{"piano_roll_draw", &draw}});
		QCOMPARE(draw.shortcut(), QKeySequence("Shift+D"));
	}

	void resolvesPatternNavigation_data()
	{
		QTest::addColumn<QString>("overrideKey");
		QTest::addColumn<bool>("accepted");
		QTest::newRow("default") << QString{} << false;
		QTest::newRow("override") << QStringLiteral("Alt+9") << true;
		QTest::newRow("duplicate") << QStringLiteral("-") << false;
		QTest::newRow("window default") << QStringLiteral("Ctrl+1") << false;
		QTest::newRow("menu default") << QKeySequence(QKeySequence::Save).toString(QKeySequence::PortableText) << false;
		QTest::newRow("editor override") << QStringLiteral("Alt+D") << false;
		QTest::newRow("editor maximize") << QStringLiteral("Shift+F11") << false;
	}

	void resolvesPatternNavigation()
	{
		QFETCH(QString, overrideKey);
		QFETCH(bool, accepted);
		auto* config = ConfigManager::inst();
		config->setValue("shortcuts", "piano_roll_draw", "Alt+D");
		config->setValue("shortcuts", "pattern_next", overrideKey);
		const auto shortcuts = patternNavigationShortcuts();
		QCOMPARE(shortcuts.value("pattern_previous"), QKeySequence(Qt::Key_Minus));
		QCOMPARE(shortcuts.value("pattern_next"), accepted ? QKeySequence(overrideKey) : QKeySequence(Qt::Key_Plus));
	}

	void patternOverrideTakesPrecedenceOverMainWindowOverrides()
	{
		auto* config = ConfigManager::inst();
		config->setValue("shortcuts", "pattern_next", "Alt+9");
		config->setValue("shortcuts", "file_save", "Alt+9");
		config->setValue("shortcuts", "window_song_editor", "Alt+9");
		const auto menu = mainMenuShortcuts({});
		QCOMPARE(patternNavigationShortcuts().value("pattern_next"), QKeySequence("Alt+9"));
		QCOMPARE(menu.value("file_save"), QKeySequence(QKeySequence::Save));
		QCOMPARE(windowToggleShortcuts(menu.values()).value("window_song_editor"), QKeySequence("Ctrl+1"));
	}

	void matchesTransportKeys()
	{
		using Action = TransportShortcuts::Action;
		const auto defaults = transportShortcuts();
		const QKeyEvent space(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier);
		const QKeyEvent shiftSpace(QEvent::KeyPress, Qt::Key_Space, Qt::ShiftModifier);
		const QKeyEvent ctrlSpace(QEvent::KeyPress, Qt::Key_Space, Qt::ControlModifier);
		const QKeyEvent left(QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier);
		QCOMPARE(defaults.match(space), Action::PlayStop);
		QCOMPARE(defaults.match(shiftSpace), Action::Pause);
		QCOMPARE(defaults.match(ctrlSpace), Action::PlayStop);
		QCOMPARE(defaults.match(left), Action::None);

		auto* config = ConfigManager::inst();
		config->setValue("shortcuts", "transport_play_stop", "Alt+F7");
		config->setValue("shortcuts", "transport_pause", "Alt+F8");
		const auto custom = transportShortcuts();
		QCOMPARE(custom.match(QKeyEvent(QEvent::KeyPress, Qt::Key_F7, Qt::AltModifier)), Action::PlayStop);
		QCOMPARE(custom.match(QKeyEvent(QEvent::KeyPress, Qt::Key_F8, Qt::AltModifier)), Action::Pause);
		QCOMPARE(custom.match(QKeyEvent(QEvent::KeyPress, Qt::Key_F7, Qt::NoModifier)), Action::None);
		QCOMPARE(custom.match(space), Action::None);
		QCOMPARE(custom.match(shiftSpace), Action::None);
		QCOMPARE(custom.match(ctrlSpace), Action::None);
	}

	void rejectsTransportConflictsAndMultiStrokeOverrides()
	{
		auto* config = ConfigManager::inst();
		config->setValue("shortcuts", "transport_pause", "Alt+F8");
		for (const auto* sequence : {"Alt+F7, Alt+F8", "Alt+8", "Shift+Alt+9", "Ctrl+1", "Num+0"})
		{
			config->setValue("shortcuts", "transport_play_stop", sequence);
			QCOMPARE(transportShortcuts().playStop, QKeySequence(Qt::Key_Space));
			QCOMPARE(transportShortcuts().pause, QKeySequence("Alt+F8"));
		}

		for (const auto* sequence : {"Alt+F8", "Shift+D", "Shift+F11", "+"})
		{
			config->setValue("shortcuts", "transport_play_stop", sequence);
			const auto shortcuts = transportShortcuts();
			QCOMPARE(shortcuts.playStop, QKeySequence(Qt::Key_Space));
			QCOMPARE(shortcuts.pause, QKeySequence("Shift+Space"));
		}
	}

	void transportOverrideTakesPrecedenceOverOtherOverrides()
	{
		auto* config = ConfigManager::inst();
		for (const auto* id : {"transport_play_stop", "piano_roll_draw", "pattern_next", "file_save", "window_song_editor"})
		{
			config->setValue("shortcuts", id, "Alt+F7");
		}
		QWidget editor;
		QAction draw(&editor);
		bindEditorShortcuts(editor, {{"piano_roll_draw", &draw}});
		QCOMPARE(draw.shortcut(), QKeySequence("Shift+D"));
		QCOMPARE(patternNavigationShortcuts().value("pattern_next"), QKeySequence(Qt::Key_Plus));
		QCOMPARE(mainMenuShortcuts({}).value("file_save"), QKeySequence(QKeySequence::Save));
		QCOMPARE(windowToggleShortcuts({}).value("window_song_editor"), QKeySequence("Ctrl+1"));
	}

	void resolvesWindowLayoutShortcuts()
	{
		const auto defaults = windowLayoutShortcuts();
		QCOMPARE(defaults.value("layout_fullscreen"), QKeySequence("F11"));
		QCOMPARE(defaults.value("layout_maximize_editor"), QKeySequence("Shift+F11"));
		QCOMPARE(defaults.value("layout_detach_all"), QKeySequence("Ctrl+Shift+D"));
		QCOMPARE(defaults.value("layout_attach_all"), QKeySequence("Ctrl+Alt+Shift+D"));

		auto* config = ConfigManager::inst();
		config->setValue("shortcuts", "layout_fullscreen", "Ctrl+Shift+F9");
		config->setValue("shortcuts", "layout_maximize_editor", "Ctrl+Shift+F10");
		QCOMPARE(windowLayoutShortcuts().value("layout_fullscreen"), QKeySequence("Ctrl+Shift+F9"));
		QCOMPARE(windowLayoutShortcuts().value("layout_maximize_editor"), QKeySequence("Ctrl+Shift+F10"));

		for (const auto* conflicting : {"Space", "Ctrl+1", "Shift+D", "-", "Ctrl+Shift+F9, F10"})
		{
			config->setValue("shortcuts", "layout_maximize_editor", conflicting);
			QCOMPARE(windowLayoutShortcuts(), defaults);
		}
	}

	void layoutOverrideTakesPrecedenceOverOtherOverrides()
	{
		auto* config = ConfigManager::inst();
		for (const auto* id : {"layout_detach_all", "transport_pause", "piano_roll_draw", "pattern_next", "file_save",
			"window_song_editor"})
		{
			config->setValue("shortcuts", id, "Ctrl+Shift+F9");
		}
		QCOMPARE(windowLayoutShortcuts().value("layout_detach_all"), QKeySequence("Ctrl+Shift+F9"));
		QCOMPARE(transportShortcuts().pause, QKeySequence("Shift+Space"));
		QWidget editor;
		QAction draw(&editor);
		bindEditorShortcuts(editor, {{"piano_roll_draw", &draw}});
		QCOMPARE(draw.shortcut(), QKeySequence("Shift+D"));
		QCOMPARE(patternNavigationShortcuts().value("pattern_next"), QKeySequence("+"));
		QCOMPARE(mainMenuShortcuts({}).value("file_save"), QKeySequence(QKeySequence::Save));
		QCOMPARE(windowToggleShortcuts({}).value("window_song_editor"), QKeySequence("Ctrl+1"));
	}

	void resolvesOptionalRecordingShortcuts()
	{
		const auto defaults = transportShortcuts();
		QVERIFY(defaults.record.isEmpty());
		QVERIFY(defaults.recordAccompany.isEmpty());
		QVERIFY(defaults.recordStep.isEmpty());

		auto* config = ConfigManager::inst();
		config->setValue("shortcuts", "transport_play_stop", "Alt+F7");
		config->setValue("shortcuts", "transport_record_step", "Alt+F6");
		QCOMPARE(transportShortcuts().recordStep, QKeySequence("Alt+F6"));
		config->setValue("shortcuts", "file_save", "Alt+F6");
		QCOMPARE(mainMenuShortcuts({}).value("file_save"), QKeySequence(QKeySequence::Save));

		for (const auto* conflicting : {"Alt+F7", "Alt+F6", "Shift+D"})
		{
			config->setValue("shortcuts", "transport_record", conflicting);
			const auto shortcuts = transportShortcuts();
			QVERIFY(shortcuts.record.isEmpty());
			QVERIFY(shortcuts.recordStep.isEmpty());
			QCOMPARE(shortcuts.playStop, QKeySequence("Alt+F7"));
		}

		for (const auto* invalid : {"Alt+F5, Alt+F6", "Alt+9", "Ctrl+Shift+Space"})
		{
			config->setValue("shortcuts", "transport_record", invalid);
			QVERIFY(transportShortcuts().record.isEmpty());
			QCOMPARE(transportShortcuts().recordStep, QKeySequence("Alt+F6"));
		}
		config->setValue("shortcuts", "transport_record", "Ctrl+Space");
		QCOMPARE(transportShortcuts().record, QKeySequence("Ctrl+Space"));
		config->setValue("shortcuts", "transport_play_stop", "");
		QVERIFY(transportShortcuts().record.isEmpty());
	}

	void dispatchesRecordingOnlyInSupportingEditor()
	{
		using lmms::gui::Editor;
		class RecordingEditor : public Editor
		{
		public:
			RecordingEditor(bool recordEnabled, bool stepEnabled) : Editor(recordEnabled, stepEnabled)
			{
				setRecordingToolTips("Record", "Record with playback", "Step record");
			}

			QAction* recordAction() const { return m_recordAction; }
			QAction* accompanyAction() const { return m_recordAccompanyAction; }
			QAction* stepAction() const { return m_toggleStepRecordingAction; }
		};

		auto* config = ConfigManager::inst();
		config->setValue("shortcuts", "transport_record", "Alt+F5");
		config->setValue("shortcuts", "transport_record_accompany", "A");
		config->setValue("shortcuts", "transport_record_step", "Alt+F6");

		RecordingEditor piano(true, true);
		RecordingEditor song(true, false);
		RecordingEditor automation(false, false);
		QSignalSpy record(piano.recordAction(), &QAction::triggered);
		QSignalSpy accompany(piano.accompanyAction(), &QAction::triggered);
		QSignalSpy step(piano.stepAction(), &QAction::triggered);
		QSignalSpy songStep(song.stepAction(), &QAction::triggered);
		QSignalSpy unavailableRecord(automation.recordAction(), &QAction::triggered);
		QVERIFY(piano.stepAction()->toolTip().contains(QKeySequence("Alt+F6").toString(QKeySequence::NativeText)));

		QTest::keyClick(&piano, Qt::Key_F5, Qt::AltModifier);
		QCOMPARE(record.count(), 1);
		QTest::keyClick(&piano, Qt::Key_A);
		QCOMPARE(accompany.count(), 1);
		QTest::keyClick(&piano, Qt::Key_F6, Qt::AltModifier);
		QCOMPARE(step.count(), 1);
		QKeyEvent repeatedStep(QEvent::KeyPress, Qt::Key_F6, Qt::AltModifier, {}, true);
		QApplication::sendEvent(&piano, &repeatedStep);
		QVERIFY(repeatedStep.isAccepted());
		QCOMPARE(step.count(), 1);

		QTest::keyClick(&song, Qt::Key_F6, Qt::AltModifier);
		QTest::keyClick(&automation, Qt::Key_F5, Qt::AltModifier);
		QCOMPARE(songStep.count(), 0);
		QCOMPARE(unavailableRecord.count(), 0);

		auto* input = new QLineEdit(&piano);
		piano.setCentralWidget(input);
		QTest::keyClick(input, Qt::Key_A);
		QCOMPARE(input->text(), QStringLiteral("a"));
		QCOMPARE(accompany.count(), 1);

		QKeyEvent mainWindowFallback(QEvent::KeyPress, Qt::Key_F5, Qt::AltModifier);
		QVERIFY(!Editor::handleTransportKey(&mainWindowFallback, &piano));
		QCOMPARE(record.count(), 1);
	}
};

QTEST_MAIN(EditorShortcutsTest)
#include "EditorShortcutsTest.moc"
