/*
 * embed.cpp - misc stuff for using embedded resources (linked into binary)
 *
 * Copyright (c) 2004-2009 Tobias Doerffel <tobydox/at/users.sourceforge.net>
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 */

#include "embed.h"

#include <QApplication>
#include <QDir>
#include <QGuiApplication>
#include <QImageReader>
#include <QPainter>
#include <QPalette>
#include <QPixmapCache>
#include <QResource>
#include <QScreen>
#include <QSvgRenderer>

namespace lmms::embed {

namespace {

// Ink color for `currentColor` SVGs. QSS tokens win over the palette so a
// theme can keep its text color separate from its icon color.
auto defaultIconColor() -> QColor
{
	if (!qApp) { return QColor(0xd1, 0xd8, 0xe4); }
	const auto color = qApp->property("_lmms_iconColor").value<QColor>();
	return color.isValid() ? color : qApp->palette().color(QPalette::Text);
}

// QPixmapCache and HiDPI compatible SVG-->QPixmap wrapper
auto loadSvgPixmap(const QString& resourceName, int width, int height, const QColor& tint) -> QPixmap
{
	// QFile requires the file extension to be present, unlike QImageReader
	QString fileName = resourceName;
	if (!fileName.endsWith(".svg", Qt::CaseInsensitive)) { fileName += ".svg"; }

	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly))
	{
		qWarning() << "Failed to open resource for SVG: " << resourceName;
		return QPixmap{1, 1};
	}

	// Resolve `currentColor` before handing the SVG to Qt. This keeps the
	// source theme-neutral while letting the loader follow the active theme.
	const auto svgText = QString::fromUtf8(file.readAll());
	const auto ink = tint.isValid() ? tint : defaultIconColor();
	auto resolvedSvg = svgText;
	if (resolvedSvg.contains("currentColor")) {
		resolvedSvg.replace("currentColor", ink.name(QColor::HexRgb));
	}

	QSvgRenderer renderer(resolvedSvg.toUtf8());
	if (!renderer.isValid())
	{
		qWarning() << "Error loading SVG file: " << resourceName;
		return QPixmap{1, 1};
	}

	// Get the default size of the SVG (without scaling)
	QSize svgSize = renderer.defaultSize();

	// If width/height are provided, use them
	if (width > 0 && height > 0) { svgSize.scale(width, height, Qt::IgnoreAspectRatio); }

	// Scale the SVG. The oversampling factor is important because Qt often draws
	// the pixmap smaller than its intrinsic size (a QToolButton defaults to a
	// 16 px icon size while most SVG artwork is 20 px) and because the DPR
	// reported by the platform can be smaller than the compositor's actual
	// scale. Keeping a higher-resolution raster around lets Qt downscale
	// instead of upscaling, which keeps the icons crisp.
	constexpr qreal svgOversampling = 2.0;
	qreal devicePixelRatio = QGuiApplication::primaryScreen()->devicePixelRatio() * svgOversampling;
	svgSize *= devicePixelRatio;

	QImage image(svgSize, QImage::Format_ARGB32);
	image.fill(Qt::transparent);
	QPainter painter(&image);
	renderer.render(&painter);
	painter.end();

	if (tint.isValid())
	{
		QPainter tintPainter(&image);
		tintPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
		tintPainter.fillRect(image.rect(), tint);
	}

	auto pixmap = QPixmap::fromImage(image);
	pixmap.setDevicePixelRatio(devicePixelRatio);

	return pixmap;
}

auto loadPixmap(const QString& name, int width, int height, const char* const* xpm, const QColor& tint) -> QPixmap
{
	if (xpm) { return QPixmap{xpm}; }

	auto resourceName = name;
	if (!QDir::isAbsolutePath(name))
	{
		resourceName = "artwork:" + name;
		for (const auto& path : QDir::searchPaths("artwork"))
		{
			const auto candidate = QDir(path).filePath(name);
			if (QFile::exists(candidate + ".svg")) { return loadSvgPixmap(candidate, width, height, tint); }
			if (QImageReader(candidate).canRead())
			{
				resourceName = candidate;
				break;
			}
		}
	}
	else if (name.endsWith(".svg", Qt::CaseInsensitive))
	{
		return loadSvgPixmap(resourceName, width, height, tint);
	}
	else
	{
		const auto svgName = resourceName + ".svg";
		if (QFile::exists(svgName)) { return loadSvgPixmap(svgName, width, height, tint); }
	}

	auto reader = QImageReader{resourceName};

	if (reader.format().toLower() == "svg")
	{
		return loadSvgPixmap(resourceName, width, height, tint);
	}

	if (width > 0 && height > 0) { reader.setScaledSize(QSize{width, height}); }

	const auto pixmap = QPixmap::fromImageReader(&reader);
	if (pixmap.isNull()) {
		qWarning().nospace() << "Error loading icon pixmap " << name << ": " << reader.errorString();
		return QPixmap{1, 1};
	}
	return pixmap;
}

} // namespace

namespace {

auto getIconPixmapImpl(
	std::string_view name,
	int width,
	int height,
	const char* const* xpm,
	const QColor& tint
) -> QPixmap
{
	if (name.empty()) { return QPixmap{}; }

	const auto pixmapName = QString::fromUtf8(name.data(), name.size());
	auto cacheName = (width > 0 && height > 0)
		? QStringLiteral("%1_%2_%3").arg(pixmapName).arg(width).arg(height)
		: pixmapName;
	// Include the resolved ink as well as an explicit tint: palette changes at
	// runtime must not return a pixmap rendered for the previous theme.
	const auto keyTint = tint.isValid() ? tint : defaultIconColor();
	cacheName += QLatin1Char('_');
	cacheName += keyTint.name(QColor::HexArgb);

	// Return cached pixmap if it exists
	if (auto pixmap = QPixmap{}; QPixmapCache::find(cacheName, &pixmap)) { return pixmap; }

	// Load the pixmap and cache it before returning
	const auto pixmap = loadPixmap(pixmapName, width, height, xpm, tint);
	QPixmapCache::insert(cacheName, pixmap);
	return pixmap;
}

} // namespace

auto getIconPixmap(std::string_view name, int width, int height, const char* const* xpm) -> QPixmap
{
	return getIconPixmapImpl(name, width, height, xpm, QColor{});
}

auto getIconPixmap(std::string_view name, int width, int height, const QColor& tint) -> QPixmap
{
	return getIconPixmapImpl(name, width, height, nullptr, tint);
}

auto iconColor(IconRole role) -> QColor
{
	const char* key = nullptr;
	switch (role)
	{
	case IconRole::Normal: key = "_lmms_iconColor"; break;
	case IconRole::Disabled: key = "_lmms_iconDisabledColor"; break;
	case IconRole::Accent: key = "_lmms_iconAccentColor"; break;
	case IconRole::Record: key = "_lmms_iconRecordColor"; break;
	case IconRole::Warning: key = "_lmms_iconWarningColor"; break;
	case IconRole::Error: key = "_lmms_iconErrorColor"; break;
	case IconRole::Info: key = "_lmms_iconInfoColor"; break;
	}

	if (qApp)
	{
		const auto color = qApp->property(key).value<QColor>();
		if (color.isValid()) { return color; }
	}

	switch (role)
	{
	case IconRole::Disabled:
	{
		auto color = defaultIconColor();
		color.setAlphaF(0.4);
		return color;
	}
	case IconRole::Accent: return QColor(0x0b, 0xd5, 0x56);
	case IconRole::Record: return QColor(0xff, 0x47, 0x57);
	case IconRole::Warning: return QColor(0xf5, 0xa6, 0x23);
	case IconRole::Error: return QColor(0xff, 0x5c, 0x5c);
	case IconRole::Info: return QColor(0x59, 0xb0, 0xff);
	case IconRole::Normal: break;
	}
	return defaultIconColor();
}

auto getText(std::string_view name) -> QString
{
	const auto resource = QResource{":/" + QString::fromUtf8(name.data(), name.size())};
	if (!resource.isValid())
	{
		qWarning().nospace() << "Error loading text resource '"
			<< QString::fromUtf8(name.data(), name.size()) << "'";
		return {};
	}
	const auto data = resource.uncompressedData();
	if (data.isNull())
	{
		qWarning().nospace() << "Error reading text resource '"
			<< QString::fromUtf8(name.data(), name.size()) << "'";
		return {};
	}
	return QString::fromUtf8(data);
}

} // namespace lmms::embed
