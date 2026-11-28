#include <QtTest>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPixmapCache>
#include <QTemporaryDir>

#include "embed.h"

using namespace lmms;

namespace {

auto hasVisiblePixels(const QPixmap& pixmap) -> bool
{
	const auto image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
	for (int y = 0; y < image.height(); ++y)
	{
		const auto* line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
		for (int x = 0; x < image.width(); ++x)
		{
			if (qAlpha(line[x]) > 0) { return true; }
		}
	}
	return false;
}

auto centerColor(const QPixmap& pixmap) -> QColor
{
	const auto image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
	return image.pixelColor(image.width() / 2, image.height() / 2);
}

auto alphaImage(const QPixmap& pixmap) -> QImage
{
	return pixmap.toImage().convertToFormat(QImage::Format_Alpha8);
}

} // namespace

class IconTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase()
	{
		m_themeDir = QDir::cleanPath(
			QCoreApplication::applicationDirPath() + QStringLiteral("/../../data/themes/default"));
		QDir::setSearchPaths(QStringLiteral("artwork"), {m_themeDir});
		qApp->setProperty("_lmms_iconColor", QColor(QStringLiteral("#d1d8e4")));
		QPixmapCache::clear();
	}

	void sizeTokens()
	{
		QCOMPARE(icon::Small, 16);
		QCOMPARE(icon::Medium, 24);
		QCOMPARE(icon::Large, 32);
		QCOMPARE(icon::Tab, 48);
	}

	void foundationIconsRender()
	{
		const QStringList names{
			QStringLiteral("play"), QStringLiteral("pause"), QStringLiteral("stop"),
			QStringLiteral("record"), QStringLiteral("edit_undo"), QStringLiteral("edit_redo"),
			QStringLiteral("add"), QStringLiteral("close"), QStringLiteral("project_save"),
			QStringLiteral("project_open"), QStringLiteral("folder"), QStringLiteral("project_file"),
			QStringLiteral("sample_file"), QStringLiteral("zoom"), QStringLiteral("metronome"),
			QStringLiteral("gear"), QStringLiteral("speaker"), QStringLiteral("headphones"),
			QStringLiteral("speaker_slash"),
			QStringLiteral("instrument_track"), QStringLiteral("sample_track"),
			QStringLiteral("pattern_track"), QStringLiteral("automation_track"),
			QStringLiteral("master_volume"), QStringLiteral("reload"), QStringLiteral("clock"),
			QStringLiteral("tempo_sync"), QStringLiteral("edit_draw"), QStringLiteral("edit_erase"),
			QStringLiteral("edit_select"), QStringLiteral("edit_cut"), QStringLiteral("edit_copy"),
			QStringLiteral("edit_paste"), QStringLiteral("quantize"), QStringLiteral("project_new"),
			QStringLiteral("project_export"), QStringLiteral("project_import"),
			QStringLiteral("star"), QStringLiteral("home"), QStringLiteral("plugins"),
			QStringLiteral("help"),
			QStringLiteral("file"), QStringLiteral("midi_file"), QStringLiteral("preset_file"),
			QStringLiteral("soundfont_file"), QStringLiteral("vst_plugin_file"),
			QStringLiteral("project_open_recent"), QStringLiteral("folder_locked"),
			QStringLiteral("setup_general"), QStringLiteral("setup_audio"),
			QStringLiteral("setup_midi"), QStringLiteral("setup_directories"),
			QStringLiteral("setup_performance"), QStringLiteral("plugin_tab"),
			QStringLiteral("env_lfo_tab"), QStringLiteral("func_tab"),
			QStringLiteral("fx_tab"), QStringLiteral("midi_tab"), QStringLiteral("tuning_tab"),
			QStringLiteral("mixer"), QStringLiteral("new_channel"), QStringLiteral("muted"),
			QStringLiteral("controller"), QStringLiteral("info"), QStringLiteral("error"),
			QStringLiteral("edit_knife"), QStringLiteral("edit_merge"), QStringLiteral("edit_move"),
			QStringLiteral("edit_rename"), QStringLiteral("edit_unlink"), QStringLiteral("edit_tangent"),
			QStringLiteral("edit_draw_small"), QStringLiteral("flip_x"), QStringLiteral("flip_y"),
			QStringLiteral("gridmode"), QStringLiteral("proportional_snap"), QStringLiteral("cut_overlaps"),
			QStringLiteral("arp_up"), QStringLiteral("arp_down"), QStringLiteral("arp_up_and_down"),
			QStringLiteral("arp_free"), QStringLiteral("arp_random"), QStringLiteral("arp_sort"),
			QStringLiteral("arp_sync"), QStringLiteral("cancel"), QStringLiteral("discard"),
			QStringLiteral("restore"), QStringLiteral("maximize"), QStringLiteral("detach"),
		};

		for (const auto& name : names)
		{
			for (const int size : {icon::Small, icon::Medium, icon::Tab})
			{
				const auto pixmap = embed::getIconPixmap(name.toUtf8().constData(), size, size);
				QVERIFY2(!pixmap.isNull(), qPrintable(name));
				QCOMPARE(embed::logicalSize(pixmap), QSize(size, size));
				QVERIFY2(hasVisiblePixels(pixmap), qPrintable(name));
			}
		}
	}

	void currentColorFollowsTint()
	{
		QTemporaryDir directory;
		QVERIFY(directory.isValid());
		const auto path = QDir(directory.path()).filePath(QStringLiteral("tint"));
		QFile file(path + QStringLiteral(".svg"));
		QVERIFY(file.open(QIODevice::WriteOnly));
		QVERIFY(file.write(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">)"
		                   R"(<rect width="24" height="24" fill="currentColor"/></svg>)") > 0);
		file.close();

		QPixmapCache::clear();
		const auto red = embed::getIconPixmap(path.toStdString(), 24, 24, QColor(QStringLiteral("#ff0000")));
		const auto green = embed::getIconPixmap(path.toStdString(), 24, 24, QColor(QStringLiteral("#00ff00")));

		QCOMPARE(centerColor(red), QColor(QStringLiteral("#ff0000")));
		QCOMPARE(centerColor(green), QColor(QStringLiteral("#00ff00")));
		QVERIFY(alphaImage(red) == alphaImage(green));
	}

	void currentColorUsesThemeInk()
	{
		QTemporaryDir directory;
		QVERIFY(directory.isValid());
		const auto path = QDir(directory.path()).filePath(QStringLiteral("ink"));
		QFile file(path + QStringLiteral(".svg"));
		QVERIFY(file.open(QIODevice::WriteOnly));
		QVERIFY(file.write(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">)"
		                   R"(<rect width="24" height="24" fill="currentColor"/></svg>)") > 0);
		file.close();

		qApp->setProperty("_lmms_iconColor", QColor(QStringLiteral("#123456")));
		QPixmapCache::clear();
		const auto pixmap = embed::getIconPixmap(path.toStdString(), 24, 24);
		QCOMPARE(centerColor(pixmap), QColor(QStringLiteral("#123456")));

		qApp->setProperty("_lmms_iconColor", QColor(QStringLiteral("#d1d8e4")));
		QPixmapCache::clear();
	}

	void allDefaultIconsRender()
	{
		const QDir directory(m_themeDir);
		const auto files = directory.entryList({QStringLiteral("*.svg")}, QDir::Files);
		QVERIFY(!files.isEmpty());

		for (const auto& file : files)
		{
			const auto name = directory.filePath(QFileInfo(file).completeBaseName());
			const auto pixmap = embed::getIconPixmap(name.toStdString(), icon::Medium, icon::Medium);
			QVERIFY2(!pixmap.isNull(), qPrintable(file));
			QVERIFY2(hasVisiblePixels(pixmap), qPrintable(file));
		}
	}

private:
	QString m_themeDir;
};

QTEST_MAIN(IconTest)
#include "IconTest.moc"
