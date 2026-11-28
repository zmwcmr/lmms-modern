/*
 * LmmsPalette.cpp - dummy class for fetching palette qproperties from CSS
 *                
 *
 * Copyright (c) 2007-2014 Vesa Kivimäki <contact/dot/diizy/at/nbl/dot/fi>
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

#include <QApplication>
#include <QStyle>
#include <QVariant>
#include "LmmsPalette.h"


namespace lmms::gui
{

LmmsPalette::LmmsPalette( QWidget * parent, QStyle * stylearg ) : 
	QWidget( parent ),
	
/*	sane defaults in case fetching from stylesheet fails*/	
	
	m_background( 91, 101, 113 ),
	m_windowText( 240, 240, 240 ),
	m_base( 128, 128, 128 ),
	m_text( 224, 224, 224 ),
	m_button( 201, 201, 201 ),
	m_shadow( 0,0,0 ),
	m_buttonText( 0,0,0 ),
	m_brightText( 74, 253, 133 ),
	m_highlight( 100, 100, 100 ),
	m_highlightedText( 255, 255, 255  )
{
	setStyle( stylearg );
	stylearg->polish( this );
	ensurePolished();
}

#define ACCESSMET( read, write ) \
	QColor LmmsPalette:: read () const \
	{	return m_##read ; } \
	void LmmsPalette:: write ( const QColor & c ) \
	{	m_##read = QColor( c ); }
	

	ACCESSMET( background, setBackground )
	ACCESSMET( windowText, setWindowText )
	ACCESSMET( base, setBase )
	ACCESSMET( text, setText )
	ACCESSMET( button, setButton )
	ACCESSMET( shadow, setShadow )
	ACCESSMET( buttonText, setButtonText )
	ACCESSMET( brightText, setBrightText )
	ACCESSMET( highlight, setHighlight )
	ACCESSMET( highlightedText, setHighlightedText )

// The style reads these application properties when drawing. Keeping them
// outside LmmsPalette's instance layout preserves its exported class layout.
#define STYLECOLOR(read, write, key, fallback) \
	QColor LmmsPalette::read() const \
	{ \
		const auto color = qApp->property(key).value<QColor>(); \
		return color.isValid() ? color : fallback; \
	} \
	void LmmsPalette::write(const QColor& color) \
	{ \
		qApp->setProperty(key, color); \
	}

STYLECOLOR(activeTitleText, setActiveTitleText, "_lmms_activeTitleText", QColor(255, 255, 255))
STYLECOLOR(inactiveTitleText, setInactiveTitleText, "_lmms_inactiveTitleText", QColor(192, 192, 192))
STYLECOLOR(titleText, setTitleText, "_lmms_titleText", QColor(64, 64, 64))
STYLECOLOR(mdiButton, setMdiButton, "_lmms_mdiButton", QColor(223, 228, 236))
STYLECOLOR(frameInner, setFrameInner, "_lmms_frameInner", QColor(0, 0, 0))
STYLECOLOR(linkColor, setLinkColor, "_lmms_linkColor", text())
STYLECOLOR(iconColor, setIconColor, "_lmms_iconColor", text())
STYLECOLOR(iconDisabledColor, setIconDisabledColor, "_lmms_iconDisabledColor", QColor(209, 216, 228, 102))
STYLECOLOR(iconAccentColor, setIconAccentColor, "_lmms_iconAccentColor", QColor(11, 213, 86))
STYLECOLOR(iconRecordColor, setIconRecordColor, "_lmms_iconRecordColor", QColor(255, 71, 87))
STYLECOLOR(iconWarningColor, setIconWarningColor, "_lmms_iconWarningColor", QColor(245, 166, 35))
STYLECOLOR(iconErrorColor, setIconErrorColor, "_lmms_iconErrorColor", QColor(255, 92, 92))
STYLECOLOR(iconInfoColor, setIconInfoColor, "_lmms_iconInfoColor", QColor(89, 176, 255))
#undef STYLECOLOR


QPalette LmmsPalette::palette() const
{
	QPalette pal = QApplication::style()->standardPalette();
	
	pal.setColor( QPalette::Window, 			background() );
	pal.setColor( QPalette::WindowText, 		windowText() );	
	pal.setColor( QPalette::Base, 				base() );	
	pal.setColor( QPalette::ButtonText, 		buttonText() );	
	pal.setColor( QPalette::BrightText, 		brightText() );	
	pal.setColor( QPalette::Text, 				text() );	
	pal.setColor( QPalette::Button, 			button() );	
	pal.setColor( QPalette::Shadow, 			shadow() );	
	pal.setColor( QPalette::Highlight, 			highlight() );	
	pal.setColor( QPalette::HighlightedText, 	highlightedText() );
	pal.setColor(QPalette::Link, linkColor());
	pal.setColor(QPalette::LinkVisited, linkColor());
	return pal;
}


} // namespace lmms::gui
