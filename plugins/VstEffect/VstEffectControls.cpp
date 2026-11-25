/*
 * VstEffectControls.cpp - controls for VST effect plugins
 *
 * Copyright (c) 2008-2014 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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

#include "VstEffectControls.h"

#include <QAction>
#include <QApplication>
#include <QDomElement>
#include <QGridLayout>
#include <QMenu>
#include <QPushButton>
#include <QScrollArea>

#include "embed.h"
#include "Engine.h"
#include "GuiApplication.h"
#include "LocaleHelper.h"
#include "MainWindow.h"
#include "ParameterCurve.h"
#include "Song.h"
#include "SubWindow.h"
#include "VstEffectControlDialog.h"
#include "VstEffect.h"
#include "VstPlugin.h"

namespace lmms
{


VstEffectControls::VstEffectControls( VstEffect * _eff ) :
	EffectControls( _eff ),
	m_effect( _eff ),
	m_subWindow( nullptr ),
	paramCount( 0 ),
	ctrHandle( nullptr ),
	lastPosInMenu (0),
	m_vstGuiVisible ( true )
//	m_presetLabel ( NULL )
{
}




VstEffectControls::~VstEffectControls()
{
	delete ctrHandle;
	ctrHandle = nullptr;
}




void VstEffectControls::loadSettings( const QDomElement & _this )
{
	//m_effect->closePlugin();
	//m_effect->openPlugin( _this.attribute( "plugin" ) );
	m_effect->m_pluginMutex.lock();
	if( m_effect->m_plugin != nullptr )
	{
		m_vstGuiVisible = _this.attribute( "guivisible" ).toInt();

		m_effect->m_plugin->loadSettings( _this );

		const QMap<QString, QString> & dump = m_effect->m_plugin->parameterDump();
		paramCount = dump.size();
		auto paramStr = std::array<char, 35>{};
		knobFModel.resize(paramCount);
		m_lastParameterPeriod.assign(paramCount, -1);
		QStringList s_dumpValues;
		for( int i = 0; i < paramCount; i++ )
		{
			std::snprintf(paramStr.data(), paramStr.size(), "param%d", i);
			s_dumpValues = dump[paramStr.data()].split(":");

			knobFModel[i] = new FloatModel( 0.0f, 0.0f, 1.0f, 0.01f, this, QString::number(i) );
			knobFModel[i]->loadSettings(_this, paramStr.data());

			if( !( knobFModel[ i ]->isAutomated() ||
						knobFModel[ i ]->controllerConnection() ) )
			{
				knobFModel[ i ]->setValue(LocaleHelper::toFloat(s_dumpValues.at(2)));
				knobFModel[ i ]->setInitValue(LocaleHelper::toFloat(s_dumpValues.at(2)));
			}

			connect( knobFModel[i], &FloatModel::dataChanged, this,
				[this, i]() { setParameter( knobFModel[i] ); }, Qt::DirectConnection);
		}

	}
	m_effect->m_pluginMutex.unlock();
}




void VstEffectControls::setParameter( Model * action )
{
	if (m_updatingParametersFromPlugin)
	{
		return;
	}

	const int knobUNID = action->displayName().toInt();
	if (knobUNID < 0 || knobUNID >= static_cast<int>(knobFModel.size()))
	{
		return;
	}

	sendParameter(knobUNID, knobFModel[knobUNID]);
}




void VstEffectControls::sendParameter( int index, FloatModel * model )
{
	if (m_effect->m_plugin == nullptr || model == nullptr)
	{
		return;
	}

	// Automation clips update the model at tick boundaries. Send one event at
	// the exact frame offset within the current block instead of letting the
	// scalar path apply the change at the block start.
	if (model->isAutomated())
	{
		const f_cnt_t frameOffset = Engine::getSong() != nullptr
			? Engine::getSong()->currentFrameOffsetInPeriod() : 0;
		m_effect->m_plugin->setParamEvents(index, {
			{static_cast<int>(frameOffset), model->value()}});
		return;
	}

	// Controllers with sample-exact data provide a block-aligned curve.
	if (model->controllerConnection())
	{
		const long period = AutomatableModel::periodCounter();
		if (index >= 0
			&& index < static_cast<int>(m_lastParameterPeriod.size()))
		{
			if (m_lastParameterPeriod[index] == period)
			{
				return;
			}
			m_lastParameterPeriod[index] = period;
		}

		ValueBuffer * values = model->valueBuffer();
		if (values != nullptr && values->length() > 0)
		{
			const auto events = compressParameterCurve(
				values->values(), values->length(),
				model->minValue(), model->maxValue());
			if (!events.empty())
			{
				m_effect->m_plugin->setParamEvents(index, events);
				return;
			}
		}
	}

	m_effect->m_plugin->setParam(index, model->value());
}




void VstEffectControls::connectToPlugin( VstPlugin* plugin )
{
	if (plugin == nullptr)
	{
		return;
	}

	connect(plugin, &VstPlugin::parameterAutomated, this,
		&VstEffectControls::handleParameterAutomated, Qt::QueuedConnection);
	connect(plugin, &VstPlugin::parameterGestureBegin, this,
		&VstEffectControls::handleParameterGestureBegin, Qt::QueuedConnection);
	connect(plugin, &VstPlugin::parameterGestureEnd, this,
		&VstEffectControls::handleParameterGestureEnd, Qt::QueuedConnection);
}




void VstEffectControls::handleParameterAutomated( int index, float value )
{
	if (m_updatingParametersFromPlugin || index < 0
		|| index >= static_cast<int>(knobFModel.size()))
	{
		return;
	}

	FloatModel* model = knobFModel[index];
	if (model == nullptr || model->isAutomatedOrControlled())
	{
		return;
	}

	m_updatingParametersFromPlugin = true;
	model->setValue(value, true);
	m_updatingParametersFromPlugin = false;
}




void VstEffectControls::handleParameterGestureBegin( int index )
{
	if (index < 0 || index >= static_cast<int>(knobFModel.size())
		|| m_parameterGestures.contains(index))
	{
		return;
	}

	FloatModel* model = knobFModel[index];
	if (model == nullptr || model->isAutomatedOrControlled())
	{
		return;
	}

	m_parameterGestures.insert(index);
	model->addJournalCheckPoint();
}




void VstEffectControls::handleParameterGestureEnd( int index )
{
	m_parameterGestures.remove(index);
}




void VstEffectControls::saveSettings( QDomDocument & _doc, QDomElement & _this )
{
	_this.setAttribute( "plugin", m_effect->m_key.attributes["file"] );
	m_effect->m_pluginMutex.lock();
	if( m_effect->m_plugin != nullptr )
	{
		m_effect->m_plugin->saveSettings( _doc, _this );
		if (!knobFModel.empty()) {
			const QMap<QString, QString> & dump = m_effect->m_plugin->parameterDump();
			paramCount = dump.size();
			auto paramStr = std::array<char, 35>{};
			for( int i = 0; i < paramCount; i++ )
			{
				if (knobFModel[i]->isAutomated() || knobFModel[i]->controllerConnection()) {
					std::snprintf(paramStr.data(), paramStr.size(), "param%d", i);
					knobFModel[i]->saveSettings(_doc, _this, paramStr.data());
				}
			}
		}
	}
	m_effect->m_pluginMutex.unlock();
}




int VstEffectControls::controlCount()
{
	return m_effect->m_plugin != nullptr ? 1 : 0;
}



gui::EffectControlDialog* VstEffectControls::createView()
{
	auto dialog = new gui::VstEffectControlDialog( this );
	dialog->togglePluginUI( m_vstGuiVisible );
	return dialog;
}




void VstEffectControls::managePlugin()
{
	if ( m_effect->m_plugin != nullptr && m_subWindow == nullptr ) {
		auto tt = new gui::ManageVSTEffectView(m_effect, this);
		ctrHandle = (QObject *)tt;
	} else if (m_subWindow != nullptr) {
		if (m_subWindow->widget()->isVisible() == false ) {
			m_scrollArea->show();
			m_subWindow->show();
		} else {
			m_scrollArea->hide();
			m_subWindow->hide();
		}
	}
}





void VstEffectControls::savePreset()
{

	if ( m_effect->m_plugin != nullptr ) {
		m_effect->m_plugin->savePreset();
/*    		bool converted;
    		QString str = m_vi->m_plugin->currentProgramName().section("/", 0, 0);
     		if (str != "")
   			lastPosInMenu = str.toInt(&converted, 10) - 1;
		QWidget::update();*/
	}

}




void VstEffectControls::updateMenu()
{

	// get all presets -
	if ( m_effect->m_plugin != nullptr )
	{
		m_effect->m_plugin->loadProgramNames();
		///QWidget::update();

     		QString str = m_effect->m_plugin->allProgramNames();

    		QStringList list1 = str.split("|");

     		QMenu * to_menu = m_selPresetButton->menu();
    		to_menu->clear();

     		for (int i = 0; i < list1.size(); i++) {
				auto presetAction = new QAction(this);
				connect(presetAction, SIGNAL(triggered()), this, SLOT(selPreset()));

        		presetAction->setText(QString("%1. %2").arg(QString::number(i+1), list1.at(i)));
        		presetAction->setData(i);
			if (i == lastPosInMenu) {
        			presetAction->setIcon(embed::getIconPixmap( "sample_file", 16, 16 ));
			} else  presetAction->setIcon(embed::getIconPixmap( "edit_copy", 16, 16 ));
			to_menu->addAction( presetAction );
     		}

	}

}




void VstEffectControls::openPreset()
{

	if ( m_effect->m_plugin != nullptr ) {
		m_effect->m_plugin->openPreset();
    		bool converted;
    		QString str = m_effect->m_plugin->currentProgramName().section("/", 0, 0);
     		if (str != "")
   			lastPosInMenu = str.toInt(&converted, 10) - 1;
		//QWidget::update();
	}

}




void VstEffectControls::rollPreset()
{

	if ( m_effect->m_plugin != nullptr ) {
		m_effect->m_plugin->rotateProgram( 1 );
    		bool converted;
    		QString str = m_effect->m_plugin->currentProgramName().section("/", 0, 0);
     		if (str != "")
   			lastPosInMenu = str.toInt(&converted, 10) - 1;
		//QWidget::update();
	}
}




void VstEffectControls::rolrPreset()
{

	if ( m_effect->m_plugin != nullptr ) {
		m_effect->m_plugin->rotateProgram( -1 );
    		bool converted;
    		QString str = m_effect->m_plugin->currentProgramName().section("/", 0, 0);
     		if (str != "")
   			lastPosInMenu = str.toInt(&converted, 10) - 1;
		//QWidget::update();
	}
}




void VstEffectControls::selPreset()
{
	auto action = qobject_cast<QAction*>(sender());
	if (action && m_effect->m_plugin != nullptr)
	{
		lastPosInMenu = action->data().toInt();
		m_effect->m_plugin->setProgram(lastPosInMenu);
		// QWidget::update();
	}
}




void VstEffectControls::paintEvent( QPaintEvent * )
{

}



namespace gui
{


ManageVSTEffectView::ManageVSTEffectView( VstEffect * _eff, VstEffectControls * m_vi ) :
	m_effect( _eff )
{
	m_vi2 = m_vi;
	widget = new QWidget();
	m_vi->m_scrollArea = new QScrollArea(widget);
	l = new QGridLayout( widget );

	m_vi->m_subWindow = getGUI()->mainWindow()->addWindowedWidget(nullptr, Qt::SubWindow |
			Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowSystemMenuHint);
	m_vi->m_subWindow->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
	m_vi->m_subWindow->setFixedSize( 960, 300);
	m_vi->m_subWindow->setWidget(m_vi->m_scrollArea);
	m_vi->m_subWindow->setWindowTitle( _eff->m_plugin->name() + tr( " - VST parameter control" ) );
	m_vi->m_subWindow->setWindowIcon( PLUGIN_NAME::getIconPixmap( "logo" ) );
	m_vi->m_subWindow->setAttribute(Qt::WA_DeleteOnClose, false);


	l->setContentsMargins( 20, 10, 10, 10 );
	l->setVerticalSpacing( 10 );
	l->setHorizontalSpacing( 23 );

	m_syncButton = new QPushButton( tr( "VST sync" ), widget );
	connect( m_syncButton, SIGNAL( clicked() ), this,
							SLOT( syncPlugin() ) );

	l->addWidget( m_syncButton, 0, 0, 1, 2, Qt::AlignLeft );

	m_displayAutomatedOnly = new QPushButton( tr( "Automated" ), widget );
	connect( m_displayAutomatedOnly, SIGNAL( clicked() ), this,
							SLOT( displayAutomatedOnly() ) );

	l->addWidget( m_displayAutomatedOnly, 0, 1, 1, 2, Qt::AlignLeft );


	m_closeButton = new QPushButton( tr( "    Close    " ), widget );
	connect( m_closeButton, SIGNAL( clicked() ), this,
							SLOT( closeWindow() ) );

	l->addWidget( m_closeButton, 0, 2, 1, 7, Qt::AlignLeft );


	for( int i = 0; i < 10; i++ )
	{
		l->addItem( new QSpacerItem( 68, 45, QSizePolicy::Fixed, QSizePolicy::Fixed ), 0, i );
	}

	const QMap<QString, QString> & dump = m_effect->m_plugin->parameterDump();
	m_vi->paramCount = dump.size();

	m_vstKnobs.reserve(m_vi->paramCount);

	bool hasKnobModel = true;
	if (m_vi->knobFModel.empty())
	{
		m_vi->knobFModel.resize(m_vi->paramCount);
		hasKnobModel = false;
	}
	m_vi->m_lastParameterPeriod.assign(m_vi->paramCount, -1);

	auto paramStr = std::array<char, 35>{};
	QStringList s_dumpValues;

	for( int i = 0; i < m_vi->paramCount; i++ )
	{
		std::snprintf(paramStr.data(), paramStr.size(), "param%d", i);
		s_dumpValues = dump[paramStr.data()].split(":");

		const auto& description = s_dumpValues.at(1);

		auto knob = new VstPluginKnob{m_effect->m_plugin.get(), i, description, widget};
		m_vstKnobs.push_back(knob);

		if( !hasKnobModel )
		{
			std::snprintf(paramStr.data(), paramStr.size(), "%d", i);
			m_vi->knobFModel[i] = new FloatModel(LocaleHelper::toFloat(s_dumpValues.at(2)),
					0.0f, 1.0f, 0.01f, _eff, paramStr.data());
		}

		FloatModel * model = m_vi->knobFModel[i];
		connect( model, &FloatModel::dataChanged, this,
			[this, model]() { setParameter( model ); }, Qt::DirectConnection);
		knob->setModel(model);
	}
	m_effect->m_plugin->loadParameterLabels();
	m_effect->m_plugin->loadParameterDisplays();

	int i = 0;
	for( int lrow = 1; lrow < ( int( m_vi->paramCount / 10 ) + 1 ) + 1; lrow++ )
	{
		for( int lcolumn = 0; lcolumn < 10; lcolumn++ )
		{
			if( i < m_vi->paramCount )
			{
				l->addWidget(m_vstKnobs[i], lrow, lcolumn, Qt::AlignCenter);
			}
			i++;
		}
	}

	l->setRowStretch( ( int( m_vi->paramCount / 10 ) + 1 ), 1 );
	l->setColumnStretch( 10, 1 );

	widget->setLayout(l);
	widget->setAutoFillBackground(true);

	m_vi->m_scrollArea->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOn );
	m_vi->m_scrollArea->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
	m_vi->m_scrollArea->setPalette( QApplication::palette( m_vi->m_scrollArea ) );
	m_vi->m_scrollArea->setMinimumHeight( 64 );

	m_vi->m_scrollArea->setWidget( widget );

	m_vi->m_subWindow->show();
}




void ManageVSTEffectView::closeWindow()
{
	m_vi2->m_subWindow->hide();
}




void ManageVSTEffectView::syncPlugin()
{
	auto paramStr = std::array<char, 35>{};
	QStringList s_dumpValues;
	const QMap<QString, QString> & dump = m_effect->m_plugin->parameterDump();

	for( int i = 0; i < m_vi2->paramCount; i++ )
	{
		// only not automated knobs are synced from VST
		// those auto-setted values are not jurnaled, tracked for undo / redo
		if( !( m_vi2->knobFModel[ i ]->isAutomated() ||
					m_vi2->knobFModel[ i ]->controllerConnection() ) )
		{
			std::snprintf(paramStr.data(), paramStr.size(), "param%d", i);
			s_dumpValues = dump[paramStr.data()].split(":");
			float f_value = LocaleHelper::toFloat(s_dumpValues.at(2));
			m_vi2->knobFModel[i]->setValue(f_value, true);
			m_vi2->knobFModel[i]->setInitValue(f_value);
		}
	}
	m_effect->m_plugin->loadParameterLabels();
	m_effect->m_plugin->loadParameterDisplays();
}



void ManageVSTEffectView::displayAutomatedOnly()
{
	bool isAuto = QString::compare( m_displayAutomatedOnly->text(), tr( "Automated" ) ) == 0;

	for( int i = 0; i< m_vi2->paramCount; i++ )
	{

		if( !( m_vi2->knobFModel[ i ]->isAutomated() ||
					m_vi2->knobFModel[ i ]->controllerConnection() ) )
		{
			if (m_vstKnobs[i]->isVisible() && isAuto)
			{
				m_vstKnobs[i]->hide();
				m_displayAutomatedOnly->setText( "All" );
			} else {
				m_vstKnobs[i]->show();
				m_displayAutomatedOnly->setText( "Automated" );
			}
		}
 	}
}




void ManageVSTEffectView::setParameter( Model * action )
{
	if (m_vi2->m_updatingParametersFromPlugin)
	{
		return;
	}

	const int knobUNID = action->displayName().toInt();
	if (knobUNID < 0
		|| knobUNID >= static_cast<int>(m_vi2->knobFModel.size()))
	{
		return;
	}

	m_vi2->sendParameter(knobUNID, m_vi2->knobFModel[knobUNID]);
}

ManageVSTEffectView::~ManageVSTEffectView()
{
	if (!m_vi2->knobFModel.empty())
	{
		for( int i = 0; i < m_vi2->paramCount; i++ )
		{
			delete m_vi2->knobFModel[ i ];
			delete m_vstKnobs[i];
		}
	}

	m_vi2->knobFModel.clear();

	if( m_vi2->m_scrollArea != nullptr )
	{
		delete m_vi2->m_scrollArea;
		m_vi2->m_scrollArea = nullptr;
	}

	if( m_vi2->m_subWindow != nullptr )
	{
		delete m_vi2->m_subWindow;
		m_vi2->m_subWindow = nullptr;
	}
	//delete m_vi2->m_subWindow;
	//m_vi2->m_subWindow = NULL;
}


} // namespace gui

} // namespace lmms
