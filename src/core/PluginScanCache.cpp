/*
 * PluginScanCache.cpp - shared on-disk cache for plugin scan results
 *
 * Copyright (c) 2026 LMMS contributors
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#include "PluginScanCache.h"

#include <algorithm>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace lmms
{

namespace
{

QString cacheName(PluginScanFormat format)
{
	return QStringLiteral("plugin-scan-") + pluginScanFormatName(format)
		+ QStringLiteral(".json");
}

} // namespace




PluginScanCache::PluginScanCache(PluginScanFormat format) :
	m_format(format)
{
}




int PluginScanCache::schemaVersion()
{
	return 1;
}




QString PluginScanCache::defaultCachePath(PluginScanFormat format)
{
	auto dir = QStandardPaths::writableLocation(
		QStandardPaths::GenericCacheLocation);
	if (dir.isEmpty())
	{
		dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
	}
	if (dir.isEmpty())
	{
		return {};
	}
	dir += QStringLiteral("/lmms");
	QDir().mkpath(dir);
	return QDir(dir).filePath(cacheName(format));
}




bool PluginScanCache::load(const QString& path)
{
	clear();
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
	{
		return false;
	}

	QJsonParseError parseError;
	const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
	if (parseError.error != QJsonParseError::NoError || !document.isObject())
	{
		return false;
	}
	const auto root = document.object();
	if (root.value(QStringLiteral("version")).toInt() != schemaVersion())
	{
		return false;
	}
	PluginScanFormat format = m_format;
	if (!pluginScanFormatFromName(
			root.value(QStringLiteral("format")).toString(), format)
		|| format != m_format)
	{
		return false;
	}

	for (const auto& value : root.value(QStringLiteral("entries")).toArray())
	{
		upsert(PluginScanEntry::fromJson(value.toObject()));
	}
	return true;
}




bool PluginScanCache::save(const QString& path) const
{
	if (path.isEmpty())
	{
		return false;
	}
	QDir().mkpath(QFileInfo(path).absolutePath());
	QSaveFile file(path);
	if (!file.open(QIODevice::WriteOnly))
	{
		return false;
	}

	QJsonArray entries;
	for (const auto& entry : m_entries)
	{
		entries.append(entry.toJson());
	}
	QJsonObject root;
	root.insert(QStringLiteral("version"), schemaVersion());
	root.insert(QStringLiteral("format"), pluginScanFormatName(m_format));
	root.insert(QStringLiteral("entries"), entries);
	file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
	return file.commit();
}




void PluginScanCache::clear()
{
	m_entries.clear();
	m_index.clear();
}




QString PluginScanCache::key(const QString& path, const QString& classId)
{
	return path + QLatin1Char('\n') + classId;
}




const PluginScanEntry* PluginScanCache::find(const QString& path,
	const QString& classId) const
{
	if (!classId.isEmpty())
	{
		const auto it = m_index.find(key(path, classId));
		return it != m_index.end() ? &m_entries[it->second] : nullptr;
	}
	for (const auto& entry : m_entries)
	{
		if (entry.path == path)
		{
			return &entry;
		}
	}
	return nullptr;
}




void PluginScanCache::upsert(PluginScanEntry entry)
{
	const auto entryKey = key(entry.path, entry.classId);
	const auto it = m_index.find(entryKey);
	if (it != m_index.end())
	{
		m_entries[it->second] = std::move(entry);
		return;
	}
	m_index.emplace(entryKey, m_entries.size());
	m_entries.push_back(std::move(entry));
}




bool PluginScanCache::remove(const QString& path, const QString& classId)
{
	if (!classId.isEmpty())
	{
		const auto it = m_index.find(key(path, classId));
		if (it == m_index.end())
		{
			return false;
		}
		m_entries.erase(m_entries.begin() + static_cast<std::ptrdiff_t>(
			it->second));
		m_index.clear();
		for (std::size_t i = 0; i < m_entries.size(); ++i)
		{
			m_index.emplace(key(m_entries[i].path, m_entries[i].classId), i);
		}
		return true;
	}

	const auto oldSize = m_entries.size();
	m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(),
		[&path](const PluginScanEntry& entry) { return entry.path == path; }),
		m_entries.end());
	if (m_entries.size() == oldSize)
	{
		return false;
	}
	m_index.clear();
	for (std::size_t i = 0; i < m_entries.size(); ++i)
	{
		m_index.emplace(key(m_entries[i].path, m_entries[i].classId), i);
	}
	return true;
}




bool PluginScanCache::isUpToDate(const QString& path, qint64 mtimeMs,
	qint64 size, const QString& classId) const
{
	const auto* entry = find(path, classId);
	return entry != nullptr && entry->mtimeMs == mtimeMs
		&& entry->size == size;
}

} // namespace lmms
