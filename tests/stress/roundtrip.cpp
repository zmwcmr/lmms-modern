/*
 * roundtrip.cpp - load/save/load helper for the stress harness
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

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include <QCoreApplication>

#include "Engine.h"
#include "Song.h"
#include "TrackContainer.h"

int main(int argc, char** argv)
{
	QCoreApplication app(argc, argv);
	if (argc < 4)
	{
		std::fprintf(stderr, "usage: %s <in.mmp> <out1.mmp> <out2.mmp> [loops]\n", argv[0]);
		return 2;
	}

	const int loops = argc > 4 ? std::max(1, std::atoi(argv[4])) : 1;
	lmms::Engine::init(true);
	auto* song = lmms::Engine::getSong();
	int result = 0;
	do
	{
		std::size_t trackCount = 0;
		for (int loop = 0; loop < loops; ++loop)
		{
			song->loadProject(QString::fromLocal8Bit(argv[1]));
			if (song->isEmpty())
			{
				result = 3;
				break;
			}
			trackCount = song->tracks().size();
			if (!song->saveProjectFile(QString::fromLocal8Bit(argv[2])))
			{
				result = 4;
				break;
			}

			song->clearProject();
			song->loadProject(QString::fromLocal8Bit(argv[2]));
			if (song->isEmpty() || song->tracks().size() != trackCount)
			{
				result = 5;
				break;
			}
			if (!song->saveProjectFile(QString::fromLocal8Bit(argv[3])))
			{
				result = 6;
				break;
			}
		}
		if (result == 0) { std::printf("tracks=%zu loops=%d\n", trackCount, loops); }
	} while (false);

	lmms::Engine::destroy();
	return result;
}
