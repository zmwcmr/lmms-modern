/*
 * Mixer.cpp - effect mixer for LMMS
 *
 * Copyright (c) 2008-2011 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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

#include "Mixer.h"

#include <algorithm>

#include <QDebug>
#include <QDomElement>

#include "AudioBusHandle.h"
#include "AudioEngine.h"
#include "AudioEngineWorkerThread.h"
#include "Mixer.h"
#include "Song.h"

#include "InstrumentTrack.h"
#include "MixHelpers.h"
#include "PatternStore.h"
#include "SampleTrack.h"
#include "TrackContainer.h" // For TrackContainer::TrackList typedef

#include <algorithm>

namespace lmms
{

namespace
{

// Upper bound for channels loaded from project files. This is deliberately
// generous for normal use while preventing malformed files from exhausting
// memory with an arbitrarily large mixer channel index.
constexpr int MaxMixerChannels = 1024;

} // namespace


MixerRoute::MixerRoute( MixerChannel * from, MixerChannel * to, float amount ) :
	m_from( from ),
	m_to( to ),
	m_amount(amount, 0, 1, 0.001f, nullptr,
			tr("Amount to send from channel %1 to channel %2").arg(m_from->index()).arg(m_to->index())),
	m_delayedBuffer(Engine::audioEngine()->framesPerPeriod())
{
	m_delayedBuffer.allocateInterleavedBuffer();
}


void MixerRoute::updateName()
{
	m_amount.setDisplayName(
			tr("Amount to send from channel %1 to channel %2").arg(m_from->index()).arg(m_to->index()));
}


MixerChannel::MixerChannel( int idx, Model * _parent ) :
	m_fxChain( nullptr ),
	m_stillRunning( false ),
	m_peakLeft( 0.0f ),
	m_peakRight( 0.0f ),
	m_buffer(Engine::audioEngine()->framesPerPeriod()),
	m_secondaryOutputBuffer(Engine::audioEngine()->framesPerPeriod()),
	m_muteBeforeSolo( false ),
	m_muteModel( false, _parent ),
	m_soloModel( false, _parent ),
	m_volumeModel(1.f, 0.f, 2.f, 0.001f, _parent),
	m_name(),
	m_lock(),
	m_queued( false ),
	m_muted( false ),
	m_dependenciesMet(0),
	m_channelIndex(idx)
{
	m_buffer.allocateInterleavedBuffer();
}




MixerChannel::~MixerChannel()
{
}




void MixerChannel::setSidechainSource(MixerChannel* source)
{
	if (m_sidechainSource == source)
	{
		return;
	}
	if (m_sidechainSource != nullptr)
	{
		auto& receivers = m_sidechainSource->m_sidechainReceivers;
		receivers.erase(std::remove(receivers.begin(), receivers.end(), this),
			receivers.end());
	}
	m_sidechainSource = source;
	if (source != nullptr)
	{
		auto& receivers = source->m_sidechainReceivers;
		if (std::find(receivers.begin(), receivers.end(), this) == receivers.end())
		{
			receivers.push_back(this);
		}
	}
}





void MixerChannel::setMultiOutputTarget(MixerChannel* target)
{
	if (m_multiOutputTarget == target)
	{
		return;
	}
	if (m_multiOutputTarget != nullptr)
	{
		auto& sources = m_multiOutputTarget->m_multiOutputSources;
		sources.erase(std::remove(sources.begin(), sources.end(), this),
			sources.end());
		auto& receivers = m_multiOutputReceivers;
		receivers.erase(std::remove(receivers.begin(), receivers.end(),
			m_multiOutputTarget), receivers.end());
	}
	m_multiOutputTarget = target;
	if (target != nullptr)
	{
		auto& sources = target->m_multiOutputSources;
		if (std::find(sources.begin(), sources.end(), this) == sources.end())
		{
			sources.push_back(this);
		}
		auto& receivers = m_multiOutputReceivers;
		if (std::find(receivers.begin(), receivers.end(), target) == receivers.end())
		{
			receivers.push_back(target);
		}
	}
}


inline void MixerChannel::processed()
{
	for( const MixerRoute * receiverRoute : m_sends )
	{
		if( receiverRoute->receiver()->m_muted == false )
		{
			receiverRoute->receiver()->incrementDeps();
		}
	}
	for (MixerChannel* receiver : m_sidechainReceivers)
	{
		if (!receiver->m_muted)
		{
			receiver->incrementDeps();
		}
	}
	for (MixerChannel* receiver : m_multiOutputReceivers)
	{
		if (!receiver->m_muted)
		{
			receiver->incrementDeps();
		}
	}
}

void MixerChannel::incrementDeps()
{
	const auto i = m_dependenciesMet++ + 1;
	if( i >= dependencyCount() && ! m_queued )
	{
		m_queued = true;
		AudioEngineWorkerThread::addJob( this );
	}
}

void MixerChannel::unmuteForSolo()
{
	m_muteModel.setValue(false);

	// if channel is not master, unmute also every channel it sends to/receives from
	if (!isMaster())
	{
		for (const MixerRoute* sendsRoute : m_sends)
		{
			sendsRoute->receiver()->unmuteSenderForSolo();
		}

		for (const MixerRoute* receiverRoute : m_receives)
		{
			receiverRoute->sender()->unmuteReceiverForSolo();
		}
	}
}

void MixerChannel::unmuteSenderForSolo()
{
	m_muteModel.setValue(false);

	// if channel is not master, unmute every channel it sends to
	if (!isMaster())
	{
		for (const MixerRoute* sendsRoute : m_sends)
		{
			sendsRoute->receiver()->unmuteSenderForSolo();
		}
	}
}


void MixerChannel::unmuteReceiverForSolo()
{
	m_muteModel.setValue(false);

	// if channel is not master, unmute every channel it receives from, and of those, unmute the channels they send to
	if (!isMaster())
	{
		for (const MixerRoute* receiverRoute : m_receives)
		{
			receiverRoute->sender()->unmuteReceiverForSolo();
		}

		for (const MixerRoute* sendsRoute : m_sends)
		{
			sendsRoute->receiver()->unmuteSenderForSolo();
		}
	}
}



void MixerChannel::doProcessing()
{
	const f_cnt_t fpp = Engine::audioEngine()->framesPerPeriod();

	if( m_muted == false )
	{
		for( MixerRoute * senderRoute : m_receives )
		{
			MixerChannel * sender = senderRoute->sender();
			FloatModel * sendModel = senderRoute->amount();
			if( ! sendModel ) qFatal( "Error: no send model found from %d to %d", senderRoute->senderIndex(), m_channelIndex );

			const AudioBuffer* senderBuffer = &sender->m_buffer;
			if (senderRoute->delayFrames() > 0)
			{
				auto src = sender->m_buffer.groupBuffers(0);
				auto dst = senderRoute->delayedBuffer().groupBuffers(0);
				const auto channels = std::min(
					src.channels(), dst.channels());
				for (ch_cnt_t ch = 0; ch < channels; ++ch)
				{
					std::copy_n(src.bufferPtr(ch), fpp, dst.bufferPtr(ch));
				}
				senderRoute->delayLine().process(dst, fpp);
				toInterleaved(dst,
					senderRoute->delayedBuffer().interleavedBuffer());
				senderRoute->delayedBuffer().updateAllSilenceFlags();
				senderBuffer = &senderRoute->delayedBuffer();
			}

			if (senderBuffer->hasAnySignal() || sender->m_stillRunning)
			{
				auto buffer = m_buffer.interleavedBuffer().asSampleFrames();

				// figure out if we're getting sample-exact input
				ValueBuffer * sendBuf = sendModel->valueBuffer();
				ValueBuffer * volBuf = sender->m_volumeModel.valueBuffer();

				// mix it's output with this one's output
				auto ch_buf = senderBuffer->interleavedBuffer().asSampleFrames();

				// use sample-exact mixing if sample-exact values are available
				if( ! volBuf && ! sendBuf ) // neither volume nor send has sample-exact data...
				{
					const float v = sender->m_volumeModel.value() * sendModel->value();
					MixHelpers::addMultiplied(buffer.data(), ch_buf.data(), v, fpp);
				}
				else if( volBuf && sendBuf ) // both volume and send have sample-exact data
				{
					MixHelpers::addMultipliedByBuffers(buffer.data(), ch_buf.data(), volBuf, sendBuf, fpp);
				}
				else if( volBuf ) // volume has sample-exact data but send does not
				{
					const float v = sendModel->value();
					MixHelpers::addMultipliedByBuffer(buffer.data(), ch_buf.data(), v, volBuf, fpp);
				}
				else // vice versa
				{
					const float v = sender->m_volumeModel.value();
					MixHelpers::addMultipliedByBuffer(buffer.data(), ch_buf.data(), v, sendBuf, fpp);
				}
				toPlanar(m_buffer.interleavedBuffer(), m_buffer.groupBuffers(0));
				m_buffer.mixSilenceFlags(*senderBuffer);
			}
		}


		m_secondaryOutputBuffer.silenceAllChannels();
		if (!m_multiOutputSources.empty())
		{
			auto dst = m_buffer.groupBuffers(0);
			for (MixerChannel* source : m_multiOutputSources)
			{
				auto src = source->m_secondaryOutputBuffer.groupBuffers(0);
				const auto channels = std::min(dst.channels(), src.channels());
				for (ch_cnt_t channel = 0; channel < channels; ++channel)
				{
					auto dstBuffer = dst.buffer(channel);
					auto srcBuffer = src.buffer(channel);
					for (f_cnt_t frame = 0; frame < fpp; ++frame)
					{
						dstBuffer[frame] += srcBuffer[frame];
					}
				}
			}
			m_buffer.updateAllSilenceFlags();
		}

		const float v = m_volumeModel.value();

		m_fxChain.setSidechainBuffer(
			m_sidechainSource != nullptr ? &m_sidechainSource->m_buffer : nullptr);
		m_fxChain.setSecondaryOutputBuffer(&m_secondaryOutputBuffer);
		m_stillRunning = m_fxChain.processAudioBuffer(m_buffer);

		const auto peakSamples = SampleFrame{m_buffer.absPeakValue(0), m_buffer.absPeakValue(1)};
		m_peakLeft = std::max(m_peakLeft, peakSamples[0] * v);
		m_peakRight = std::max(m_peakRight, peakSamples[1] * v);
	}
	else
	{
		m_peakLeft = m_peakRight = 0.0f;
	}

	// increment dependency counter of all receivers
	processed();
}



Mixer::Mixer() :
	Model( nullptr ),
	JournallingObject(),
	m_mixerChannels(),
	m_lastSoloed(-1)
{
	// create master channel
	createChannel();
}



Mixer::~Mixer()
{
	while (!m_mixerRoutes.empty())
	{
		deleteChannelSend(m_mixerRoutes.front());
	}
	while( m_mixerChannels.size() )
	{
		MixerChannel * f = m_mixerChannels[m_mixerChannels.size() - 1];
		m_mixerChannels.pop_back();
		delete f;
	}
}



int Mixer::createChannel()
{
	const auto guard = Engine::audioEngine()->requestChangesGuard();

	const int index = m_mixerChannels.size();
	// create new channel
	m_mixerChannels.push_back( new MixerChannel( index, this ) );
	m_latencyByChannel.push_back( 0 );
	m_outputLatencyByChannel.push_back( 0 );

	// reset channel state
	clearChannel( index );

	// if there is a soloed channel, mute the new track
	if (m_lastSoloed != -1 && m_mixerChannels[m_lastSoloed]->m_soloModel.value())
	{
		m_mixerChannels[index]->m_muteBeforeSolo = m_mixerChannels[index]->m_muteModel.value();
		m_mixerChannels[index]->m_muteModel.setValue(true);
	}

	return index;
}

void Mixer::activateSolo()
{
	for (auto i = std::size_t{1}; i < m_mixerChannels.size(); ++i)
	{
		m_mixerChannels[i]->m_muteBeforeSolo = m_mixerChannels[i]->m_muteModel.value();
		m_mixerChannels[i]->m_muteModel.setValue( true );
	}
}

void Mixer::deactivateSolo()
{
	for (auto i = std::size_t{1}; i < m_mixerChannels.size(); ++i)
	{
		m_mixerChannels[i]->m_muteModel.setValue( m_mixerChannels[i]->m_muteBeforeSolo );
	}
}

bool Mixer::setChannelSidechain(int channel, int source)
{
	if (channel < 0 || static_cast<std::size_t>(channel) >= m_mixerChannels.size())
	{
		return false;
	}
	if (source < -1 || (source >= 0
		&& static_cast<std::size_t>(source) >= m_mixerChannels.size()))
	{
		return false;
	}
	if (source == channel || source == 0)
	{
		// Self-routing and the master channel as a source are not useful.
		return false;
	}

	MixerChannel* target = m_mixerChannels[channel];
	MixerChannel* sourceChannel = source >= 0 ? m_mixerChannels[source] : nullptr;

	// Reject cycles through the sidechain graph.
	for (MixerChannel* current = sourceChannel; current != nullptr;
		current = current->sidechainSource())
	{
		if (current == target)
		{
			return false;
		}
	}

	Engine::audioEngine()->requestChangeInModel();
	target->setSidechainSource(sourceChannel);
	Engine::audioEngine()->doneChangeInModel();
	return true;
}




int Mixer::channelSidechain(int channel) const
{
	if (channel < 0 || static_cast<std::size_t>(channel) >= m_mixerChannels.size())
	{
		return -1;
	}
	return m_mixerChannels[channel]->sidechainSourceIndex();
}





bool Mixer::setChannelMultiOutput(int channel, int target)
{
	if (channel <= 0 || static_cast<std::size_t>(channel) >= m_mixerChannels.size())
	{
		return false;
	}
	if (target < -1 || (target >= 0
		&& static_cast<std::size_t>(target) >= m_mixerChannels.size()))
	{
		return false;
	}
	if (target == channel)
	{
		return false;
	}

	MixerChannel* source = m_mixerChannels[channel];
	MixerChannel* targetChannel = target >= 0 ? m_mixerChannels[target] : nullptr;

	// Reject cycles through the multi-output graph.
	for (MixerChannel* current = targetChannel; current != nullptr;
		current = current->multiOutputTarget())
	{
		if (current == source)
		{
			return false;
		}
	}

	Engine::audioEngine()->requestChangeInModel();
	source->setMultiOutputTarget(targetChannel);
	Engine::audioEngine()->doneChangeInModel();
	return true;
}




int Mixer::channelMultiOutput(int channel) const
{
	if (channel < 0 || static_cast<std::size_t>(channel) >= m_mixerChannels.size())
	{
		return -1;
	}
	return m_mixerChannels[channel]->multiOutputTargetIndex();
}




void Mixer::toggledSolo()
{
	int soloedChan = -1;
	bool resetSolo = m_lastSoloed != -1;
	//untoggle if lastsoloed is entered
	if (resetSolo)
	{
		m_mixerChannels[m_lastSoloed]->m_soloModel.setValue( false );
	}
	//determine the soloed channel
	for (auto i = std::size_t{0}; i < m_mixerChannels.size(); ++i)
	{
		if (m_mixerChannels[i]->m_soloModel.value() == true)
			soloedChan = i;
	}
	// if no channel is soloed, unmute everything, else mute everything
	if (soloedChan != -1)
	{
		if (resetSolo)
		{
			deactivateSolo();
			activateSolo();
		} else {
			activateSolo();
		}
		// unmute the soloed chan and every channel it sends to/receives from
		m_mixerChannels[soloedChan]->unmuteForSolo();
	} else {
		deactivateSolo();
	}
	m_lastSoloed = soloedChan;
}



void Mixer::deleteChannel( int index )
{
	// channel deletion is performed between mixer rounds
	Engine::audioEngine()->requestChangeInModel();

	// go through every instrument and adjust for the channel index change
	TrackContainer::TrackList tracks;

	auto& songTracks = Engine::getSong()->tracks();
	auto& patternStoreTracks = Engine::patternStore()->tracks();
	tracks.insert(tracks.end(), songTracks.begin(), songTracks.end());
	tracks.insert(tracks.end(), patternStoreTracks.begin(), patternStoreTracks.end());

	for( Track* t : tracks )
	{
		if( t->type() == Track::Type::Instrument )
		{
			auto inst = dynamic_cast<InstrumentTrack*>(t);
			int val = inst->mixerChannelModel()->value(0);
			if( val == index )
			{
				// we are deleting this track's channel send
				// send to master
				inst->mixerChannelModel()->setValue(0);
			}
			else if( val > index )
			{
				// subtract 1 to make up for the missing channel
				inst->mixerChannelModel()->setValue(val-1);
			}
		}
		else if( t->type() == Track::Type::Sample )
		{
			auto strk = dynamic_cast<SampleTrack*>(t);
			int val = strk->mixerChannelModel()->value(0);
			if( val == index )
			{
				// we are deleting this track's channel send
				// send to master
				strk->mixerChannelModel()->setValue(0);
			}
			else if( val > index )
			{
				// subtract 1 to make up for the missing channel
				strk->mixerChannelModel()->setValue(val-1);
			}
		}
	}

	MixerChannel * ch = m_mixerChannels[index];

	// Remove sidechain/multi-output edges that reference the deleted channel.
	ch->setSidechainSource(nullptr);
	ch->setMultiOutputTarget(nullptr);
	for (MixerChannel* other : m_mixerChannels)
	{
		if (other != ch && other->sidechainSource() == ch)
		{
			other->setSidechainSource(nullptr);
		}
		if (other != ch && other->multiOutputTarget() == ch)
		{
			other->setMultiOutputTarget(nullptr);
		}
	}

	// delete all of this channel's sends and receives
	while (!ch->m_sends.empty())
	{
		deleteChannelSend(ch->m_sends.front());
	}
	while (!ch->m_receives.empty())
	{
		deleteChannelSend(ch->m_receives.front());
	}

	// if m_lastSoloed was our index, reset it
	if (m_lastSoloed == index) { m_lastSoloed = -1; }
	// if m_lastSoloed is > delete index, it will move left
	else if (m_lastSoloed > index) { --m_lastSoloed; }

	// actually delete the channel
	m_mixerChannels.erase(m_mixerChannels.begin() + index);
	m_latencyByChannel.erase(m_latencyByChannel.begin() + index);
	m_outputLatencyByChannel.erase(m_outputLatencyByChannel.begin() + index);
	delete ch;

	for (auto i = static_cast<std::size_t>(index); i < m_mixerChannels.size(); ++i)
	{
		validateChannelName( i, i + 1 );

		// set correct channel index
		m_mixerChannels[i]->setIndex(i);

		// now check all routes and update names of the send models
		for( MixerRoute * r : m_mixerChannels[i]->m_sends )
		{
			r->updateName();
		}
		for( MixerRoute * r : m_mixerChannels[i]->m_receives )
		{
			r->updateName();
		}
	}

	Engine::audioEngine()->doneChangeInModel();
}



void Mixer::moveChannelLeft( int index )
{
	// can't move master or first channel
	if (index <= 1 || static_cast<std::size_t>(index) >= m_mixerChannels.size())
	{
		return;
	}

	const auto guard = Engine::audioEngine()->requestChangesGuard();

	// channels to swap
	int a = index - 1, b = index;

	// check if m_lastSoloed is one of our swaps
	if (m_lastSoloed == a) { m_lastSoloed = b; }
	else if (m_lastSoloed == b) { m_lastSoloed = a; }

	// go through every instrument and adjust for the channel index change
	const TrackContainer::TrackList& songTrackList = Engine::getSong()->tracks();
	const TrackContainer::TrackList& patternTrackList = Engine::patternStore()->tracks();

	for (const auto& trackList : {songTrackList, patternTrackList})
	{
		for (const auto& track : trackList)
		{
			if (track->type() == Track::Type::Instrument)
			{
				auto inst = (InstrumentTrack*)track;
				int val = inst->mixerChannelModel()->value(0);
				if( val == a )
				{
					inst->mixerChannelModel()->setValue(b);
				}
				else if( val == b )
				{
					inst->mixerChannelModel()->setValue(a);
				}
			}
			else if (track->type() == Track::Type::Sample)
			{
				auto strk = (SampleTrack*)track;
				int val = strk->mixerChannelModel()->value(0);
				if( val == a )
				{
					strk->mixerChannelModel()->setValue(b);
				}
				else if( val == b )
				{
					strk->mixerChannelModel()->setValue(a);
				}
			}
		}
	}

	// Swap positions in array
	qSwap(m_mixerChannels[index], m_mixerChannels[index - 1]);

	// Update m_channelIndex of both channels
	m_mixerChannels[index]->setIndex(index);
	m_mixerChannels[index - 1]->setIndex(index - 1);
}



void Mixer::moveChannelRight( int index )
{
	moveChannelLeft( index + 1 );
}



MixerRoute * Mixer::createChannelSend( mix_ch_t fromChannel, mix_ch_t toChannel,
								float amount )
{
//	qDebug( "requested: %d to %d", fromChannel, toChannel );
	// find the existing connection
	MixerChannel * from = m_mixerChannels[fromChannel];
	MixerChannel * to = m_mixerChannels[toChannel];

	for (const auto& send : from->m_sends)
	{
		if (send->receiver() == to)
		{
			// simply adjust the amount
			send->amount()->setValue(amount);
			return send;
		}
	}

	// connection does not exist. create a new one
	return createRoute( from, to, amount );
}


MixerRoute * Mixer::createRoute( MixerChannel * from, MixerChannel * to, float amount )
{
	if( from == to )
	{
		return nullptr;
	}
	Engine::audioEngine()->requestChangeInModel();
	auto route = new MixerRoute(from, to, amount);

	// add us to from's sends
	from->m_sends.push_back(route);

	// add us to to's receives
	to->m_receives.push_back(route);

	// add us to mixer's list
	m_mixerRoutes.push_back(route);
	Engine::audioEngine()->doneChangeInModel();

	return route;
}


// delete the connection made by createChannelSend
void Mixer::deleteChannelSend( mix_ch_t fromChannel, mix_ch_t toChannel )
{
	// delete the send
	MixerChannel * from = m_mixerChannels[fromChannel];
	MixerChannel * to	 = m_mixerChannels[toChannel];

	// find and delete the send entry
	for (const auto& send : from->m_sends)
	{
		if (send->receiver() == to)
		{
			deleteChannelSend(send);
			break;
		}
	}
}


void Mixer::deleteChannelSend( MixerRoute * route )
{
	Engine::audioEngine()->requestChangeInModel();

	auto removeFromMixerRoute = [route](MixerRouteVector& routeVec)
	{
		if (auto it = std::ranges::find(routeVec, route); it != routeVec.end()) { routeVec.erase(it); }
	};

	// remove us from from's sends
	removeFromMixerRoute(route->sender()->m_sends);

	// remove us from to's receives
	removeFromMixerRoute(route->receiver()->m_receives);

	// remove us from mixer's list
	removeFromMixerRoute(m_mixerRoutes);

	delete route;
	Engine::audioEngine()->doneChangeInModel();
}


bool Mixer::isInfiniteLoop( mix_ch_t sendFrom, mix_ch_t sendTo )
{
	if( sendFrom == sendTo ) return true;
	MixerChannel * from = m_mixerChannels[sendFrom];
	MixerChannel * to = m_mixerChannels[sendTo];
	bool b = checkInfiniteLoop( from, to );
	return b;
}


bool Mixer::checkInfiniteLoop( MixerChannel * from, MixerChannel * to )
{
	// can't send master to anything
	if( from == m_mixerChannels[0] )
	{
		return true;
	}

	// can't send channel to itself
	if( from == to )
	{
		return true;
	}

	// follow sendTo's outputs recursively looking for something that sends
	// to sendFrom
	for (const auto& send : to->m_sends)
	{
		if (checkInfiniteLoop(from, send->receiver()))
		{
			return true;
		}
	}

	return false;
}


// how much does fromChannel send its output to the input of toChannel?
FloatModel * Mixer::channelSendModel( mix_ch_t fromChannel, mix_ch_t toChannel )
{
	if( fromChannel == toChannel )
	{
		return nullptr;
	}
	const MixerChannel * from = m_mixerChannels[fromChannel];
	const MixerChannel * to = m_mixerChannels[toChannel];

	for( MixerRoute * route : from->m_sends )
	{
		if( route->receiver() == to )
		{
			return route->amount();
		}
	}

	return nullptr;
}



void Mixer::mixToChannel(const AudioBuffer& buffer, mix_ch_t dest)
{
	const auto channel = m_mixerChannels[dest];
	if (!channel->m_muteModel.value())
	{
		channel->m_lock.lock();
		MixHelpers::add(channel->m_buffer.groupBuffers(0), buffer.groupBuffers(0));

		// Copy the planar buffer to the temporary interleaved buffer so they stay in sync
		toInterleaved(channel->m_buffer.groupBuffers(0), channel->m_buffer.interleavedBuffer());

		channel->m_buffer.mixSilenceFlags(buffer);
		channel->m_lock.unlock();
	}
}




void Mixer::prepareMasterMix()
{
	m_mixerChannels[0]->m_buffer.silenceAllChannels();
}




void Mixer::updateLatencyCompensation()
{
	const auto& buses = Engine::audioEngine()->audioBusHandles();
	if (m_latencyByChannel.size() != m_mixerChannels.size())
	{
		m_latencyByChannel.resize(m_mixerChannels.size(), 0);
	}
	if (m_outputLatencyByChannel.size() != m_mixerChannels.size())
	{
		m_outputLatencyByChannel.resize(m_mixerChannels.size(), 0);
	}
	std::fill(m_latencyByChannel.begin(), m_latencyByChannel.end(), 0);
	std::fill(m_outputLatencyByChannel.begin(),
		m_outputLatencyByChannel.end(), 0);
	bool saturated = false;

	// Start with the track buses feeding each channel.
	for (AudioBusHandle* bus : buses)
	{
		const auto destination = bus->nextMixerChannel();
		if (destination >= 0
			&& destination < static_cast<mix_ch_t>(m_latencyByChannel.size()))
		{
			m_latencyByChannel[destination] = std::min(
				DelayLine::maxFrames,
				std::max(m_latencyByChannel[destination], bus->latency()));
		}
	}

	// Propagate the longest path through the send graph. The graph is a DAG
	// (mixer sends reject cycles), so this converges within numChannels
	// iterations.
	const auto numChannels = static_cast<int>(m_mixerChannels.size());
	for (int iteration = 0; iteration < numChannels; ++iteration)
	{
		bool changed = false;
		for (int i = 0; i < numChannels; ++i)
		{
			m_outputLatencyByChannel[i] = std::min(
				DelayLine::maxFrames,
				m_latencyByChannel[i] + m_mixerChannels[i]->latency());
		}

		for (MixerRoute* route : m_mixerRoutes)
		{
			const auto from = route->senderIndex();
			const auto to = route->receiverIndex();
			if (from >= static_cast<mix_ch_t>(numChannels)
				|| to >= static_cast<mix_ch_t>(numChannels))
			{
				continue;
			}

			const f_cnt_t pathLatency = m_outputLatencyByChannel[from];
			if (pathLatency > m_latencyByChannel[to])
			{
				m_latencyByChannel[to] = pathLatency;
				changed = true;
			}
		}

		if (!changed)
		{
			break;
		}
	}

	for (AudioBusHandle* bus : buses)
	{
		const auto destination = bus->nextMixerChannel();
		const f_cnt_t inputLatency = destination >= 0
			&& destination < static_cast<mix_ch_t>(numChannels)
			? m_latencyByChannel[destination] : 0;
		const f_cnt_t requiredDelay =
			std::max<f_cnt_t>(0, inputLatency - bus->latency());
		if (requiredDelay > DelayLine::maxFrames - 1)
		{
			saturated = true;
		}
		bus->setDelayFrames(requiredDelay);
	}

	for (MixerRoute* route : m_mixerRoutes)
	{
		const auto from = route->senderIndex();
		const auto to = route->receiverIndex();
		if (from >= static_cast<mix_ch_t>(numChannels)
			|| to >= static_cast<mix_ch_t>(numChannels))
		{
			continue;
		}

		const f_cnt_t routeLatency = std::max<f_cnt_t>(
			0, m_latencyByChannel[to] - m_outputLatencyByChannel[from]);
		if (routeLatency > DelayLine::maxFrames - 1)
		{
			saturated = true;
		}
		route->setDelayFrames(routeLatency);
	}

	if (saturated && !m_latencySaturated)
	{
		qWarning() << "Latency compensation exceeds the"
			<< DelayLine::maxFrames
			<< "sample delay line; some paths will not be aligned";
	}
	m_latencySaturated = saturated;
}




f_cnt_t Mixer::totalLatency() const
{
	if (m_latencyByChannel.empty() || m_mixerChannels.empty())
	{
		return 0;
	}
	return m_latencyByChannel[0] + m_mixerChannels[0]->latency();
}




void Mixer::masterMix( SampleFrame* _buf )
{
	const int fpp = Engine::audioEngine()->framesPerPeriod();

	// add the channels that have no dependencies (no incoming senders, ie.
	// no receives) to the jobqueue. The channels that have receives get
	// added when their senders get processed, which is detected by
	// dependency counting.
	// also instantly add all muted channels as they don't need to care
	// about their senders, and can just increment the deps of their
	// recipients right away.
	AudioEngineWorkerThread::resetJobQueue( AudioEngineWorkerThread::JobQueue::OperationMode::Dynamic );
	for( MixerChannel * ch : m_mixerChannels )
	{
		ch->m_muted = ch->m_muteModel.value();
		if( ch->m_muted ) // instantly "process" muted channels
		{
			ch->processed();
			ch->done();
		}
		else if( ch->m_receives.size() == 0 )
		{
			ch->m_queued = true;
			AudioEngineWorkerThread::addJob( ch );
		}
	}
	while (m_mixerChannels[0]->state() != ThreadableJob::ProcessingState::Done)
	{
		bool found = false;
		for( MixerChannel * ch : m_mixerChannels )
		{
			const auto s = ch->state();
			if (s == ThreadableJob::ProcessingState::Queued
				|| s == ThreadableJob::ProcessingState::InProgress)
			{
				found = true;
				break;
			}
		}
		if( !found )
		{
			break;
		}
		AudioEngineWorkerThread::startAndWaitForJobs();
	}

	auto buffer = m_mixerChannels[0]->m_buffer.interleavedBuffer().asSampleFrames();

	// handle sample-exact data in master volume fader
	ValueBuffer * volBuf = m_mixerChannels[0]->m_volumeModel.valueBuffer();

	if( volBuf )
	{
		for( int f = 0; f < fpp; f++ )
		{
			buffer[f][0] *= volBuf->values()[f];
			buffer[f][1] *= volBuf->values()[f];
		}
	}

	const float v = volBuf
		? 1.0f
		: m_mixerChannels[0]->m_volumeModel.value();
	MixHelpers::addMultiplied(_buf, buffer.data(), v, fpp);

	// clear all channel buffers and
	// reset channel process state
	for( int i = 0; i < numChannels(); ++i)
	{
		m_mixerChannels[i]->m_buffer.silenceAllChannels();
		m_mixerChannels[i]->reset();
		m_mixerChannels[i]->m_queued = false;
		m_mixerChannels[i]->m_dependenciesMet = 0;
	}
}




void Mixer::clear()
{
	while( m_mixerChannels.size() > 1 )
	{
		deleteChannel(1);
	}

	clearChannel(0);
}



void Mixer::clearChannel(mix_ch_t index)
{
	MixerChannel * ch = m_mixerChannels[index];
	ch->m_fxChain.clear();
	ch->m_volumeModel.setValue( 1.0f );
	ch->m_muteModel.setValue( false );
	ch->m_soloModel.setValue( false );
	ch->m_name = ( index == 0 ) ? tr( "Master" ) : tr( "Channel %1" ).arg( index );
	ch->m_volumeModel.setDisplayName( ch->m_name + ">" + tr( "Volume" ) );
	ch->m_muteModel.setDisplayName( ch->m_name + ">" + tr( "Mute" ) );
	ch->m_soloModel.setDisplayName( ch->m_name + ">" + tr( "Solo" ) );
	ch->setColor(std::nullopt);

	// send only to master
	if( index > 0)
	{
		// delete existing sends
		while (!ch->m_sends.empty())
		{
			deleteChannelSend(ch->m_sends.front());
		}

		// add send to master
		createChannelSend( index, 0 );
	}

	// delete receives
	while (!ch->m_receives.empty())
	{
		deleteChannelSend(ch->m_receives.front());
	}
}

void Mixer::saveSettings( QDomDocument & _doc, QDomElement & _this )
{
	// save channels
	for (auto i = std::size_t{0}; i < m_mixerChannels.size(); ++i)
	{
		MixerChannel * ch = m_mixerChannels[i];

		QDomElement mixch = _doc.createElement( QString( "mixerchannel" ) );
		_this.appendChild( mixch );

		ch->m_fxChain.saveState( _doc, mixch );
		ch->m_volumeModel.saveSettings( _doc, mixch, "volume" );
		ch->m_muteModel.saveSettings( _doc, mixch, "muted" );
		ch->m_soloModel.saveSettings( _doc, mixch, "soloed" );
		mixch.setAttribute("num", static_cast<qulonglong>(i));
		mixch.setAttribute( "name", ch->m_name );
		if (ch->sidechainSource() != nullptr)
		{
			mixch.setAttribute("sidechain", ch->sidechainSource()->index());
		}
		if (ch->multiOutputTarget() != nullptr)
		{
			mixch.setAttribute("multioutput", ch->multiOutputTarget()->index());
		}
		if (const auto& color = ch->color()) { mixch.setAttribute("color", color->name()); }

		// add the channel sends
		for (const auto& send : ch->m_sends)
		{
			QDomElement sendsDom = _doc.createElement( QString( "send" ) );
			mixch.appendChild( sendsDom );

			sendsDom.setAttribute("channel", send->receiverIndex());
			send->amount()->saveSettings(_doc, sendsDom, "amount");
		}
	}
}

// make sure we have at least num channels
void Mixer::allocateChannelsTo(int num)
{
	// Guard against malformed project files. A negative index would otherwise
	// be used to index m_mixerChannels, and an arbitrarily large index would
	// make us allocate channels until the process runs out of memory.
	if (num <= 0) { return; }
	if (num > MaxMixerChannels)
	{
		qWarning() << "Ignoring mixer channel index" << num
			<< "(maximum is" << MaxMixerChannels << ")";
		return;
	}
	while (static_cast<std::size_t>(num) > m_mixerChannels.size() - 1)
	{
		createChannel();

		// delete the default send to master
		deleteChannelSend( m_mixerChannels.size()-1, 0 );
	}
}


void Mixer::loadSettings( const QDomElement & _this )
{
	clear();
	std::vector<std::pair<int, int>> sidechains;
	std::vector<std::pair<int, int>> multiOutputs;
	QDomNode node = _this.firstChild();

	while( ! node.isNull() )
	{
		if (!node.isElement() || node.toElement().nodeName() != "mixerchannel")
		{
			node = node.nextSibling();
			continue;
		}

		QDomElement mixch = node.toElement();

		// index of the channel we are about to load
		int num = mixch.attribute( "num" ).toInt();

		// allocate enough channels
		allocateChannelsTo( num );

		if (num < 0 || static_cast<std::size_t>(num) >= m_mixerChannels.size())
		{
			qWarning() << "Ignoring invalid mixer channel index" << num;
			node = node.nextSibling();
			continue;
		}

		m_mixerChannels[num]->m_volumeModel.loadSettings( mixch, "volume" );
		m_mixerChannels[num]->m_muteModel.loadSettings( mixch, "muted" );
		m_mixerChannels[num]->m_soloModel.loadSettings( mixch, "soloed" );
		m_mixerChannels[num]->m_name = mixch.attribute( "name" );
		if (mixch.hasAttribute("color"))
		{
			m_mixerChannels[num]->setColor(QColor{mixch.attribute("color")});
		}

		m_mixerChannels[num]->m_fxChain.restoreState( mixch.firstChildElement(
			m_mixerChannels[num]->m_fxChain.nodeName() ) );

		if (mixch.hasAttribute("sidechain"))
		{
			sidechains.emplace_back(num,
				mixch.attribute("sidechain").toInt());
		}
		if (mixch.hasAttribute("multioutput"))
		{
			multiOutputs.emplace_back(num,
				mixch.attribute("multioutput").toInt());
		}

		// mixer sends
		QDomNodeList chData = mixch.childNodes();
		for (auto i = 0; i < chData.length(); ++i)
		{
			QDomElement chDataItem = chData.at(i).toElement();
			if( chDataItem.nodeName() == QString( "send" ) )
			{
				int sendTo = chDataItem.attribute( "channel" ).toInt();
				if (sendTo < 0 || sendTo > MaxMixerChannels)
				{
					qWarning() << "Ignoring invalid mixer send target" << sendTo;
					continue;
				}
				allocateChannelsTo( sendTo ) ;
				if (static_cast<std::size_t>(sendTo) >= m_mixerChannels.size())
				{
					continue;
				}
				MixerRoute * mxr = createChannelSend( num, sendTo, 1.0f );
				if( mxr ) mxr->amount()->loadSettings( chDataItem, "amount" );
			}
		}



		node = node.nextSibling();
	}

	for (const auto& [channel, source] : sidechains)
	{
		if (source < 0)
		{
			continue;
		}
		allocateChannelsTo(source);
		if (!setChannelSidechain(channel, source))
		{
			qWarning() << "Ignoring invalid sidechain route" << channel
				<< "<-" << source;
		}
	}

	for (const auto& [channel, target] : multiOutputs)
	{
		if (target < 0)
		{
			continue;
		}
		allocateChannelsTo(target);
		if (!setChannelMultiOutput(channel, target))
		{
			qWarning() << "Ignoring invalid multi-output route" << channel
				<< "->" << target;
		}
	}

	emit dataChanged();
}


void Mixer::validateChannelName( int index, int oldIndex )
{
	if( m_mixerChannels[index]->m_name == tr( "Channel %1" ).arg( oldIndex ) )
	{
		m_mixerChannels[index]->m_name = tr( "Channel %1" ).arg( index );
	}
}

bool Mixer::isChannelInUse(int index)
{
	// check if the index mixer channel receives audio from any other channel
	if (!m_mixerChannels[index]->m_receives.empty())
	{
		return true;
	}

	// check if the destination mixer channel on any instrument or sample track is the index mixer channel
	TrackContainer::TrackList tracks;

	auto& songTracks = Engine::getSong()->tracks();
	auto& patternStoreTracks = Engine::patternStore()->tracks();
	tracks.insert(tracks.end(), songTracks.begin(), songTracks.end());
	tracks.insert(tracks.end(), patternStoreTracks.begin(), patternStoreTracks.end());

	for (const auto t : tracks)
	{
		if (t->type() == Track::Type::Instrument)
		{
			auto inst = dynamic_cast<InstrumentTrack*>(t);
			if (inst->mixerChannelModel()->value() == index)
			{
				return true;
			}
		}
		else if (t->type() == Track::Type::Sample)
		{
			auto strack = dynamic_cast<SampleTrack*>(t);
			if (strack->mixerChannelModel()->value() == index)
			{
				return true;
			}
		}
	}

	return false;
}


} // namespace lmms
