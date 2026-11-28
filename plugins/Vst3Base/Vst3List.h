/*
 * Vst3List.h - list of VST3 plugins found on the system
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

#ifndef LMMS_VST3_LIST_H
#define LMMS_VST3_LIST_H

#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <QString>
#include <QtGlobal>

#include "PluginScanCache.h"
#include "Vst3Scanner.h"
#include "vst3base_export.h"

namespace lmms
{

/**
 * List of VST3 plugins found in the standard VST3 directories.
 *
 * Scanning a bundle means starting it in the separate
 * `lmms-vst3-scanner` process once. Results (including failures) are cached
 * on disk and only bundles whose module binary changed are scanned again.
 */
class VST3BASE_EXPORT Vst3List
{
public:
	struct Metadata
	{
		std::filesystem::path bundlePath;
		//! SHA-256 of the module binary, used to deduplicate identical files.
		QString hash;
		Vst3ClassInfo classInfo;
	};

	//! Singleton instance. The scan happens lazily on first use.
	static Vst3List* inst();

	//! All classes identified as instruments, sorted by name.
	std::vector<Metadata> instrumentPlugins();

	//! All classes identified as effects, sorted by name.
	std::vector<Metadata> effectPlugins();

	//! Look up a class that was found during the scan. Returns nullptr if
	//! the bundle or class is unknown. Triggers a scan if needed.
	const Metadata* metadataForClass(
		const QString& bundlePath, const QString& classId);

	//! Discard the in-memory list and scan everything again. The on-disk
	//! cache still avoids rescanning bundles that did not change.
	void rescan();

	bool isScanned() const { return m_scanned; }

	static QString cacheFilePath();
	static std::vector<std::filesystem::path> searchPaths();

private:
	Vst3List() = default;

	void ensureScanned();
	void loadCache();
	void saveCache();
	void scanDefaultDirs();
	void scanDirRecursive(const std::filesystem::path& dirPath);
	void scanBundle(const std::filesystem::path& bundlePath);
	void addToVisible(const Vst3BundleInfo& bundle);
	void clearVisible();

	struct BundleStamp
	{
		qint64 mtimeMs = 0;
		qint64 size = 0;
	};

	static BundleStamp bundleStamp(const std::filesystem::path& bundlePath);

	bool m_scanned = false;
	bool m_cacheChanged = false;
	bool m_cacheLoaded = false;
	bool m_cacheValid = false;

	//! Metadata for every bundle we ever looked at, keyed by canonical path.
	std::map<std::string, Vst3BundleInfo> m_cache;
	//! Bundle stamp belonging to each cache entry.
	std::map<std::string, BundleStamp> m_cacheStamps;
	PluginScanCache m_scanCache{PluginScanFormat::Vst3};
	//! Visible classes, keyed by canonical bundle path + class ID.
	std::map<std::string, Metadata> m_plugins;
	//! Bundle hash -> canonical path, used for deduplication.
	std::map<QString, std::string> m_hashToPath;
	//! Bundles encountered during the current scan.
	std::set<std::string> m_seenBundles;
	//! Directories already visited (protects against symlink loops).
	std::set<std::string> m_visitedDirs;
};

} // namespace lmms

#endif // LMMS_VST3_LIST_H
