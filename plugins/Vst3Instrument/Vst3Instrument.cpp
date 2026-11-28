/*
 * Vst3Instrument.cpp - VST3 instrument host
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

#include "Vst3Instrument.h"

#include "Vst3PluginHost.h"

#include <algorithm>

#include <QDomElement>

#include "AudioEngine.h"
#include "Engine.h"
#include "InstrumentPlayHandle.h"
#include "InstrumentTrack.h"
#include "MidiEvent.h"
#include "PlayHandle.h"
#include "Song.h"
#include "Vst3Host.h"
#include "Vst3RemoteHostClient.h"
#include "Vst3SubPluginFeatures.h"
#include "Vst3EditorWindow.h"
#include "Vst3InstrumentView.h"

#include "embed.h"
#include "plugin_export.h"

namespace lmms
{

extern "C"
{

Plugin::Descriptor PLUGIN_EXPORT vst3instrument_plugin_descriptor =
{
	LMMS_STRINGIFY(PLUGIN_NAME),
	"VST3",
	QT_TRANSLATE_NOOP("PluginBrowser", "Native VST3 instrument host"),
	"LMMS developers",
	0x0100,
	Plugin::Type::Instrument,
	new PixmapLoader("lmms-plugin-logo"),
	nullptr,
	new Vst3SubPluginFeatures(Plugin::Type::Instrument),
};

}

Vst3Instrument::Vst3Instrument(InstrumentTrack* instrumentTrack,
	const Plugin::Descriptor::SubPluginFeatures::Key* key) :
	HostedInstrument(instrumentTrack, &vst3instrument_plugin_descriptor, key,
		std::make_unique<Vst3PluginHost>(),
		Flag::IsSingleStreamed | Flag::IsMidiBased)
{
	if (key != nullptr)
	{
		m_file = key->attributes.value(QStringLiteral("file"));
		m_classId = key->attributes.value(QStringLiteral("class"));
		setDisplayName(key->displayName());
	}

	const bool loaded = openPlugin(m_file, m_classId);
	Engine::audioEngine()->addPlayHandle(new InstrumentPlayHandle(this, instrumentTrack));
	if (!loaded)
	{
		collectErrorForUI(tr("The VST3 instrument %1 could not be loaded.").arg(m_file));
	}
}

Vst3Instrument::~Vst3Instrument()
{
	if (Engine::audioEngine() != nullptr)
	{
		Engine::audioEngine()->removePlayHandlesOfTypes(instrumentTrack(),
			PlayHandle::Type::NotePlayHandle | PlayHandle::Type::InstrumentPlayHandle);
	}
	closePlugin();
}

bool Vst3Instrument::openPlugin(const QString& file, const QString& classId)
{
	if (file.isEmpty() || classId.isEmpty())
	{
		return false;
	}

	double sampleRate = 44100.0;
	int blockSize = 4096;
	if (auto* engine = Engine::audioEngine())
	{
		sampleRate = engine->outputSampleRate();
		blockSize = std::max<int>(engine->framesPerPeriod(), 4096);
	}

	const bool remoteRequested = qEnvironmentVariableIsEmpty("LMMS_VST3_REMOTE_DSP")
		|| qEnvironmentVariableIntValue("LMMS_VST3_REMOTE_DSP") != 0;
	const bool remoteDsp = remoteRequested
		&& qEnvironmentVariableIsEmpty("LMMS_VST3_EDITOR_LOCAL")
		&& !Vst3RemoteHostClient::hostExecutable().isEmpty();

	if (remoteDsp)
	{
		auto remote = std::make_unique<Vst3RemoteHostClient>();
		QString remoteError;
		if (remote->load(file, classId, sampleRate, blockSize, &remoteError))
		{
			m_host = std::move(remote);
		}
		else
		{
			qWarning("VST3 remote DSP host failed: %s",
				remoteError.toUtf8().constData());
		}
	}
	if (m_host == nullptr)
	{
		if (remoteRequested)
		{
			qWarning("VST3 remote DSP unavailable, using the in-process fallback");
		}
		auto local = std::make_unique<Vst3Host>();
		QString error;
		if (!local->load(file, classId, sampleRate, blockSize, &error))
		{
			m_parameterModels.initialize(this, nullptr);
			static_cast<Vst3PluginHost*>(HostedInstrument::host())->adopt(nullptr);
			return false;
		}
		m_host = std::move(local);
	}
	static_cast<Vst3PluginHost*>(HostedInstrument::host())->adopt(m_host.get());
	m_parameterModels.initialize(this, m_host.get());

	setDisplayName(m_host->name());
	m_file = file;
	m_classId = classId;
	return true;
}

void Vst3Instrument::closePlugin()
{
	hideEditor();
	m_parameterModels.setQueueCallback({});
	static_cast<Vst3PluginHost*>(HostedInstrument::host())->adopt(nullptr);
	m_host.reset();
}

void Vst3Instrument::requestShowEditor()
{
	showEditor(nullptr);
}




void Vst3Instrument::showEditor(QWidget* parent)
{
	if (!hasEditor())
	{
		return;
	}
	m_editorRequested = true;
	if (!m_editorWindow.isNull())
	{
		m_editorWindow->raiseEditor();
		m_editorWindow->activateEditor();
		return;
	}
	m_editorWindow = Vst3EditorWindow::open(m_host.get(), parent,
		displayName(), [this](uint32_t id, double value) {
			setParameterValue(id, value);
		});
}




void Vst3Instrument::hideEditor()
{
	m_editorRequested = false;
	if (!m_editorWindow.isNull()) { m_editorWindow->close(); }
}




bool Vst3Instrument::isEditorVisible() const
{
	return m_editorRequested && !m_editorWindow.isNull();
}




bool Vst3Instrument::hasEditor() const
{
	return m_host != nullptr && m_host->hasEditor();
}




bool Vst3Instrument::reloadPlugin()
{
	if (m_host == nullptr || m_file.isEmpty() || m_classId.isEmpty())
	{
		return false;
	}
	const auto component = m_host->componentState();
	const auto controller = m_host->controllerState();
	const bool wasVisible = isEditorVisible();
	const QString file = m_file;
	const QString classId = m_classId;
	closePlugin();
	if (!openPlugin(file, classId))
	{
		return false;
	}
	if (!component.isEmpty() || !controller.isEmpty())
	{
		m_host->setState(component, controller);
	}
	m_parameterModels.refreshFromHost();
	if (wasVisible) { showEditor(nullptr); }
	return true;
}




void Vst3Instrument::play(SampleFrame* workingBuffer)
{
	if (workingBuffer == nullptr || Engine::audioEngine() == nullptr)
	{
		return;
	}
	const int frames = Engine::audioEngine()->framesPerPeriod();
	if (frames <= 0)
	{
		return;
	}

	if (!m_mutex.tryLock())
	{
		for (int frame = 0; frame < frames; ++frame)
		{
			workingBuffer[frame][0] = 0.0f;
			workingBuffer[frame][1] = 0.0f;
		}
		return;
	}

	m_parameterModels.queueChanges(frames);
	HostedInstrument::play(workingBuffer);
	m_mutex.unlock();
}

bool Vst3Instrument::handleMidiEvent(const MidiEvent& event,
	const TimePos& time, f_cnt_t offset)
{
	Q_UNUSED(time);
	QMutexLocker locker(&m_mutex);
	if (m_host == nullptr)
	{
		return false;
	}

	Vst3HostBase::MidiEvent midi;
	midi.sampleOffset = static_cast<int>(offset);
	midi.channel = std::clamp<int>(event.channel(), 0, 15);

	switch (event.type())
	{
	case MidiNoteOn:
		if (event.velocity() == 0)
		{
			midi.type = Vst3HostBase::MidiType::NoteOff;
		}
		else
		{
			midi.type = Vst3HostBase::MidiType::NoteOn;
		}
		midi.data1 = event.key();
		midi.value = static_cast<float>(event.velocity()) / 127.0f;
		break;
	case MidiNoteOff:
		midi.type = Vst3HostBase::MidiType::NoteOff;
		midi.data1 = event.key();
		midi.value = static_cast<float>(event.velocity()) / 127.0f;
		break;
	case MidiControlChange:
		midi.type = Vst3HostBase::MidiType::ControlChange;
		midi.data1 = event.controllerNumber();
		midi.data2 = event.controllerValue();
		break;
	case MidiPitchBend:
		midi.type = Vst3HostBase::MidiType::PitchBend;
		midi.data1 = event.pitchBend() & 0x7F;
		midi.data2 = (event.pitchBend() >> 7) & 0x7F;
		break;
	case MidiChannelPressure:
		midi.type = Vst3HostBase::MidiType::ChannelPressure;
		midi.data1 = event.channelPressure();
		break;
	case MidiKeyPressure:
		midi.type = Vst3HostBase::MidiType::PolyPressure;
		midi.data1 = event.key();
		midi.value = static_cast<float>(event.param(1) & 0x7F) / 127.0f;
		break;
	case MidiProgramChange:
		midi.type = Vst3HostBase::MidiType::ProgramChange;
		midi.data1 = event.program();
		break;
	default:
		return false;
	}

	m_host->queueMidi(midi);
	return true;
}

bool Vst3Instrument::setProgram(int index)
{
	QMutexLocker locker(&m_mutex);
	return m_host != nullptr && m_host->setProgram(index);
}

bool Vst3Instrument::setUnit(int unitId)
{
	QMutexLocker locker(&m_mutex);
	return m_host != nullptr && m_host->selectUnit(unitId);
}

void Vst3Instrument::setParameterValue(uint32_t id, double value)
{
	QMutexLocker locker(&m_mutex);
	if (m_host != nullptr)
	{
		m_host->setParameterNormalised(id, value);
	}
}

void Vst3Instrument::saveSettings(QDomDocument& doc, QDomElement& parent)
{
	parent.setAttribute(QStringLiteral("file"), m_file);
	parent.setAttribute(QStringLiteral("class"), m_classId);
	m_parameterModels.saveSettings(doc, parent);

	if (m_host != nullptr)
	{
		if (m_host->currentProgram() >= 0)
		{
			parent.setAttribute(QStringLiteral("program"), m_host->currentProgram());
		}
		if (m_host->currentUnit() >= 0)
		{
			parent.setAttribute(QStringLiteral("unit"), m_host->currentUnit());
		}
		const auto component = m_host->componentState();
		const auto controller = m_host->controllerState();
		if (!component.isEmpty())
		{
			parent.setAttribute(QStringLiteral("componentstate"),
				QString::fromLatin1(component.toBase64()));
		}
		if (!controller.isEmpty())
		{
			parent.setAttribute(QStringLiteral("controllerstate"),
				QString::fromLatin1(controller.toBase64()));
		}
	}
}

void Vst3Instrument::loadSettings(const QDomElement& parent)
{
	const auto file = parent.attribute(QStringLiteral("file"));
	const auto classId = parent.attribute(QStringLiteral("class"));
	if (!file.isEmpty() && !classId.isEmpty()
		&& (m_host == nullptr || file != m_file || classId != m_classId))
	{
		openPlugin(file, classId);
	}

	if (m_host == nullptr)
	{
		return;
	}

	const auto component = QByteArray::fromBase64(
		parent.attribute(QStringLiteral("componentstate")).toLatin1());
	const auto controller = QByteArray::fromBase64(
		parent.attribute(QStringLiteral("controllerstate")).toLatin1());
	if (!component.isEmpty() || !controller.isEmpty())
	{
		m_host->setState(component, controller);
	}
	m_parameterModels.refreshFromHost();
	m_parameterModels.loadSettings(parent);
	if (parent.hasAttribute(QStringLiteral("program")))
	{
		setProgram(parent.attribute(QStringLiteral("program")).toInt());
	}
	if (parent.hasAttribute(QStringLiteral("unit")))
	{
		setUnit(parent.attribute(QStringLiteral("unit")).toInt());
	}
}

QString Vst3Instrument::nodeName() const
{
	return vst3instrument_plugin_descriptor.name;
}

gui::PluginView* Vst3Instrument::instantiateView(QWidget* parent)
{
	return new gui::Vst3InstrumentView(this, parent);
}

extern "C"
{

PLUGIN_EXPORT Plugin* lmms_plugin_main(Model* parent, void* data)
{
	return new Vst3Instrument(static_cast<InstrumentTrack*>(parent),
		static_cast<const Plugin::Descriptor::SubPluginFeatures::Key*>(data));
}

}

} // namespace lmms
