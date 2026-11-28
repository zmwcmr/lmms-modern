/*
 * VstList.cpp - list of VST plugins found on the system
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

#include "VstList.h"

#include <algorithm>
#include <cctype>
#include <set>

#include <QByteArray>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QString>

#include "ConfigManager.h"
#include "GuiApplication.h"
#include "VstPlugin.h"
#include "lmmsconfig.h"

namespace fs = std::filesystem;

namespace
{

constexpr int CACHE_VERSION = 1;

QString fromPath(const fs::path& path)
{
#ifdef LMMS_BUILD_WIN32
	return QString::fromStdWString(path.wstring());
#else
	return QString::fromStdString(path.string());
#endif
}

fs::path toPath(const QString& path)
{
#ifdef LMMS_BUILD_WIN32
	return fs::path(path.toStdWString());
#else
	return fs::path(path.toStdString());
#endif
}

fs::path canonicalPath(const fs::path& path)
{
	std::error_code ec;
	const auto result = fs::weakly_canonical(path, ec);
	return ec ? path.lexically_normal() : result;
}

QString cacheFilePath()
{
	auto dir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
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
	return dir + QStringLiteral("/vst-list.json");
}

lmms::VstList::PluginType pluginTypeFromString(const QString& type)
{
	using PluginType = lmms::VstList::PluginType;
	if (type == QLatin1String("instrument")) { return PluginType::Instrument; }
	if (type == QLatin1String("effect")) { return PluginType::Effect; }
	return PluginType::Invalid;
}


lmms::PluginScanEntry toScanEntry(const lmms::VstList::Metadata& metadata)
{
	lmms::PluginScanEntry entry;
	entry.format = lmms::PluginScanFormat::Vst2;
	entry.path = fromPath(metadata.path);
	entry.uniqueId = QString::fromStdString(metadata.uniqueID);
	entry.name = QString::fromStdString(metadata.name);
	entry.vendor = QString::fromStdString(metadata.vendor);
	entry.product = QString::fromStdString(metadata.product);
	entry.mtimeMs = metadata.mtimeMs;
	entry.size = metadata.size;
	entry.hash = QString::fromStdString(metadata.hash);
	entry.isInstrument = metadata.type == lmms::VstList::PluginType::Instrument;
	entry.isEffect = metadata.type == lmms::VstList::PluginType::Effect;
	if (metadata.type == lmms::VstList::PluginType::Invalid)
	{
		entry.error = QStringLiteral("not a loadable VST2 plugin");
	}
	return entry;
}




lmms::VstList::Metadata fromScanEntry(const lmms::PluginScanEntry& entry)
{
	lmms::VstList::Metadata metadata;
	metadata.path = toPath(entry.path);
	metadata.mtimeMs = entry.mtimeMs;
	metadata.size = entry.size;
	metadata.hash = entry.hash.toStdString();
	metadata.uniqueID = entry.uniqueId.toStdString();
	metadata.name = entry.name.toStdString();
	metadata.product = entry.product.toStdString();
	metadata.vendor = entry.vendor.toStdString();
	if (entry.isInstrument)
	{
		metadata.type = lmms::VstList::PluginType::Instrument;
	}
	else if (entry.isEffect)
	{
		metadata.type = lmms::VstList::PluginType::Effect;
	}
	else
	{
		metadata.type = lmms::VstList::PluginType::Invalid;
	}
	return metadata;
}

bool hasVstExtension(const fs::path& path)
{
	auto extension = path.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

#if defined(LMMS_BUILD_WIN32)
	return extension == ".dll";
#elif defined(LMMS_BUILD_LINUX)
	bool result = extension == ".so";
#	if defined(LMMS_HAVE_VST_32) || defined(LMMS_HAVE_VST_64)
	result = result || extension == ".dll";
#	endif
	return result;
#else
	Q_UNUSED(extension);
	return false;
#endif
}

std::string sha256OfFile(const fs::path& path)
{
	QFile file(fromPath(path));
	if (!file.open(QFile::ReadOnly))
	{
		return {};
	}
	QCryptographicHash hash(QCryptographicHash::Sha256);
	if (!hash.addData(&file))
	{
		return {};
	}
	return hash.result().toHex().toStdString();
}

#if defined(LMMS_BUILD_LINUX) && (defined(LMMS_HAVE_VST_32) || defined(LMMS_HAVE_VST_64))
fs::path wineVstDir()
{
	const QByteArray prefix = qgetenv("WINEPREFIX");
	const QString base = prefix.isEmpty()
		? QDir::homePath() + QStringLiteral("/.wine")
		: QString::fromLocal8Bit(prefix);
	return toPath(base + QStringLiteral("/drive_c/Program Files/Steinberg/VstPlugins"));
}
#endif

} // namespace

namespace lmms
{

VstList* VstList::inst()
{
	static VstList list;
	return &list;
}

void VstList::ensureScanned()
{
	if (m_scanned)
	{
		return;
	}
	// Set the flag first: loading a plugin may process Qt events and
	// re-enter this function through the plugin browser.
	m_scanned = true;

	loadCache();
	m_seenPaths.clear();
	m_visitedDirs.clear();

	scanDefaultDirs();

	// Build the visible list from scratch so that removed or changed files
	// do not leave stale entries behind.
	m_plugins.clear();
	m_hashToPath.clear();
	for (const auto& path : m_seenPaths)
	{
		const auto it = m_cache.find(path);
		if (it != m_cache.end())
		{
			addToVisible(it->second);
		}
	}

	saveCache();
}

void VstList::rescan()
{
	m_scanned = false;
	m_plugins.clear();
	m_hashToPath.clear();
	m_seenPaths.clear();
	ensureScanned();
}

void VstList::scanDefaultDirs()
{
	if (!ConfigManager::inst()->vstDir().isEmpty())
	{
		scanDirRecursive(toPath(ConfigManager::inst()->vstDir()));
	}

#if defined(LMMS_BUILD_WIN32)
	scanDirRecursive(fs::path("C:/Program Files/Steinberg/VstPlugins"));
#elif defined(LMMS_BUILD_LINUX)
	// Honour the de-facto standard VST2 search path used by most Linux hosts.
	for (const auto& variable : {QByteArrayLiteral("VST_PATH"),
		QByteArrayLiteral("VST2_PATH")})
	{
		const auto value = qgetenv(variable.constData());
		for (const auto& entry : value.split(':'))
		{
			if (!entry.isEmpty())
			{
				scanDirRecursive(toPath(QString::fromLocal8Bit(entry)));
			}
		}
	}
	scanDirRecursive(toPath(QDir::homePath() + QStringLiteral("/.vst")));
	scanDirRecursive(toPath(QDir::homePath() + QStringLiteral("/.local/lib/vst")));
	scanDirRecursive(fs::path("/usr/local/lib64/vst"));
	scanDirRecursive(fs::path("/usr/local/lib/vst"));
	scanDirRecursive(fs::path("/usr/lib64/vst"));
	scanDirRecursive(fs::path("/usr/lib/vst"));
#	if defined(LMMS_HAVE_VST_32) || defined(LMMS_HAVE_VST_64)
	scanDirRecursive(wineVstDir());
#	endif
#endif
}

void VstList::scanDirRecursive(const fs::path& dirPath)
{
	std::error_code ec;
	if (!fs::is_directory(dirPath, ec))
	{
		return;
	}

	const auto canonicalDir = canonicalPath(dirPath);
	if (!m_visitedDirs.insert(canonicalDir.string()).second)
	{
		return;
	}

	fs::directory_iterator it(dirPath,
		fs::directory_options::skip_permission_denied, ec);
	if (ec)
	{
		return;
	}

	for (const auto& entry : it)
	{
		std::error_code statusError;
		const auto& path = entry.path();
		const auto fileName = path.filename().string();
		if (fileName.empty() || fileName.front() == '.')
		{
			continue;
		}

		if (entry.is_directory(statusError))
		{
			scanDirRecursive(path);
		}
		else if (!statusError && entry.is_regular_file(statusError) && hasVstExtension(path))
		{
			scanFile(path);
		}
	}
}

void VstList::scanFile(const fs::path& filePath)
{
	const auto canonical = canonicalPath(filePath);
	const auto key = canonical.string();
	if (!m_seenPaths.insert(key).second)
	{
		return;
	}

	const QFileInfo fileInfo(fromPath(canonical));
	const qint64 mtimeMs = fileInfo.lastModified().toMSecsSinceEpoch();
	const qint64 size = fileInfo.size();

	const auto cached = m_cache.find(key);
	if (cached != m_cache.end()
		&& cached->second.mtimeMs == mtimeMs
		&& cached->second.size == size)
	{
		return;
	}

	if (gui::getGUI() != nullptr)
	{
		gui::getGUI()->displayInitProgress(
			QCoreApplication::translate("VstList", "Scanning VSTs: %1")
				.arg(fromPath(canonical)));
	}

	Metadata metadata;
	metadata.path = canonical;
	metadata.mtimeMs = mtimeMs;
	metadata.size = size;

	VstPlugin plugin(fromPath(canonical), true);
	if (plugin.failed() || plugin.name().isEmpty())
	{
		metadata.type = PluginType::Invalid;
	}
	else
	{
		metadata.type = plugin.isSynth() ? PluginType::Instrument : PluginType::Effect;
		metadata.uniqueID = plugin.uniqueID().toStdString();
		metadata.name = plugin.name().toStdString();
		metadata.product = plugin.productString().toStdString();
		metadata.vendor = plugin.vendorString().toStdString();
		metadata.hash = sha256OfFile(canonical);
	}

	m_cache.insert_or_assign(key, std::move(metadata));
	m_cacheChanged = true;

	// Persist after every probe so an interrupted first scan does not lose
	// all work and the next launch can continue where this one stopped.
	saveCache();
}

void VstList::addToVisible(const Metadata& metadata)
{
	if (metadata.type == PluginType::Invalid)
	{
		return;
	}

	const auto key = metadata.path.string();
	if (!metadata.hash.empty())
	{
		const auto [it, inserted] = m_hashToPath.emplace(metadata.hash, key);
		if (!inserted && it->second != key)
		{
			// Another file with identical contents is already listed.
			return;
		}
	}
	m_plugins.insert_or_assign(key, metadata);
}

std::vector<VstList::Metadata> VstList::instrumentPlugins()
{
	ensureScanned();

	std::vector<Metadata> result;
	for (const auto& [_, metadata] : m_plugins)
	{
		if (metadata.type == PluginType::Instrument)
		{
			result.push_back(metadata);
		}
	}
	std::sort(result.begin(), result.end(), [](const auto& lhs, const auto& rhs)
	{
		const auto cmp = QString::fromStdString(lhs.name)
			.compare(QString::fromStdString(rhs.name), Qt::CaseInsensitive);
		return cmp != 0 ? cmp < 0 : lhs.path < rhs.path;
	});
	return result;
}

std::vector<VstList::Metadata> VstList::effectPlugins()
{
	ensureScanned();

	std::vector<Metadata> result;
	for (const auto& [_, metadata] : m_plugins)
	{
		if (metadata.type == PluginType::Effect)
		{
			result.push_back(metadata);
		}
	}
	std::sort(result.begin(), result.end(), [](const auto& lhs, const auto& rhs)
	{
		const auto cmp = QString::fromStdString(lhs.name)
			.compare(QString::fromStdString(rhs.name), Qt::CaseInsensitive);
		return cmp != 0 ? cmp < 0 : lhs.path < rhs.path;
	});
	return result;
}

const VstList::Metadata* VstList::metadataForFile(const QString& filePath)
{
	ensureScanned();

	const auto canonical = canonicalPath(toPath(filePath));
	auto it = m_plugins.find(canonical.string());
	if (it != m_plugins.end())
	{
		return &it->second;
	}

	// Saved projects may contain non-canonical (e.g. relative) paths.
	it = m_plugins.find(toPath(filePath).lexically_normal().string());
	return it != m_plugins.end() ? &it->second : nullptr;
}

void VstList::loadCache()
{
	if (m_cacheLoaded)
	{
		return;
	}
	m_cacheLoaded = true;

	const auto sharedPath =
		PluginScanCache::defaultCachePath(PluginScanFormat::Vst2);
	if (m_scanCache.load(sharedPath))
	{
		for (const auto& entry : m_scanCache.entries())
		{
			auto metadata = fromScanEntry(entry);
			m_cache.insert_or_assign(metadata.path.string(),
				std::move(metadata));
		}
		m_cacheValid = true;
		return;
	}

	// Import the pre-Stage-4 cache once. The next save writes the shared
	// per-format file and keeps the old file untouched for older builds.
	const auto legacyPath = cacheFilePath();
	if (legacyPath.isEmpty())
	{
		return;
	}
	QFile file(legacyPath);
	if (!file.open(QFile::ReadOnly))
	{
		return;
	}
	const auto document = QJsonDocument::fromJson(file.readAll());
	if (!document.isObject())
	{
		return;
	}
	const auto root = document.object();
	if (root.value(QStringLiteral("version")).toInt() != CACHE_VERSION)
	{
		return;
	}

	bool imported = false;
	for (const auto& value : root.value(QStringLiteral("plugins")).toArray())
	{
		const auto object = value.toObject();
		const auto pathString = object.value(QStringLiteral("path")).toString();
		if (pathString.isEmpty())
		{
			continue;
		}
		Metadata metadata;
		metadata.path = canonicalPath(toPath(pathString));
		metadata.mtimeMs = static_cast<qint64>(
			object.value(QStringLiteral("mtime")).toDouble());
		metadata.size = static_cast<qint64>(
			object.value(QStringLiteral("size")).toDouble());
		metadata.hash = object.value(QStringLiteral("hash")).toString().toStdString();
		metadata.type = pluginTypeFromString(
			object.value(QStringLiteral("type")).toString());
		metadata.uniqueID = object.value(QStringLiteral("uniqueID")).toString().toStdString();
		metadata.name = object.value(QStringLiteral("name")).toString().toStdString();
		metadata.product = object.value(QStringLiteral("product")).toString().toStdString();
		metadata.vendor = object.value(QStringLiteral("vendor")).toString().toStdString();
		m_cache.insert_or_assign(metadata.path.string(), std::move(metadata));
		imported = true;
	}
	m_cacheValid = imported;
	if (imported)
	{
		m_cacheChanged = true;
		saveCache();
	}
}




void VstList::saveCache()
{
	if (m_cacheValid && !m_cacheChanged)
	{
		return;
	}

	const auto path =
		PluginScanCache::defaultCachePath(PluginScanFormat::Vst2);
	if (path.isEmpty())
	{
		return;
	}

	m_scanCache.clear();
	for (const auto& [_, metadata] : m_cache)
	{
		m_scanCache.upsert(toScanEntry(metadata));
	}
	if (m_scanCache.save(path))
	{
		m_cacheValid = true;
		m_cacheChanged = false;
	}
}




} // namespace lmms
