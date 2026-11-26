/*
 * Vestige.cpp - instrument-plugin for hosting VST-instruments
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

#include "Vestige.h"

#include <QDomElement>
#include <QDropEvent>
#include <QGridLayout>
#include <QMdiArea>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QtGlobal>
#include <memory>

#include "AudioEngine.h"
#include "Clipboard.h"
#include "ConfigManager.h"
#include "embed.h"
#include "Engine.h"
#include "FileDialog.h"
#include "FontHelper.h"
#include "GuiApplication.h"
#include "InstrumentPlayHandle.h"
#include "InstrumentTrack.h"
#include "MidiEvent.h"
#include "LocaleHelper.h"
#include "MainWindow.h"
#include "ParameterCurve.h"
#include "PathUtil.h"
#include "PixmapButton.h"
#include "Song.h"
#include "StringPairDrag.h"
#include "SubWindow.h"
#include "TextFloat.h"
#include "VstPlugin.h"
#include "VstSubPluginFeatures.h"

namespace lmms
{


extern "C"
{

Plugin::Descriptor Q_DECL_EXPORT  vestige_plugin_descriptor =
{
	LMMS_STRINGIFY( PLUGIN_NAME ),
	"VeSTige",
	QT_TRANSLATE_NOOP( "PluginBrowser",
			"VST-host for using VST(i)-plugins within LMMS" ),
	"Tobias Doerffel <tobydox/at/users.sf.net>",
	0x0100,
	Plugin::Type::Instrument,
	new PluginPixmapLoader( "logo" ),
#if defined(LMMS_BUILD_WIN32) || defined(LMMS_BUILD_CYGWIN)
	"dll",
#elif defined(LMMS_BUILD_LINUX)
#	if defined(LMMS_HAVE_VST_32) || defined(LMMS_HAVE_VST_64)
		"dll,so",
#	else
		"so",
#	endif
#endif
	new VstSubPluginFeatures(Plugin::Type::Instrument),
} ;

}

namespace gui
{


class vstSubWin : public SubWindow
{
public:
	vstSubWin( QWidget * _parent ) :
		SubWindow( _parent )
	{
		setAttribute(Qt::WA_DeleteOnClose, false);
		setWindowFlag(Qt::WindowMaximizeButtonHint, false);
		setDetachable(false);
	}

	~vstSubWin() override = default;

	void closeEvent( QCloseEvent * e ) override
	{
		// ignore close-events - for some reason otherwise the VST GUI
		// remains hidden when re-opening
		hide();
		e->ignore();
	}
};


} // namespace gui


class VstInstrumentPlugin : public VstPlugin
{
public:
	using VstPlugin::VstPlugin;

	void createUI( QWidget *parent ) override
	{
		Q_UNUSED(parent);
		if ( !hasEditor() ) {
			return;
		}
		if ( embedMethod() != "none" ) {
			m_pluginSubWindow.reset(new gui::vstSubWin( gui::getGUI()->mainWindow()->workspace() ));
			VstPlugin::createUI( m_pluginSubWindow.get() );
			m_pluginSubWindow->setWidget(pluginWidget());
		} else {
			VstPlugin::createUI( nullptr );
		}
	}

	/// Overwrite editor() to return the sub window instead of the embed widget
	/// itself. This makes toggleUI() and related functions toggle the
	/// sub window's visibility.
	QWidget* editor() override
	{
		return m_pluginSubWindow.get();
	}
private:
	std::unique_ptr<QMdiSubWindow> m_pluginSubWindow;
};



VestigeInstrument::VestigeInstrument( InstrumentTrack * _instrument_track,
		const Plugin::Descriptor::SubPluginFeatures::Key* key ) :
	Instrument(_instrument_track, &vestige_plugin_descriptor, nullptr, Flag::IsSingleStreamed | Flag::IsMidiBased),
	m_plugin( nullptr ),
	m_pluginMutex(),
	m_subWindow( nullptr ),
	m_scrollArea( nullptr ),
	knobFModel( nullptr ),
	p_subWindow( nullptr ),
	paramCount( 0 )
{
	// now we need a play-handle which cares for calling play()
	auto iph = new InstrumentPlayHandle(this, _instrument_track);
	Engine::audioEngine()->addPlayHandle( iph );

	connect( ConfigManager::inst(), SIGNAL( valueChanged(QString,QString,QString) ),
			 this, SLOT( handleConfigChange(QString, QString, QString) ),
			 Qt::QueuedConnection );

	if( key != nullptr && !key->attributes.value( "file" ).isEmpty() )
	{
		loadFile( key->attributes.value( "file" ) );
		setDisplayName( key->displayName() );
	}
}




VestigeInstrument::~VestigeInstrument()
{
	if (p_subWindow != nullptr) {
		delete p_subWindow;
		p_subWindow = nullptr;
	}

	if (knobFModel != nullptr) {
		delete []knobFModel;
		knobFModel = nullptr;
	}

	Engine::audioEngine()->removePlayHandlesOfTypes( instrumentTrack(),
				PlayHandle::Type::NotePlayHandle
				| PlayHandle::Type::InstrumentPlayHandle );
	closePlugin();
}




void VestigeInstrument::loadSettings( const QDomElement & _this )
{
	QString plugin = _this.attribute( "plugin" );
	if( plugin.isEmpty() )
	{
		return;
	}

	loadFile( plugin );
	m_pluginMutex.lock();
	if( m_plugin != nullptr )
	{
		m_plugin->loadSettings( _this );

		if (instrumentTrack() != nullptr && instrumentTrack()->isPreviewMode())
		{
			m_plugin->hideUI();
		}
		else if (_this.attribute( "guivisible" ).toInt())
		{
			m_plugin->showUI();
		} else
		{
			m_plugin->hideUI();
		}

		const QMap<QString, QString> & dump = m_plugin->parameterDump();
		paramCount = dump.size();
		m_lastParameterPeriod.assign(paramCount, -1);
		auto paramStr = std::array<char, 35>{};
		knobFModel = new FloatModel *[ paramCount ];
		QStringList s_dumpValues;
		for( int i = 0; i < paramCount; i++ )
		{
			std::snprintf(paramStr.data(), paramStr.size(), "param%d", i);
			s_dumpValues = dump[paramStr.data()].split(":");

			knobFModel[i] = new FloatModel( 0.0f, 0.0f, 1.0f, 0.01f, this, QString::number(i) );
			knobFModel[i]->loadSettings(_this, paramStr.data());

			if( !( knobFModel[ i ]->isAutomated() || knobFModel[ i ]->controllerConnection() ) )
			{
				knobFModel[ i ]->setValue(LocaleHelper::toFloat(s_dumpValues.at(2)));
				knobFModel[ i ]->setInitValue(LocaleHelper::toFloat(s_dumpValues.at(2)));
			}

			connect( knobFModel[i], &FloatModel::dataChanged, this,
				[this, i]() { setParameter( knobFModel[i] ); }, Qt::DirectConnection);
		}
	}
	m_pluginMutex.unlock();
}




void VestigeInstrument::setParameter( Model * action )
{
	if (m_updatingParametersFromPlugin)
	{
		return;
	}

	const int knobUNID = action->displayName().toInt();
	if (knobUNID < 0 || knobUNID >= paramCount || knobFModel == nullptr)
	{
		return;
	}

	sendParameter(knobUNID, knobFModel[knobUNID]);
}




void VestigeInstrument::sendParameter( int index, FloatModel * model )
{
	if (m_plugin == nullptr || model == nullptr)
	{
		return;
	}

	// Automation clips update the model at tick boundaries. Send one event at
	// the exact frame offset within the current block. valueBuffer() is not
	// used here: for a scalar automation change it interpolates over the whole
	// period instead of starting at the tick.
	if (model->isAutomated())
	{
		const f_cnt_t frameOffset = Engine::getSong() != nullptr
			? Engine::getSong()->currentFrameOffsetInPeriod() : 0;
		m_plugin->setParamEvents(index, {
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
				m_plugin->setParamEvents(index, events);
				return;
			}
		}
	}

	m_plugin->setParam(index, model->value());
}




void VestigeInstrument::handleParameterAutomated( int index, float value )
{
	if (m_updatingParametersFromPlugin || knobFModel == nullptr
		|| index < 0 || index >= paramCount)
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




void VestigeInstrument::handleParameterGestureBegin( int index )
{
	if (knobFModel == nullptr || index < 0 || index >= paramCount
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




void VestigeInstrument::handleParameterGestureEnd( int index )
{
	m_parameterGestures.remove(index);
}

void VestigeInstrument::handleConfigChange(QString cls, QString attr, QString value)
{
    Q_UNUSED(cls); Q_UNUSED(attr); Q_UNUSED(value);
    // Disabled for consistency with VST effects that don't implement this. (#3786)
    // if ( cls == "ui" && attr == "vstembedmethod" )
    // {
    // 	reloadPlugin();
    // }
}

void VestigeInstrument::reloadPlugin()
{
	closePlugin();
	loadFile( m_pluginDLL );
}




void VestigeInstrument::saveSettings( QDomDocument & _doc, QDomElement & _this )
{
	_this.setAttribute( "plugin", PathUtil::toShortestRelative(m_pluginDLL) );
	m_pluginMutex.lock();
	if( m_plugin != nullptr )
	{
		m_plugin->saveSettings( _doc, _this );
		if (knobFModel != nullptr) {
			const QMap<QString, QString> & dump = m_plugin->parameterDump();
			paramCount = dump.size();
			auto paramStr = std::array<char, 35>{};
			for( int i = 0; i < paramCount; i++ )
			{
				if (knobFModel[i]->isAutomated() || knobFModel[i]->controllerConnection()) {
					std::snprintf(paramStr.data(), paramStr.size(), "param%d", i);
					knobFModel[i]->saveSettings(_doc, _this, paramStr.data());
				}

/*				QDomElement me = _doc.createElement( paramStr );
				me.setAttribute( "id", knobFModel[i]->id() );
				me.setAttribute( "value", knobFModel[i]->value() );
				_this.appendChild( me );

				ControllerConnection * m_controllerConnection = knobFModel[i]->controllerConnection();
				if (m_controllerConnection) {
					QDomElement controller_element;
					QDomNode node = _this.namedItem( "connection" );
					if( node.isElement() )
					{
						controller_element = node.toElement();
					}
					else
					{
						controller_element = _doc.createElement( "connection" );
						_this.appendChild( controller_element );
					}
					QDomElement element = _doc.createElement( paramStr );
					m_controllerConnection->saveSettings( _doc, element );
					controller_element.appendChild( element );
				}*/
			}
		}
	}
	m_pluginMutex.unlock();
}




QString VestigeInstrument::nodeName( void ) const
{
	return( vestige_plugin_descriptor.name );
}




void VestigeInstrument::loadFile( const QString & _file )
{
	m_pluginMutex.lock();
	const bool set_ch_name = ( m_plugin != nullptr &&
			instrumentTrack()->name() == m_plugin->name() ) ||
			instrumentTrack()->name() == InstrumentTrack::tr( "Default preset" ) ||
			instrumentTrack()->name() == displayName();

	m_pluginMutex.unlock();

	// Do not reload a plugin which is already loaded from the same file.
	// This also avoids tearing down a freshly created editor during project
	// loading, which some plugins do not survive.
	if ( m_plugin != nullptr &&
			m_pluginDLL == PathUtil::toShortestRelative( _file ) )
	{
		return;
	}

	if ( m_plugin != nullptr )
	{
		closePlugin();
	}
	m_pluginDLL = PathUtil::toShortestRelative( _file );
	gui::TextFloat * tf = nullptr;
	if( gui::getGUI() != nullptr )
	{
		tf = gui::TextFloat::displayMessage(
				tr( "Loading plugin" ),
				tr( "Please wait while loading the VST plugin..." ),
				PLUGIN_NAME::getIconPixmap( "logo", 24, 24 ), 0 );
	}

	m_pluginMutex.lock();
	m_plugin = new VstInstrumentPlugin( m_pluginDLL );
	if( !m_plugin->failed() )
	{
		connect(m_plugin, &VstPlugin::midiOutEvent, this,
			[this](int /*deltaFrames*/, const QByteArray& data)
			{
				if (data.isEmpty()) { return; }
				const auto status = static_cast<quint8>(data[0]);
				const auto type = static_cast<MidiEventTypes>(status & 0xF0);
				if (type == MidiSysEx) { return; }
				const auto channel = static_cast<int8_t>(status & 0x0F);
				const auto param1 = data.size() > 1
					? static_cast<int16_t>(static_cast<quint8>(data[1])) : 0;
				const auto param2 = data.size() > 2
					? static_cast<int16_t>(static_cast<quint8>(data[2])) : 0;

				std::lock_guard<std::mutex> lock(m_midiOutMutex);
				if (m_midiOutQueue.size() < 4096)
				{
					m_midiOutQueue.emplace_back(type, channel, param1, param2,
						nullptr, MidiEvent::Source::Internal);
				}
			}, Qt::DirectConnection);
		connect(m_plugin, &VstPlugin::parameterAutomated, this,
			&VestigeInstrument::handleParameterAutomated,
			Qt::QueuedConnection);
		connect(m_plugin, &VstPlugin::parameterGestureBegin, this,
			&VestigeInstrument::handleParameterGestureBegin,
			Qt::QueuedConnection);
		connect(m_plugin, &VstPlugin::parameterGestureEnd, this,
			&VestigeInstrument::handleParameterGestureEnd,
			Qt::QueuedConnection);
	}
	if( m_plugin->failed() )
	{
		m_pluginMutex.unlock();
		closePlugin();
		delete tf;
		collectErrorForUI( VstPlugin::tr( "The VST plugin %1 could not be loaded." ).arg( m_pluginDLL ) );
		m_pluginDLL = "";
		return;
	}

	if ( !(instrumentTrack() != nullptr && instrumentTrack()->isPreviewMode()))
	{
		m_plugin->createUI(nullptr);
		m_plugin->showUI();
	}

	if( set_ch_name )
	{
		instrumentTrack()->setName( m_plugin->name() );
	}

	m_pluginMutex.unlock();

	emit dataChanged();

	delete tf;
}




void VestigeInstrument::play( SampleFrame* _buf )
{
	if (!m_pluginMutex.tryLock(Engine::getSong()->isExporting() ? -1 : 0)) {return;}

	if( m_plugin == nullptr )
	{
		m_pluginMutex.unlock();
		return;
	}

	m_plugin->process( nullptr, _buf );
	drainMidiOutQueue();

	if (auto* track = instrumentTrack())
	{
		track->audioBusHandle()->setSourceLatency(m_plugin->latency());
	}

	m_pluginMutex.unlock();
}




void VestigeInstrument::drainMidiOutQueue()
{
	std::vector<MidiEvent> events;
	{
		std::lock_guard<std::mutex> lock(m_midiOutMutex);
		events.swap(m_midiOutQueue);
	}

	if (auto* track = instrumentTrack())
	{
		for (const MidiEvent& event : events)
		{
			track->routeMidiOutEvent(event, TimePos());
		}
	}
}




f_cnt_t VestigeInstrument::latency() const
{
	QMutexLocker lock( &m_pluginMutex );
	return m_plugin != nullptr ? m_plugin->latency() : 0;
}




bool VestigeInstrument::handleMidiEvent( const MidiEvent& event, const TimePos& time, f_cnt_t offset )
{
	m_pluginMutex.lock();
	if( m_plugin != nullptr )
	{
		m_plugin->processMidiEvent( event, offset );
	}
	m_pluginMutex.unlock();

	return true;
}




void VestigeInstrument::closePlugin( void )
{
	{
		std::lock_guard<std::mutex> lock(m_midiOutMutex);
		m_midiOutQueue.clear();
	}

	// disconnect all signals
	if( knobFModel != nullptr )
	{
		for( int i = 0; i < paramCount; i++ )
		{
			delete knobFModel[ i ];
		}
	}

	if( knobFModel != nullptr )
	{
		delete [] knobFModel;
		knobFModel = nullptr;
	}
	m_lastParameterPeriod.clear();

	if( m_scrollArea != nullptr )
	{
//		delete m_scrollArea;
		m_scrollArea = nullptr;
	}

	if( m_subWindow != nullptr )
	{
		delete m_subWindow;
		m_subWindow = nullptr;
	}

	if( p_subWindow != nullptr )
	{
		p_subWindow = nullptr;
	}

	m_pluginMutex.lock();
	delete m_plugin;
	m_plugin = nullptr;
	m_pluginMutex.unlock();
}



gui::PluginView * VestigeInstrument::instantiateView( QWidget * _parent )
{
	return new gui::VestigeInstrumentView( this, _parent );
}


namespace gui
{

VestigeInstrumentView::VestigeInstrumentView( Instrument * _instrument,
							QWidget * _parent ) :
	InstrumentViewFixedSize( _instrument, _parent ),
	lastPosInMenu (0)
{
	m_openPluginButton = new PixmapButton( this, "" );
	m_openPluginButton->setCheckable( false );
	m_openPluginButton->setCursor( Qt::PointingHandCursor );
	m_openPluginButton->move( 216, 81 );
	m_openPluginButton->setActiveGraphic( PLUGIN_NAME::getIconPixmap(
							"select_file_active" ) );
	m_openPluginButton->setInactiveGraphic( PLUGIN_NAME::getIconPixmap(
							"select_file" ) );
	connect( m_openPluginButton, SIGNAL( clicked() ), this,
						SLOT( openPlugin() ) );
	m_openPluginButton->setToolTip(tr("Open VST plugin"));

	m_managePluginButton = new PixmapButton( this, "" );
	m_managePluginButton->setCheckable( false );
	m_managePluginButton->setCursor( Qt::PointingHandCursor );
	m_managePluginButton->move( 216, 101 );
	m_managePluginButton->setActiveGraphic( PLUGIN_NAME::getIconPixmap(
							"controls_active" ) );
	m_managePluginButton->setInactiveGraphic( PLUGIN_NAME::getIconPixmap(
							"controls" ) );
	connect( m_managePluginButton, SIGNAL( clicked() ), this,
						SLOT( managePlugin() ) );
	m_managePluginButton->setToolTip(tr("Control VST plugin from LMMS host"));


	m_openPresetButton = new PixmapButton( this, "" );
	m_openPresetButton->setCheckable( false );
	m_openPresetButton->setCursor( Qt::PointingHandCursor );
	m_openPresetButton->move( 200, 224 );
	m_openPresetButton->setActiveGraphic( embed::getIconPixmap(
							"project_open", 20, 20 ) );
	m_openPresetButton->setInactiveGraphic( embed::getIconPixmap(
							"project_open", 20, 20 ) );
	connect( m_openPresetButton, SIGNAL( clicked() ), this,
						SLOT( openPreset() ) );
	m_openPresetButton->setToolTip(tr("Open VST plugin preset"));


	m_rolLPresetButton = new QPushButton(this);
	m_rolLPresetButton->setObjectName("btn-stepper-left");
	m_rolLPresetButton->setCursor(Qt::PointingHandCursor);
	m_rolLPresetButton->move( 190, 201 );
	connect( m_rolLPresetButton, SIGNAL( clicked() ), this,
						SLOT( previousProgram() ) );
	m_rolLPresetButton->setToolTip(tr("Previous (-)"));

	m_rolLPresetButton->setShortcut( Qt::Key_Minus );


	m_savePresetButton = new PixmapButton( this, "" );
	m_savePresetButton->setCheckable( false );
	m_savePresetButton->setCursor( Qt::PointingHandCursor );
	m_savePresetButton->move( 224, 224 );
	m_savePresetButton->setActiveGraphic( embed::getIconPixmap(
							"project_save", 20, 20  ) );
	m_savePresetButton->setInactiveGraphic( embed::getIconPixmap(
							"project_save", 20, 20  ) );
	connect( m_savePresetButton, SIGNAL( clicked() ), this,
						SLOT( savePreset() ) );
	m_savePresetButton->setToolTip(tr("Save preset"));


	m_rolRPresetButton = new QPushButton(this);
	m_rolRPresetButton->setObjectName("btn-stepper-right");
	m_rolRPresetButton->setCursor(Qt::PointingHandCursor);
	m_rolRPresetButton->move( 209, 201 );
	connect( m_rolRPresetButton, SIGNAL( clicked() ), this,
						SLOT( nextProgram() ) );
	m_rolRPresetButton->setToolTip(tr("Next (+)"));

	m_rolRPresetButton->setShortcut( Qt::Key_Plus );


	m_selPresetButton = new QPushButton(this);
	m_selPresetButton->setObjectName("btn-stepper-down");
	m_selPresetButton->setCursor(Qt::PointingHandCursor);
	m_selPresetButton->move(228, 201);

	auto menu = new QMenu;

	connect( menu, SIGNAL( aboutToShow() ), this, SLOT( updateMenu() ) );

	m_selPresetButton->setMenu(menu);

	m_toggleGUIButton = new QPushButton( tr( "Show/hide GUI" ), this );
	m_toggleGUIButton->setGeometry( 20, 130, 200, 24 );
	m_toggleGUIButton->setIcon( embed::getIconPixmap( "zoom" ) );
	m_toggleGUIButton->setFont(adjustedToPixelSize(m_toggleGUIButton->font(), LARGE_FONT_SIZE));
	connect( m_toggleGUIButton, SIGNAL( clicked() ), this,
							SLOT( toggleGUI() ) );

	auto note_off_all_btn = new QPushButton(tr("Turn off all "
											   "notes"),
		this);
	note_off_all_btn->setGeometry( 20, 160, 200, 24 );
	note_off_all_btn->setIcon( embed::getIconPixmap( "stop" ) );
	note_off_all_btn->setFont(adjustedToPixelSize(note_off_all_btn->font(), LARGE_FONT_SIZE));
	connect( note_off_all_btn, SIGNAL( clicked() ), this,
							SLOT( noteOffAll() ) );

	setAcceptDrops( true );
	_instrument2 = _instrument;
	_parent2 = _parent;
}


void VestigeInstrumentView::managePlugin( void )
{
	if ( m_vi->m_plugin != nullptr && m_vi->m_subWindow == nullptr ) {
		m_vi->p_subWindow = new ManageVestigeInstrumentView( _instrument2, _parent2, m_vi);
	} else if (m_vi->m_subWindow != nullptr) {
		if (m_vi->m_subWindow->widget()->isVisible() == false ) {
			m_vi->m_scrollArea->show();
			m_vi->m_subWindow->show();
		} else {
			m_vi->m_scrollArea->hide();
			m_vi->m_subWindow->hide();
		}
	}
}


void VestigeInstrumentView::updateMenu( void )
{

	// get all presets -
	if ( m_vi->m_plugin != nullptr )
	{
		m_vi->m_plugin->loadProgramNames();
		QWidget::update();

     		QString str = m_vi->m_plugin->allProgramNames();

    		QStringList list1 = str.split("|");

     		QMenu * to_menu = m_selPresetButton->menu();
    		to_menu->clear();

			QVector<QAction*> presetActions(list1.size());

     		for (int i = 0; i < list1.size(); i++) {
			presetActions[i] = new QAction(this);
			connect(presetActions[i], SIGNAL(triggered()), this, SLOT(selPreset()));

        		presetActions[i]->setText(QString("%1. %2").arg(QString::number(i+1), list1.at(i)));
        		presetActions[i]->setData(i);
			if (i == lastPosInMenu) {
        			presetActions[i]->setIcon(embed::getIconPixmap( "sample_file", 16, 16 ));
			} else  presetActions[i]->setIcon(embed::getIconPixmap( "edit_copy", 16, 16 ));
			to_menu->addAction( presetActions[i] );
     		}

	}
}


void VestigeInstrumentView::modelChanged()
{
	m_vi = castModel<VestigeInstrument>();
}




void VestigeInstrumentView::openPlugin()
{
	FileDialog ofd( nullptr, tr( "Open VST plugin" ) );

	// set filters
	QStringList types;
#if defined(LMMS_BUILD_WIN32)
	types << tr("VST2 files (*.dll)");
#elif defined(LMMS_BUILD_LINUX)
#	if defined(LMMS_HAVE_VST_32) || defined(LMMS_HAVE_VST_64)
		types << tr("All VST files (*.dll *.so)")
			<< tr("Windows VST2 files (*.dll)");
#	endif
	types << tr("LinuxVST files (*.so)");
#endif

	ofd.setNameFilters(types);

	if( m_vi->m_pluginDLL != "" )
	{
		QString f = PathUtil::toAbsolute( m_vi->m_pluginDLL );
		ofd.setDirectory( QFileInfo( f ).absolutePath() );
		ofd.selectFile( QFileInfo( f ).fileName() );
	}
	else
	{
		ofd.setDirectory( ConfigManager::inst()->vstDir() );
	}

	if ( ofd.exec () == QDialog::Accepted )
	{
		if( ofd.selectedFiles().isEmpty() )
		{
			return;
		}
		Engine::audioEngine()->requestChangeInModel();

		if (m_vi->p_subWindow != nullptr) {
			delete m_vi->p_subWindow;
			m_vi->p_subWindow = nullptr;
		}

		m_vi->loadFile( ofd.selectedFiles()[0] );
		Engine::audioEngine()->doneChangeInModel();
		if( m_vi->m_plugin && m_vi->m_plugin->pluginWidget() )
		{
			m_vi->m_plugin->pluginWidget()->setWindowIcon(
									PLUGIN_NAME::getIconPixmap( "logo" ) );
		}
	}
}




void VestigeInstrumentView::openPreset()
{

	if ( m_vi->m_plugin != nullptr ) {
		m_vi->m_plugin->openPreset();
    		bool converted;
    		QString str = m_vi->m_plugin->currentProgramName().section("/", 0, 0);
     		if (str != "")
   			lastPosInMenu = str.toInt(&converted, 10) - 1;
		QWidget::update();
	}

}




void VestigeInstrumentView::savePreset()
{

	if ( m_vi->m_plugin != nullptr )
	{
		m_vi->m_plugin->savePreset();
/*    		bool converted;
    		QString str = m_vi->m_plugin->presetString().section("/", 0, 0);
     		if (str != "")
   			lastPosInMenu = str.toInt(&converted, 10) - 1;
		QWidget::update();*/
	}

}




void VestigeInstrumentView::nextProgram()
{

	if ( m_vi->m_plugin != nullptr ) {
		m_vi->m_plugin->rotateProgram( 1 );
    		bool converted;
    		QString str = m_vi->m_plugin->currentProgramName().section("/", 0, 0);
     		if (str != "")
   			lastPosInMenu = str.toInt(&converted, 10) - 1;
		QWidget::update();
	}
}




void VestigeInstrumentView::previousProgram()
{

	if ( m_vi->m_plugin != nullptr ) {
		m_vi->m_plugin->rotateProgram( -1 );
    		bool converted;
    		QString str = m_vi->m_plugin->currentProgramName().section("/", 0, 0);
     		if (str != "")
   			lastPosInMenu = str.toInt(&converted, 10) - 1;
		QWidget::update();
	}
}




void VestigeInstrumentView::selPreset( void )
{
	auto action = qobject_cast<QAction*>(sender());
	if (action && m_vi->m_plugin != nullptr)
	{
		lastPosInMenu = action->data().toInt();
		m_vi->m_plugin->setProgram(action->data().toInt());
		QWidget::update();
	}
}




void VestigeInstrumentView::toggleGUI( void )
{
	if( m_vi == nullptr || m_vi->m_plugin == nullptr )
	{
		return;
	}
	m_vi->m_plugin->toggleUI();
}




void VestigeInstrumentView::noteOffAll( void )
{
	m_vi->m_pluginMutex.lock();
	if( m_vi->m_plugin != nullptr )
	{
		for( int key = 0; key <= MidiMaxKey; ++key )
		{
			m_vi->m_plugin->processMidiEvent( MidiEvent( MidiNoteOff, 0, key, 0 ), 0 );
		}
	}
	m_vi->m_pluginMutex.unlock();
}




void VestigeInstrumentView::dragEnterEvent( QDragEnterEvent * _dee )
{
	// For mimeType() and MimeType enum class
	using namespace Clipboard;

	if( _dee->mimeData()->hasFormat( mimeType( MimeType::StringPair ) ) )
	{
		QString txt = _dee->mimeData()->data(
						mimeType( MimeType::StringPair ) );
		if( txt.section( ':', 0, 0 ) == "vstplugin" )
		{
			_dee->acceptProposedAction();
		}
		else
		{
			_dee->ignore();
		}
	}
	else
	{
		_dee->ignore();
	}
}




void VestigeInstrumentView::dropEvent( QDropEvent * _de )
{
	QString type = StringPairDrag::decodeKey( _de );
	QString value = StringPairDrag::decodeValue( _de );
	if( type == "vstplugin" )
	{
		m_vi->loadFile( value );
		_de->accept();
		return;
	}
	_de->ignore();
}




void VestigeInstrumentView::paintEvent( QPaintEvent * )
{
	QPainter p( this );

	static auto s_artwork = PLUGIN_NAME::getIconPixmap("artwork");
	p.drawPixmap(0, 0, s_artwork);

	QString plugin_name = ( m_vi->m_plugin != nullptr ) ?
				m_vi->m_plugin->name()/* + QString::number(
						m_plugin->version() )*/
					:
				tr( "No VST plugin loaded" );
	QFont f = p.font();
	f.setBold( true );
	p.setFont(adjustedToPixelSize(f, DEFAULT_FONT_SIZE));
	p.setPen( QColor( 255, 255, 255 ) );
	p.drawText( 10, 100, plugin_name );

	p.setPen( QColor( 50, 50, 50 ) );
	p.drawText( 10, 211, tr( "Preset" ) );

//	m_pluginMutex.lock();
	if( m_vi->m_plugin != nullptr )
	{
		p.setPen( QColor( 0, 0, 0 ) );
		f.setBold( false );
		p.setFont(adjustedToPixelSize(f, SMALL_FONT_SIZE));
		p.drawText( 10, 114, tr( "by " ) +
					m_vi->m_plugin->vendorString() );
		p.setPen( QColor( 255, 255, 255 ) );
		p.drawText( 10, 225, m_vi->m_plugin->currentProgramName() );
	}

	if( m_vi->m_subWindow != nullptr )
	{
		m_vi->m_subWindow->setWindowTitle( m_vi->instrumentTrack()->name()
								+ tr( " - VST plugin control" ) );
	}
//	m_pluginMutex.unlock();
}




ManageVestigeInstrumentView::ManageVestigeInstrumentView( Instrument * _instrument,
							QWidget * _parent, VestigeInstrument * m_vi2 ) :
	InstrumentViewFixedSize( _instrument, _parent )
{
	m_vi = m_vi2;
	m_vi->m_scrollArea = new QScrollArea( this );
	widget = new QWidget(this);
	l = new QGridLayout( this );

	m_vi->m_subWindow = getGUI()->mainWindow()->addWindowedWidget(m_vi->m_scrollArea);
	m_vi->m_scrollArea->setFixedWidth(960);
	m_vi->m_scrollArea->setMinimumHeight(300);
	m_vi->m_subWindow->setWindowTitle( m_vi->instrumentTrack()->name()
								+ tr( " - VST plugin control" ) );
	m_vi->m_subWindow->setWindowIcon( PLUGIN_NAME::getIconPixmap( "logo" ) );
	m_vi->m_subWindow->setAttribute( Qt::WA_DeleteOnClose, false );


	l->setContentsMargins( 20, 10, 10, 10 );
	l->setVerticalSpacing( 10 );
	l->setHorizontalSpacing( 23 );

	m_syncButton = new QPushButton( tr( "VST Sync" ), this );
	connect( m_syncButton, SIGNAL( clicked() ), this,
							SLOT( syncPlugin() ) );

	l->addWidget( m_syncButton, 0, 0, 1, 2, Qt::AlignLeft );

	m_displayAutomatedOnly = new QPushButton( tr( "Automated" ), this );
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

	const QMap<QString, QString> & dump = m_vi->m_plugin->parameterDump();
	m_vi->paramCount = dump.size();

	m_vstKnobs.reserve(m_vi->paramCount);

	bool hasKnobModel = true;
	if (m_vi->knobFModel == nullptr) {
		m_vi->knobFModel = new FloatModel *[ m_vi->paramCount ];
		hasKnobModel = false;
	}
	if (!hasKnobModel) {
		m_vi->m_lastParameterPeriod.assign(m_vi->paramCount, -1);
	}

	auto paramStr = std::array<char, 35>{};
	QStringList s_dumpValues;

	for( int i = 0; i < m_vi->paramCount; i++ )
	{
		std::snprintf(paramStr.data(), paramStr.size(), "param%d", i);
		s_dumpValues = dump[paramStr.data()].split(":");

		const auto& description = s_dumpValues.at(1);

		auto knob = new VstPluginKnob{m_vi->m_plugin, i, description, this};
		m_vstKnobs.push_back(knob);

		if( !hasKnobModel )
		{
			std::snprintf(paramStr.data(), paramStr.size(), "%d", i);
			m_vi->knobFModel[i] = new FloatModel(LocaleHelper::toFloat(s_dumpValues.at(2)),
				0.0f, 1.0f, 0.01f, castModel<VestigeInstrument>(), paramStr.data());
		}

		FloatModel * model = m_vi->knobFModel[i];
		connect( model, &FloatModel::dataChanged, this,
			[this, model]() { setParameter( model ); }, Qt::DirectConnection);
		knob->setModel(model);
	}
	m_vi->m_plugin->loadParameterLabels();
	m_vi->m_plugin->loadParameterDisplays();

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

	l->setRowStretch( ( int( m_vi->paramCount / 10) + 1), 1 );
	l->setColumnStretch( 10, 1 );

	widget->setLayout(l);
	widget->setAutoFillBackground(true);

	m_vi->m_scrollArea->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOn );
	m_vi->m_scrollArea->setPalette( QApplication::palette( m_vi->m_scrollArea ) );
	m_vi->m_scrollArea->setMinimumHeight( 64 );

	m_vi->m_scrollArea->setWidget( widget );

	m_vi->m_subWindow->show();
}




void ManageVestigeInstrumentView::closeWindow()
{
	m_vi->m_subWindow->hide();
}




void ManageVestigeInstrumentView::syncPlugin( void )
{
	auto paramStr = std::array<char, 35>{};
	QStringList s_dumpValues;
	const QMap<QString, QString> & dump = m_vi->m_plugin->parameterDump();

	for( int i = 0; i < m_vi->paramCount; i++ )
	{
		// only not automated knobs are synced from VST
		// those auto-setted values are not jurnaled, tracked for undo / redo
		if( !( m_vi->knobFModel[ i ]->isAutomated() || m_vi->knobFModel[ i ]->controllerConnection() ) )
		{
			std::snprintf(paramStr.data(), paramStr.size(), "param%d", i);
			s_dumpValues = dump[paramStr.data()].split(":");
			float f_value = LocaleHelper::toFloat(s_dumpValues.at(2));
			m_vi->knobFModel[i]->setValue(f_value, true);
			m_vi->knobFModel[i]->setInitValue(f_value);
		}
	}
	m_vi->m_plugin->loadParameterLabels();
	m_vi->m_plugin->loadParameterDisplays();
}




void ManageVestigeInstrumentView::displayAutomatedOnly( void )
{
	bool isAuto = QString::compare( m_displayAutomatedOnly->text(), tr( "Automated" ) ) == 0;

	for( int i = 0; i< m_vi->paramCount; i++ )
	{

		if( !( m_vi->knobFModel[ i ]->isAutomated() || m_vi->knobFModel[ i ]->controllerConnection() ) )
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


ManageVestigeInstrumentView::~ManageVestigeInstrumentView()
{
	// Delete the knobs first: their ModelView destructor still looks at the
	// model, and the models are owned by the instrument.
	if( m_vi->knobFModel != nullptr )
	{
		for( int i = 0; i < m_vi->paramCount; i++ )
		{
			delete m_vstKnobs[i];
		}
		for( int i = 0; i < m_vi->paramCount; i++ )
		{
			delete m_vi->knobFModel[ i ];
		}
	}

	if( m_vi->knobFModel != nullptr )
	{
		delete [] m_vi->knobFModel;
		m_vi->knobFModel = nullptr;
	}

	if (m_vi->m_scrollArea != nullptr) {
		delete m_vi->m_scrollArea;
		m_vi->m_scrollArea = nullptr;
	}

	if ( m_vi->m_subWindow != nullptr ) {
		delete m_vi->m_subWindow;
		m_vi->m_subWindow = nullptr;
	}

	m_vi->p_subWindow = nullptr;
}




void ManageVestigeInstrumentView::setParameter( Model * action )
{
	if (m_vi->m_updatingParametersFromPlugin)
	{
		return;
	}

	int knobUNID = action->displayName().toInt();

	if ( m_vi->m_plugin != nullptr && m_vi->knobFModel != nullptr
		&& knobUNID >= 0 && knobUNID < m_vi->paramCount )
	{
		m_vi->sendParameter( knobUNID, m_vi->knobFModel[knobUNID] );
	}
}




void ManageVestigeInstrumentView::dragEnterEvent( QDragEnterEvent * _dee )
{
	// For mimeType() and MimeType enum class
	using namespace Clipboard;

	if( _dee->mimeData()->hasFormat( mimeType( MimeType::StringPair ) ) )
	{
		QString txt = _dee->mimeData()->data(
						mimeType( MimeType::StringPair ) );
		if( txt.section( ':', 0, 0 ) == "vstplugin" )
		{
			_dee->acceptProposedAction();
		}
		else
		{
			_dee->ignore();
		}
	}
	else
	{
		_dee->ignore();
	}
}




void ManageVestigeInstrumentView::dropEvent( QDropEvent * _de )
{
	QString type = StringPairDrag::decodeKey( _de );
	QString value = StringPairDrag::decodeValue( _de );
	if( type == "vstplugin" )
	{
		m_vi->loadFile( value );
		_de->accept();
		return;
	}
	_de->ignore();
}




void ManageVestigeInstrumentView::paintEvent( QPaintEvent * )
{
	m_vi->m_subWindow->setWindowTitle( m_vi->instrumentTrack()->name()
					+ tr( " - VST plugin control" ) );
}


} // namespace gui


extern "C"
{
// necessary for getting instance out of shared lib
Q_DECL_EXPORT Plugin * lmms_plugin_main( Model *m, void * data )
{
	return new VestigeInstrument( static_cast<InstrumentTrack *>( m ),
		static_cast<Plugin::Descriptor::SubPluginFeatures::Key*>( data ) );
}

}


} // namespace lmms
