/*
 * Vst3Scanner.h - metadata extraction for VST3 bundles
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

#ifndef LMMS_VST3_SCANNER_H
#define LMMS_VST3_SCANNER_H

#include <filesystem>
#include <vector>

#include <QString>

namespace lmms
{

/** Metadata for one audio class inside a VST3 bundle. */
struct Vst3ClassInfo
{
	//! 32 lowercase hex characters, unique inside the bundle.
	QString classId;
	QString name;
	QString vendor;
	QString version;
	QString category;
	QString subCategories;
	QString sdkVersion;

	bool isInstrument = false;
	bool supported = false;
	QString unsupportedReason;
	QString warning;

	int inputBuses = 0;
	int outputBuses = 0;
	int inputChannels = 0;
	int outputChannels = 0;
	int mainInputChannels = 0;
	int mainOutputChannels = 0;
	int midiInputs = 0;
	int midiOutputs = 0;

	bool supports32Bit = false;
};

/** Result of scanning one VST3 bundle. */
struct Vst3BundleInfo
{
	std::filesystem::path path;
	QString hash;
	std::vector<Vst3ClassInfo> classes;
	//! Empty when the bundle was loaded successfully, otherwise the reason.
	QString error;
};

/**
 * Loads a VST3 bundle in the current process and enumerates its audio
 * classes. This is used by the standalone scanner executable. The LMMS
 * process only consumes the scanner's JSON output.
 */
class Vst3Scanner
{
public:
	static bool scanBundle(
		const std::filesystem::path& bundlePath,
		Vst3BundleInfo& result,
		QString* error = nullptr);
};

} // namespace lmms

#endif // LMMS_VST3_SCANNER_H
