/*
 * Vst3SubPluginFeatures.cpp - sub-plugin features for VST3 classes
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

#include "Vst3SubPluginFeatures.h"

#include <QFileInfo>
#include <QLabel>
#include <QString>

#include <filesystem>

#include "Vst3List.h"

namespace
{

QString fromPath(const std::filesystem::path& path)
{
#ifdef Q_OS_WIN
	return QString::fromStdWString(path.wstring());
#else
	return QString::fromStdString(path.string());
#endif
}

QString bundlePathForKey(const lmms::Plugin::Descriptor::SubPluginFeatures::Key& key)
{
	return key.attributes.value(QStringLiteral("file"));
}

QString classIdForKey(const lmms::Plugin::Descriptor::SubPluginFeatures::Key& key)
{
	return key.attributes.value(QStringLiteral("class"));
}

const lmms::Vst3List::Metadata* metadataForKey(
	const lmms::Plugin::Descriptor::SubPluginFeatures::Key& key)
{
	const auto bundlePath = bundlePathForKey(key);
	const auto classId = classIdForKey(key);
	if (bundlePath.isEmpty() || classId.isEmpty())
	{
		return nullptr;
	}
	return lmms::Vst3List::inst()->metadataForClass(bundlePath, classId);
}

} // namespace

namespace lmms
{

Vst3SubPluginFeatures::Vst3SubPluginFeatures(Plugin::Type type) :
	SubPluginFeatures(type),
	m_pluginType(type)
{
}

bool Vst3SubPluginFeatures::canRescan() const
{
	return true;
}

void Vst3SubPluginFeatures::rescan()
{
	Vst3List::inst()->rescan();
}

void Vst3SubPluginFeatures::fillDescriptionWidget(QWidget* parent, const Key* key) const
{
	const auto* metadata = metadataForKey(*key);
	const auto name = metadata && !metadata->classInfo.name.isEmpty()
		? metadata->classInfo.name
		: key->name;
	new QLabel(QWidget::tr("Name: ") + name, parent);

	if (metadata && !metadata->classInfo.vendor.isEmpty())
	{
		new QLabel(QWidget::tr("Vendor: ") + metadata->classInfo.vendor, parent);
	}
	if (metadata && !metadata->classInfo.version.isEmpty())
	{
		new QLabel(QWidget::tr("Version: ") + metadata->classInfo.version, parent);
	}
	if (metadata && !metadata->classInfo.subCategories.isEmpty())
	{
		new QLabel(QWidget::tr("Categories: ") + metadata->classInfo.subCategories, parent);
	}
	if (metadata && !metadata->classInfo.warning.isEmpty())
	{
		new QLabel(QWidget::tr("Note: ") + metadata->classInfo.warning, parent);
	}

	new QLabel(QWidget::tr("File: ") + bundlePathForKey(*key), parent);
	new QLabel(QWidget::tr("Class: ") + classIdForKey(*key), parent);
}

void Vst3SubPluginFeatures::listSubPluginKeys(
	const Plugin::Descriptor* desc, KeyList& keyList) const
{
	const auto plugins = m_pluginType == Plugin::Type::Instrument
		? Vst3List::inst()->instrumentPlugins()
		: Vst3List::inst()->effectPlugins();

	for (const auto& metadata : plugins)
	{
		Plugin::Descriptor::SubPluginFeatures::Key::AttributeMap attributes;
		attributes[QStringLiteral("file")] = fromPath(metadata.bundlePath);
		attributes[QStringLiteral("class")] = metadata.classInfo.classId;
		keyList.push_back(Key(desc, metadata.classInfo.name, attributes));
	}
}

QString Vst3SubPluginFeatures::displayName(const Key& key) const
{
	if (const auto* metadata = metadataForKey(key);
		metadata && !metadata->classInfo.name.isEmpty())
	{
		return metadata->classInfo.name;
	}
	if (!key.name.isEmpty())
	{
		return key.name;
	}
	const auto fileName = QFileInfo(bundlePathForKey(key)).completeBaseName();
	if (!fileName.isEmpty())
	{
		return fileName;
	}
	return key.desc ? QString::fromUtf8(key.desc->displayName) : QString();
}

QString Vst3SubPluginFeatures::description(const Key& key) const
{
	const auto bundlePath = bundlePathForKey(key);
	if (const auto* metadata = metadataForKey(key))
	{
		QString description;
		if (!metadata->classInfo.subCategories.isEmpty())
		{
			description = metadata->classInfo.subCategories;
		}
		if (!metadata->classInfo.vendor.isEmpty())
		{
			if (!description.isEmpty())
			{
				description += QStringLiteral(" by ");
			}
			description += metadata->classInfo.vendor;
		}
		if (!description.isEmpty())
		{
			description += QStringLiteral(" (") + fromPath(metadata->bundlePath)
				+ QLatin1Char(')');
		}
		else
		{
			description = fromPath(metadata->bundlePath);
		}
		return description;
	}
	return bundlePath.isEmpty() ? key.name : bundlePath;
}

} // namespace lmms
