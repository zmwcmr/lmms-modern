/*
 * CPULoadWidget.cpp - widget for displaying CPU-load (partly based on
 *                      Hydrogen's CPU-load-widget)
 *
 * Copyright (c) 2005-2014 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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


#include <algorithm>
#include <QFont>
#include <QPainter>

#include "AudioEngine.h"
#include "CPULoadWidget.h"
#include "embed.h"
#include "Engine.h"


namespace lmms::gui
{


CPULoadWidget::CPULoadWidget( QWidget * _parent ) :
	QWidget( _parent ),
	m_currentLoad( 0 ),
	m_background( embed::getIconPixmap( "cpuload_bg" ) ),
	m_leds( embed::getIconPixmap( "cpuload_leds" ) ),
	m_updateTimer()
{
	setFixedSize(embed::logicalSize(m_background));

	connect( &m_updateTimer, SIGNAL(timeout()),
					this, SLOT(updateCpuLoad()));
	m_updateTimer.start( 100 );	// update cpu-load at 10 fps
}







void CPULoadWidget::paintEvent( QPaintEvent *  )
{
	QPainter p( this );
	p.drawPixmap(0, 0, m_background);

	// Keep the color ramp anchored to the full meter while revealing it in theme-defined steps.
	const auto ledSize = embed::logicalSize(m_leds);
	const int litWidth = (ledSize.width() * std::min(m_currentLoad, 100) / 100 / stepSize()) * stepSize();
	if (litWidth > 0)
	{
		p.save();
		p.setClipRect(23, 3, litWidth, ledSize.height());
		p.drawPixmap(23, 3, m_leds);
		p.restore();
	}

	// The label is drawn as text instead of being baked into the background
	// bitmap: a 1x raster image gets scaled up on high-DPI screens, which made
	// the text look blurry. Drawing it with QPainter keeps it sharp at any
	// device pixel ratio.
	QFont labelFont = font();
	labelFont.setPixelSize( height() - 2 );
	p.setFont( labelFont );
	p.setPen( m_labelColor );
	p.drawText( 0, height() - 2, QStringLiteral( "CPU" ) );
}




void CPULoadWidget::updateCpuLoad()
{
	// Additional display smoothing for the main load-value. Stronger averaging
	// cannot be used directly in the profiler: cpuLoad() must react fast enough
	// to be useful as overload indicator in AudioEngine::criticalXRuns().
	const int new_load = (m_currentLoad + Engine::audioEngine()->cpuLoad()) / 2;

	if (new_load != m_currentLoad)
	{
		auto engine = Engine::audioEngine();
		setToolTip(
			tr("DSP total: %1%").arg(new_load) + "\n"
			+ tr(" - Notes and setup: %1%").arg(engine->detailLoad(AudioEngineProfiler::DetailType::NoteSetup)) + "\n"
			+ tr(" - Instruments: %1%").arg(engine->detailLoad(AudioEngineProfiler::DetailType::Instruments)) + "\n"
			+ tr(" - Effects: %1%").arg(engine->detailLoad(AudioEngineProfiler::DetailType::Effects)) + "\n"
			+ tr(" - Mixing: %1%").arg(engine->detailLoad(AudioEngineProfiler::DetailType::Mixing))
		);
		m_currentLoad = new_load;
		update();
	}
}


} // namespace lmms::gui
