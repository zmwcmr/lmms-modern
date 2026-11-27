#include <QApplication>
#include <QProxyStyle>
#include <QVariantAnimation>
#include <QtTest>

#include "ConfigManager.h"
#include "Engine.h"
#include "FileBrowser.h"

class FileBrowserTestStyle : public QProxyStyle
{
public:
	explicit FileBrowserTestStyle(int duration) : m_duration(duration) {}

	int styleHint(StyleHint hint, const QStyleOption* option, const QWidget* widget,
		QStyleHintReturn* result) const override
	{
		return hint == SH_Widget_Animation_Duration
			? m_duration : QProxyStyle::styleHint(hint, option, widget, result);
	}

private:
	int m_duration;
};

class FileBrowserTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase()
	{
		lmms::Engine::init(true);
		QApplication::setStyle(new FileBrowserTestStyle(100));
		lmms::ConfigManager::inst()->setValue("ui", "animations", "1");
	}

	void cleanupTestCase() { lmms::Engine::destroy(); }

	void animatesExpansion()
	{
		QWidget host;
		lmms::gui::FileBrowserTreeWidget tree(&host);
		tree.resize(300, 200);
		tree.show();

		auto* root = new QTreeWidgetItem(&tree);
		root->setText(0, QStringLiteral("folder"));
		root->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
		new QTreeWidgetItem(root);
		QCoreApplication::processEvents();

		tree.expandItem(root);
		auto* animation = tree.findChild<QVariantAnimation*>();
		QVERIFY(animation);
		QCOMPARE(animation->state(), QAbstractAnimation::Running);
		QTRY_VERIFY(!tree.findChild<QVariantAnimation*>());
	}

	void respectsDisabledAnimations()
	{
		QApplication::setStyle(new FileBrowserTestStyle(0));
		QWidget host;
		lmms::gui::FileBrowserTreeWidget tree(&host);
		tree.resize(300, 200);
		tree.show();

		auto* root = new QTreeWidgetItem(&tree);
		root->setText(0, QStringLiteral("folder"));
		root->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
		new QTreeWidgetItem(root);
		QCoreApplication::processEvents();

		tree.expandItem(root);
		QVERIFY(!tree.findChild<QVariantAnimation*>());
		QApplication::setStyle(new FileBrowserTestStyle(100));
	}
};

QTEST_MAIN(FileBrowserTest)
#include "FileBrowserTest.moc"
