/*
 * concurrency_stress.cpp - audio thread vs model/editor mutations
 *
 * Intended to be run under ThreadSanitizer. It deliberately exercises the
 * same objects from multiple threads: one thread calls
 * Song::processNextBuffer(), another mutates models, clips, notes and
 * effects.
 */

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>

#include "AudioEngine.h"
#include "AutomationClip.h"
#include "AutomationTrack.h"
#include "Effect.h"
#include "EffectChain.h"
#include "Engine.h"
#include "InstrumentTrack.h"
#include "MidiClip.h"
#include "Mixer.h"
#include "Note.h"
#include "Song.h"
#include "TimePos.h"
#include "Track.h"

using namespace lmms;

namespace
{

std::atomic<bool> g_stop{false};
std::atomic<unsigned long long> g_audioIterations{0};
std::atomic<unsigned long long> g_editIterations{0};
std::atomic<unsigned long long> g_errors{0};

struct ModelGuard
{
	ModelGuard() { Engine::audioEngine()->requestChangeInModel(); }
	~ModelGuard() { Engine::audioEngine()->doneChangeInModel(); }
};

InstrumentTrack* makeInstrumentTrack(Song* song)
{
	auto* track = dynamic_cast<InstrumentTrack*>(
		Track::create(Track::Type::Instrument, song));
	if (track != nullptr)
	{
		track->loadInstrument("tripleoscillator");
	}
	return track;
}

MidiClip* makeClip(InstrumentTrack* track)
{
	auto* clip = dynamic_cast<MidiClip*>(track->createClip(TimePos(0)));
	if (clip != nullptr)
	{
		for (int i = 0; i < 8; ++i)
		{
			clip->addNote(Note(TimePos(24), TimePos(i * 48), 60 + i, 100, 0), false);
		}
	}
	return clip;
}

void audioThread(Song* song)
{
	song->playSong();
	while (!g_stop.load(std::memory_order_relaxed))
	{
		song->processNextBuffer();
		g_audioIterations.fetch_add(1, std::memory_order_relaxed);
	}
}

void modelThread(Song* song, InstrumentTrack* track, MidiClip* clip, int mode)
{
	std::mt19937 rng(0xC0FFEE);
	std::uniform_int_distribution<int> tempo(20, 300);
	std::uniform_int_distribution<int> volume(0, 200);
	std::uniform_int_distribution<int> position(0, 4096);
	std::uniform_int_distribution<int> key(0, 127);
	std::uniform_int_distribution<int> length(1, 192);

	while (!g_stop.load(std::memory_order_relaxed))
	{
		try
		{
			if (mode == 0)
			{
				song->tempoModel().setValue(tempo(rng));
				track->volumeModel()->setValue(volume(rng));
				track->setMuted((rng() & 1) != 0);
			}
			else if (mode == 1)
			{
				ModelGuard guard;
				clip->addNote(
					Note(TimePos(length(rng)), TimePos(position(rng)),
						key(rng), 100, 0),
					false);
				if (clip->notes().size() > 64)
				{
					clip->removeNote(clip->notes().front());
				}
			}
			else if (mode == 2)
			{
				auto* automationTrack = dynamic_cast<AutomationTrack*>(
					Track::create(Track::Type::Automation, song));
				if (automationTrack != nullptr)
				{
					auto* automationClip = dynamic_cast<AutomationClip*>(
						automationTrack->createClip(TimePos(0)));
					if (automationClip != nullptr)
					{
						automationClip->addObject(track->volumeModel());
						for (int i = 0; i < 8; ++i)
						{
							automationClip->putValue(
								TimePos(i * 48), (i * 37) % 200, false);
						}
						ModelGuard guard;
						delete automationTrack;
					}
					else
					{
						delete automationTrack;
					}
				}
			}
			else
			{
				auto* channel = Engine::mixer()->mixerChannel(1);
				if (channel == nullptr)
				{
					channel = Engine::mixer()->mixerChannel(
						Engine::mixer()->createChannel());
				}
				if (channel != nullptr)
				{
					auto* effect = Effect::instantiate(
						"amplifier", &channel->m_fxChain, nullptr);
					if (effect != nullptr)
					{
						channel->m_fxChain.appendEffect(effect);
						std::this_thread::sleep_for(
							std::chrono::milliseconds(1));
						channel->m_fxChain.removeEffect(effect);
						delete effect;
					}
				}
			}
			g_editIterations.fetch_add(1, std::memory_order_relaxed);
		}
		catch (...)
		{
			g_errors.fetch_add(1, std::memory_order_relaxed);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

} // namespace

int main(int argc, char** argv)
{
	QCoreApplication app(argc, argv);
	int mode = 0;
	double seconds = 10.0;
	for (int i = 1; i < argc; ++i)
	{
		const std::string arg = argv[i];
		if (arg == "--seconds" && i + 1 < argc)
		{
			seconds = std::atof(argv[++i]);
		}
		else if (arg == "--mode" && i + 1 < argc)
		{
			const std::string value = argv[++i];
			if (value == "audio-model") { mode = 0; }
			else if (value == "audio-editor") { mode = 1; }
			else if (value == "automation") { mode = 2; }
			else if (value == "mixer") { mode = 3; }
			else
			{
				std::fprintf(stderr, "unknown mode: %s\n", value.c_str());
				return 2;
			}
		}
	}

	Engine::init(true);
	Engine::audioEngine()->audioDev()->stopProcessing();
	auto* song = Engine::getSong();
	auto* track = makeInstrumentTrack(song);
	auto* clip = track != nullptr ? makeClip(track) : nullptr;
	if (track == nullptr || clip == nullptr)
	{
		std::fprintf(stderr, "failed to create test track/clip\n");
		Engine::destroy();
		return 3;
	}

	std::thread audio(audioThread, song);
	std::thread editor(modelThread, song, track, clip, mode);

	QElapsedTimer timer;
	timer.start();
	while (timer.elapsed() < static_cast<qint64>(seconds * 1000.0))
	{
		QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	g_stop.store(true, std::memory_order_relaxed);
	audio.join();
	editor.join();

	std::printf(
		"mode=%d seconds=%.2f audio=%llu edits=%llu errors=%llu\n",
		mode, seconds,
		static_cast<unsigned long long>(g_audioIterations.load()),
		static_cast<unsigned long long>(g_editIterations.load()),
		static_cast<unsigned long long>(g_errors.load()));

	Engine::destroy();
	return g_errors.load() == 0 ? 0 : 1;
}
