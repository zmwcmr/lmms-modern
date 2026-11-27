#include <QAction>
#include <QFile>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QTemporaryDir>
#include <QToolBar>
#include <QtTest>

#include "ConfigManager.h"
#include "EditorShortcuts.h"
#include "ToolButton.h"

using lmms::ConfigManager;
using lmms::gui::bindEditorShortcuts;
using lmms::gui::windowToggleShortcuts;

class EditorShortcutsTest : public QObject
{
	Q_OBJECT

private slots:
	void cleanup()
	{
		for (const auto* id : {"piano_roll_draw", "piano_roll_erase", "automation_editor_draw", "window_song_editor"})
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
		QVERIFY(QTest::qWaitForWindowActive(&workspace));

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
};

QTEST_MAIN(EditorShortcutsTest)
#include "EditorShortcutsTest.moc"
