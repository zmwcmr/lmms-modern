#include <QApplication>
#include <QCheckBox>
#include <QFile>
#include <QLineEdit>
#include <QProxyStyle>
#include <QPixmapCache>
#include <QTemporaryDir>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <QtTest>

#include "ConfigManager.h"
#include "Engine.h"
#include "FileBrowser.h"
#include "LmmsStyle.h"
#include "LmmsPalette.h"
#include "embed.h"

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

	void captureBrowserPreview()
	{
		if (!qEnvironmentVariableIsSet("LMMS_FONT_PREVIEW_DIR")) { QSKIP("Preview not requested"); }
		const auto originalFont = QApplication::font();
		const auto originalPalette = QApplication::palette();
		auto font = originalFont;
		font.setPointSize(11);
		QApplication::setFont(font);
		QTemporaryDir directory;
		QVERIFY(directory.isValid());
		QDir root(directory.path());
		QVERIFY(root.mkdir(QStringLiteral("demos")));
		QVERIFY(root.mkdir(QStringLiteral("projects")));
		QDir::setSearchPaths(QStringLiteral("resources"), {QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../../data/themes/default"))});
		QDir::setSearchPaths(QStringLiteral("artwork"), QDir::searchPaths(QStringLiteral("resources")));
		QPixmapCache::clear();
		{
			lmms::gui::LmmsStyle style;
			lmms::gui::LmmsPalette palette(nullptr, &style);
			QApplication::setPalette(palette.palette());
			lmms::gui::FileBrowser browser(lmms::gui::FileBrowser::Type::Normal, directory.path(),
				"*.mmp", "My projects", lmms::embed::getIconPixmap("project_file").transformed(QTransform().rotate(90)), nullptr, false, directory.path(), directory.path());
			browser.resize(350, 520);
			browser.show();
			QApplication::processEvents();
			QVERIFY(browser.grab().save(qEnvironmentVariable("LMMS_FONT_PREVIEW_DIR") + "/browser.png"));
		}
		QApplication::setPalette(originalPalette);
		QApplication::setFont(originalFont);
	}

	void cleanup()
	{
		auto* config = lmms::ConfigManager::inst();
		config->deleteValue("shortcuts", "browser_search");
		config->deleteValue("shortcuts", "browser_refresh");
	}

	void contentFiltersFitAtLargeFontSize()
	{
		const auto originalFont = QApplication::font();
		auto font = originalFont;
		font.setPointSize(18);
		QApplication::setFont(font);
		QTemporaryDir directory;
		QVERIFY(directory.isValid());
		{
			lmms::gui::FileBrowser browser(lmms::gui::FileBrowser::Type::Normal,
				directory.path(), "*.wav", "Browser", QPixmap{}, nullptr, false,
				directory.path(), directory.path());
			browser.resize(300, 500);
			browser.show();
			QCoreApplication::processEvents();
			const auto boxes = browser.findChildren<QCheckBox*>();
			QCOMPARE(boxes.size(), 3);
			for (auto* box : boxes)
			{
				QVERIFY(box->width() >= box->sizeHint().width());
				QVERIFY(box->height() >= box->sizeHint().height());
			}
			QVERIFY(boxes[0]->geometry().bottom() < boxes[1]->geometry().top());
		}
		QApplication::setFont(originalFont);
	}

	void dispatchesBrowserShortcuts()
	{
		auto* config = lmms::ConfigManager::inst();
		config->setValue("shortcuts", "browser_search", "F6");
		config->setValue("shortcuts", "browser_refresh", "F7");
		QTemporaryDir directory;
		QVERIFY(directory.isValid());

		QWidget host;
		QVBoxLayout layout(&host);
		QLineEdit outside(&host);
		lmms::gui::FileBrowser browser(lmms::gui::FileBrowser::Type::Normal, directory.path(), "*.wav",
			"Test Browser", QPixmap{}, &host);
		layout.addWidget(&outside);
		layout.addWidget(&browser);
		host.show();
		QVERIFY(QTest::qWaitForWindowExposed(&host));
		QApplication::setActiveWindow(&host);

		auto* filter = browser.findChild<QLineEdit*>();
		QVERIFY(filter);
		outside.setFocus();
		QTest::keyClick(&outside, Qt::Key_F6);
		QCOMPARE(QApplication::focusWidget(), &outside);

		lmms::gui::FileBrowserTreeWidget* tree = nullptr;
		for (auto* candidate : browser.findChildren<lmms::gui::FileBrowserTreeWidget*>())
		{
			if (candidate->isVisible()) { tree = candidate; }
		}
		QVERIFY(tree);
		tree->setFocus();
		QTest::keyClick(tree, Qt::Key_F6);
		QCOMPARE(QApplication::focusWidget(), filter);
		QTest::keyClicks(filter, "a");
		QCOMPARE(filter->text(), QStringLiteral("a"));
		QTest::keyClick(filter, Qt::Key_Backspace);

		QFile sample(directory.filePath("new.wav"));
		QVERIFY(sample.open(QIODevice::WriteOnly));
		sample.close();
		tree->setFocus();
		QTest::keyClick(tree, Qt::Key_F5);
		QCOMPARE(tree->topLevelItemCount(), 0);
		QTest::keyClick(tree, Qt::Key_F7);
		QCOMPARE(tree->topLevelItemCount(), 1);
		QCOMPARE(tree->topLevelItem(0)->text(0), QStringLiteral("new.wav"));
	}

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
