#include <QApplication>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>
#include <QtTest>

#include "EffectSelectDialog.h"
#include "Engine.h"

using namespace lmms::gui;

class EffectSelectDialogTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase()
	{
		lmms::Engine::init(true);
	}

	void cleanupTestCase()
	{
		lmms::Engine::destroy();
	}

	void keyboardTabOrder()
	{
		EffectSelectDialog dialog(nullptr);
		QVERIFY(QTest::qWaitForWindowExposed(&dialog));
		QApplication::setActiveWindow(&dialog);

		auto* filter = dialog.findChild<QLineEdit*>();
		auto* list = dialog.findChild<QTableView*>();
		QVERIFY(filter);
		QVERIFY(list);
		QVERIFY(list->focusPolicy() & Qt::TabFocus);

		QPushButton* allButton = nullptr;
		for (auto* button : dialog.findChildren<QPushButton*>())
		{
			if (button->text() == QLatin1String("All")) { allButton = button; }
		}
		QVERIFY(allButton);
		QVERIFY(allButton->focusPolicy() & Qt::TabFocus);

		filter->setFocus();
		QCOMPARE(QApplication::focusWidget(), filter);

		QTest::keyClick(filter, Qt::Key_Tab);
		QCOMPARE(QApplication::focusWidget(), list);

		QTest::keyClick(list, Qt::Key_Tab);
		QCOMPARE(QApplication::focusWidget(), allButton);

		QTest::keyClick(allButton, Qt::Key_Backtab);
		QCOMPARE(QApplication::focusWidget(), list);

		QTest::keyClick(list, Qt::Key_Backtab);
		QCOMPARE(QApplication::focusWidget(), filter);
	}

	void listHasAccessibleName()
	{
		EffectSelectDialog dialog(nullptr);
		auto* list = dialog.findChild<QTableView*>();
		QVERIFY(list);
		QVERIFY(!list->accessibleName().isEmpty());
		QVERIFY(!list->accessibleDescription().isEmpty());
	}
};

QTEST_MAIN(EffectSelectDialogTest)
#include "EffectSelectDialogTest.moc"
