/*
 * Vst3List.cpp - list of VST3 plugins found on the system
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

#include "Vst3List.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QString>
#include <QTemporaryFile>

#include "Vst3Module.h"
#include "Vst3Protocol.h"

namespace fs = std::filesystem;

namespace
{

constexpr int SCANNER_TIMEOUT_MS = 60000;

QString fromPath(const fs::path& path)
{
#ifdef Q_OS_WIN
	return QString::fromStdWString(path.wstring());
#else
	return QString::fromStdString(path.string());
#endif
}

fs::path toPath(const QString& path)
{
#ifdef Q_OS_WIN
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

QString scannerExecutable()
{
	const char* envScanner = std::getenv("LMMS_VST3_SCANNER");
	if (envScanner != nullptr && QFileInfo::exists(QString::fromLocal8Bit(envScanner)))
	{
		return QString::fromLocal8Bit(envScanner);
	}

	QString name = QStringLiteral("lmms-vst3-scanner");
#ifdef Q_OS_WIN
	name += QStringLiteral(".exe");
#endif

	const QStringList candidates = {
		QFileInfo(QDir(QStringLiteral("plugins:")), name).absoluteFilePath(),
		QDir(QCoreApplication::applicationDirPath()).filePath(name),
		QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("plugins/") + name),
#ifdef LMMS_VST3_SCANNER_PATH
		QString::fromUtf8(LMMS_VST3_SCANNER_PATH),
#endif
	};

	for (const auto& candidate : candidates)
	{
		if (!candidate.isEmpty() && QFileInfo::exists(candidate))
		{
			return candidate;
		}
	}
	return {};
}

QString sha256OfFile(const fs::path& path)
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
	return QString::fromLatin1(hash.result().toHex());
}

QJsonObject classToJson(const lmms::Vst3ClassInfo& info)
{
	QJsonObject object;
	object.insert(QStringLiteral("classId"), info.classId);
	object.insert(QStringLiteral("name"), info.name);
	object.insert(QStringLiteral("vendor"), info.vendor);
	object.insert(QStringLiteral("version"), info.version);
	object.insert(QStringLiteral("category"), info.category);
	object.insert(QStringLiteral("subCategories"), info.subCategories);
	object.insert(QStringLiteral("sdkVersion"), info.sdkVersion);
	object.insert(QStringLiteral("isInstrument"), info.isInstrument);
	object.insert(QStringLiteral("supported"), info.supported);
	object.insert(QStringLiteral("unsupportedReason"), info.unsupportedReason);
	object.insert(QStringLiteral("warning"), info.warning);
	object.insert(QStringLiteral("inputBuses"), info.inputBuses);
	object.insert(QStringLiteral("outputBuses"), info.outputBuses);
	object.insert(QStringLiteral("inputChannels"), info.inputChannels);
	object.insert(QStringLiteral("outputChannels"), info.outputChannels);
	object.insert(QStringLiteral("mainInputChannels"), info.mainInputChannels);
	object.insert(QStringLiteral("mainOutputChannels"), info.mainOutputChannels);
	object.insert(QStringLiteral("midiInputs"), info.midiInputs);
	object.insert(QStringLiteral("midiOutputs"), info.midiOutputs);
	object.insert(QStringLiteral("supports32Bit"), info.supports32Bit);
	return object;
}

lmms::Vst3ClassInfo classFromJson(const QJsonObject& object)
{
	lmms::Vst3ClassInfo info;
	info.classId = object.value(QStringLiteral("classId")).toString();
	info.name = object.value(QStringLiteral("name")).toString();
	info.vendor = object.value(QStringLiteral("vendor")).toString();
	info.version = object.value(QStringLiteral("version")).toString();
	info.category = object.value(QStringLiteral("category")).toString();
	info.subCategories = object.value(QStringLiteral("subCategories")).toString();
	info.sdkVersion = object.value(QStringLiteral("sdkVersion")).toString();
	info.isInstrument = object.value(QStringLiteral("isInstrument")).toBool();
	info.supported = object.value(QStringLiteral("supported")).toBool();
	info.unsupportedReason = object.value(QStringLiteral("unsupportedReason")).toString();
	info.warning = object.value(QStringLiteral("warning")).toString();
	info.inputBuses = object.value(QStringLiteral("inputBuses")).toInt();
	info.outputBuses = object.value(QStringLiteral("outputBuses")).toInt();
	info.inputChannels = object.value(QStringLiteral("inputChannels")).toInt();
	info.outputChannels = object.value(QStringLiteral("outputChannels")).toInt();
	info.mainInputChannels = object.value(QStringLiteral("mainInputChannels")).toInt();
	info.mainOutputChannels = object.value(QStringLiteral("mainOutputChannels")).toInt();
	info.midiInputs = object.value(QStringLiteral("midiInputs")).toInt();
	info.midiOutputs = object.value(QStringLiteral("midiOutputs")).toInt();
	info.supports32Bit = object.value(QStringLiteral("supports32Bit")).toBool();
	return info;
}

lmms::PluginScanEntry bundleToEntry(const lmms::Vst3BundleInfo& bundle,
	qint64 mtimeMs, qint64 size)
{
	lmms::PluginScanEntry entry;
	entry.format = lmms::PluginScanFormat::Vst3;
	entry.path = fromPath(bundle.path);
	entry.hash = bundle.hash;
	entry.mtimeMs = mtimeMs;
	entry.size = size;
	entry.error = bundle.error;
	entry.isInstrument = std::any_of(bundle.classes.begin(), bundle.classes.end(),
		[](const auto& item) { return item.isInstrument; });
	entry.isEffect = std::any_of(bundle.classes.begin(), bundle.classes.end(),
		[](const auto& item) { return !item.isInstrument; });
	if (!bundle.classes.empty())
	{
		const auto& first = bundle.classes.front();
		entry.name = first.name;
		entry.vendor = first.vendor;
		entry.version = first.version;
	}
	for (const auto& item : bundle.classes)
	{
		lmms::PluginScanClass common;
		common.id = item.classId;
		common.name = item.name;
		common.isInstrument = item.isInstrument;
		common.extra = classToJson(item);
		entry.classes.push_back(std::move(common));
	}
	return entry;
}




lmms::Vst3BundleInfo entryToBundle(const lmms::PluginScanEntry& entry)
{
	lmms::Vst3BundleInfo bundle;
	bundle.path = canonicalPath(toPath(entry.path));
	bundle.hash = entry.hash;
	bundle.error = entry.error;
	for (const auto& item : entry.classes)
	{
		if (!item.extra.isEmpty())
		{
			bundle.classes.push_back(classFromJson(item.extra));
		}
		else
		{
			lmms::Vst3ClassInfo info;
			info.classId = item.id;
			info.name = item.name;
			info.isInstrument = item.isInstrument;
			bundle.classes.push_back(std::move(info));
		}
	}
	return bundle;
}




bool hasVst3Extension(const fs::path& path)
{
	auto extension = path.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return extension == ".vst3";
}

bool bundleStampFromJson(const QJsonObject& object, qint64& mtimeMs, qint64& size)
{
	if (!object.contains(QStringLiteral("mtime")) || !object.contains(QStringLiteral("size")))
	{
		return false;
	}
	mtimeMs = static_cast<qint64>(object.value(QStringLiteral("mtime")).toDouble());
	size = static_cast<qint64>(object.value(QStringLiteral("size")).toDouble());
	return true;
}

QString scanError(
	const QString& scanner,
	const fs::path& bundlePath,
	const QString& message)
{
	return QStringLiteral("%1 (%2: %3)")
		.arg(message, scanner, fromPath(bundlePath));
}

lmms::Vst3BundleInfo runScanner(const fs::path& bundlePath, const QString& scanner)
{
	lmms::Vst3BundleInfo result;
	result.path = bundlePath;

	if (scanner.isEmpty())
	{
		result.error = QStringLiteral("lmms-vst3-scanner was not found");
		return result;
	}

	QProcess process;
	process.setProgram(scanner);

	// Plugins are allowed to print to stdout, so the scanner writes its JSON
	// to a separate file instead of relying on a clean stdout stream.
	QTemporaryFile outputFile;
	if (!outputFile.open())
	{
		result.error = QStringLiteral("Unable to create scanner output file");
		return result;
	}
	const auto outputPath = outputFile.fileName();
	outputFile.close();

	process.setArguments({QStringLiteral("--json"), fromPath(bundlePath),
		QStringLiteral("--output"), outputPath});
	process.setProcessChannelMode(QProcess::SeparateChannels);
	process.setStandardOutputFile(QProcess::nullDevice());
	process.start();

	if (!process.waitForStarted(10000))
	{
		result.error = scanError(scanner, bundlePath, QStringLiteral("Failed to start scanner"));
		return result;
	}
	if (!process.waitForFinished(SCANNER_TIMEOUT_MS))
	{
		process.kill();
		process.waitForFinished(5000);
		result.error = QStringLiteral("Scanner timed out");
		return result;
	}

	const auto stderrData = process.readAllStandardError();

	if (process.exitStatus() != QProcess::NormalExit)
	{
		result.error = QStringLiteral("Scanner crashed");
		return result;
	}
	if (process.exitCode() != 0)
	{
		QString detail = QString::fromLocal8Bit(stderrData).trimmed();
		result.error = detail.isEmpty()
			? QStringLiteral("Scanner exited with code %1").arg(process.exitCode())
			: detail;
		return result;
	}

	QFile file(outputPath);
	if (!file.open(QFile::ReadOnly))
	{
		result.error = QStringLiteral("Unable to read scanner output");
		return result;
	}

	QJsonParseError parseError;
	const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
	if (!document.isObject())
	{
		result.error = QStringLiteral("Invalid scanner output: %1")
			.arg(parseError.errorString());
		return result;
	}

	const auto root = document.object();
	if (root.value(QStringLiteral("version")).toInt() != lmms::vst3::scannerProtocolVersion)
	{
		result.error = QStringLiteral("Unsupported scanner output version");
		return result;
	}

	const auto bundle = root.value(QStringLiteral("bundle")).toObject();
	result.error = bundle.value(QStringLiteral("error")).toString();
	if (result.error.isEmpty())
	{
		result.error = root.value(QStringLiteral("error")).toString();
	}
	result.hash = bundle.value(QStringLiteral("hash")).toString();
	for (const auto& value : bundle.value(QStringLiteral("classes")).toArray())
	{
		result.classes.push_back(classFromJson(value.toObject()));
	}
	return result;
}

} // namespace

namespace lmms
{

Vst3List* Vst3List::inst()
{
	static Vst3List list;
	return &list;
}

QString Vst3List::cacheFilePath()
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
	return dir + QStringLiteral("/vst3-list.json");
}

std::vector<std::filesystem::path> Vst3List::searchPaths()
{
	std::vector<fs::path> result;

	const auto addPath = [&result](const QString& path) {
		if (!path.isEmpty())
		{
			result.push_back(toPath(QDir::cleanPath(path)));
		}
	};

#ifdef Q_OS_WIN
	const auto pathSeparator = QLatin1Char(';');
#else
	const auto pathSeparator = QLatin1Char(':');
#endif

	for (const auto& entry : QString::fromLocal8Bit(qgetenv("VST3_PATH")).split(
			pathSeparator, Qt::SkipEmptyParts))
	{
		addPath(entry);
	}

#ifdef Q_OS_WIN
	const auto commonProgramFiles = qEnvironmentVariable("CommonProgramFiles");
	if (!commonProgramFiles.isEmpty())
	{
		addPath(commonProgramFiles + QStringLiteral("/VST3"));
	}
	const auto programFiles = qEnvironmentVariable("ProgramFiles");
	if (!programFiles.isEmpty())
	{
		addPath(programFiles + QStringLiteral("/Common Files/VST3"));
	}
#elif defined(Q_OS_MAC)
	addPath(QDir::homePath() + QStringLiteral("/Library/Audio/Plug-Ins/VST3"));
	addPath(QStringLiteral("/Library/Audio/Plug-Ins/VST3"));
	addPath(QStringLiteral("/Network/Library/Audio/Plug-Ins/VST3"));
#else
	addPath(QDir::homePath() + QStringLiteral("/.vst3"));
	addPath(QStringLiteral("/usr/local/lib/vst3"));
	addPath(QStringLiteral("/usr/local/lib64/vst3"));
	addPath(QStringLiteral("/usr/lib/vst3"));
	addPath(QStringLiteral("/usr/lib64/vst3"));
#endif

	return result;
}

void Vst3List::ensureScanned()
{
	if (m_scanned)
	{
		return;
	}
	// Set the flag first: loading a plugin may process Qt events and
	// re-enter this function through the plugin browser.
	m_scanned = true;

	loadCache();
	m_seenBundles.clear();
	m_visitedDirs.clear();

	scanDefaultDirs();

	clearVisible();
	for (const auto& key : m_seenBundles)
	{
		const auto it = m_cache.find(key);
		if (it != m_cache.end())
		{
			addToVisible(it->second);
		}
	}

	saveCache();
}

void Vst3List::rescan()
{
	m_scanned = false;
	clearVisible();
	m_seenBundles.clear();
	m_visitedDirs.clear();
	ensureScanned();
}

void Vst3List::scanDefaultDirs()
{
	for (const auto& path : searchPaths())
	{
		scanDirRecursive(path);
	}
}

void Vst3List::scanDirRecursive(const std::filesystem::path& dirPath)
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
			if (hasVst3Extension(path))
			{
				scanBundle(path);
			}
			else
			{
				scanDirRecursive(path);
			}
		}
		else if (!statusError && entry.is_regular_file(statusError)
			&& hasVst3Extension(path))
		{
			scanBundle(path);
		}
	}
}

Vst3List::BundleStamp Vst3List::bundleStamp(
	const std::filesystem::path& bundlePath)
{
	BundleStamp stamp;
	auto binaryPath = Vst3Module::resolveBundleBinary(bundlePath);
	if (binaryPath.empty())
	{
		binaryPath = bundlePath;
	}
	const QFileInfo info(fromPath(binaryPath));
	stamp.mtimeMs = info.lastModified().toMSecsSinceEpoch();
	stamp.size = info.size();
	return stamp;
}

void Vst3List::scanBundle(const std::filesystem::path& bundlePath)
{
	const auto canonical = canonicalPath(bundlePath);
	const auto key = canonical.string();
	if (!m_seenBundles.insert(key).second)
	{
		return;
	}

	const auto stamp = bundleStamp(canonical);
	const auto stampIt = m_cacheStamps.find(key);
	if (stampIt != m_cacheStamps.end()
		&& stampIt->second.mtimeMs == stamp.mtimeMs
		&& stampIt->second.size == stamp.size)
	{
		return;
	}

	const auto scanner = scannerExecutable();
	if (scanner.isEmpty())
	{
		// Do not cache this failure: it is a host configuration problem, not
		// a property of the bundle, and should be retried on the next run.
		qWarning("lmms-vst3-scanner was not found, cannot scan VST3 plugin '%s'",
			canonical.string().c_str());
		return;
	}

	auto result = runScanner(canonical, scanner);
	result.path = canonical;

	auto binaryPath = Vst3Module::resolveBundleBinary(canonical);
	if (binaryPath.empty())
	{
		binaryPath = canonical;
	}
	if (result.hash.isEmpty())
	{
		result.hash = sha256OfFile(binaryPath);
	}

	m_cache.insert_or_assign(key, std::move(result));
	m_cacheStamps.insert_or_assign(key, stamp);
	m_cacheChanged = true;

	// Persist after every probe so an interrupted first scan does not lose
	// all work and the next launch can continue where this one stopped.
	saveCache();
}

void Vst3List::clearVisible()
{
	m_plugins.clear();
	m_hashToPath.clear();
}

void Vst3List::addToVisible(const Vst3BundleInfo& bundle)
{
	if (!bundle.error.isEmpty())
	{
		return;
	}

	const auto bundleKey = bundle.path.string();
	if (!bundle.hash.isEmpty())
	{
		const auto [it, inserted] = m_hashToPath.emplace(bundle.hash, bundleKey);
		if (!inserted && it->second != bundleKey)
		{
			// Another bundle with identical contents is already listed.
			return;
		}
	}

	for (const auto& classInfo : bundle.classes)
	{
		if (!classInfo.supported || classInfo.classId.isEmpty())
		{
			continue;
		}
		Metadata metadata;
		metadata.bundlePath = bundle.path;
		metadata.hash = bundle.hash;
		metadata.classInfo = classInfo;
		m_plugins.insert_or_assign(
			bundleKey + "|" + classInfo.classId.toStdString(), std::move(metadata));
	}
}

namespace
{

std::vector<lmms::Vst3List::Metadata> sortedPlugins(
	const std::map<std::string, lmms::Vst3List::Metadata>& plugins, bool instruments)
{
	std::vector<lmms::Vst3List::Metadata> result;
	for (const auto& [_, metadata] : plugins)
	{
		if (metadata.classInfo.isInstrument == instruments
			&& metadata.classInfo.supported)
		{
			result.push_back(metadata);
		}
	}
	std::sort(result.begin(), result.end(), [](const auto& lhs, const auto& rhs)
	{
		const auto cmp = lhs.classInfo.name.compare(
			rhs.classInfo.name, Qt::CaseInsensitive);
		if (cmp != 0)
		{
			return cmp < 0;
		}
		if (lhs.classInfo.classId != rhs.classInfo.classId)
		{
			return lhs.classInfo.classId < rhs.classInfo.classId;
		}
		return lhs.bundlePath < rhs.bundlePath;
	});
	return result;
}

} // namespace

std::vector<Vst3List::Metadata> Vst3List::instrumentPlugins()
{
	ensureScanned();
	return sortedPlugins(m_plugins, true);
}

std::vector<Vst3List::Metadata> Vst3List::effectPlugins()
{
	ensureScanned();
	return sortedPlugins(m_plugins, false);
}

const Vst3List::Metadata* Vst3List::metadataForClass(
	const QString& bundlePath, const QString& classId)
{
	ensureScanned();

	const auto canonical = canonicalPath(toPath(bundlePath));
	auto it = m_plugins.find(canonical.string() + "|" + classId.toStdString());
	if (it != m_plugins.end())
	{
		return &it->second;
	}

	// Saved projects may contain non-canonical (e.g. relative) paths.
	it = m_plugins.find(toPath(bundlePath).lexically_normal().string()
		+ "|" + classId.toStdString());
	return it != m_plugins.end() ? &it->second : nullptr;
}

void Vst3List::loadCache()
{
	if (m_cacheLoaded)
	{
		return;
	}
	m_cacheLoaded = true;

	const auto sharedPath =
		PluginScanCache::defaultCachePath(PluginScanFormat::Vst3);
	if (m_scanCache.load(sharedPath))
	{
		for (const auto& entry : m_scanCache.entries())
		{
			auto bundle = entryToBundle(entry);
			const auto key = bundle.path.string();
			m_cacheStamps.insert_or_assign(key,
				BundleStamp{entry.mtimeMs, entry.size});
			m_cache.insert_or_assign(key, std::move(bundle));
		}
		m_cacheValid = true;
		return;
	}

	// Import the pre-Stage-4 cache once. The shared file is written on the
	// next save; the legacy file stays untouched for older builds.
	const auto path = cacheFilePath();
	if (path.isEmpty())
	{
		return;
	}
	QFile file(path);
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
	if (root.value(QStringLiteral("version")).toInt() != vst3::cacheVersion
		|| root.value(QStringLiteral("scanner")).toInt()
			!= vst3::scannerProtocolVersion)
	{
		return;
	}

	for (const auto& value : root.value(QStringLiteral("bundles")).toArray())
	{
		const auto object = value.toObject();
		const auto pathString = object.value(QStringLiteral("path")).toString();
		if (pathString.isEmpty())
		{
			continue;
		}
		Vst3BundleInfo bundle;
		bundle.path = canonicalPath(toPath(pathString));
		bundle.hash = object.value(QStringLiteral("hash")).toString();
		bundle.error = object.value(QStringLiteral("error")).toString();
		for (const auto& classValue :
			object.value(QStringLiteral("classes")).toArray())
		{
			bundle.classes.push_back(classFromJson(classValue.toObject()));
		}
		qint64 mtimeMs = 0;
		qint64 size = 0;
		if (!bundleStampFromJson(object, mtimeMs, size))
		{
			continue;
		}
		const auto key = bundle.path.string();
		m_cacheStamps.insert_or_assign(key, BundleStamp{mtimeMs, size});
		m_cache.insert_or_assign(key, std::move(bundle));
	}
	if (!m_cache.empty())
	{
		m_cacheValid = true;
		m_cacheChanged = true;
		saveCache();
	}
}




void Vst3List::saveCache()
{
	if (m_cacheValid && !m_cacheChanged)
	{
		return;
	}

	const auto path =
		PluginScanCache::defaultCachePath(PluginScanFormat::Vst3);
	if (path.isEmpty())
	{
		return;
	}

	m_scanCache.clear();
	for (const auto& [key, bundle] : m_cache)
	{
		qint64 mtimeMs = 0;
		qint64 size = 0;
		const auto stampIt = m_cacheStamps.find(key);
		if (stampIt != m_cacheStamps.end())
		{
			mtimeMs = stampIt->second.mtimeMs;
			size = stampIt->second.size;
		}
		m_scanCache.upsert(bundleToEntry(bundle, mtimeMs, size));
	}
	if (m_scanCache.save(path))
	{
		m_cacheValid = true;
		m_cacheChanged = false;
	}
}




} // namespace lmms
