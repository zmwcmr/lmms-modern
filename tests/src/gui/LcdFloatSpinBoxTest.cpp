#include <QLabel>
#include <QtTest>

#include "Engine.h"
#include "InlineValueEditor.h"
#include "LcdFloatSpinBox.h"

using lmms::FloatModel;
using lmms::gui::InlineValueEditor;
using lmms::gui::LcdFloatSpinBox;

class LcdFloatSpinBoxTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase() { lmms::Engine::init(true); }
	void cleanupTestCase() { lmms::Engine::destroy(); }

	void editsFloatValueInPlace()
	{
		FloatModel model{440.f, 100.f, 1000.f, 0.1f};
		LcdFloatSpinBox spin{4, 3, QStringLiteral("test")};
		spin.setModel(&model);
		spin.resize(240, 80);
		spin.show();
		QCoreApplication::processEvents();

		QTest::mouseDClick(&spin, Qt::LeftButton, Qt::NoModifier, spin.rect().center());

		auto* editor = spin.findChild<InlineValueEditor*>();
		QVERIFY(editor);
		QCOMPARE(editor->text(), QStringLiteral("440.000"));

		auto* hint = spin.findChild<QLabel*>(QStringLiteral("inlineValueEditorHint"));
		QVERIFY(hint);
		QCOMPARE(hint->text(), InlineValueEditor::rangeHint(100.f, 1000.f));

		editor->setText(QStringLiteral("442.5"));
		QTest::keyClick(editor, Qt::Key_Return);
		QCOMPARE(model.value(), 442.5f);
		QTRY_VERIFY(!spin.findChild<InlineValueEditor*>());
	}
};

QTEST_MAIN(LcdFloatSpinBoxTest)
#include "LcdFloatSpinBoxTest.moc"
