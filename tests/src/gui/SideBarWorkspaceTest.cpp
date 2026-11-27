#include <QEnterEvent>
#include <QMdiArea>
#include <QPushButton>
#include <QToolButton>
#include <QtTest>

#include "ConfigManager.h"
#include "SideBar.h"
#include "SideBarWidget.h"

class TrackingWorkspace : public QMdiArea
{
public:
	int resizeCount = 0;
	int paintCount = 0;

protected:
	void resizeEvent(QResizeEvent* event) override
	{
		++resizeCount;
		QMdiArea::resizeEvent(event);
	}

	void paintEvent(QPaintEvent* event) override
	{
		++paintCount;
		QMdiArea::paintEvent(event);
	}
};

class SideBarWorkspaceTest : public QObject
{
	Q_OBJECT

private slots:
	void switchingTabsKeepsWorkspaceGeometry_data()
	{
		QTest::addColumn<bool>("right");
		QTest::addColumn<int>("closeDelay");
		QTest::newRow("left-immediate") << false << 0;
		QTest::newRow("left-settled") << false << 300;
		QTest::newRow("right-immediate") << true << 0;
		QTest::newRow("right-settled") << true << 300;
	}

	void switchingTabsKeepsWorkspaceGeometry()
	{
		using namespace lmms::gui;
		QFETCH(bool, right);
		QFETCH(int, closeDelay);
		QWidget window;
		auto* layout = new QHBoxLayout(&window);
		auto* sidebar = new SideBar(Qt::Vertical, &window);
		auto* splitter = new SideBarSplitter(Qt::Horizontal, &window);
		splitter->setChildrenCollapsible(false);
		layout->addWidget(sidebar);
		layout->addWidget(splitter);
		sidebar->appendTab(new SideBarWidget("First", {}, splitter));
		auto* secondPage = new SideBarWidget("Second", {}, splitter);
		sidebar->appendTab(secondPage);
		auto* workspace = new TrackingWorkspace;
		auto* workspacePanel = new SideBarWorkspace(splitter, workspace);
		splitter->setWorkspace(workspacePanel);
		if (right)
		{
			splitter->insertWidget(0, workspacePanel);
			layout->insertWidget(0, splitter);
		}
		window.resize(800, 480);
		window.show();

		const auto tabs = sidebar->findChildren<QToolButton*>();
		const auto findTab = [&tabs](const QString& title) -> QToolButton* {
			for (auto* button : tabs) { if (button->toolTip() == title) { return button; } }
			return nullptr;
		};
		auto* first = findTab("First");
		auto* second = findTab("Second");
		QVERIFY(first && second);
		const auto closedWidth = workspace->width();
		workspacePanel->preparePreview();
		first->click();
		QTRY_VERIFY(workspace->updatesEnabled() && workspace->width() < closedWidth);

		splitter->setSizes(right ? QList<int>{480, 260} : QList<int>{260, 480});
		const auto openGeometry = workspacePanel->geometry();
		workspace->resizeCount = 0;
		second->click();
		QCoreApplication::processEvents();
		QCOMPARE(workspacePanel->geometry(), openGeometry);
		QCOMPARE(workspace->resizeCount, 0);
		QVERIFY(secondPage->isVisible());

		if (closeDelay > 0) { QTest::qWait(closeDelay); }
		workspacePanel->discardPreparedPreview();
		const auto paintsBeforeHover = workspace->paintCount;
		auto* closeControl = closeDelay > 0 ? static_cast<QAbstractButton*>(secondPage->closeButton()) : second;
		auto* hoverArea = closeDelay > 0 ? secondPage->parentWidget() : sidebar;
		hoverArea->setAttribute(Qt::WA_UnderMouse);
		QEnterEvent enter(QPointF(1, 1), QPointF(1, 1), QPointF(1, 1));
		QCoreApplication::sendEvent(closeControl, &enter);
		QTRY_VERIFY(workspace->paintCount > paintsBeforeHover);
		// The hover timer prepares a preview, but a repaint can discard it
		// again on slower platforms. Prepare it synchronously right before
		// closing so this assertion tests the close animation, not preview
		// timing.
		workspacePanel->preparePreview();
		QTest::mouseClick(closeControl, Qt::LeftButton);
		QVERIFY(!workspace->updatesEnabled());
		QTRY_COMPARE(workspace->width(), closedWidth);
		QVERIFY(workspace->updatesEnabled());
		QCOMPARE(workspace->resizeCount, 1);

		auto* config = lmms::ConfigManager::inst();
		const auto previous = config->value("ui", "animations", "1");
		config->setValue("ui", "animations", "0");
		workspacePanel->preparePreview();
		first->click();
		const bool openedImmediately = workspace->updatesEnabled() && workspace->width() < closedWidth;
		first->click();
		config->setValue("ui", "animations", previous);
		QVERIFY(openedImmediately);
		QCOMPARE(workspace->width(), closedWidth);
	}

	void freezesLayoutUntilSlideFinishes()
	{
		QSplitter splitter;
		splitter.resize(520, 360);
		auto* workspace = new QMdiArea;
		auto* panel = new lmms::gui::SideBarWorkspace(&splitter, workspace);
		splitter.show();
		QCoreApplication::processEvents();
		const auto initialSize = panel->size();
		QCOMPARE(workspace->size(), initialSize);

		panel->preparePreview();
		panel->freeze();
		QVERIFY(!workspace->updatesEnabled());
		panel->resize(initialSize.width() - 120, initialSize.height());
		QCOMPARE(workspace->size(), initialSize);

		panel->thaw();
		QVERIFY(workspace->updatesEnabled());
		QCOMPARE(workspace->size(), panel->size());
	}

	void skipsUnpreparedAndStalePreviews()
	{
		QSplitter splitter;
		splitter.resize(520, 360);
		auto* workspace = new TrackingWorkspace;
		auto* panel = new lmms::gui::SideBarWorkspace(&splitter, workspace);
		splitter.show();
		QCoreApplication::processEvents();

		panel->freeze();
		QVERIFY(workspace->updatesEnabled());
		panel->thaw();
		panel->preparePreview();
		const int paintsSinceCapture = workspace->paintCount;
		workspace->viewport()->update();
		QTRY_VERIFY(workspace->paintCount > paintsSinceCapture);
		panel->freeze();
		QVERIFY(workspace->updatesEnabled());
	}

	void clipsLiveWorkspaceWithoutResizingItPerFrame()
	{
		QSplitter splitter;
		splitter.resize(520, 360);
		auto* workspace = new TrackingWorkspace;
		auto* panel = new lmms::gui::SideBarWorkspace(&splitter, workspace);
		splitter.show();
		QCoreApplication::processEvents();
		const auto initialSize = workspace->size();
		workspace->resizeCount = 0;

		panel->freeze();
		panel->resize(initialSize.width() - 40, initialSize.height());
		panel->resize(initialSize.width() - 120, initialSize.height());
		QVERIFY(workspace->updatesEnabled());
		QCOMPARE(workspace->size(), initialSize);
		QCOMPARE(workspace->resizeCount, 0);
		panel->thaw();
		QCOMPARE(workspace->size(), panel->size());
		QCOMPARE(workspace->resizeCount, 1);
	}
};

QTEST_MAIN(SideBarWorkspaceTest)
#include "SideBarWorkspaceTest.moc"
