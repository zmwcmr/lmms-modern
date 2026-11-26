/*
 * main.cpp - lmms-vst3-scanner
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

#include <cstdio>
#include <filesystem>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QString>
#include <QStringList>
#include <QTextStream>

#include "Vst3List.h"
#include "Vst3Protocol.h"
#include "Vst3Scanner.h"

namespace
{

QString g_outputPath;

std::filesystem::path toPath(const QString& path)
{
#ifdef Q_OS_WIN
	return std::filesystem::path(path.toStdWString());
#else
	return std::filesystem::path(path.toStdString());
#endif
}

QString fromPath(const std::filesystem::path& path)
{
#ifdef Q_OS_WIN
	return QString::fromStdWString(path.wstring());
#else
	return QString::fromStdString(path.string());
#endif
}

bool writeDocument(const QJsonDocument& document)
{
	QByteArray data = document.toJson(QJsonDocument::Compact);
	data += '\n';

	if (g_outputPath.isEmpty())
	{
		return std::fwrite(data.constData(), 1, data.size(), stdout) ==
			static_cast<size_t>(data.size());
	}

	QSaveFile file(g_outputPath);
	if (!file.open(QIODevice::WriteOnly))
	{
		return false;
	}
	file.write(data);
	return file.commit();
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

QJsonObject bundleToJson(const lmms::Vst3BundleInfo& bundle)
{
	QJsonObject object;
	object.insert(QStringLiteral("path"), fromPath(bundle.path));
	object.insert(QStringLiteral("hash"), bundle.hash);
	if (!bundle.error.isEmpty())
	{
		object.insert(QStringLiteral("error"), bundle.error);
	}
	QJsonArray classes;
	for (const auto& classInfo : bundle.classes)
	{
		classes.append(classToJson(classInfo));
	}
	object.insert(QStringLiteral("classes"), classes);
	return object;
}

int printBundle(const QString& path)
{
	lmms::Vst3BundleInfo bundle;
	QString error;
	lmms::Vst3Scanner::scanBundle(toPath(path), bundle, &error);

	QJsonObject root;
	root.insert(QStringLiteral("version"), lmms::vst3::scannerProtocolVersion);
	root.insert(QStringLiteral("bundle"), bundleToJson(bundle));
	if (!error.isEmpty())
	{
		root.insert(QStringLiteral("error"), error);
	}

	return writeDocument(QJsonDocument(root)) ? 0 : 1;
}

int printList(bool forceRescan)
{
	if (forceRescan)
	{
		const auto cachePath = lmms::Vst3List::cacheFilePath();
		if (!cachePath.isEmpty())
		{
			QFile::remove(cachePath);
		}
	}

	auto* const list = lmms::Vst3List::inst();
	if (forceRescan)
	{
		list->rescan();
	}

	const auto addEntries = [](QJsonArray& array,
		const std::vector<lmms::Vst3List::Metadata>& plugins) {
		for (const auto& plugin : plugins)
		{
			QJsonObject object;
			object.insert(QStringLiteral("bundle"),
				fromPath(plugin.bundlePath));
			object.insert(QStringLiteral("classId"), plugin.classInfo.classId);
			object.insert(QStringLiteral("name"), plugin.classInfo.name);
			object.insert(QStringLiteral("vendor"), plugin.classInfo.vendor);
			object.insert(QStringLiteral("version"), plugin.classInfo.version);
			object.insert(QStringLiteral("subCategories"), plugin.classInfo.subCategories);
			object.insert(QStringLiteral("inputChannels"), plugin.classInfo.mainInputChannels);
			object.insert(QStringLiteral("outputChannels"), plugin.classInfo.mainOutputChannels);
			object.insert(QStringLiteral("midiInputs"), plugin.classInfo.midiInputs);
			object.insert(QStringLiteral("warning"), plugin.classInfo.warning);
			array.append(object);
		}
	};

	QJsonArray instruments;
	QJsonArray effects;
	addEntries(instruments, list->instrumentPlugins());
	addEntries(effects, list->effectPlugins());

	QJsonObject root;
	root.insert(QStringLiteral("version"), lmms::vst3::scannerProtocolVersion);
	root.insert(QStringLiteral("cache"), lmms::Vst3List::cacheFilePath());
	root.insert(QStringLiteral("instruments"), instruments);
	root.insert(QStringLiteral("effects"), effects);

	return writeDocument(QJsonDocument(root)) ? 0 : 1;
}

void printUsage()
{
	QTextStream err(stderr);
	err << "Usage:\n"
		<< "  lmms-vst3-scanner --json <bundle.vst3> [--output <file>]\n"
		<< "  lmms-vst3-scanner --list [--rescan]\n";
}

} // namespace

int main(int argc, char* argv[])
{
	QCoreApplication application(argc, argv);
	const auto arguments = application.arguments();

	if (arguments.size() < 2)
	{
		printUsage();
		return 2;
	}

	if (arguments.contains(QStringLiteral("--help")))
	{
		printUsage();
		return 0;
	}

	const int outputIndex = arguments.indexOf(QStringLiteral("--output"));
	if (outputIndex >= 0 && outputIndex + 1 < arguments.size())
	{
		g_outputPath = arguments.at(outputIndex + 1);
	}

	if (arguments.contains(QStringLiteral("--list"))
		|| arguments.contains(QStringLiteral("--rescan")))
	{
		return printList(arguments.contains(QStringLiteral("--rescan")));
	}

	const int jsonIndex = arguments.indexOf(QStringLiteral("--json"));
	if (jsonIndex >= 0 && jsonIndex + 1 < arguments.size())
	{
		return printBundle(arguments.at(jsonIndex + 1));
	}

	printUsage();
	return 2;
}
