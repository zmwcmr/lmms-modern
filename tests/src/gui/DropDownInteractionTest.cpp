#include <QMenu>
#include <QtTest>

#include "DropDownInteraction.h"

class DropDownInteractionTest : public QObject
{
	Q_OBJECT

private slots:
	void opensAndSelectsOnSeparateClicks()
	{
		lmms::gui::DropDownInteraction filter;
		qApp->installEventFilter(&filter);
		QComboBox combo;
		combo.addItems({"First", "Second", "Third"});
		combo.resize(180, 30);
		combo.show();
		QTest::mousePress(&combo, Qt::LeftButton);
		QVERIFY(!combo.view()->isVisible());
		QTest::mouseRelease(&combo, Qt::LeftButton);
		QTRY_VERIFY(combo.view()->isVisible());
		QCOMPARE(combo.currentIndex(), 0);
		const auto second = combo.view()->visualRect(combo.model()->index(1, 0)).center();
		QVERIFY(combo.view()->viewport()->rect().contains(second));
		QMouseEvent strayRelease(QEvent::MouseButtonRelease, QPointF(second),
			QPointF(combo.view()->viewport()->mapToGlobal(second)), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
		QCoreApplication::sendEvent(combo.view()->viewport(), &strayRelease);
		QCOMPARE(combo.currentIndex(), 0);
		QTest::mouseClick(combo.view()->viewport(), Qt::LeftButton, Qt::NoModifier, second);
		QTRY_COMPARE(combo.currentIndex(), 1);
	}

	void wheelDoesNotChangeClosedCombo()
	{
		lmms::gui::DropDownInteraction filter;
		qApp->installEventFilter(&filter);
		QComboBox combo;
		combo.addItems({"First", "Second"});
		combo.show();
		combo.setFocus();
		QWheelEvent wheel(QPointF(10, 10), QPointF(10, 10), QPoint{}, QPoint(0, -120),
			Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
		QCoreApplication::sendEvent(&combo, &wheel);
		QVERIFY(!wheel.isAccepted());
		QCOMPARE(combo.currentIndex(), 0);
		QTest::keyClick(&combo, Qt::Key_Down);
		QCOMPARE(combo.currentIndex(), 1);
	}

	void instantMenuWaitsForRelease()
	{
		lmms::gui::DropDownInteraction filter;
		qApp->installEventFilter(&filter);
		QToolButton button;
		QMenu menu(&button);
		menu.addAction("First");
		button.setMenu(&menu);
		button.setPopupMode(QToolButton::InstantPopup);
		button.show();
		QSignalSpy opened(&menu, &QMenu::aboutToShow);
		connect(&menu, &QMenu::aboutToShow, &menu, [&menu] { QTimer::singleShot(0, &menu, &QMenu::hide); });
		QTest::mousePress(&button, Qt::LeftButton);
		QCOMPARE(opened.count(), 0);
		QTest::mouseRelease(&button, Qt::LeftButton);
		QTRY_COMPARE(opened.count(), 1);
	}

	void splitButtonKeepsItsPrimaryAction()
	{
		lmms::gui::DropDownInteraction filter;
		qApp->installEventFilter(&filter);
		QToolButton button;
		button.resize(120, 36);
		QMenu menu(&button);
		menu.addAction("First");
		button.setMenu(&menu);
		button.setPopupMode(QToolButton::MenuButtonPopup);
		button.show();
		QSignalSpy opened(&menu, &QMenu::aboutToShow);
		QSignalSpy clicked(&button, &QToolButton::clicked);
		connect(&menu, &QMenu::aboutToShow, &menu, [&menu] { QTimer::singleShot(0, &menu, &QMenu::hide); });
		QTest::mouseClick(&button, Qt::LeftButton, Qt::NoModifier, QPoint(10, 18));
		QCOMPARE(clicked.count(), 1);
		QCOMPARE(opened.count(), 0);
		const QPoint arrow(button.width() - 4, 18);
		QTest::mousePress(&button, Qt::LeftButton, Qt::NoModifier, arrow);
		QCOMPARE(opened.count(), 0);
		QTest::mouseRelease(&button, Qt::LeftButton, Qt::NoModifier, arrow);
		QTRY_COMPARE(opened.count(), 1);
		QCOMPARE(clicked.count(), 1);
	}
};

QTEST_MAIN(DropDownInteractionTest)
#include "DropDownInteractionTest.moc"
