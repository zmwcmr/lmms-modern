#include <memory>

#include <QtTest>
#include <QLabel>

#include "AutomatableModel.h"
#include "Engine.h"
#include "InlineValueEditor.h"

using lmms::FloatModel;
using lmms::gui::InlineValueEditor;

class InlineValueEditorTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase() { lmms::Engine::init(true); }
	void cleanupTestCase() { lmms::Engine::destroy(); }

	void edit_data()
	{
		QTest::addColumn<QString>("input");
		QTest::addColumn<int>("key");
		QTest::addColumn<float>("expected");
		QTest::newRow("return") << QString("7.5") << int(Qt::Key_Return) << 7.5f;
		QTest::newRow("decimal-comma") << QString("7,5") << int(Qt::Key_Return) << 7.5f;
		QTest::newRow("escape") << QString("7.5") << int(Qt::Key_Escape) << 5.f;
		QTest::newRow("outside") << QString("7.5") << 0 << 7.5f;
		QTest::newRow("clamp") << QString("99") << int(Qt::Key_Return) << 10.f;
		QTest::newRow("invalid") << QString("abc") << int(Qt::Key_Return) << 5.f;
	}

	void edit()
	{
		QFETCH(QString, input);
		QFETCH(int, key);
		QFETCH(float, expected);
		FloatModel model(5.f, 0.f, 10.f, 0.1f);
		QWidget window;
		QWidget control(&window);
		window.resize(320, 200);
		control.setGeometry(140, 80, 40, 25);
		window.show();
		const auto hint = InlineValueEditor::rangeHint(0.f, 10.f, "%");
		InlineValueEditor::showEditorWithHint(&control, &model, hint);
		auto* editor = window.findChild<InlineValueEditor*>();
		QVERIFY(editor);
		auto* label = window.findChild<QLabel*>("inlineValueEditorHint");
		QVERIFY(label);
		QCOMPARE(label->text(), hint);
		QVERIFY(window.rect().contains(label->geometry()));
		QCOMPARE(editor->selectedText(), editor->text());
		QTest::keyClicks(editor, input);
		if (key) { QTest::keyClick(editor, Qt::Key(key)); }
		else { QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, QPoint(10, 10)); }
		QCOMPARE(model.value(), expected);
		QTRY_VERIFY(!window.findChild<InlineValueEditor*>());
		QTRY_VERIFY(!window.findChild<QLabel*>("inlineValueEditorHint"));
	}

	void modelDestroyed()
	{
		QWidget window;
		QWidget control(&window);
		auto model = std::make_unique<FloatModel>();
		InlineValueEditor::showEditor(&control, model.get());
		QVERIFY(window.findChild<InlineValueEditor*>());
		model.reset();
		QTRY_VERIFY(!window.findChild<InlineValueEditor*>());
	}

	void leavesCaptionVisible()
	{
		FloatModel model;
		QWidget window;
		QWidget control(&window);
		control.setGeometry(140, 80, 40, 25);
		constexpr int digitsHeight = 16;
		InlineValueEditor::showEditorWithHint(&control, &model, "Range", {}, {}, 0, digitsHeight);
		auto* editor = window.findChild<InlineValueEditor*>();
		QVERIFY(editor);
		QCOMPARE(editor->geometry().bottom() + 1, control.y() + digitsHeight);
	}
};

QTEST_MAIN(InlineValueEditorTest)
#include "InlineValueEditorTest.moc"
