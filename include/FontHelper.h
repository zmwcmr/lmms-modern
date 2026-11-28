/*
 * FontHelper.h - Header function to help with fonts
 *
 * Copyright (c) 2005-2008 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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

#ifndef LMMS_GUI_FONT_HELPER_H
#define LMMS_GUI_FONT_HELPER_H

#include <QApplication>
#include <QFont>
#include <QFontInfo>
#include <QtGlobal>

constexpr int BODY_FONT_SIZE = 10;
constexpr int AUXILIARY_FONT_SIZE = 8;
constexpr int TITLE_FONT_SIZE = 13;

// Legacy names kept for call sites and out-of-tree plugins.
constexpr int DEFAULT_FONT_SIZE = BODY_FONT_SIZE;
constexpr int SMALL_FONT_SIZE = AUXILIARY_FONT_SIZE;
constexpr int LARGE_FONT_SIZE = TITLE_FONT_SIZE;

namespace lmms::gui
{

//! Semantic text roles. Keep the hierarchy small: title, body, auxiliary.
enum class TextRole
{
	Title,
	Body,
	Auxiliary
};

//! The base sizes above are design pixels for the 9 pt font that corresponds
//! to 12 px at 96 DPI. Scale them with the application font so that text in
//! custom-painted widgets follows the desktop font setting instead of staying
//! at a fixed pixel size. Sizes are never shrunk below the design size, so
//! users who prefer a smaller system font still get the intended layout.
inline qreal fontScaleFactor()
{
	// Cover 200% system fonts. Fixed-size widgets still need their own
	// elastic-layout work beyond this point.
	constexpr qreal maxScaleFactor = 2.0;

	const qreal pointSize = QApplication::font().pointSizeF();
	if (pointSize > 0.0)
	{
		return qBound<qreal>(1.0, pointSize / 9.0, maxScaleFactor);
	}

	// Some platform themes specify the font in pixels, which has no point
	// size. Fall back to the effective pixel size.
	const int pixelSize = QFontInfo(QApplication::font()).pixelSize();
	if (pixelSize > 0)
	{
		return qBound<qreal>(1.0, static_cast<qreal>(pixelSize) / 12.0, maxScaleFactor);
	}
	return 1.0;
}

//! Returns \a basePixelSize scaled to the current application font.
inline int scaledFontSize(int basePixelSize)
{
	return qRound(basePixelSize * fontScaleFactor());
}

inline int designFontSize(TextRole role)
{
	switch (role)
	{
	case TextRole::Title: return TITLE_FONT_SIZE;
	case TextRole::Auxiliary: return AUXILIARY_FONT_SIZE;
	case TextRole::Body:
	default: return BODY_FONT_SIZE;
	}
}

//! Returns \a font at the scaled pixel size for \a role. Titles are semibold;
//! body and auxiliary text keep the caller's weight.
inline QFont fontForRole(QFont font, TextRole role)
{
	font.setPixelSize(scaledFontSize(designFontSize(role)));
	if (role == TextRole::Title)
	{
		font.setWeight(QFont::DemiBold);
	}
	return font;
}

inline QFont titleFont(QFont font) { return fontForRole(font, TextRole::Title); }
inline QFont bodyFont(QFont font) { return fontForRole(font, TextRole::Body); }
inline QFont auxiliaryFont(QFont font) { return fontForRole(font, TextRole::Auxiliary); }

//! Group box titles use the body size with semibold weight, so tabs and
//! section headers differ by weight rather than by an extra font size.
inline QFont groupTitleFont(QFont font)
{
	auto result = fontForRole(font, TextRole::Body);
	result.setWeight(QFont::DemiBold);
	return result;
}

// Convenience method to set the font size in scaled pixels. Sizes that are
// derived from widget geometry rather than from the constants above should be
// set with QFont::setPixelSize() directly, because the geometry already
// scales with the layout.
inline QFont adjustedToPixelSize(QFont font, int size)
{
	font.setPixelSize(scaledFontSize(size));
	return font;
}

} // namespace lmms::gui

#endif // LMMS_GUI_FONT_HELPER_H
