/*
 * VstSubPluginFeatures.cpp - derivation from
 *                            Plugin::Descriptor::SubPluginFeatures for
 *                            hosting VST-plugins
 *
 * Copyright (c) 2006-2009 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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

#include "VstSubPluginFeatures.h"

#include <QLabel>
#include <QFileInfo>
#include <QString>

#include <filesystem>

#include "VstList.h"
#include "lmmsconfig.h"

namespace
{

QString fromPath(const std::filesystem::path& path)
{
#ifdef LMMS_BUILD_WIN32
	return QString::fromStdWString(path.wstring());
#else
	return QString::fromStdString(path.string());
#endif
}

QString filePathForKey(const lmms::Plugin::Descriptor::SubPluginFeatures::Key& key)
{
	return key.attributes.value(QStringLiteral("file"));
}

const lmms::VstList::Metadata* metadataForKey(
	const lmms::Plugin::Descriptor::SubPluginFeatures::Key& key)
{
	const auto path = filePathForKey(key);
	if (path.isEmpty())
	{
		return nullptr;
	}
	return lmms::VstList::inst()->metadataForFile(path);
}

} // namespace

namespace lmms
{

VstSubPluginFeatures::VstSubPluginFeatures(Plugin::Type type) :
	SubPluginFeatures(type),
	m_pluginType(type)
{
}

bool VstSubPluginFeatures::canRescan() const
{
	return true;
}

void VstSubPluginFeatures::rescan()
{
	VstList::inst()->rescan();
}

void VstSubPluginFeatures::fillDescriptionWidget(QWidget* parent, const Key* key) const
{
	const auto* metadata = metadataForKey(*key);
	const auto name = metadata && !metadata->name.empty()
		? QString::fromStdString(metadata->name)
		: key->name;
	new QLabel(QWidget::tr("Name: ") + name, parent);

	if (metadata && !metadata->vendor.empty())
	{
		new QLabel(QWidget::tr("Vendor: ") + QString::fromStdString(metadata->vendor), parent);
	}
	if (metadata && !metadata->product.empty())
	{
		new QLabel(QWidget::tr("Product: ") + QString::fromStdString(metadata->product), parent);
	}

	new QLabel(QWidget::tr("File: ") + filePathForKey(*key), parent);
}

void VstSubPluginFeatures::listSubPluginKeys(
	const Plugin::Descriptor* desc, KeyList& keyList) const
{
	const auto plugins = m_pluginType == Plugin::Type::Instrument
		? VstList::inst()->instrumentPlugins()
		: VstList::inst()->effectPlugins();

	if (m_pluginType == Plugin::Type::Instrument)
	{
		// Keep a way to pick a plugin which is not in the scanned directories
		// (or which failed to scan) instead of only showing the scanned list.
		Plugin::Descriptor::SubPluginFeatures::Key::AttributeMap attributes;
		attributes[QStringLiteral("file")] = QString();
		keyList.push_back(Key(desc, QWidget::tr("Open VST plugin file..."), attributes));
	}

	for (const auto& metadata : plugins)
	{
		Plugin::Descriptor::SubPluginFeatures::Key::AttributeMap attributes;
		attributes[QStringLiteral("file")] = fromPath(metadata.path);
		keyList.push_back(Key(desc, QString::fromStdString(metadata.name), attributes));
	}
}

QString VstSubPluginFeatures::displayName(const Key& key) const
{
	if (const auto* metadata = metadataForKey(key);
		metadata && !metadata->name.empty())
	{
		return QString::fromStdString(metadata->name);
	}
	if (!key.name.isEmpty())
	{
		return key.name;
	}
	// A saved project may reference a plugin which is currently not in the
	// scan cache. Show a sensible file name instead of the generic plugin name.
	const auto fileName = QFileInfo(filePathForKey(key)).completeBaseName();
	if (!fileName.isEmpty())
	{
		return fileName;
	}
	return key.desc ? QString::fromUtf8(key.desc->displayName) : QString();
}

QString VstSubPluginFeatures::description(const Key& key) const
{
	const auto path = filePathForKey(key);
	if (const auto* metadata = metadataForKey(key))
	{
		QString description;
		if (!metadata->product.empty())
		{
			description = QString::fromStdString(metadata->product);
		}
		if (!metadata->vendor.empty())
		{
			if (!description.isEmpty())
			{
				description += QStringLiteral(" by ");
			}
			description += QString::fromStdString(metadata->vendor);
		}
		if (!description.isEmpty())
		{
			description += QStringLiteral(" (") + fromPath(metadata->path) + QLatin1Char(')');
		}
		else
		{
			description = fromPath(metadata->path);
		}
		return description;
	}
	return path.isEmpty() ? key.name : path;
}

} // namespace lmms
