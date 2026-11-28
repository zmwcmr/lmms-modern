/*
 * single_process_soak.cpp - long-running load/render/save in one process
 *
 * Unlike the per-case Python runner, this keeps LMMS alive for the whole
 * soak and samples its RSS/FD/thread counts. Intended for leak and drift
 * detection with ASan/LSan or a release build.
 */

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QString>

#include "AudioEngine.h"
#include "Engine.h"
#include "Song.h"

using namespace lmms;

namespace
{

struct ProcStats
{
	long rssKb = 0;
	int fds = 0;
	int threads = 0;
};

ProcStats readStats()
{
	ProcStats stats;
	QFile status("/proc/self/status");
	if (status.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		const QString text = QString::fromLatin1(status.readAll());
		for (const QString& line : text.split('\n'))
		{
			if (line.startsWith("VmRSS:"))
			{
				stats.rssKb = line.mid(line.indexOf(':') + 1).trimmed().toLong();
			}
			else if (line.startsWith("Threads:"))
			{
				stats.threads = line.mid(line.indexOf(':') + 1).trimmed().toInt();
			}
		}
	}
	stats.fds = QDir("/proc/self/fd").entryList(QDir::NoDotAndDotDot).size();
	return stats;
}

} // namespace

int main(int argc, char** argv)
{
	QCoreApplication app(argc, argv);
	double seconds = 60.0;
	int buffersPerIteration = 64;
	QString project;
	QString output;
	QString mix = "load-render-save";
	long rssLimitMb = 0;

	for (int i = 1; i < argc; ++i)
	{
		const std::string arg = argv[i];
		if (arg == "--seconds" && i + 1 < argc) { seconds = std::atof(argv[++i]); }
		else if (arg == "--buffers" && i + 1 < argc) { buffersPerIteration = std::atoi(argv[++i]); }
		else if (arg == "--project" && i + 1 < argc) { project = QString::fromLocal8Bit(argv[++i]); }
		else if (arg == "--output" && i + 1 < argc) { output = QString::fromLocal8Bit(argv[++i]); }
		else if (arg == "--mix" && i + 1 < argc) { mix = QString::fromLocal8Bit(argv[++i]); }
		else if (arg == "--rss-limit-mb" && i + 1 < argc) { rssLimitMb = std::atol(argv[++i]); }
	}
	if (project.isEmpty())
	{
		std::fprintf(stderr, "--project is required\n");
		return 2;
	}
	if (output.isEmpty())
	{
		output = QDir::tempPath() + "/lmms-soak-output.mmp";
	}

	Engine::init(true);
	Engine::audioEngine()->audioDev()->stopProcessing();
	auto* song = Engine::getSong();

	unsigned long long iterations = 0;
	long maxRss = 0;
	int maxFds = 0;
	int maxThreads = 0;
	int failures = 0;
	QElapsedTimer timer;
	timer.start();

	while (timer.elapsed() < static_cast<qint64>(seconds * 1000.0))
	{
		song->loadProject(project);
		if (song->isEmpty())
		{
			std::fprintf(stderr, "project load failed; aborting soak\n");
			++failures;
			break;
		}
		if (mix != "load-save")
		{
			song->playSong();
			for (int i = 0; i < buffersPerIteration; ++i)
			{
				song->processNextBuffer();
			}
			song->stop();
		}
		if (mix != "render")
		{
			if (!song->saveProjectFile(output))
			{
				++failures;
				break;
			}
		}
		++iterations;

		const ProcStats stats = readStats();
		maxRss = std::max(maxRss, stats.rssKb);
		maxFds = std::max(maxFds, stats.fds);
		maxThreads = std::max(maxThreads, stats.threads);
		if (rssLimitMb > 0 && stats.rssKb > rssLimitMb * 1024)
		{
			std::fprintf(
				stderr,
				"RSS limit exceeded: %ld KB > %ld MB\n",
				stats.rssKb, rssLimitMb);
			++failures;
			break;
		}
		if ((iterations % 100) == 0)
		{
			std::printf(
				"iter=%llu elapsed=%.1fs rss=%ldKB fds=%d threads=%d\n",
				iterations, timer.elapsed() / 1000.0,
				stats.rssKb, stats.fds, stats.threads);
			std::fflush(stdout);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	std::printf(
		"done iterations=%llu elapsed=%.1fs maxrss=%ldKB maxfds=%d "
		"maxthreads=%d failures=%d mix=%s\n",
		iterations, timer.elapsed() / 1000.0, maxRss, maxFds,
		maxThreads, failures, mix.toUtf8().constData());

	Engine::destroy();
	return failures == 0 ? 0 : 1;
}
