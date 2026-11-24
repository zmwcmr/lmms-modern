/*
 * VstList.h - list of VST plugins found on the system
 *
 * Copyright (c) 2025-2026 LMMS developers
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

#ifndef LMMS_VST_LIST_H
#define LMMS_VST_LIST_H

#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <QString>
#include <QtGlobal>

#include "vstbase_export.h"

namespace lmms
{

/**
 * List of VST2 plugins found in the configured VST directory and, on Linux,
 * in the usual system-wide VST directories.
 *
 * Scanning a plugin means starting it in the remote VST host once, which is
 * expensive. Results (including failures) are therefore cached on disk and
 * only plugins whose modification time or size changed are scanned again.
 */
class VSTBASE_EXPORT VstList
{
public:
	enum class PluginType : uint8_t
	{
		Invalid, //!< file exists but is not a loadable VST plugin
		Instrument,
		Effect,
	};

	struct Metadata
	{
		std::filesystem::path path;
		qint64 mtimeMs = 0;
		qint64 size = 0;
		//! SHA-256 of the file contents, used to deduplicate identical files
		std::string hash;
		PluginType type = PluginType::Invalid;
		std::string uniqueID;
		std::string name;
		std::string product;
		std::string vendor;
	};

	//! Singleton instance. The actual scan happens lazily on first use.
	static VstList* inst();

	//! All plugins identified as instruments, sorted by name.
	std::vector<Metadata> instrumentPlugins();

	//! All plugins identified as effects, sorted by name.
	std::vector<Metadata> effectPlugins();

	//! Look up a plugin that was found during the scan. Returns nullptr if
	//! the file is unknown or is not a VST plugin. Triggers a scan if needed.
	const Metadata* metadataForFile(const QString& filePath);

	//! Discard the in-memory list and scan everything again. The on-disk
	//! cache still avoids reloading files that did not change.
	void rescan();

private:
	VstList() = default;

	void ensureScanned();
	void loadCache();
	void saveCache();
	void scanDefaultDirs();
	void scanDirRecursive(const std::filesystem::path& dirPath);

	//! Scan a single candidate file and remember it for this scan. Results
	//! are stored in the cache, including files which are not loadable VSTs.
	void scanFile(const std::filesystem::path& filePath);
	void addToVisible(const Metadata& metadata);

	bool m_scanned = false;
	bool m_cacheChanged = false;
	bool m_cacheLoaded = false;
	bool m_cacheValid = false;

	//! Metadata for every file we ever looked at, keyed by canonical path.
	std::map<std::string, Metadata> m_cache;
	//! Visible plugins (valid instruments/effects), keyed by canonical path.
	std::map<std::string, Metadata> m_plugins;
	//! File contents hash -> canonical path, used for deduplication.
	std::map<std::string, std::string> m_hashToPath;
	//! Files encountered during the current scan.
	std::set<std::string> m_seenPaths;
	//! Directories already visited (protects against symlink loops).
	std::set<std::string> m_visitedDirs;
};

} // namespace lmms

#endif // LMMS_VST_LIST_H
