#include <QtTest>
#include <QDir>
#include <QLabel>
#include <QPixmapCache>

#include "AboutDialog.h"
#include "FontHelper.h"
#include "LmmsStyle.h"
#include "LmmsPalette.h"

using namespace lmms::gui;

class FontScaleTest : public QObject
{
	Q_OBJECT

private slots:
	void rolesFollowApplicationFont()
	{
		const auto originalFont = QApplication::font();

		auto font = originalFont;
		font.setPointSizeF(9.0);
		QApplication::setFont(font);

		QCOMPARE(scaledFontSize(BODY_FONT_SIZE), BODY_FONT_SIZE);
		QCOMPARE(scaledFontSize(AUXILIARY_FONT_SIZE), AUXILIARY_FONT_SIZE);
		QCOMPARE(scaledFontSize(TITLE_FONT_SIZE), TITLE_FONT_SIZE);

		const auto title = titleFont(font);
		QCOMPARE(title.pixelSize(), TITLE_FONT_SIZE);
		QCOMPARE(title.weight(), int(QFont::DemiBold));

		const auto body = bodyFont(font);
		QCOMPARE(body.pixelSize(), BODY_FONT_SIZE);

		const auto auxiliary = auxiliaryFont(font);
		QCOMPARE(auxiliary.pixelSize(), AUXILIARY_FONT_SIZE);

		const auto group = groupTitleFont(font);
		QCOMPARE(group.pixelSize(), BODY_FONT_SIZE);
		QCOMPARE(group.weight(), int(QFont::DemiBold));

		font.setPointSizeF(18.0);
		QApplication::setFont(font);

		QCOMPARE(scaledFontSize(BODY_FONT_SIZE), BODY_FONT_SIZE * 2);
		QCOMPARE(scaledFontSize(AUXILIARY_FONT_SIZE), AUXILIARY_FONT_SIZE * 2);
		QCOMPARE(scaledFontSize(TITLE_FONT_SIZE), TITLE_FONT_SIZE * 2);

		QApplication::setFont(originalFont);
	}

	void themeFontSizesScaleWithApplicationFont()
	{
		const auto originalFont = QApplication::font();
		const auto themeDir = QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../../data/themes/default"));
		QDir::setSearchPaths(QStringLiteral("resources"), {themeDir});

		auto font = originalFont;
		font.setPointSizeF(18.0);
		QApplication::setFont(font);

		LmmsStyle style;
		const auto css = qApp->styleSheet();
		QVERIFY2(css.contains(QStringLiteral("font-size: 20px")), "10px design tier was not scaled to 20px");
		QVERIFY2(css.contains(QStringLiteral("font-size: 26px")), "13px title tier was not scaled to 26px");
		QVERIFY2(css.contains(QStringLiteral("font-size: 16px")), "8px design tier was not scaled to 16px");
		LmmsPalette palette(nullptr, &style);
		QCOMPARE(palette.palette().color(QPalette::Link), QColor(QStringLiteral("#8cfab6")));
		const auto originalPalette = QApplication::palette();
		QApplication::setPalette(palette.palette());
		{
			AboutDialog dialog(nullptr);
			auto* link = dialog.findChild<QLabel*>(QStringLiteral("label_4"));
			auto* version = dialog.findChild<QLabel*>(QStringLiteral("versionLabel"));
			QVERIFY(link);
			QVERIFY(version);
			QVERIFY(link->text().contains(QStringLiteral("#8cfab6")));
			QVERIFY(version->wordWrap());
			dialog.resize(800, 600);
			dialog.show();
			QApplication::processEvents();
			const int rightEdge = version->mapTo(&dialog, QPoint(version->width(), 0)).x();
			QVERIFY(dialog.width() - rightEdge < 30);
			auto changedPalette = QApplication::palette();
			changedPalette.setColor(QPalette::Link, QColor(QStringLiteral("#80ff80")));
			QApplication::setPalette(changedPalette);
			QVERIFY(link->text().contains(QStringLiteral("#80ff80")));
		}
		QApplication::setPalette(originalPalette);

		QApplication::setFont(originalFont);
		QDir::setSearchPaths(QStringLiteral("resources"), {});
	}

	void captureAboutPreview()
	{
		if (!qEnvironmentVariableIsSet("LMMS_FONT_PREVIEW_DIR")) { QSKIP("Preview not requested"); }
		const auto originalFont = QApplication::font();
		const auto originalPalette = QApplication::palette();
		auto font = originalFont;
		font.setPointSize(11);
		QApplication::setFont(font);
		QDir::setSearchPaths(QStringLiteral("resources"), {QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../../data/themes/default"))});
		QDir::setSearchPaths(QStringLiteral("artwork"), QDir::searchPaths(QStringLiteral("resources")));
		QPixmapCache::clear();
		{
			LmmsStyle style;
			LmmsPalette palette(nullptr, &style);
			QApplication::setPalette(palette.palette());
			AboutDialog dialog(nullptr);
			dialog.resize(760, 460);
			dialog.show();
			QApplication::processEvents();
			QVERIFY(dialog.grab().save(qEnvironmentVariable("LMMS_FONT_PREVIEW_DIR") + "/about.png"));
		}
		QApplication::setPalette(originalPalette);
		QApplication::setFont(originalFont);
		QDir::setSearchPaths(QStringLiteral("resources"), {});
	}
};

QTEST_MAIN(FontScaleTest)
#include "FontScaleTest.moc"
