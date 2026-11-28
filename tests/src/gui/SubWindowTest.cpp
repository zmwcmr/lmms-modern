#include <QApplication>
#include <QLabel>
#include <QMdiArea>
#include <QProxyStyle>
#include <QVariantAnimation>
#include <QtTest>

#include "ConfigManager.h"
#include "Engine.h"
#include "LmmsStyle.h"
#include "SubWindow.h"

class SubWindowTestStyle : public QProxyStyle
{
public:
	explicit SubWindowTestStyle(int duration) : m_duration(duration) {}

	int styleHint(StyleHint hint, const QStyleOption* option, const QWidget* widget,
		QStyleHintReturn* result) const override
	{
		return hint == SH_Widget_Animation_Duration
			? m_duration : QProxyStyle::styleHint(hint, option, widget, result);
	}

private:
	int m_duration;
};

class SubWindowTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase()
	{
		lmms::Engine::init(true);
		QApplication::setStyle(new SubWindowTestStyle(100));
		lmms::ConfigManager::inst()->setValue("ui", "animations", "1");
	}

	void cleanupTestCase() { lmms::Engine::destroy(); }

	void animatesActivation()
	{
		QMdiArea area;
		auto* window = new lmms::gui::SubWindow(&area);
		window->setWidget(new QWidget);
		area.addSubWindow(window);
		window->show();
		area.show();
		QCoreApplication::processEvents();

		area.setActiveSubWindow(window);
		QCoreApplication::processEvents();
		auto* animation = window->findChild<QVariantAnimation*>();
		QVERIFY(animation);
		QCOMPARE(animation->state(), QAbstractAnimation::Running);
		QTRY_VERIFY(!window->findChild<QVariantAnimation*>());
	}

	void reservesTitleSpaceAtLargeFontSize()
	{
		const auto originalFont = QApplication::font();
		auto largeFont = originalFont;
		largeFont.setPointSize(18);
		QApplication::setFont(largeFont);
		QApplication::setStyle(new lmms::gui::LmmsStyle);

		QMdiArea area;
		auto* window = new lmms::gui::SubWindow(&area);
		auto* content = new QWidget;
		content->setWindowTitle("Song Editor");
		window->setWidget(content);
		area.addSubWindow(window);
		area.show();
		window->show();
		QCoreApplication::processEvents();

		const auto* title = window->findChild<QLabel*>(QString(), Qt::FindDirectChildrenOnly);
		const int titleBottom = title ? title->geometry().bottom() : -1;
		const int contentTop = content->y();
		const int titleBarHeight = window->titleBarHeight();

		qApp->setStyleSheet({});
		QApplication::setStyle(new SubWindowTestStyle(100));
		QApplication::setFont(originalFont);

		QVERIFY(titleBarHeight > 24);
		QVERIFY(titleBottom >= 0);
		QVERIFY(titleBottom < contentTop);
		QVERIFY(contentTop >= titleBarHeight);
	}

	void respectsDisabledAnimations()
	{
		QApplication::setStyle(new SubWindowTestStyle(0));
		QMdiArea area;
		auto* window = new lmms::gui::SubWindow(&area);
		window->setWidget(new QWidget);
		area.addSubWindow(window);
		window->show();
		area.show();
		QCoreApplication::processEvents();

		area.setActiveSubWindow(window);
		QCoreApplication::processEvents();
		QVERIFY(!window->findChild<QVariantAnimation*>());
		QApplication::setStyle(new SubWindowTestStyle(100));
	}
};

QTEST_MAIN(SubWindowTest)
#include "SubWindowTest.moc"
