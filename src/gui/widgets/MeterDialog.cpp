/*
 * MeterDialog.cpp - dialog for entering meter settings
 *
 * Copyright (c) 2008-2009 Tobias Doerffel <tobydox/at/users.sourceforge.net>
 * Copyright (c) 2006-2008 Danny McRae <khjklujn/at/yahoo.com>
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


#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "MeterDialog.h"
#include "FontHelper.h"
#include "MeterModel.h"
#include "LcdSpinBox.h"

namespace lmms::gui
{


MeterDialog::MeterDialog( QWidget * _parent, bool _simple ) :
	QWidget( _parent ),
	ModelView( nullptr, this )
{
	m_numerator = new LcdSpinBox(2, this, tr("Meter Numerator"));
	m_denominator = new LcdSpinBox(2, this, tr("Meter Denominator"));
	m_numerator->setToolTip(tr("Meter numerator"));
	m_denominator->setToolTip(tr("Meter denominator"));

	if (_simple)
	{
		auto layout = new QVBoxLayout(this);
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(0);
		setToolTip(tr("Time signature"));

		auto digits = new QHBoxLayout;
		digits->setSpacing(2);
		digits->addStretch();
		digits->addWidget(m_numerator);
		digits->addWidget(new QLabel(QStringLiteral("/"), this));
		digits->addWidget(m_denominator);
		digits->addStretch();
		layout->addLayout(digits);

		auto caption = new QLabel(tr("TIME SIG"), this);
		caption->setFont(adjustedToPixelSize(caption->font(), DEFAULT_FONT_SIZE));
		caption->setAlignment(Qt::AlignCenter);
		caption->setFixedHeight(9);
		layout->addWidget(caption);
		return;
	}

	auto vlayout = new QVBoxLayout(this);
	vlayout->setSpacing( 0 );
	vlayout->setContentsMargins(0, 0, 0, 0);

	auto num = new QWidget(this);
	auto num_layout = new QHBoxLayout(num);
	num_layout->setSpacing( 0 );
	num_layout->setContentsMargins(0, 0, 0, 0);


	num_layout->addWidget( m_numerator );

	num_layout->addSpacing( 5 );
	num_layout->addWidget(new QLabel(tr("Meter Numerator"), num));
	num_layout->addStretch();

	auto den = new QWidget(this);
	auto den_layout = new QHBoxLayout(den);
	den_layout->setSpacing( 0 );
	den_layout->setContentsMargins(0, 0, 0, 0);

	den_layout->addWidget( m_denominator );
	den_layout->addSpacing( 5 );
	den_layout->addWidget(new QLabel(tr("Meter Denominator"), den));
	den_layout->addStretch();


	vlayout->addSpacing( 3 );
	vlayout->addWidget( num );
	vlayout->addSpacing( 2 );
	vlayout->addWidget( den );
	vlayout->addStretch();
}







void MeterDialog::modelChanged()
{
	auto mm = castModel<MeterModel>();
	m_numerator->setModel( &mm->numeratorModel() );
	m_denominator->setModel( &mm->denominatorModel() );
}


} // namespace lmms::gui
