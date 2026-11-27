#include <QEvent>
#include <QMenu>
#include <QProxyStyle>
#include <QToolBar>
#include <QtTest>

#include "ConfigManager.h"
#include "ToolButton.h"

class ToolButtonTestStyle : public QProxyStyle
{
public:
	explicit ToolButtonTestStyle(int duration) : m_duration(duration) {}

	int styleHint(StyleHint hint, const QStyleOption* option, const QWidget* widget,
		QStyleHintReturn* result) const override
	{
		return hint == SH_Widget_Animation_Duration
			? m_duration : QProxyStyle::styleHint(hint, option, widget, result);
	}

private:
	int m_duration;
};

class ToolButtonTest : public QObject
{
	Q_OBJECT

private slots:
	void toolbarActionKeepsMenuAndButtonBehavior()
	{
		QToolBar toolbar;
		QMenu menu;
		QPixmap pixmap(16, 16);
		pixmap.fill(Qt::red);
		QIcon icon(pixmap);
		lmms::gui::ToolButtonAction action(icon, "Draw", &toolbar);
		action.setCheckable(true);
		toolbar.addAction(&action);

		auto* button = qobject_cast<lmms::gui::ToolButton*>(toolbar.widgetForAction(&action));
		QVERIFY(button);
		QVERIFY(button->findChild<QVariantAnimation*>());
		QCOMPARE(button->defaultAction(), &action);
		toolbar.setIconSize(QSize(20, 20));
		toolbar.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		QCOMPARE(button->iconSize(), QSize(20, 20));
		QCOMPARE(button->toolButtonStyle(), Qt::ToolButtonTextBesideIcon);

		QSignalSpy triggered(&action, &QAction::triggered);
		QSignalSpy toolbarTriggered(&toolbar, &QToolBar::actionTriggered);
		button->click();
		QCOMPARE(triggered.count(), 1);
		QCOMPARE(toolbarTriggered.count(), 1);
		QVERIFY(action.isChecked());

		menu.addAction(&action);
		menu.show();
		QVERIFY(!menu.findChild<QToolButton*>());
		menu.setActiveAction(&action);
		QTest::keyClick(&menu, Qt::Key_Return);
		QCOMPARE(triggered.count(), 2);
		QVERIFY(!button->isChecked());
	}

	void hoverBlendsWithoutDelayingPress()
	{
		lmms::gui::ToolButton button(nullptr);
		auto* style = new ToolButtonTestStyle(100);
		style->setParent(&button);
		button.setStyle(style);
		button.setStyleSheet(
			"QToolButton { border: 0; background: #102030; }"
			"QToolButton:hover { background: #506070; }"
			"QToolButton:pressed { background: #8090a0; }");
		QPixmap icon(12, 12);
		icon.fill(Qt::red);
		button.setIcon(icon);
		button.resize(40, 40);
		button.setCheckable(true);
		button.show();
		QCoreApplication::processEvents();
		button.setHoverTransitionEnabled(true);
		auto* animation = button.findChild<QVariantAnimation*>();
		QVERIFY(animation);

		button.setAttribute(Qt::WA_UnderMouse, true);
		QEnterEvent enter(QPointF(20, 20), QPointF(20, 20), QPointF(button.mapToGlobal(QPoint(20, 20))));
		QCoreApplication::sendEvent(&button, &enter);
		QCOMPARE(animation->state(), QAbstractAnimation::Running);
		animation->setCurrentTime(35);
		const auto frame = button.grab().toImage();
		QVERIFY(frame.pixelColor(3, 3).red() > 16 && frame.pixelColor(3, 3).red() < 80);
		QCOMPARE(frame.pixelColor(frame.width() / 2, frame.height() / 2), QColor(Qt::red));

		QTest::mousePress(&button, Qt::LeftButton, Qt::NoModifier, QPoint(20, 20));
		QVERIFY(button.isDown());
		QCOMPARE(button.grab().toImage().pixelColor(3, 3), QColor("#8090a0"));
		QTest::mouseRelease(&button, Qt::LeftButton, Qt::NoModifier, QPoint(20, 20));
		QVERIFY(button.isChecked());

		const auto progress = animation->currentValue();
		button.setAttribute(Qt::WA_UnderMouse, false);
		QEvent leave(QEvent::Leave);
		QCoreApplication::sendEvent(&button, &leave);
		QCOMPARE(animation->startValue(), progress);
		QCOMPARE(animation->endValue().toReal(), 0.0);
	}

	void respectsDisabledAnimations_data()
	{
		QTest::addColumn<bool>("appEnabled");
		QTest::addColumn<int>("styleDuration");
		QTest::newRow("disabled-by-style") << true << 0;
		QTest::newRow("disabled-in-settings") << false << 100;
	}

	void respectsDisabledAnimations()
	{
		QFETCH(bool, appEnabled);
		QFETCH(int, styleDuration);
		lmms::gui::ToolButton button(nullptr);
		auto* style = new ToolButtonTestStyle(styleDuration);
		style->setParent(&button);
		button.setStyle(style);
		button.setHoverTransitionEnabled(true);
		QEnterEvent enter(QPointF(1, 1), QPointF(1, 1), QPointF(1, 1));
		auto* config = lmms::ConfigManager::inst();
		const auto previous = config->value("ui", "animations", "1");
		config->setValue("ui", "animations", QString::number(appEnabled));
		QCoreApplication::sendEvent(&button, &enter);
		config->setValue("ui", "animations", previous);
		auto* animation = button.findChild<QVariantAnimation*>();
		QVERIFY(animation);
		QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
	}
};

QTEST_MAIN(ToolButtonTest)
#include "ToolButtonTest.moc"
