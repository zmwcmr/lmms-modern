#include <QVBoxLayout>
#include <QtTest>

#include "ComboBox.h"
#include "Engine.h"
#include "FontHelper.h"
#include "GroupBox.h"
#include "LcdFloatSpinBox.h"
#include "LcdWidget.h"
#include "LedCheckBox.h"
#include "PixmapButton.h"

class CaptionWidgetsTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase() { lmms::Engine::init(true); }
	void cleanupTestCase() { lmms::Engine::destroy(); }

	void comboFitsLargerFontWithoutChangingExplicitGeometry()
	{
		using namespace lmms::gui;

		const auto originalFont = QApplication::font();
		auto font = originalFont;
		font.setPointSize(9);
		QApplication::setFont(font);

		QWidget host;
		QVBoxLayout layout(&host);
		ComboBox combo(&host);
		ComboBox fixedCombo(&host);
		const QString option = QStringLiteral("Gypsy tuning with quickly changing pitches");
		combo.model()->addItem(option);
		combo.setMinimumContentWidth(80);
		fixedCombo.setFixedSize(130, ComboBox::DEFAULT_HEIGHT);
		layout.addWidget(&combo);
		layout.addWidget(&fixedCombo);
		host.show();
		QCoreApplication::processEvents();
		const auto initialSize = combo.size();

		font.setPointSize(16);
		QApplication::setFont(font);
		QCoreApplication::processEvents();
		layout.activate();
		const QFontMetrics metrics(adjustedToPixelSize(combo.font(), DEFAULT_FONT_SIZE));
		QVERIFY(combo.height() > initialSize.height());
		QVERIFY(combo.width() > initialSize.width());
		QVERIFY(combo.height() >= metrics.height() + 4);
		QCOMPARE(combo.model()->currentText(), option);
		QCOMPARE(fixedCombo.size(), QSize(130, ComboBox::DEFAULT_HEIGHT));

		QApplication::setFont(originalFont);
	}

	void lcdCaptionsUseScaledAuxiliaryFont()
	{
		using namespace lmms::gui;
		const auto originalFont = QApplication::font();
		auto font = originalFont;
		const QString caption = QStringLiteral("Base note frequency");
		font.setPointSize(9);
		QApplication::setFont(font);

		LcdWidget integerLcd(3, nullptr);
		integerLcd.setLabel(caption);
		LcdFloatSpinBox fractionalLcd(4, 3);
		fractionalLcd.setLabel(caption);
		const auto initialIntegerSize = integerLcd.size();
		const auto initialFractionalSize = fractionalLcd.sizeHint();
		const auto digitHeight = integerLcd.cellHeight();
		const auto basePixelSize = integerLcd.labelFont().pixelSize();

		font.setPointSize(18);
		QApplication::setFont(font);
		QCoreApplication::processEvents();
		const auto captionSize = integerLcd.labelSize(caption);
		const auto pixelSize = integerLcd.labelFont().pixelSize();
		const auto integerSize = integerLcd.size();
		const auto fractionalSize = fractionalLcd.sizeHint();
		const auto scaledDigitHeight = integerLcd.cellHeight();
		QApplication::setFont(originalFont);

		QCOMPARE(basePixelSize, AUXILIARY_FONT_SIZE);
		QCOMPARE(pixelSize, 2 * AUXILIARY_FONT_SIZE);
		QCOMPARE(scaledDigitHeight, digitHeight);
		QVERIFY(integerSize.height() > initialIntegerSize.height());
		QVERIFY(fractionalSize.height() > initialFractionalSize.height());
		QVERIFY(integerSize.width() >= captionSize.width());
		QVERIFY(fractionalSize.width() >= captionSize.width());
	}

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

	void namedButtonsExposeAccessibleNames()
	{
		using namespace lmms::gui;

		PixmapButton button(nullptr, QStringLiteral("Mute"));
		QCOMPARE(button.accessibleName(), QStringLiteral("Mute"));
	}
};

QTEST_MAIN(CaptionWidgetsTest)
#include "CaptionWidgetsTest.moc"
