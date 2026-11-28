/*
 * Vst2HostedInstrument.cpp - VST2 instrument host
 *
 * Copyright (c) 2026 LMMS developers
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

#include "Vst2HostedInstrument.h"

#include <algorithm>
#include <memory>

#include <QDomElement>
#include <QFileInfo>
#include <QTimer>

#include "AudioEngine.h"
#include "Engine.h"
#include "InstrumentPlayHandle.h"
#include "InstrumentTrack.h"
#include "PluginLoadingWindow.h"
#include "PlayHandle.h"
#include "Song.h"
#include "Vst2PluginHost.h"
#include "Vst2InstrumentView.h"
#include "VstPlugin.h"
#include "VstSubPluginFeatures.h"

#include "embed.h"
#include "plugin_export.h"

namespace lmms
{

extern "C"
{

Plugin::Descriptor PLUGIN_EXPORT vst2instrument_plugin_descriptor =
{
	LMMS_STRINGIFY(PLUGIN_NAME),
	"VST2",
	QT_TRANSLATE_NOOP("PluginBrowser", "Native VST2 instrument host"),
	"LMMS developers",
	0x0100,
	Plugin::Type::Instrument,
	new PixmapLoader("lmms-plugin-logo"),
#if defined(LMMS_BUILD_WIN32) || defined(LMMS_BUILD_CYGWIN)
	"dll",
#elif defined(LMMS_BUILD_LINUX)
#	if defined(LMMS_HAVE_VST_32) || defined(LMMS_HAVE_VST_64)
		"dll,so",
#	else
		"so",
#	endif
#else
	nullptr,
#endif
	new VstSubPluginFeatures(Plugin::Type::Instrument),
};

}

Vst2HostedInstrument::Vst2HostedInstrument(InstrumentTrack* instrumentTrack,
	const Plugin::Descriptor::SubPluginFeatures::Key* key) :
	HostedInstrument(instrumentTrack, &vst2instrument_plugin_descriptor, key,
		std::make_unique<Vst2PluginHost>(),
		Flag::IsSingleStreamed | Flag::IsMidiBased)
{
	if (key != nullptr)
	{
		m_file = key->attributes.value(QStringLiteral("file"));
		setDisplayName(key->displayName());
	}

	const bool loaded = openPlugin(m_file);
	Engine::audioEngine()->addPlayHandle(
		new InstrumentPlayHandle(this, instrumentTrack));
	if (!loaded && !m_file.isEmpty())
	{
		collectErrorForUI(tr("The VST2 instrument %1 could not be loaded.")
			.arg(m_file));
	}
}




Vst2PluginHost* Vst2HostedInstrument::vst2Host() const
{
	return static_cast<Vst2PluginHost*>(HostedInstrument::host());
}




VstPlugin* Vst2HostedInstrument::nativePlugin() const
{
	auto* host = vst2Host();
	return host != nullptr ? host->nativePlugin() : nullptr;
}




bool Vst2HostedInstrument::openPlugin(const QString& file)
{
	auto* host = vst2Host();
	if (host == nullptr)
	{
		return false;
	}

	// The first-class VST2 entry always uses the plugin child's own window.
	// The host does not embed it, on Wayland or X11.
	host->setPluginFactory([](const QString& path) {
		return std::make_unique<VstPlugin>(path, false,
			QStringLiteral("none"));
	});

	PluginLoadRequest request;
	request.path = file;
	request.isInstrument = true;
	if (auto* engine = Engine::audioEngine())
	{
		request.sampleRate = engine->outputSampleRate();
		request.maxBlockSize = std::max<int>(engine->framesPerPeriod(), 4096);
	}

	QString error;
	if (!host->load(request, &error))
	{
		initializeModels();
		return false;
	}

	initializeModels();
	auto* plugin = host->nativePlugin();
	if (plugin != nullptr)
	{
		const QString name = plugin->name().isEmpty()
			? QFileInfo(file).completeBaseName() : plugin->name();
		if (!name.isEmpty()) { setDisplayName(name); }
		connectPluginSignals(plugin);
		// The remote host maps the editor while initialising. Start hidden so
		// the view's GUI toggle and the actual window state agree.
		plugin->hideUI();
		m_editorVisible = false;
	}
	m_file = file;
	setKeyAttribute(QStringLiteral("file"), file);
	return true;
}




void Vst2HostedInstrument::connectPluginSignals(VstPlugin* plugin)
{
	if (plugin == nullptr)
	{
		return;
	}

	connect(plugin, &VstPlugin::parameterAutomated, this,
		[this](int index, float value) {
			if (m_updatingFromPlugin)
			{
				return;
			}
			auto* model = parameterModels().model(index);
			if (model == nullptr || model->isAutomatedOrControlled())
			{
				return;
			}
			if (auto* host = vst2Host())
			{
				host->noteParameterValue(static_cast<uint32_t>(index), value);
			}
			m_updatingFromPlugin = true;
			model->setValue(value, true);
			m_updatingFromPlugin = false;
		}, Qt::QueuedConnection);

	connect(plugin, &VstPlugin::parameterGestureBegin, this,
		[this](int index) {
			if (auto* model = parameterModels().model(index);
				model != nullptr && !model->isAutomatedOrControlled())
			{
				model->addJournalCheckPoint();
			}
		}, Qt::QueuedConnection);
}




void Vst2HostedInstrument::closePlugin()
{
	m_editorVisible = false;
	parameterModels().clear();
	if (auto* host = vst2Host())
	{
		host->unload();
	}
	m_file.clear();
}




bool Vst2HostedInstrument::openFile(const QString& file)
{
	if (file.isEmpty())
	{
		return false;
	}
	QMutexLocker locker(&m_mutex);
	closePlugin();
	return openPlugin(file);
}




bool Vst2HostedInstrument::reloadPlugin()
{
	QMutexLocker locker(&m_mutex);
	auto* host = vst2Host();
	if (host == nullptr || !host->isLoaded() || m_file.isEmpty())
	{
		return false;
	}

	const bool wasVisible = m_editorVisible;
	const auto state = host->saveState();
	const QString file = m_file;
	closePlugin();
	if (!openPlugin(file))
	{
		return false;
	}
	if (!state.primary.isEmpty())
	{
		host->loadState(state);
	}
	parameterModels().refreshFromHost();
	if (wasVisible)
	{
		showEditor();
	}
	return true;
}




void Vst2HostedInstrument::requestShowEditor()
{
	showEditor();
}




void Vst2HostedInstrument::showEditor()
{
	const bool wasVisible = m_editorVisible;
	auto* plugin = nativePlugin();
	if (!wasVisible)
	{
		gui::PluginLoadingWindow::showFor(plugin != nullptr
			? plugin->name() : displayName());
	}
	m_editorVisible = true;
	if (plugin != nullptr) { plugin->showUI(); }
	if (!wasVisible)
	{
		QTimer::singleShot(600, this, [] { gui::PluginLoadingWindow::hide(); });
	}
}




void Vst2HostedInstrument::hideEditor()
{
	m_editorVisible = false;
	gui::PluginLoadingWindow::hide();
	if (auto* plugin = nativePlugin()) { plugin->hideUI(); }
}




void Vst2HostedInstrument::setProgram(int index)
{
	QMutexLocker locker(&m_mutex);
	if (auto* host = vst2Host())
	{
		host->setProgram(index);
	}
}




void Vst2HostedInstrument::play(SampleFrame* workingBuffer)
{
	if (workingBuffer == nullptr || Engine::audioEngine() == nullptr)
	{
		return;
	}

	const bool exporting = Engine::getSong() != nullptr
		&& Engine::getSong()->isExporting();
	if (!m_mutex.tryLock(exporting ? -1 : 0))
	{
		const f_cnt_t frames = Engine::audioEngine()->framesPerPeriod();
		for (f_cnt_t frame = 0; frame < frames; ++frame)
		{
			workingBuffer[frame][0] = 0.0f;
			workingBuffer[frame][1] = 0.0f;
		}
		return;
	}

	HostedInstrument::play(workingBuffer);
	m_mutex.unlock();
}




bool Vst2HostedInstrument::handleMidiEvent(const MidiEvent& event,
	const TimePos& time, f_cnt_t offset)
{
	Q_UNUSED(time)
	QMutexLocker locker(&m_mutex);
	return HostedInstrument::handleMidiEvent(event, time, offset);
}




void Vst2HostedInstrument::saveSettings(QDomDocument& doc, QDomElement& parent)
{
	QMutexLocker locker(&m_mutex);
	parent.setAttribute(QStringLiteral("file"), m_file);
	HostedInstrument::saveSettings(doc, parent);
}




void Vst2HostedInstrument::loadSettings(const QDomElement& parent)
{
	const QString file = parent.attribute(QStringLiteral("file"));
	if (!file.isEmpty()
		&& (m_file.isEmpty() || file != m_file
			|| nativePlugin() == nullptr))
	{
		QMutexLocker locker(&m_mutex);
		closePlugin();
		openPlugin(file);
	}

	HostedInstrument::loadSettings(parent);
}




QString Vst2HostedInstrument::nodeName() const
{
	return vst2instrument_plugin_descriptor.name;
}




gui::PluginView* Vst2HostedInstrument::instantiateView(QWidget* parent)
{
	return new gui::Vst2InstrumentView(this, parent);
}




extern "C"
{

PLUGIN_EXPORT Plugin* lmms_plugin_main(Model* parent, void* data)
{
	return new Vst2HostedInstrument(static_cast<InstrumentTrack*>(parent),
		static_cast<const Plugin::Descriptor::SubPluginFeatures::Key*>(data));
}

}

} // namespace lmms
