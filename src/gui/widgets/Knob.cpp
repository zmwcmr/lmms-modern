/*
 * Knob.cpp - powerful knob-widget
 *
 * Copyright (c) 2004-2014 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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

#include "Knob.h"

#include <QPainter>

#include "DeprecationHelper.h"
#include "embed.h"
#include "FontHelper.h"
#include "InlineValueEditor.h"
#include "lmms_math.h"

#include <algorithm>


namespace lmms::gui
{

Knob::Knob( KnobType _knob_num, QWidget * _parent, const QString & _name ) :
	FloatModelEditorBase(DirectionOfManipulation::Vertical, _parent, _name),
	m_label( "" ),
	m_angle( -10 ),
	m_lineWidth( 0 ),
	m_textColor( 255, 255, 255 ),
	m_knobNum( _knob_num )
{
	initUi( _name );
}

Knob::Knob(KnobType knobNum, const QString& labelText, QWidget* parent, LabelRendering labelRendering, const QString& name) :
	Knob(knobNum, parent, name)
{
	setLabel(labelText);

	if (labelRendering == LabelRendering::LegacyFixedFontSize)
	{
		setFixedFontSizeLabelRendering();
	}
}

Knob::Knob(KnobType knobNum, const QString& labelText, int labelPixelSize, QWidget* parent, const QString& name) :
	Knob(knobNum, parent, name)
{
	setFont(adjustedToPixelSize(font(), labelPixelSize));
	setLabel(labelText);
}

Knob::Knob( QWidget * _parent, const QString & _name ) :
	Knob( KnobType::Bright26, _parent, _name )
{
}



void Knob::initUi( const QString & _name )
{
	onKnobNumUpdated();
	setTotalAngle( 270.0f );
	setInnerRadius( 1.0f );
	setOuterRadius( 10.0f );

	// This is a workaround to enable style sheets for knobs which are not styled knobs.
	//
	// It works as follows: the palette colors that are assigned as the line color previously
	// had been hard coded in the drawKnob method for the different knob types. Now the
	// drawKnob method uses the line color to draw the lines. By assigning the palette colors
	// as the line colors here the knob lines will be drawn in this color unless the stylesheet
	// overrides that color.
	switch (knobNum())
	{
	case KnobType::Small17:
	case KnobType::Bright26:
	case KnobType::Dark28:
		m_lineActiveColor = QApplication::palette().color(QPalette::Active, QPalette::WindowText);
		m_arcActiveColor = QColor(QApplication::palette().color(
									QPalette::Active, QPalette::WindowText));
		m_arcActiveColor.setAlpha(70);
		break;
	case KnobType::Vintage32:
		m_lineActiveColor = QApplication::palette().color(QPalette::Active, QPalette::Shadow);
		m_arcActiveColor = QColor(QApplication::palette().color(
									QPalette::Active, QPalette::Shadow));
		m_arcActiveColor.setAlpha(70);
		break;
	default:
		break;
	}
}


void Knob::onKnobNumUpdated()
{
	if( m_knobNum != KnobType::Styled )
	{
		QString knobFilename;
		switch (m_knobNum)
		{
		case KnobType::Dark28:
			knobFilename = "knob01";
			break;
		case KnobType::Bright26:
			knobFilename = "knob02";
			break;
		case KnobType::Small17:
			knobFilename = "knob03";
			break;
		case KnobType::Vintage32:
			knobFilename = "knob05";
			break;
		case KnobType::Styled: // only here to stop the compiler from complaining
			break;
		}

		// If knobFilename is still empty here we should get the fallback pixmap of size 1x1
		m_knobPixmap = std::make_unique<QPixmap>(QPixmap(embed::getIconPixmap(knobFilename.toUtf8().constData())));
		if (!this->isEnabled())
		{
			convertPixmapToGrayScale(*m_knobPixmap.get());
		}
		// The pixmap may be an oversampled SVG raster. Size the widget in
		// logical, not device, pixels so that HiDPI artwork does not blow up
		// the layout.
		setFixedSize( embed::logicalSize( *m_knobPixmap ) );
	}
}


const QString& Knob::getLabel() const
{
	return m_label;
}


void Knob::setLabel(const QString& txt)
{
	m_label = txt;

	updateFixedSize();

	update();
}


void Knob::setFixedFontSizeLabelRendering()
{
	m_fixedFontSizeLabelRendering = true;

	updateFixedSize();

	update();
}

void Knob::updateFixedSize()
{
	if (fixedFontSizeLabelRendering())
	{
		if (m_knobPixmap)
		{
			const QSize pixmapSize = embed::logicalSize(*m_knobPixmap);

			// In legacy mode only the width of the label is taken into account while the height is not
			const int labelWidth = QFontMetrics(adjustedToPixelSize(font(), SMALL_FONT_SIZE)).horizontalAdvance(m_label);
			const int width = std::max(pixmapSize.width(), labelWidth);

			// Legacy mode assumes that the label will fit into the space of
			// the small font size plus some of the pixmap area.
			setFixedSize(width, pixmapSize.height() + scaledFontSize(SMALL_FONT_SIZE));
		}
	}
	else
	{
		// Styled knobs do not use pixmaps and have no labels. Their size is set from the outside and
		// they are painted within these limits. Hence we should not compute a new size from a pixmap
		// and/or label the case of styled knobs.
		if (knobNum() == KnobType::Styled)
		{
			return;
		}

		const QSize pixmapSize = m_knobPixmap ? embed::logicalSize(*m_knobPixmap) : QSize(0, 0);

		auto fm = QFontMetrics(font());

		const int width = std::max(pixmapSize.width(), fm.horizontalAdvance(m_label));
		const int height = pixmapSize.height() + fm.height();

		setFixedSize(width, height);
	}
}

void Knob::setTotalAngle( float angle )
{
	if( angle < 10.0 )
	{
		m_totalAngle = 10.0;
	}
	else
	{
		m_totalAngle = angle;
	}

	update();
}




float Knob::innerRadius() const
{
	return m_innerRadius;
}



void Knob::setInnerRadius( float r )
{
	m_innerRadius = r;
}



float Knob::outerRadius() const
{
	return m_outerRadius;
}



void Knob::setOuterRadius( float r )
{
	m_outerRadius = r;
}




KnobType Knob::knobNum() const
{
	return m_knobNum;
}




void Knob::setknobNum( KnobType k )
{
	if( m_knobNum != k )
	{
		m_knobNum = k;
		onKnobNumUpdated();
	}
}




QPointF Knob::centerPoint() const
{
	return m_centerPoint;
}



float Knob::centerPointX() const
{
	return m_centerPoint.x();
}



void Knob::setCenterPointX( float c )
{
	m_centerPoint.setX( c );
}



float Knob::centerPointY() const
{
	return m_centerPoint.y();
}



void Knob::setCenterPointY( float c )
{
	m_centerPoint.setY( c );
}



float Knob::lineWidth() const
{
	return m_lineWidth;
}



void Knob::setLineWidth( float w )
{
	m_lineWidth = w;
}



QColor Knob::outerColor() const
{
	return m_outerColor;
}



void Knob::setOuterColor( const QColor & c )
{
	m_outerColor = c;
}



QColor Knob::textColor() const
{
	return m_textColor;
}



void Knob::setTextColor( const QColor & c )
{
	m_textColor = c;
}



QLineF Knob::calculateLine( const QPointF & _mid, float _radius, float _innerRadius ) const
{
	const float rarc = m_angle * std::numbers::pi_v<float> / 180.0;
	const float ca = std::cos(rarc);
	const float sa = -std::sin(rarc);

	return QLineF( _mid.x() - sa*_innerRadius, _mid.y() - ca*_innerRadius,
					_mid.x() - sa*_radius, _mid.y() - ca*_radius );
}



bool Knob::updateAngle()
{
	int angle = 0;
	if( model() && model()->maxValue() != model()->minValue() )
	{
		angle = angleFromValue( model()->inverseScaledValue( model()->value() ), model()->minValue(), model()->maxValue(), m_totalAngle );
	}
	if( qAbs( angle - m_angle ) > 0 )
	{
		m_angle = angle;
		return true;
	}
	return false;
}




void Knob::drawKnob( QPainter * _p )
{
	bool enabled = this->isEnabled();
	QColor currentArcColor = enabled ? m_arcActiveColor : m_arcInactiveColor;
	QColor currentLineColor = enabled ? m_lineActiveColor : m_lineInactiveColor;

	if( updateAngle() == false && !m_cache.isNull() )
	{
		_p->drawImage( 0, 0, m_cache );
		return;
	}

	// Render the cache at the widget's device resolution. Otherwise the
	// logical-size image gets upscaled on HiDPI screens, which blurs both the
	// knob artwork and the vector arc and line drawn on top of it.
	const qreal dpr = devicePixelRatioF();
	m_cache = QImage( (QSizeF(size()) * dpr).toSize(), QImage::Format_ARGB32 );
	m_cache.setDevicePixelRatio( dpr );
	m_cache.fill( qRgba( 0, 0, 0, 0 ) );

	QPainter p( &m_cache );

	QPointF mid;

	if( m_knobNum == KnobType::Styled )
	{
		p.setRenderHint( QPainter::Antialiasing );

		// Perhaps this can move to setOuterRadius()
		if( m_outerColor.isValid() )
		{
			QRadialGradient gradient( centerPoint(), outerRadius() );
			gradient.setColorAt( 0.4, _p->pen().brush().color() );
			gradient.setColorAt( 1, m_outerColor );

			p.setPen( QPen( gradient, lineWidth(),
						Qt::SolidLine, Qt::RoundCap ) );
		}
		else {
			QPen pen = p.pen();
			pen.setWidth( (int) lineWidth() );
			pen.setCapStyle( Qt::RoundCap );

			p.setPen( pen );
		}

		p.drawLine( calculateLine( centerPoint(), outerRadius(),
							innerRadius() ) );
		p.end();
		_p->drawImage( 0, 0, m_cache );
		return;
	}


	// Old-skool knobs
	const QSize knobSize = embed::logicalSize( *m_knobPixmap );
	const float radius = knobSize.width() / 2.0f - 1;

	// Center the pixmap, the arc and the pointer on the same (fractional)
	// point. Computing the center with integer arithmetic made the pointer
	// sit half a pixel off the arc center, which became visible at high DPI
	// as a gap between the pointer and the arc that changed with the angle.
	// Keep the pixmap on whole pixels so the artwork stays crisp, and derive
	// the arc/pointer center from the pixmap position.
	const int knobX = ( width() - knobSize.width() ) / 2;
	mid = QPointF( knobX + knobSize.width() / 2.0, knobSize.height() / 2.0 );

	p.drawPixmap( knobX, 0, *m_knobPixmap );

	p.setRenderHint( QPainter::Antialiasing );

	const int centerAngle = angleFromValue( model()->inverseScaledValue( model()->centerValue() ), model()->minValue(), model()->maxValue(), m_totalAngle );

	const qreal arcLineWidth = 2;
	const qreal arcRectSize = knobSize.width() - arcLineWidth;
	const QRectF arcRect( mid.x() - arcRectSize / 2.0, mid.y() - arcRectSize / 2.0,
						  arcRectSize, arcRectSize );

	p.setPen(QPen(currentArcColor, arcLineWidth));
	p.drawArc( arcRect, 315*16, static_cast<int>( 16 * m_totalAngle ) );

	p.setPen(QPen(currentLineColor, 2));
	switch( m_knobNum )
	{
		case KnobType::Small17:
		{
			p.drawLine( calculateLine( mid, radius-2 ) );
			break;
		}
		case KnobType::Bright26:
		{
			p.drawLine( calculateLine( mid, radius-5 ) );
			break;
		}
		case KnobType::Dark28:
		{
			const float rb = qMax<float>( ( radius - 10 ) / 3.0,
									0.0 );
			const float re = qMax<float>( ( radius - 4 ), 0.0 );
			QLineF ln = calculateLine( mid, re, rb );
			ln.translate( 1, 1 );
			p.drawLine( ln );
			break;
		}
		case KnobType::Vintage32:
		{
			p.drawLine( calculateLine( mid, radius-2, 2 ) );
			break;
		}
		case KnobType::Styled:
			break;
	}

	p.drawArc( arcRect, (90-centerAngle)*16, -16*(m_angle-centerAngle) );

	p.end();

	_p->drawImage( 0, 0, m_cache );
}

void Knob::drawLabel(QPainter& p)
{
	if( !m_label.isEmpty() )
	{
		if (fixedFontSizeLabelRendering())
		{
			p.setFont(adjustedToPixelSize(p.font(), SMALL_FONT_SIZE));
		}
		auto fm = p.fontMetrics();
		const auto x = (width() - fm.horizontalAdvance(m_label)) / 2;
		const auto descent = fixedFontSizeLabelRendering() ? 2 : fm.descent();
		const auto y = height() - descent; 

		p.setPen(textColor());
		p.drawText(x, y, m_label);
	}
}

void Knob::paintEvent(QPaintEvent*)
{
	QPainter p(this);

	drawKnob(&p);
	drawLabel(p);
}

void Knob::changeEvent(QEvent * ev)
{
	if (ev->type() == QEvent::EnabledChange)
	{
		onKnobNumUpdated();
		if (!m_label.isEmpty())
		{
			setLabel(m_label);
		}
		m_cache = QImage();
		update();
	}
	else if (ev->type() == QEvent::FontChange)
	{
		// The size of the label might have changed so update
		// the size of this widget.
		updateFixedSize();

		update();
	}
}

QString VolumeKnob::currentValueToText()
{
	const auto* m = model();

	// Using std::abs to support volume models that allow negative values.
	// value == 0 is always assumed to be -inf.
	const auto roundedValue = m->getRoundedValue();
	const auto valueToVolumeRatio = std::abs(roundedValue) / m_zeroDbfsPoint;

	// NOTE: The " dBFS" units are hardcoded here instead of being set by setUnit(),
	//       allowing the model's context menu entries to display the correct units (usually "%").
	//       This workaround should be revisited after the parameter text refactor (#8379).
	if (valueToVolumeRatio == 0.) { return QStringLiteral("-∞ dBFS"); }

	if (roundedValue > 0)
	{
		return QStringLiteral("%1 dBFS").arg(ampToDbfs(valueToVolumeRatio), 3, 'f', 2);
	}
	else
	{
		return QStringLiteral("%1 dBFS (inverted)").arg(ampToDbfs(valueToVolumeRatio), 3, 'f', 2);
	}
}

QString VolumeKnob::getDynamicFloatingText(const QString& currentValue) const
{
	// Don't include the unit - the " dBFS" unit is included in currentValue
	return m_description + ' ' + currentValue;
}

void VolumeKnob::enterValue()
{
	assert(m_zeroDbfsPoint > 0);

	const auto roundedValue = model()->getRoundedValue();
	const auto magnitude = roundedValue >= 0 ? model()->maxValue() : std::abs(model()->minValue());
	const auto upperBound = ampToDbfs(magnitude / m_zeroDbfsPoint);
	const auto zeroDbfsPoint = m_zeroDbfsPoint;
	constexpr float lowerBound = -96.f;
	InlineValueEditor::showEditorWithHint(this, model(),
		InlineValueEditor::rangeHint(lowerBound, upperBound, QStringLiteral("dBFS")),
		[zeroDbfsPoint](float value) {
			const auto ratio = std::abs(value) / zeroDbfsPoint;
			return ratio > 0.f ? ampToDbfs(ratio) : lowerBound;
		},
		[zeroDbfsPoint, roundedValue, upperBound, lowerBound](float db) {
			const auto boundedDb = std::clamp(db, lowerBound, static_cast<float>(upperBound));
			const auto amplitude = boundedDb <= lowerBound ? 0.f : dbfsToAmp(boundedDb) * zeroDbfsPoint;
			return std::copysign(amplitude, roundedValue);
		}, model()->getDigitCount());
}

void convertPixmapToGrayScale(QPixmap& pixMap)
{
	const qreal dpr = pixMap.devicePixelRatio();
	QImage temp = pixMap.toImage().convertToFormat(QImage::Format_ARGB32);
	for (int i = 0; i < temp.height(); ++i)
	{
		for (int j = 0; j < temp.width(); ++j)
		{
			const auto pix = temp.pixelColor(i, j);
			const auto gscale = 0.2126 * pix.redF() + 0.7152 * pix.greenF() + 0.0722 * pix.blueF();
			const auto pixGray = QColor::fromRgbF(gscale, gscale, gscale, pix.alphaF());
			temp.setPixelColor(i, j, pixGray);
		}
	}
	pixMap.convertFromImage(temp);
	pixMap.setDevicePixelRatio(dpr);
}


} // namespace lmms::gui
