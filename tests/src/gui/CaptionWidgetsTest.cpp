#include <QVBoxLayout>
#include <QtTest>

#include "Engine.h"
#include "FontHelper.h"
#include "GroupBox.h"
#include "LedCheckBox.h"

class CaptionWidgetsTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase() { lmms::Engine::init(true); }
	void cleanupTestCase() { lmms::Engine::destroy(); }

	void fitsCaptionsWhenApplicationFontChanges()
	{
		using namespace lmms::gui;

		const auto originalFont = QApplication::font();
		auto appFont = originalFont;
		appFont.setPointSize(9);
		QApplication::setFont(appFont);

		const QString title = QStringLiteral("ENABLE MIDI OUTPUT");
		GroupBox group(title);
		auto* layout = new QVBoxLayout(&group);
		const int spacing = 7;
		layout->setContentsMargins(8, group.titleBarHeight() + spacing, 8, 8);
		auto* legacy = new LedCheckBox(QStringLiteral("Discard MIDI connections"), &group);
		auto* modern = new LedCheckBox(QStringLiteral("Auto Detect"), &group, {}, LedCheckBox::LedColor::Yellow, false);
		auto* icon = new LedCheckBox(&group);
		layout->addWidget(legacy);
		layout->addWidget(modern);
		layout->addWidget(icon);
		const auto initialTitleHeight = group.titleBarHeight();
		const auto initialLegacySize = legacy->size();
		const auto initialModernSize = modern->size();
		const auto iconSize = icon->size();

		appFont.setPointSize(16);
		QApplication::setFont(appFont);
		QCoreApplication::processEvents();
		const QFontMetrics titleMetrics(adjustedToPixelSize(group.font(), DEFAULT_FONT_SIZE));
		QVERIFY(group.titleBarHeight() > initialTitleHeight);
		QVERIFY(group.titleBarHeight() >= titleMetrics.height());
		QVERIFY(group.minimumSizeHint().width() >= titleMetrics.horizontalAdvance(title) + group.ledButton()->width());
		QVERIFY(group.minimumSizeHint().width() >= legacy->width() + 16);
		QCOMPARE(layout->contentsMargins().top(), group.titleBarHeight() + spacing);
		QVERIFY(legacy->height() > initialLegacySize.height());
		QVERIFY(legacy->width() > initialLegacySize.width());
		QVERIFY(modern->height() > initialModernSize.height());
		QVERIFY(modern->width() > initialModernSize.width());
		QCOMPARE(icon->size(), iconSize);

		appFont.setPointSize(9);
		QApplication::setFont(appFont);
		QCoreApplication::processEvents();
		QCOMPARE(group.titleBarHeight(), initialTitleHeight);
		QCOMPARE(layout->contentsMargins().top(), group.titleBarHeight() + spacing);
		QCOMPARE(legacy->size(), initialLegacySize);
		QCOMPARE(modern->size(), initialModernSize);
		QApplication::setFont(originalFont);
	}
};

QTEST_MAIN(CaptionWidgetsTest)
#include "CaptionWidgetsTest.moc"
