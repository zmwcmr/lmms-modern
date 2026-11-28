/*
 * Lv2Proc.cpp - Lv2 processor class
 *
 * Copyright (c) 2019-2024 Johannes Lorenz <jlsf2013$users.sourceforge.net, $=@>
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

#include "Lv2Proc.h"

#include "Lv2HostClient.h"
#include <limits>

#ifdef LMMS_HAVE_LV2

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <lv2/core/lv2.h>
#include <lv2/midi/midi.h>
#include <lv2/atom/atom.h>
#include <lv2/presets/presets.h>
#include <lv2/resize-port/resize-port.h>
#include <lv2/state/state.h>
#include <lv2/worker/worker.h>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QDomDocument>
#include <QDomElement>
#include <QFile>
#include <QFileInfo>
#include <QtGlobal>
#include <algorithm>

#include "AudioEngine.h"
#include "AutomatableModel.h"
#include "ComboBoxModel.h"
#include "ConfigManager.h"
#include "Engine.h"
#include "Lv2Features.h"
#include "Lv2Manager.h"
#include "Lv2Ports.h"
#include "Lv2Evbuf.h"
#include "MidiEvent.h"
#include "MidiEventToByteSeq.h"
#include "ModelVisitor.h"
#include "NoCopyNoMove.h"


namespace lmms
{

namespace
{
bool lv2RemoteMode()
{
	if (qEnvironmentVariableIsSet("LMMS_LV2_INPROCESS"))
	{
		return false;
	}
	const QByteArray value = qgetenv("LMMS_LV2_REMOTE");
	return !value.isEmpty() && value != "0";
}
} // namespace







// container for everything required to store MIDI events going to the plugin
struct MidiInputEvent
{
	MidiEvent ev;
	TimePos time;
	f_cnt_t offset;
};




namespace
{
void setModelFromControlValue(AutomatableModel* model,
	const std::vector<float>& scalePointMap, float value)
{
	struct SetModelVisitor : ModelVisitor
	{
		const std::vector<float>* m_scalePointMap = nullptr;
		float m_val = 0.f;

		void visit(FloatModel& m) override { m.setValue(m_val); }
		void visit(IntModel& m) override { m.setValue(m_val); }
		void visit(BoolModel& m) override { m.setValue(m_val); }
		void visit(ComboBoxModel& m) override
		{
			if (!m_scalePointMap || m_scalePointMap->empty())
			{
				m.setValue(m_val);
				return;
			}

			float bestDist = std::numeric_limits<float>::max();
			std::size_t bestIdx = 0;
			for (std::size_t idx = 0; idx < m_scalePointMap->size(); ++idx)
			{
				const float dist = std::fabs((*m_scalePointMap)[idx] - m_val);
				if (dist < bestDist)
				{
					bestDist = dist;
					bestIdx = idx;
				}
			}
			m.setValue(static_cast<float>(bestIdx));
		}
	};

	SetModelVisitor visitor;
	visitor.m_scalePointMap = &scalePointMap;
	visitor.m_val = value;
	model->accept(visitor);
}
}




Plugin::Type Lv2Proc::check(const LilvPlugin *plugin,
	std::vector<PluginIssue>& issues)
{
	unsigned maxPorts = lilv_plugin_get_num_ports(plugin);
	enum { inCount, outCount, maxCount };
	auto audioChannels = std::array<unsigned, maxCount>{}; // audio input and output count
	auto optionalAudioChannels = std::array<unsigned, maxCount>{}; // optional audio ports
	auto midiChannels = std::array<unsigned, maxCount>{}; // MIDI input and output count

	const char* pluginUri = lilv_node_as_uri(lilv_plugin_get_uri(plugin));
	//qDebug() << "Checking plugin" << pluginUri << "...";

	// TODO: manage a global list of blocked plugins outside of the code
	//       for now, this will help
	//       this is only a fix for the meantime
	if (!ConfigManager::enableBlockedPlugins())
	{
		if( // plugin unstable?
			Lv2Manager::pluginIsUnstable(pluginUri) ||
			// plugins only useful with UI?
			(!Lv2Manager::wantUi() &&
			Lv2Manager::pluginIsOnlyUsefulWithUi(pluginUri)) ||
			// plugin unstable with 32 or less fpp?
			(Engine::audioEngine()->framesPerPeriod() <= 32 &&
			Lv2Manager::pluginIsUnstableWithBuffersizeLessEqual32(pluginUri)) )
		{
			issues.emplace_back(PluginIssueType::Blocked);
		}
	}

	for (unsigned portNum = 0; portNum < maxPorts; ++portNum)
	{
		Lv2Ports::Meta meta;
		// does all port checks:
		std::vector<PluginIssue> tmp = meta.get(plugin, portNum);
		std::move(tmp.begin(), tmp.end(), std::back_inserter(issues));

		const bool sideChain = portIsSideChain(plugin,
			lilv_plugin_get_port_by_index(plugin, portNum));
		const bool portMustBeUsed = !sideChain && !meta.m_optional;
		if (meta.m_type == Lv2Ports::Type::Audio)
		{
			if (portMustBeUsed)
			{
				++audioChannels[meta.m_flow == Lv2Ports::Flow::Output
					? outCount : inCount];
			}
			else if (!sideChain)
			{
				++optionalAudioChannels[
					meta.m_flow == Lv2Ports::Flow::Output
						? outCount : inCount];
			}
		}
		else if(meta.m_type == Lv2Ports::Type::AtomSeq && portMustBeUsed)
		{
			++midiChannels[meta.m_flow == Lv2Ports::Flow::Output
				? outCount : inCount];
		}
	}

	// LMMS only consumes the first stereo pair. Extra audio ports remain
	// connected to their own buffers so the plugin can run, but their data is
	// ignored. This matches the VST2 host's behaviour.
	if (audioChannels[outCount] == 0 && optionalAudioChannels[outCount] == 0)
		issues.emplace_back(PluginIssueType::NoOutputChannel);

	if (midiChannels[inCount] > 1)
		issues.emplace_back(PluginIssueType::TooManyMidiInputChannels,
			std::to_string(midiChannels[inCount]));
	if (midiChannels[outCount] > 1)
		issues.emplace_back(PluginIssueType::TooManyMidiOutputChannels,
			std::to_string(midiChannels[outCount]));

	AutoLilvNodes reqFeats(lilv_plugin_get_required_features(plugin));
	LILV_FOREACH (nodes, itr, reqFeats.get())
	{
		const char* reqFeatName = lilv_node_as_string(
								lilv_nodes_get(reqFeats.get(), itr));
		if(!Lv2Features::isFeatureSupported(reqFeatName))
		{
			issues.emplace_back(PluginIssueType::FeatureNotSupported, reqFeatName);
		}
	}

	Lv2Manager* mgr = Engine::getLv2Manager();
	AutoLilvNode requiredOptionNode(mgr->uri(LV2_OPTIONS__requiredOption));
	AutoLilvNodes requiredOptions = mgr->findNodes(lilv_plugin_get_uri (plugin), requiredOptionNode.get(), nullptr);
	if (requiredOptions)
	{
		LILV_FOREACH(nodes, i, requiredOptions.get())
		{
			const char* ro = lilv_node_as_uri (lilv_nodes_get (requiredOptions.get(), i));
			if (!Lv2Options::isOptionSupported(mgr->uridMap().map(ro)))
			{
				// yes, this is not a Lv2 feature,
				// but it's a feature in abstract sense
				issues.emplace_back(PluginIssueType::FeatureNotSupported, ro);
			}
		}
	}

	return (audioChannels[inCount] > 0)
		? Plugin::Type::Effect
		: Plugin::Type::Instrument;
}




class Lv2ProcSuspender : NoCopyNoMove
{
public:
	Lv2ProcSuspender(Lv2Proc* proc)
		: m_proc(proc)
		, m_wasActive(proc->m_instance)
	{
		if (m_wasActive) { m_proc->shutdownPlugin(); }
	}
	~Lv2ProcSuspender()
	{
		if (m_wasActive) { m_proc->initPlugin(); }
	}
private:
	Lv2Proc* const m_proc;
	const bool m_wasActive;
};




Lv2Proc::Lv2Proc(Model* parent, const QString &uri) :
	ModelGroup(parent),
	m_plugin(Engine::getLv2Manager()->getPlugin(uri)),
	m_workLock(1),
	m_midiInputBuf(m_maxMidiInputEvents),
	m_midiInputReader(m_midiInputBuf),
	m_makePath{this, &Lv2Proc::makePath},
	m_freePath{nullptr, &Lv2Proc::freePath},
	m_mapPath{this, &Lv2Proc::absolutePath, &Lv2Proc::abstractPath}
{
	if (!m_plugin)
	{
		qCritical() << "No Lv2 plugin found for URI" << uri;
		throw std::runtime_error("No Lv2 plugin found for given URI");
	}
	createPorts();
	initPlugin();
	scanPresets();
}




Lv2Proc::~Lv2Proc()
{
	if (m_viewDetacher)
	{
		m_viewDetacher();
		m_viewDetacher = nullptr;
	}
	shutdownPlugin();
}




void Lv2Proc::reload() { Lv2ProcSuspender(this); }




void Lv2Proc::dumpPorts()
{
	std::size_t num = 0;
	for (const std::unique_ptr<Lv2Ports::PortBase>& port: m_ports)
	{
		(void)port;
		dumpPort(num++);
	}
}




void Lv2Proc::scanPresets()
{
	m_presets.clear();

	Lv2Manager* mgr = Engine::getLv2Manager();
	AutoLilvNode presetType = mgr->uri(LV2_PRESETS__Preset);
	AutoLilvNode labelPredicate = mgr->uri(LILV_NS_RDFS "label");
	AutoLilvNodes related(lilv_plugin_get_related(m_plugin, presetType.get()));
	if (!related)
	{
		return;
	}

	LILV_FOREACH(nodes, it, related.get())
	{
		const LilvNode* node = lilv_nodes_get(related.get(), it);
		if (!lilv_node_is_uri(node))
		{
			continue;
		}

		QString label;
		auto fetchLabel = [&]() -> QString
		{
			LilvNode* labelNode = lilv_world_get(
				mgr->world(), node, labelPredicate.get(), nullptr);
			if (!labelNode)
			{
				return {};
			}
			QString text;
			if (lilv_node_is_string(labelNode))
			{
				text = QString::fromUtf8(
					lilv_node_as_string(labelNode)).trimmed();
			}
			lilv_node_free(labelNode);
			return text;
		};

		label = fetchLabel();
		if (label.isEmpty())
		{
			// Older bundles (e.g. mda) only store the label in the file
			// referenced by rdfs:seeAlso.
			lilv_world_load_resource(mgr->world(), node);
			label = fetchLabel();
		}

		const QString uri = QString::fromUtf8(lilv_node_as_uri(node));
		if (label.isEmpty())
		{
			label = uri.section(QLatin1Char('#'), -1);
			if (label.isEmpty())
			{
				label = uri;
			}
		}
		m_presets.push_back({label, uri});
	}

	std::sort(m_presets.begin(), m_presets.end(),
		[](const Preset& a, const Preset& b)
		{
			const int cmp = QString::localeAwareCompare(a.label, b.label);
			return cmp < 0 || (cmp == 0 && a.uri < b.uri);
		});
}




bool Lv2Proc::loadPreset(std::size_t index)
{
	if (index >= m_presets.size())
	{
		return false;
	}

	if (m_remote)
	{
		bool restored = false;
		{
			std::lock_guard<std::mutex> lock(m_stateMutex);
			restored = m_remote->loadPreset(m_presets[index].uri);
		}
		if (restored)
		{
			applyRemoteControlValues();
		}
		return restored;
	}

	if (!m_instance)
	{
		return false;
	}

	Lv2Manager* mgr = Engine::getLv2Manager();
	const QByteArray presetUri = m_presets[index].uri.toUtf8();
	AutoLilvNode presetNode(lilv_new_uri(mgr->world(), presetUri.constData()));
	if (!presetNode)
	{
		return false;
	}

	if (lilv_world_load_resource(mgr->world(), presetNode.get()) < 0)
	{
		qWarning() << "Could not load LV2 preset resource"
			<< m_presets[index].uri;
	}

	LilvState* state = lilv_state_new_from_world(
		mgr->world(), mgr->uridMap().mapFeature(), presetNode.get());
	if (!state)
	{
		qWarning() << "Could not create LV2 state for preset"
			<< m_presets[index].uri;
		return false;
	}

	struct RestoredValue
	{
		std::size_t portIndex;
		float value;
	};
	struct RestoreContext
	{
		Lv2Proc* proc;
		std::vector<RestoredValue>* values;
	};

	std::vector<RestoredValue> restoredValues;
	RestoreContext context{this, &restoredValues};

	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		lilv_state_restore(state, m_instance,
			[](const char* portSymbol, void* userData, const void* value,
				uint32_t size, uint32_t)
			{
				auto* ctx = static_cast<RestoreContext*>(userData);
				if (!ctx || !ctx->proc || !ctx->values || !portSymbol
					|| !value || size != sizeof(float))
				{
					return;
				}

				const std::size_t portIndex =
					ctx->proc->getIdOfPort(portSymbol);
				if (portIndex >= ctx->proc->portNum())
				{
					return;
				}

				auto* ctrl = Lv2Ports::dcast<Lv2Ports::Control>(
					ctx->proc->port(portIndex));
				if (!ctrl)
				{
					return;
				}

				ctrl->m_val = *static_cast<const float*>(value);
				ctx->values->push_back({portIndex, ctrl->m_val});
			},
			&context, 0, nullptr);
	}
	lilv_state_free(state);

	for (const RestoredValue& restored : restoredValues)
	{
		auto* ctrl = Lv2Ports::dcast<Lv2Ports::Control>(port(restored.portIndex));
		if (ctrl && ctrl->m_connectedModel)
		{
			setModelFromControlValue(ctrl->m_connectedModel.get(),
				ctrl->m_scalePointMap, restored.value);
		}
	}

	return true;
}




void Lv2Proc::copyModelsFromCore()
{
	struct FloatFromModelVisitor : public ConstModelVisitor
	{
		const std::vector<float>* m_scalePointMap = nullptr; // in
		float m_res = 0.f; // out
		void visit(const FloatModel& m) override { m_res = m.value(); }
		void visit(const IntModel& m) override {
			m_res = static_cast<float>(m.value()); }
		void visit(const BoolModel& m) override {
			m_res = static_cast<float>(m.value()); }
		void visit(const ComboBoxModel& m) override
		{
			// Plugins may advertise lv2:enumeration without scale points.
			// Never index an empty or too-short scale point map.
			if (m_scalePointMap == nullptr || m_scalePointMap->empty())
			{
				m_res = 0.f;
				return;
			}
			const std::size_t index = static_cast<std::size_t>(
				std::max(0, m.value()));
			m_res = (*m_scalePointMap)[
				std::min(index, m_scalePointMap->size() - 1)];
		}
	};

	struct Copy : public Lv2Ports::Visitor
	{
		void visit(Lv2Ports::Control& ctrl) override
		{
			FloatFromModelVisitor ffm;
			ffm.m_scalePointMap = &ctrl.m_scalePointMap;
			ctrl.m_connectedModel->accept(ffm);
			ctrl.m_val = ffm.m_res;
		}
		void visit(Lv2Ports::Cv& cv) override
		{
			FloatFromModelVisitor ffm;
			ffm.m_scalePointMap = &cv.m_scalePointMap;
			cv.m_connectedModel->accept(ffm);
			// dirty fix, needs better interpolation
			std::ranges::fill(cv.m_buffer, ffm.m_res);
		}
		void visit(Lv2Ports::AtomSeq& atomPort) override
		{
			lv2_evbuf_reset(atomPort.m_buf.get(), true);
		}
	} copy;

	// feed each input port with the respective data from the LMMS core
	for (const std::unique_ptr<Lv2Ports::PortBase>& port : m_ports)
	{
		if (port->m_flow == Lv2Ports::Flow::Input)
		{
			port->accept(copy);
		}
	}

	if (m_remote)
	{
		copyModelsToRemote();
		return;
	}

	// send pending MIDI events to atom port
	if(m_midiIn)
	{
		LV2_Evbuf_Iterator iter = lv2_evbuf_begin(m_midiIn->m_buf.get());
		// MIDI events waiting to go to the plugin?
		while(m_midiInputReader.read_space() > 0)
		{
			const MidiInputEvent ev = m_midiInputReader.read(1)[0];
			uint32_t atomStamp =
				ev.time.frames(Engine::framesPerTick()) + ev.offset;
			uint32_t type = Engine::getLv2Manager()->
				uridCache()[Lv2UridCache::Id::midi_MidiEvent];
			auto buf = std::array<uint8_t, 4>{};
			std::size_t bufsize = writeToByteSeq(ev.ev, buf.data(), buf.size());
			if(bufsize)
			{
				lv2_evbuf_write(&iter, atomStamp, type, bufsize, buf.data());
			}
		}
	}
}




void Lv2Proc::copyModelsToCore()
{
	struct Copy : public Lv2Ports::Visitor
	{
		void visit(Lv2Ports::AtomSeq& atomPort) override
		{
			// we currently don't copy anything, but we need to clear the buffer
			// for the plugin to write again
			lv2_evbuf_reset(atomPort.m_buf.get(), false);
		}
	} copy;

	// fetch data from each output port and bring it to the LMMS core
	for (const std::unique_ptr<Lv2Ports::PortBase>& port : m_ports)
	{
		if (port->m_flow == Lv2Ports::Flow::Output)
		{
			port->accept(copy);
		}
	}
}




void Lv2Proc::copyBuffersFromCore(const SampleFrame* buf, f_cnt_t frames)
{
	if (!inPorts().m_left)
	{
		return;
	}

	inPorts().m_left->copyBuffersFromCore(buf, 0, frames);
	if (inPorts().m_right)
	{
		inPorts().m_right->copyBuffersFromCore(buf, 1, frames);
	}
	else
	{
		// LMMS is always stereo, but the effect can only do mono
		// => merge both input buffers together on left port
		inPorts().m_left->averageWithBuffersFromCore(buf, 1, frames);
	}
}




void Lv2Proc::copyBuffersToCore(SampleFrame* buf, f_cnt_t frames) const
{
	if (!outPorts().m_left)
	{
		// No audio output was registered; keep the track silent instead of
		// dereferencing a null port.
		for (f_cnt_t f = 0; f < frames; ++f)
		{
			buf[f][0] = 0.f;
			buf[f][1] = 0.f;
		}
		return;
	}

	outPorts().m_left->copyBuffersToCore(buf, 0, frames);
	// if the caller requests to copy into two channels, but we only have
	// one output channel, duplicate our output
	// (this happens if we have two inputs and only one output)
	Lv2Ports::Audio* ap = outPorts().m_right
		? outPorts().m_right : outPorts().m_left;
	ap->copyBuffersToCore(buf, 1, frames);
}




std::size_t Lv2Proc::getIdOfPort(const char* symbol) const
{
	for (std::size_t i = 0; i < m_ports.size(); ++i)
	{
		const auto* port = m_ports[i].get();
		const char* portSymbol = lilv_node_as_string(
			lilv_port_get_symbol(port->m_plugin, port->m_port));
		if (portSymbol && std::strcmp(portSymbol, symbol) == 0)
		{
			return i;
		}
	}
	return static_cast<std::size_t>(-1);
}




void Lv2Proc::setControlFromUi(uint32_t portIndex, float value)
{
	if (portIndex >= m_ports.size())
	{
		return;
	}

	auto* ctrl = Lv2Ports::dcast<Lv2Ports::Control>(m_ports[portIndex].get());
	if (!ctrl || !ctrl->m_connectedModel)
	{
		return;
	}

	ctrl->m_val = value;
	setModelFromControlValue(ctrl->m_connectedModel.get(),
		ctrl->m_scalePointMap, value);
}




bool Lv2Proc::writeUiEvent(uint32_t portIndex, uint32_t protocol,
	uint32_t size, const void* buffer)
{
	if (protocol == 0)
	{
		if (size != sizeof(float) || !buffer)
		{
			return false;
		}
		setControlFromUi(portIndex, *static_cast<const float*>(buffer));
		return true;
	}

	Lv2Manager* mgr = Engine::getLv2Manager();
	const LV2_URID atomEvent = mgr->uridMap().map(LV2_ATOM__eventTransfer);
	if (protocol != atomEvent)
	{
		qWarning() << "Unsupported LV2 UI event: protocol" << protocol
			<< "size" << size;
		return false;
	}

	if (portIndex >= m_ports.size() || !buffer || size < sizeof(LV2_Atom))
	{
		return false;
	}

	const auto* atom = static_cast<const LV2_Atom*>(buffer);
	if (atom->size > size - sizeof(LV2_Atom))
	{
		return false;
	}

	auto* atomSeq = dcast<Lv2Ports::AtomSeq>(m_ports[portIndex].get());
	if (!atomSeq)
	{
		return false;
	}

	std::lock_guard<std::mutex> lock(m_stateMutex);
	auto it = lv2_evbuf_end(atomSeq->m_buf.get());
	lv2_evbuf_write(&it, 0, atom->type, atom->size,
		static_cast<const uint8_t*>(LV2_ATOM_BODY_CONST(atom)));
	return true;
}




void Lv2Proc::connectToPluginEvents()
{
	{
		std::lock_guard<std::mutex> lock(m_uiEventMutex);
		m_uiEvents.clear();
	}
	m_uiActive.store(true, std::memory_order_relaxed);
}




std::vector<Lv2Proc::UiEvent> Lv2Proc::takeUiEvents()
{
	std::deque<UiEvent> events;
	{
		std::lock_guard<std::mutex> lock(m_uiEventMutex);
		events.swap(m_uiEvents);
	}

	std::vector<UiEvent> result;
	result.reserve(events.size());
	for (auto& event : events)
	{
		result.push_back(std::move(event));
	}
	return result;
}




bool Lv2Proc::sendToUi(uint32_t portIndex, uint32_t type, uint32_t size,
	const void* body)
{
	if (!m_uiActive.load(std::memory_order_relaxed))
	{
		return false;
	}

	constexpr std::size_t maxBodySize = 4096;
	if (size > maxBodySize || (size > 0 && !body))
	{
		qWarning() << "Dropping oversized LV2 UI event for port" << portIndex
			<< "size" << size;
		return false;
	}

	const LV2_URID atomEvent =
		Engine::getLv2Manager()->uridMap().map(LV2_ATOM__eventTransfer);

	std::lock_guard<std::mutex> lock(m_uiEventMutex);
	if (m_uiEvents.size() >= 256)
	{
		qWarning() << "Plugin => UI queue overflow for port" << portIndex;
		return false;
	}

	UiEvent event;
	event.index = portIndex;
	event.protocol = atomEvent;
	event.data.resize(sizeof(LV2_Atom) + size);
	auto* atom = reinterpret_cast<LV2_Atom*>(event.data.data());
	atom->size = size;
	atom->type = type;
	if (size > 0)
	{
		std::memcpy(event.data.data() + sizeof(LV2_Atom), body, size);
	}
	m_uiEvents.push_back(std::move(event));
	return true;
}




void Lv2Proc::saveSettings(QDomDocument& doc, QDomElement& that)
{
	ModelGroup::saveSettings(doc, that);
	saveState(doc, that);
}




void Lv2Proc::loadSettings(const QDomElement& that)
{
	ModelGroup::loadSettings(that);
	loadState(that);
}




namespace
{
char* duplicateCString(const QByteArray& str)
{
	auto* result = static_cast<char*>(std::malloc(static_cast<std::size_t>(str.size()) + 1));
	if (result)
	{
		std::memcpy(result, str.constData(), static_cast<std::size_t>(str.size()) + 1);
	}
	return result;
}
}




/** Write the files stored in an lv2state element into @a directory.
 *  Returns true when state.ttl was written, which is what lilv needs.
 */
bool writeStateFilesFromXml(const QDomElement& stateElem, const QString& directory)
{
	for (QDomElement fileElem = stateElem.firstChildElement("file");
		!fileElem.isNull(); fileElem = fileElem.nextSiblingElement("file"))
	{
		const QString name = fileElem.attribute("name");
		if (name.isEmpty() || QDir::isAbsolutePath(name) || name.contains(".."))
		{
			qWarning() << "Ignoring invalid LV2 state file name" << name;
			continue;
		}

		const QString path = QDir(directory).filePath(name);
		QDir().mkpath(QFileInfo(path).absolutePath());
		QFile file(path);
		if (!file.open(QIODevice::WriteOnly))
		{
			qWarning() << "Could not write LV2 state file" << path;
			return false;
		}
		if (file.write(QByteArray::fromBase64(fileElem.text().toLatin1())) < 0)
		{
			qWarning() << "Could not write LV2 state file" << path;
			return false;
		}
	}
	return QFileInfo::exists(QDir(directory).filePath("state.ttl"));
}

/** Append all files from @a directory (except manifest.ttl) to @a stateElem. */
void appendStateFilesToXml(QDomDocument& doc, QDomElement& stateElem,
	const QString& directory)
{
	const QDir dir(directory);
	QDirIterator it(directory, QDir::Files | QDir::NoDotAndDotDot,
		QDirIterator::Subdirectories);
	while (it.hasNext())
	{
		const QString path = it.next();
		const QString rel = dir.relativeFilePath(path);
		if (rel == "manifest.ttl")
		{
			continue;
		}

		QFile file(path);
		if (!file.open(QIODevice::ReadOnly))
		{
			qWarning() << "Could not read LV2 state file" << path;
			continue;
		}

		QDomElement fileElem = doc.createElement("file");
		fileElem.setAttribute("name", rel);
		fileElem.appendChild(doc.createTextNode(
			QString::fromLatin1(file.readAll().toBase64())));
		stateElem.appendChild(fileElem);
	}
}




void Lv2Proc::saveState(QDomDocument& doc, QDomElement& that)
{
	if (m_remote)
	{
		saveStateRemote(doc, that);
		return;
	}

	if (!m_instance || !m_plugin || !m_stateDir)
	{
		return;
	}

	Lv2Manager* mgr = Engine::getLv2Manager();
	AutoLilvNode stateIface = mgr->uri(LV2_STATE__interface);
	if (!lilv_plugin_has_extension_data(m_plugin, stateIface.get()))
	{
		return;
	}

	QTemporaryDir saveDir(QDir::tempPath() + "/lmms-lv2save-XXXXXX");
	if (!saveDir.isValid())
	{
		qWarning() << "Could not create temporary LV2 save directory";
		return;
	}

	const QByteArray savePath = saveDir.path().toUtf8();
	const QByteArray scratchPath = m_stateDir ? m_stateDir->path().toUtf8() : QByteArray();
	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		LilvState* state = lilv_state_new_from_instance(
			m_plugin, m_instance, mgr->uridMap().mapFeature(),
			scratchPath.isEmpty() ? nullptr : scratchPath.constData(),
			savePath.constData(), savePath.constData(), savePath.constData(),
			nullptr, nullptr, 0, nullptr);
		if (!state)
		{
			qWarning() << "Could not save LV2 state for"
				<< qStringFromPluginNode(m_plugin, lilv_plugin_get_name);
			return;
		}

		const int ret = lilv_state_save(
			mgr->world(), mgr->uridMap().mapFeature(), mgr->uridMap().unmapFeature(),
			state, nullptr, savePath.constData(), "state.ttl");
		lilv_state_free(state);
		if (ret != 0)
		{
			qWarning() << "Could not write LV2 state for"
				<< qStringFromPluginNode(m_plugin, lilv_plugin_get_name)
				<< "(error" << ret << ")";
			return;
		}
	}

	QDomElement stateElem = doc.createElement("lv2state");
	stateElem.setAttribute("version", 1);
	appendStateFilesToXml(doc, stateElem, saveDir.path());

	if (stateElem.hasChildNodes())
	{
		that.appendChild(stateElem);
	}
}




void Lv2Proc::saveStateRemote(QDomDocument& doc, QDomElement& that)
{
	if (!m_plugin || !m_remote)
	{
		return;
	}

	Lv2Manager* mgr = Engine::getLv2Manager();
	AutoLilvNode stateIface = mgr->uri(LV2_STATE__interface);
	if (!lilv_plugin_has_extension_data(m_plugin, stateIface.get()))
	{
		return;
	}

	QTemporaryDir saveDir(QDir::tempPath() + "/lmms-lv2save-XXXXXX");
	if (!saveDir.isValid())
	{
		qWarning() << "Could not create temporary LV2 save directory";
		return;
	}

	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		if (!m_remote->saveState(saveDir.path()))
		{
			qWarning() << "Could not save the remote LV2 state for"
				<< qStringFromPluginNode(m_plugin, lilv_plugin_get_name);
			return;
		}
	}

	QDomElement stateElem = doc.createElement("lv2state");
	stateElem.setAttribute("version", 1);
	appendStateFilesToXml(doc, stateElem, saveDir.path());
	if (stateElem.hasChildNodes())
	{
		that.appendChild(stateElem);
	}
}




void Lv2Proc::loadState(const QDomElement& that)
{
	if (m_remote)
	{
		loadStateRemote(that);
		return;
	}

	const QDomElement stateElem = that.firstChildElement("lv2state");
	if (stateElem.isNull() || !m_instance || !m_plugin)
	{
		return;
	}

	const int version = stateElem.attribute("version", "0").toInt();
	if (version != 1)
	{
		qWarning() << "Unsupported LV2 state version" << version;
		return;
	}

	auto stateDir = std::make_unique<QTemporaryDir>(QDir::tempPath() + "/lmms-lv2state-XXXXXX");
	if (!stateDir->isValid())
	{
		qWarning() << "Could not create temporary LV2 state directory";
		return;
	}

	if (!writeStateFilesFromXml(stateElem, stateDir->path()))
	{
		qWarning() << "LV2 state has no state.ttl";
		return;
	}

	const QString statePath = QDir(stateDir->path()).filePath("state.ttl");

	Lv2Manager* mgr = Engine::getLv2Manager();
	LilvState* state = lilv_state_new_from_file(
		mgr->world(), mgr->uridMap().mapFeature(), nullptr,
		statePath.toUtf8().constData());
	if (!state)
	{
		qWarning() << "Could not load LV2 state from" << statePath;
		return;
	}

	m_stateDir = std::move(stateDir);
	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		LV2_Feature mapPathFeature{LV2_STATE__mapPath, &m_mapPath};
		LV2_Feature makePathFeature{LV2_STATE__makePath, &m_makePath};
		LV2_Feature freePathFeature{LV2_STATE__freePath, &m_freePath};
		const LV2_Feature* features[] = {
			&mapPathFeature, &makePathFeature, &freePathFeature, nullptr
		};
		lilv_state_restore(state, m_instance, nullptr, nullptr, 0, features);
	}
	lilv_state_free(state);

	copyModelsFromCore();
}




void Lv2Proc::loadStateRemote(const QDomElement& that)
{
	const QDomElement stateElem = that.firstChildElement("lv2state");
	if (stateElem.isNull() || !m_remote || !m_plugin)
	{
		return;
	}

	const int version = stateElem.attribute("version", "0").toInt();
	if (version != 1)
	{
		qWarning() << "Unsupported LV2 state version" << version;
		return;
	}

	auto stateDir = std::make_unique<QTemporaryDir>(QDir::tempPath() + "/lmms-lv2state-XXXXXX");
	if (!stateDir->isValid())
	{
		qWarning() << "Could not create temporary LV2 state directory";
		return;
	}
	if (!writeStateFilesFromXml(stateElem, stateDir->path()))
	{
		qWarning() << "LV2 state has no state.ttl";
		return;
	}

	bool restored = false;
	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		restored = m_remote->restoreState(stateDir->path());
	}
	if (!restored)
	{
		qWarning() << "Could not restore the remote LV2 state for"
			<< qStringFromPluginNode(m_plugin, lilv_plugin_get_name);
		return;
	}

	m_stateDir = std::move(stateDir);
	applyRemoteControlValues();
}




void Lv2Proc::applyRemoteControlValues()
{
	if (!m_remote)
	{
		return;
	}
	const auto& values = m_remote->controlValues();
	for (std::size_t i = 0; i < values.size() && i < m_ports.size(); ++i)
	{
		auto* control = dcast<Lv2Ports::Control>(m_ports[i].get());
		if (control != nullptr && control->m_flow == Lv2Ports::Flow::Input)
		{
			setControlFromUi(static_cast<uint32_t>(i), values[i]);
		}
	}
	// Push the model values again on the next block.
	m_lastRemoteControl.assign(m_ports.size(),
		std::numeric_limits<float>::quiet_NaN());
}




char* Lv2Proc::makePath(LV2_State_Make_Path_Handle handle, const char* path)
{
	auto* self = static_cast<Lv2Proc*>(handle);
	if (!self || !self->m_stateDir || !self->m_stateDir->isValid() || !path || !*path)
	{
		return nullptr;
	}

	const QString full = QDir(self->m_stateDir->path()).filePath(QString::fromUtf8(path));
	if (!QDir().mkpath(QFileInfo(full).absolutePath()))
	{
		return nullptr;
	}
	return duplicateCString(full.toUtf8());
}




void Lv2Proc::freePath(LV2_State_Free_Path_Handle, char* path)
{
	std::free(path);
}




char* Lv2Proc::absolutePath(LV2_State_Map_Path_Handle handle, const char* path)
{
	auto* self = static_cast<Lv2Proc*>(handle);
	if (!self || !self->m_stateDir || !path || !*path)
	{
		return duplicateCString(QByteArray());
	}

	if (QDir::isAbsolutePath(QString::fromUtf8(path)))
	{
		return duplicateCString(QByteArray(path));
	}

	return duplicateCString(
		QDir(self->m_stateDir->path()).filePath(QString::fromUtf8(path)).toUtf8());
}




char* Lv2Proc::abstractPath(LV2_State_Map_Path_Handle handle, const char* path)
{
	auto* self = static_cast<Lv2Proc*>(handle);
	if (!self || !self->m_stateDir || !path || !*path)
	{
		return duplicateCString(QByteArray());
	}

	const QString absPath = QString::fromUtf8(path);
	const QString stateDir = QDir(self->m_stateDir->path()).absolutePath();
	if (absPath.startsWith(stateDir + '/'))
	{
		return duplicateCString(absPath.mid(stateDir.size() + 1).toUtf8());
	}
	return duplicateCString(QByteArray(path));
}




void Lv2Proc::run(f_cnt_t frames)
{
	// State save/restore must not run concurrently with the plugin instance
	std::lock_guard<std::mutex> lock(m_stateMutex);

	if (m_remote)
	{
		runRemote(frames);
		return;
	}

	if (m_worker)
	{
		// Process any worker replies
		m_worker->emitResponses();
	}

	lilv_instance_run(m_instance, static_cast<uint32_t>(frames));

	if (m_latencyPort)
	{
		m_latency.store(
			std::max<f_cnt_t>(0, static_cast<f_cnt_t>(std::lround(m_latencyPort->m_val))),
			std::memory_order_relaxed);
	}

	m_midiOutEvents.clear();
	const uint32_t midiEventType = Engine::getLv2Manager()->
		uridCache()[Lv2UridCache::Id::midi_MidiEvent];

	// Forward atom output events (MIDI, patch messages, meters, ...) to the UI
	for (std::size_t p = 0; p < m_ports.size(); ++p)
	{
		auto* port = m_ports[p].get();
		if (port->m_flow != Lv2Ports::Flow::Output
			|| port->m_type != Lv2Ports::Type::AtomSeq)
		{
			continue;
		}

		auto* atomSeq = dcast<Lv2Ports::AtomSeq>(port);
		if (!atomSeq)
		{
			continue;
		}

		for (auto i = lv2_evbuf_begin(atomSeq->m_buf.get());
			lv2_evbuf_is_valid(i); i = lv2_evbuf_next(i))
		{
			uint32_t frames = 0;
			uint32_t type = 0;
			uint32_t size = 0;
			uint8_t* body = nullptr;
			if (!lv2_evbuf_get(i, &frames, &type, &size, &body))
			{
				continue;
			}
			if (atomSeq == m_midiOut && type == midiEventType)
			{
				collectMidiOutEvent(frames, body, size);
			}
			sendToUi(static_cast<uint32_t>(p), type, size, body);
		}
	}

	if (m_worker)
	{
		// Notify the plugin the run() cycle is finished
		m_worker->notifyPluginThatRunFinished();
	}
}




void Lv2Proc::collectMidiOutEvent(uint32_t offset, const uint8_t* data,
	uint32_t size)
{
	if (!data || size == 0 || m_midiOutEvents.size() >= 1024)
	{
		return;
	}

	const uint8_t status = data[0];
	if (status < 0x80)
	{
		// Running status is not expected in LV2 MIDI atoms.
		return;
	}

	switch (status & 0xF0)
	{
	case 0x80:
	case 0x90:
	case 0xA0:
	case 0xB0:
	case 0xE0:
		if (size < 3) { return; }
		break;
	case 0xC0:
	case 0xD0:
		if (size < 2) { return; }
		break;
	default:
		// SysEx and system messages are not routed yet.
		return;
	}

	MidiOutEvent event;
	event.offset = offset;
	event.size = static_cast<uint8_t>((status & 0xF0) == 0xC0
		|| (status & 0xF0) == 0xD0 ? 2 : 3);
	std::copy(data, data + event.size, event.data.begin());
	m_midiOutEvents.push_back(event);
}




// in case there will be a PR which removes this callback and instead adds a
// `ringbuffer_t<MidiEvent + time info>` to `class Instrument`, this
// function (and the ringbuffer and its reader in `Lv2Proc`) will simply vanish
void Lv2Proc::handleMidiInputEvent(const MidiEvent &event, const TimePos &time, f_cnt_t offset)
{
	if(m_midiIn)
	{
		// ringbuffer allows only one writer at a time
		// however, this function can be called by multiple threads
		// (different RT and non-RT!) at the same time
		// for now, a spinlock looks like the most safe/easy compromise

		// source: https://en.cppreference.com/w/cpp/atomic/atomic_flag
		while (m_ringLock.test_and_set(std::memory_order_acquire))  // acquire lock
			 ; // spin

		MidiInputEvent ev { event, time, offset };
		std::size_t written = m_midiInputBuf.write(&ev, 1);
		if(written != 1)
		{
			qWarning("MIDI ringbuffer is too small! Discarding MIDI event.");
		}

		m_ringLock.clear(std::memory_order_release);
	}
	else
	{
		qWarning() << "Warning: Caught MIDI event for an Lv2 instrument"
					<< "that can not handle MIDI... Ignoring";
	}
}




AutomatableModel *Lv2Proc::modelAtPort(const QString &uri)
{
	const auto itr = m_connectedModels.find(uri.toUtf8().data());
	return itr != m_connectedModels.end() ? itr->second : nullptr;
}




void Lv2Proc::initPlugin()
{
	if (lv2RemoteMode())
	{
		startRemote();
		return;
	}

	m_features.initCommon();
	initPluginSpecificFeatures();
	m_features.createFeatureVectors();

	m_instance = lilv_plugin_instantiate(m_plugin,
		Engine::audioEngine()->outputSampleRate(),
		m_features.featurePointers());

	if (m_instance)
	{
		// Data-access feature for plugin UIs (extension_data of the plugin)
		m_features.m_extData.data_access =
			lilv_instance_get_descriptor(m_instance)->extension_data;

		if (m_worker)
		{
			const auto iface = static_cast<const LV2_Worker_Interface*>(
				lilv_instance_get_extension_data(m_instance, LV2_WORKER__interface));
			if (iface)
			{
				m_worker->setHandle(lilv_instance_get_handle(m_instance));
				m_worker->setInterface(iface);
			}
			else
			{
				qWarning() << "Plugin" << qStringFromPluginNode(m_plugin, lilv_plugin_get_name)
					<< "(URI:"
					<< lilv_node_as_uri(lilv_plugin_get_uri(m_plugin))
					<< ") announces Lv2 Worker extension data in TTL metadata, but does not provide it."
					<< "Will continue without the extension.";
			}
		}
		for (std::size_t portNum = 0; portNum < m_ports.size(); ++portNum)
		{
			connectPort(portNum);
		}
		lilv_instance_activate(m_instance);
	}
	else
	{
		qCritical() << "Failed to create an instance of"
			<< qStringFromPluginNode(m_plugin, lilv_plugin_get_name)
			<< "(URI:"
			<< lilv_node_as_uri(lilv_plugin_get_uri(m_plugin))
			<< ")";
		throw std::runtime_error("Failed to create Lv2 processor");
	}
}




void Lv2Proc::startRemote()
{
	m_remote = std::make_unique<Lv2HostClient>();
	const QString uri = QString::fromUtf8(pluginUri());
	const auto sampleRate = Engine::audioEngine()->outputSampleRate();
	const f_cnt_t frames = Engine::audioEngine()->framesPerPeriod();
	if (!m_remote->start(QString(), uri, sampleRate, frames))
	{
		qCritical() << "Could not start the isolated LV2 host for" << uri
			<< ":" << m_remote->errorString();
		m_remote.reset();
		throw std::runtime_error("Failed to create remote Lv2 processor");
	}
	if (m_remote->ports().size() != m_ports.size())
	{
		qWarning() << "Isolated LV2 host reported"
			<< static_cast<int>(m_remote->ports().size())
			<< "ports, LMMS built" << static_cast<int>(m_ports.size());
	}
	m_lastRemoteControl.assign(m_ports.size(),
		std::numeric_limits<float>::quiet_NaN());
}




void Lv2Proc::runRemote(f_cnt_t frames)
{
	const float* input[2] = { nullptr, nullptr };
	float* output[2] = { nullptr, nullptr };
	if (inPorts().m_left) { input[0] = inPorts().m_left->data(); }
	if (inPorts().m_right) { input[1] = inPorts().m_right->data(); }
	if (outPorts().m_left) { output[0] = outPorts().m_left->data(); }
	if (outPorts().m_right) { output[1] = outPorts().m_right->data(); }

	m_remote->processBlock(input, output, frames);
	m_latency.store(m_remote->latency(), std::memory_order_relaxed);

	m_midiOutEvents.clear();
	for (const auto& event : m_remote->midiOutput())
	{
		if (event.data.empty() || event.data.size() > 3)
		{
			continue;
		}
		MidiOutEvent outEvent;
		outEvent.offset = event.frame;
		outEvent.size = static_cast<uint8_t>(event.data.size());
		std::memcpy(outEvent.data.data(), event.data.data(), event.data.size());
		m_midiOutEvents.push_back(outEvent);
	}
}




void Lv2Proc::copyModelsToRemote()
{
	std::vector<lv2host::MidiEventData> events;
	if (m_midiIn)
	{
		while (m_midiInputReader.read_space() > 0)
		{
			const MidiInputEvent ev = m_midiInputReader.read(1)[0];
			const uint32_t stamp =
				ev.time.frames(Engine::framesPerTick()) + ev.offset;
			std::array<uint8_t, 4> buffer{};
			const std::size_t size = writeToByteSeq(ev.ev, buffer.data(), buffer.size());
			if (size > 0)
			{
				events.push_back(lv2host::MidiEventData{
					stamp,
					std::vector<uint8_t>(buffer.data(), buffer.data() + size)});
			}
		}
	}
	m_remote->setMidiEvents(std::move(events));

	for (std::size_t i = 0; i < m_ports.size(); ++i)
	{
		auto* control = dcast<Lv2Ports::Control>(m_ports[i].get());
		if (control == nullptr || control->m_flow != Lv2Ports::Flow::Input)
		{
			continue;
		}
		const float value = control->m_val;
		if (std::isnan(m_lastRemoteControl[i]) || m_lastRemoteControl[i] != value)
		{
			m_remote->setControlValue(static_cast<int>(i), value);
			m_lastRemoteControl[i] = value;
		}
	}
}




void Lv2Proc::shutdownPlugin()
{
	if (m_remote)
	{
		m_remote->stop();
		m_remote.reset();
	}

	if (m_instance)
	{
		lilv_instance_deactivate(m_instance);
		lilv_instance_free(m_instance);
		m_instance = nullptr;
	}

	m_features.clear();
	m_options.clear();
}




bool Lv2Proc::hasNoteInput() const
{
	return m_midiIn;
	// we could additionally check for
	// http://lv2plug.in/ns/lv2core#InstrumentPlugin
	// however, jalv does not do that, too
	// so, if there's any MIDI input, we just assume we can send notes there
}




void Lv2Proc::initMOptions()
{
	/*
		sampleRate:
		LMMS can in theory inform plugins of a new sample rate.
		However, Lv2 plugins seem to not allow sample rate changes
		(not even through LV2_Options_Interface) - it's assumed to be
		fixed after being passed via LV2_Descriptor::instantiate.
		So, if the sampleRate would change, the plugin will need to
		re-initialize, and this code section will be
		executed again, creating a new option vector.
	*/
	float sampleRate = Engine::audioEngine()->outputSampleRate();
	int32_t blockLength = Engine::audioEngine()->framesPerPeriod();
	int32_t sequenceSize = defaultEvbufSize();

	using Id = Lv2UridCache::Id;
	m_options.initOption<float>(Id::param_sampleRate, sampleRate);
	m_options.initOption<int32_t>(Id::bufsz_maxBlockLength, blockLength);
	m_options.initOption<int32_t>(Id::bufsz_minBlockLength, blockLength);
	m_options.initOption<int32_t>(Id::bufsz_nominalBlockLength, blockLength);
	m_options.initOption<int32_t>(Id::bufsz_sequenceSize, sequenceSize);
	m_options.createOptionVectors();
}




void Lv2Proc::initPluginSpecificFeatures()
{
	// options
	initMOptions();
	m_features[LV2_OPTIONS__options] = const_cast<LV2_Options_Option*>(m_options.feature());

	Lv2Manager* mgr = Engine::getLv2Manager();

	// state: some plugins create files at instantiate time (state:makePath).
	// Keep those files for the lifetime of this instance.
	AutoLilvNode stateIface = mgr->uri(LV2_STATE__interface);
	AutoLilvNode makePath = mgr->uri(LV2_STATE__makePath);
	if (lilv_plugin_has_extension_data(m_plugin, stateIface.get())
		|| lilv_plugin_has_feature(m_plugin, makePath.get()))
	{
		m_stateDir = std::make_unique<QTemporaryDir>(QDir::tempPath() + "/lmms-lv2state-XXXXXX");
		m_features[LV2_STATE__makePath] = &m_makePath;
		m_features[LV2_STATE__freePath] = &m_freePath;
		if (!m_stateDir->isValid())
		{
			qWarning() << "Could not create temporary LV2 state directory;"
				<< "plugins requiring state:makePath at instantiate time may fail";
			m_stateDir.reset();
		}
	}

	// worker (if plugin has worker extension)
	if (lilv_plugin_has_extension_data(m_plugin, mgr->uri(LV2_WORKER__interface).get()))
	{
		bool threaded = !Engine::audioEngine()->renderOnly();
		m_worker.emplace(&m_workLock, threaded);
		m_features[LV2_WORKER__schedule] = m_worker->feature();
		// note: the worker interface can not be instantiated yet - it requires m_instance. see initPlugin()
	}
}




void Lv2Proc::loadFileInternal(const QString &file)
{
	(void)file;
}




void Lv2Proc::createPort(std::size_t portNum)
{
	Lv2Ports::Meta meta;
	meta.get(m_plugin, portNum);

	const LilvPort* lilvPort = lilv_plugin_get_port_by_index(m_plugin,
								static_cast<uint32_t>(portNum));
	Lv2Ports::PortBase* port;

	switch (meta.m_type)
	{
		case Lv2Ports::Type::Control:
		{
			auto ctrl = new Lv2Ports::Control;
			if (meta.m_flow == Lv2Ports::Flow::Input)
			{
				AutoLilvNode node(lilv_port_get_name(m_plugin, lilvPort));
				QString dispName = lilv_node_as_string(node.get());
				sample_rate_t sr = Engine::audioEngine()->outputSampleRate();
				if(meta.def() < meta.min(sr) || meta.def() > meta.max(sr))
				{
					qWarning()	<< "Warning: Plugin"
								<< qStringFromPluginNode(m_plugin, lilv_plugin_get_name)
								<< "(URI:"
								<< lilv_node_as_uri(lilv_plugin_get_uri(m_plugin))
								<< ") has a default value for port"
								<< dispName
								<< "which is not in range [min, max].";
				}
				switch (meta.m_vis)
				{
					case Lv2Ports::Vis::Generic:
					{
						// allow ~1000 steps
						float stepSize = (meta.max(sr) - meta.min(sr)) / 1000.0f;

						// make multiples of 0.01 (or 0.1 for larger values)
						float minStep = (stepSize >= 1.0f) ? 0.1f : 0.01f;
						stepSize -= std::fmod(stepSize, minStep);
						stepSize = std::max(stepSize, minStep);

						ctrl->m_connectedModel.reset(
							new FloatModel(meta.def(), meta.min(sr), meta.max(sr),
											stepSize, nullptr, dispName));
						break;
					}
					case Lv2Ports::Vis::Integer:
						ctrl->m_connectedModel.reset(
							new IntModel(static_cast<int>(meta.def()),
											static_cast<int>(meta.min(sr)),
											static_cast<int>(meta.max(sr)),
											nullptr, dispName));
						break;
					case Lv2Ports::Vis::Enumeration:
					{
						std::map<float, const char*> scalePointMap;
						{
							AutoLilvScalePoints sps (static_cast<LilvScalePoints*>(lilv_port_get_scale_points(m_plugin, lilvPort)));
							// temporary map, since lilv may return scale points in random order
							LILV_FOREACH(scale_points, i, sps.get())
							{
								const LilvScalePoint* sp = lilv_scale_points_get(sps.get(), i);
								const float f = lilv_node_as_float(lilv_scale_point_get_value(sp));
								const char* s = lilv_node_as_string(lilv_scale_point_get_label(sp));
								scalePointMap[f] = s;
							}
						}

						// Some plugins mark integer controls as lv2:enumeration
						// but do not provide scale points (e.g. sfizz). A combo
						// box without entries would index an empty map when the
						// parameter values are copied to the plugin.
						if (scalePointMap.empty())
						{
							const float range = meta.max(sr) - meta.min(sr);
							const bool integralValues =
								meta.def() == std::floor(meta.def())
								&& meta.min(sr) == std::floor(meta.min(sr))
								&& meta.max(sr) == std::floor(meta.max(sr));
							const float stepSize = integralValues
								? 1.0f : std::max(range / 1000.0f, 0.01f);
							ctrl->m_connectedModel.reset(
								new FloatModel(meta.def(), meta.min(sr),
									meta.max(sr), stepSize, nullptr, dispName));
							break;
						}

						ComboBoxModel* comboModel = new ComboBoxModel(nullptr, dispName);
						for (const auto& [f,s] : scalePointMap)
						{
							ctrl->m_scalePointMap.push_back(f);
							comboModel->addItem(s);
						}
						for(std::size_t i = 0; i < ctrl->m_scalePointMap.size(); ++i)
						{
							if(meta.def() == ctrl->m_scalePointMap[i])
							{
								comboModel->setValue(i);
								comboModel->setInitValue(i);
								break;
							}
						}
						ctrl->m_connectedModel.reset(comboModel);
						break;
					}
					case Lv2Ports::Vis::Toggled:
						ctrl->m_connectedModel.reset(
							new BoolModel(static_cast<bool>(meta.def()),
											nullptr, dispName));
						break;
				}
				if(meta.m_logarithmic)
				{
					ctrl->m_connectedModel->setScaleLogarithmic();
				}

			} // if m_flow == Input
			port = ctrl;
			break;
		}
		case Lv2Ports::Type::Audio:
		{
			auto audio = new Lv2Ports::Audio(static_cast<std::size_t>(Engine::audioEngine()->framesPerPeriod()),
				portIsSideChain(m_plugin, lilvPort));
			port = audio;
			break;
		}
		case Lv2Ports::Type::AtomSeq:
		{
			auto atomPort = new Lv2Ports::AtomSeq;

			{
				AutoLilvNode uriAtomSupports(Engine::getLv2Manager()->uri(LV2_ATOM__supports));
				AutoLilvNodes atomSupports(lilv_port_get_value(m_plugin, lilvPort, uriAtomSupports.get()));
				AutoLilvNode uriMidiEvent(Engine::getLv2Manager()->uri(LV2_MIDI__MidiEvent));

				LILV_FOREACH (nodes, itr, atomSupports.get())
				{
					if(lilv_node_equals(lilv_nodes_get(atomSupports.get(), itr), uriMidiEvent.get()))
					{
						atomPort->flags |= Lv2Ports::AtomSeq::FlagType::Midi;
					}
				}
			}

			int minimumSize = defaultEvbufSize();

			Lv2Manager* mgr = Engine::getLv2Manager();

			// check for alternative minimum size
			{
				AutoLilvNode rszMinimumSize = mgr->uri(LV2_RESIZE_PORT__minimumSize);
				AutoLilvNodes minSizeV(lilv_port_get_value(m_plugin, lilvPort, rszMinimumSize.get()));
				LilvNode* minSize = minSizeV ? lilv_nodes_get_first(minSizeV.get()) : nullptr;
				if (minSize && lilv_node_is_int(minSize))
				{
					minimumSize = std::max(minimumSize, lilv_node_as_int(minSize));
				}
			}

			atomPort->m_buf.reset(
				lv2_evbuf_new(static_cast<uint32_t>(minimumSize),
								mgr->uridMap().map(LV2_ATOM__Chunk),
								mgr->uridMap().map(LV2_ATOM__Sequence)));

			port = atomPort;
			break;
		}
		default:
			port = new Lv2Ports::Unknown;
	}

	// `meta` is of class `Lv2Ports::Meta` and `port` is of a child class
	// we can now assign the `Lv2Ports::Meta` part of meta to ports, leaving
	// the additional members of `port` unchanged
	*static_cast<Lv2Ports::Meta*>(port) = meta;
	port->m_port = lilvPort;
	port->m_plugin = m_plugin;

	m_ports[portNum].reset(port);
}




void Lv2Proc::createPorts()
{
	// register ports at the processor after creation,
	// i.e. link their data or count them
	struct RegisterPort : public Lv2Ports::Visitor
	{
		Lv2Proc* m_proc;

		void visit(Lv2Ports::Control& ctrl) override
		{
			if (ctrl.m_flow == Lv2Ports::Flow::Input)
			{
				AutomatableModel* amo = ctrl.m_connectedModel.get();
				m_proc->m_connectedModels.emplace(
					lilv_node_as_string(lilv_port_get_symbol(
										m_proc->m_plugin, ctrl.m_port)),
					amo);
				m_proc->addModel(amo, ctrl.uri());
			}
			else if (ctrl.m_flow == Lv2Ports::Flow::Output)
			{
				AutoLilvNode designation = Engine::getLv2Manager()->
					uri(LV2_CORE__designation);
				AutoLilvNode latency = Engine::getLv2Manager()->
					uri(LV2_CORE__latency);
				AutoLilvNodes values(lilv_port_get_value(
					m_proc->m_plugin, ctrl.m_port, designation.get()));
				if (values && lilv_nodes_contains(values.get(), latency.get()))
				{
					m_proc->m_latencyPort = &ctrl;
				}
			}
		}

		void visit(Lv2Ports::Audio& audio) override
		{
			// Optional audio ports still have to be usable: some instruments
			// (e.g. OB-Xd) mark their only outputs as connectionOptional.
			// Sidechain ports are intentionally not routed here.
			if (!audio.isSideChain())
			{
				StereoPortRef dummy;
				StereoPortRef* portRef = &dummy;
				switch (audio.m_flow)
				{
					case Lv2Ports::Flow::Input:
						portRef = &m_proc->m_inPorts;
						break;
					case Lv2Ports::Flow::Output:
						portRef = &m_proc->m_outPorts;
						break;
					case Lv2Ports::Flow::Unknown:
						break;
				}
				// in Lv2, leftPort is defined to be the first port
				if (!portRef->m_left) { portRef->m_left = &audio; }
				else if (!portRef->m_right) { portRef->m_right = &audio; }
			}
		}

		void visit(Lv2Ports::AtomSeq& atomPort) override
		{
			if(atomPort.m_flow == Lv2Ports::Flow::Input)
			{
				if(atomPort.flags & Lv2Ports::AtomSeq::FlagType::Midi)
				{
					// take any MIDI input, prefer mandatory MIDI input
					// (Lv2Proc::check() assures there are <=1 mandatory MIDI
					// input ports)
					if(!m_proc->m_midiIn || !atomPort.m_optional)
						m_proc->m_midiIn = &atomPort;
				}
			}
			else if(atomPort.m_flow == Lv2Ports::Flow::Output)
			{
				if(atomPort.flags & Lv2Ports::AtomSeq::FlagType::Midi)
				{
					// take any MIDI output, prefer mandatory MIDI output
					// (Lv2Proc::check() assures there are <=1 mandatory MIDI
					// output ports)
					if(!m_proc->m_midiOut || !atomPort.m_optional)
						m_proc->m_midiOut = &atomPort;
				}
			}
			else { Q_ASSERT(false); }
		}
	};

	std::size_t maxPorts = lilv_plugin_get_num_ports(m_plugin);
	m_ports.resize(maxPorts);

	for (std::size_t portNum = 0; portNum < maxPorts; ++portNum)
	{
		createPort(portNum);
		RegisterPort registerPort;
		registerPort.m_proc = this;
		m_ports[portNum]->accept(registerPort);
	}

	// initially assign model values to port values
	copyModelsFromCore();

	// debugging:
	//dumpPorts();
}




struct ConnectPortVisitor : public Lv2Ports::Visitor
{
	std::size_t m_num;
	LilvInstance* m_instance;
	void connectPort(void* location)
	{
		lilv_instance_connect_port(m_instance,
			static_cast<uint32_t>(m_num), location);
	}
	void visit(Lv2Ports::AtomSeq& atomSeq) override
	{
		connectPort(lv2_evbuf_get_buffer(atomSeq.m_buf.get()));
	}
	void visit(Lv2Ports::Control& ctrl) override { connectPort(&ctrl.m_val); }
	void visit(Lv2Ports::Audio& audio) override
	{
		connectPort((audio.mustBeUsed()) ? audio.m_buffer.data() : nullptr);
	}
	void visit(Lv2Ports::Unknown&) override { connectPort(nullptr); }
	~ConnectPortVisitor() override = default;
};

// !This function must be realtime safe!
// use createPort to create any port before connecting
void Lv2Proc::connectPort(std::size_t num)
{
	ConnectPortVisitor connect;
	connect.m_num = num;
	connect.m_instance = m_instance;
	m_ports[num]->accept(connect);
}




void Lv2Proc::dumpPort(std::size_t num)
{
	struct DumpPortDetail : public Lv2Ports::ConstVisitor
	{
		void visit(const Lv2Ports::Control& ctrl) override
		{
			qDebug() << "  control port";
			// output ports may be uninitialized yet, only print inputs
			if (ctrl.m_flow == Lv2Ports::Flow::Input)
			{
				qDebug() << "    value:" << ctrl.m_val;
			}
		}
		void visit(const Lv2Ports::Audio& audio) override
		{
			qDebug() << (audio.isSideChain()	? "  audio port (sidechain)"
												: "  audio port");
			qDebug() << "    buffer size:" << audio.bufferSize();
		}
	};

	const Lv2Ports::PortBase& port = *m_ports[num];
	qDebug().nospace() << "port " << num << ":";
	qDebug() << "  name:"
		<< qStringFromPortName(m_plugin, port.m_port);
	qDebug() << "  flow: " << Lv2Ports::toStr(port.m_flow);
	qDebug() << "  type: " << Lv2Ports::toStr(port.m_type);
	qDebug() << "  visualization: " << Lv2Ports::toStr(port.m_vis);
	if (port.m_type == Lv2Ports::Type::Control || port.m_type == Lv2Ports::Type::Cv)
	{
		sample_rate_t sr = Engine::audioEngine()->outputSampleRate();
		qDebug() << "  default:" << port.def();
		qDebug() << "  min:" << port.min(sr);
		qDebug() << "  max:" << port.max(sr);
	}
	qDebug() << "  optional: " << port.m_optional;
	qDebug() << "  => USED: " << port.m_used;

	DumpPortDetail dumper;
	port.accept(dumper);
}




bool Lv2Proc::portIsSideChain(const LilvPlugin *plugin, const LilvPort *port)
{
	return	lilv_port_has_property(plugin, port,
									uri(LV2_CORE_PREFIX "isSidechain").get());
}




bool Lv2Proc::portIsOptional(const LilvPlugin *plugin, const LilvPort *port)
{
	return	lilv_port_has_property(plugin, port,
									uri(LV2_CORE__connectionOptional).get());
}




AutoLilvNode Lv2Proc::uri(const char *uriStr)
{
	return Engine::getLv2Manager()->uri(uriStr);
}


} // namespace lmms

#endif // LMMS_HAVE_LV2
