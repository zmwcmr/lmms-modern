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
#include <QEvent>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>

#include "AudioEngine.h"
#include "CPULoadWidget.h"
#include "embed.h"
#include "Engine.h"
#include "FontHelper.h"


namespace lmms::gui
{

namespace
{

const QString cpuLabel = QStringLiteral("CPU");
constexpr int meterLeft = 21;
constexpr int labelGap = 3;

int meterOffset(const QFontMetrics& metrics)
{
	return std::max(0, metrics.horizontalAdvance(cpuLabel) + labelGap - meterLeft);
}

} // namespace


CPULoadWidget::CPULoadWidget( QWidget * _parent ) :
	QWidget( _parent ),
	m_currentLoad( 0 ),
	m_background( embed::getIconPixmap( "cpuload_bg" ) ),
	m_leds( embed::getIconPixmap( "cpuload_leds" ) ),
	m_updateTimer()
{
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	// Theme artwork may contain transparent padding after the meter.
	const auto image = m_background.toImage();
	for (int x = image.width() - 1; x >= 0 && m_backgroundVisibleWidth == 0; --x)
	{
		for (int y = 0; y < image.height(); ++y)
		{
			if (image.pixelColor(x, y).alpha() > 32)
			{
				m_backgroundVisibleWidth = (x + 1) / m_background.devicePixelRatio();
				break;
			}
		}
	}

	connect( &m_updateTimer, SIGNAL(timeout()),
					this, SLOT(updateCpuLoad()));
	m_updateTimer.start( 100 );	// update cpu-load at 10 fps
}







QSize CPULoadWidget::sizeHint() const
{
	const auto meterSize = embed::logicalSize(m_background);
	const QFontMetrics metrics(auxiliaryFont(font()));
	return {meterSize.width() + meterOffset(metrics), std::max(meterSize.height(), metrics.height())};
}

void CPULoadWidget::changeEvent(QEvent* event)
{
	QWidget::changeEvent(event);
	if (event->type() == QEvent::FontChange) { updateGeometry(); }
}

void CPULoadWidget::paintEvent( QPaintEvent *  )
{
	QPainter p( this );
	p.setFont(auxiliaryFont(font()));
	const int offset = meterOffset(p.fontMetrics());
	const auto backgroundSize = embed::logicalSize(m_background);
	const int meterTop = (height() - backgroundSize.height()) / 2;
	// Align the visible end of the themed meter, ignoring transparent padding.
	const qreal scaleX = qreal(qMax(1, width() - offset)) /
		qMax<qreal>(1, m_backgroundVisibleWidth > 0 ? m_backgroundVisibleWidth : backgroundSize.width());
	p.save();
	p.translate(offset, meterTop);
	p.scale(scaleX, 1.0);
	p.drawPixmap(0, 0, m_background);

	// Keep the color ramp anchored to the full meter while revealing it in theme-defined steps.
	const auto ledSize = embed::logicalSize(m_leds);
	const int litWidth = (ledSize.width() * std::min(m_currentLoad, 100) / 100 / stepSize()) * stepSize();
	if (litWidth > 0)
	{
		p.save();
		const QPoint ledOrigin(23, 3);
		p.setClipRect(QRect(ledOrigin, QSize(litWidth, ledSize.height())));
		p.drawPixmap(ledOrigin, m_leds);
		p.restore();
	}
	p.restore();

	p.setPen(m_labelColor);
	p.drawText(QRect(0, 0, offset + meterLeft, height()), Qt::AlignLeft | Qt::AlignVCenter, cpuLabel);
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
