/*
 * PluginScanCache.h - shared on-disk cache for plugin scan results
 *
 * Copyright (c) 2026 LMMS contributors
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

#ifndef LMMS_PLUGIN_SCAN_CACHE_H
#define LMMS_PLUGIN_SCAN_CACHE_H

#include <map>
#include <vector>

#include <QString>

#include "PluginScanResult.h"
#include "lmms_export.h"

namespace lmms
{

/*! One JSON cache per plugin format, using the shared schema.
 *
 *  Keys are canonical paths plus class ID for VST3-style bundles. Entries
 *  carry the file stamp so `isUpToDate()` can skip unchanged plugins without
 *  starting the format-specific scanner.
 */
class LMMS_EXPORT PluginScanCache
{
public:
	explicit PluginScanCache(PluginScanFormat format);

	//! Bump when the on-disk layout changes.
	static int schemaVersion();

	//! Default cache path, e.g. `~/.cache/lmms/plugin-scan-vst3.json`.
	static QString defaultCachePath(PluginScanFormat format);

	//! Load a cache file. Returns false for a missing, malformed, wrong
	//! version or wrong-format file; old data is replaced by an empty cache.
	bool load(const QString& path);
	bool save(const QString& path) const;

	void clear();
	const std::vector<PluginScanEntry>& entries() const { return m_entries; }

	const PluginScanEntry* find(const QString& path,
		const QString& classId = QString()) const;
	void upsert(PluginScanEntry entry);
	bool remove(const QString& path, const QString& classId = QString());

	bool isUpToDate(const QString& path, qint64 mtimeMs, qint64 size,
		const QString& classId = QString()) const;

	PluginScanFormat format() const { return m_format; }

private:
	static QString key(const QString& path, const QString& classId);

	PluginScanFormat m_format;
	std::vector<PluginScanEntry> m_entries;
	//! Key -> index in m_entries.
	std::map<QString, std::size_t> m_index;
};

} // namespace lmms

#endif // LMMS_PLUGIN_SCAN_CACHE_H
