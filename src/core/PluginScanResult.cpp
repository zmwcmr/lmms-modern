/*
 * PluginScanResult.cpp - format-neutral plugin scan result
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

#include "PluginScanResult.h"

#include <QJsonArray>

namespace lmms
{

QString pluginScanFormatName(PluginScanFormat format)
{
	switch (format)
	{
		case PluginScanFormat::Vst2: return QStringLiteral("vst2");
		case PluginScanFormat::Vst3: return QStringLiteral("vst3");
		case PluginScanFormat::Lv2: return QStringLiteral("lv2");
	}
	return QStringLiteral("unknown");
}




bool pluginScanFormatFromName(const QString& name, PluginScanFormat& format)
{
	if (name == QLatin1String("vst2")) { format = PluginScanFormat::Vst2; return true; }
	if (name == QLatin1String("vst3")) { format = PluginScanFormat::Vst3; return true; }
	if (name == QLatin1String("lv2")) { format = PluginScanFormat::Lv2; return true; }
	return false;
}




QJsonObject PluginScanEntry::toJson() const
{
	QJsonObject object;
	object.insert(QStringLiteral("format"), pluginScanFormatName(format));
	object.insert(QStringLiteral("path"), path);
	object.insert(QStringLiteral("classId"), classId);
	object.insert(QStringLiteral("uniqueId"), uniqueId);
	object.insert(QStringLiteral("name"), name);
	object.insert(QStringLiteral("vendor"), vendor);
	object.insert(QStringLiteral("version"), version);
	object.insert(QStringLiteral("mtime"), static_cast<double>(mtimeMs));
	object.insert(QStringLiteral("size"), static_cast<double>(size));
	object.insert(QStringLiteral("hash"), hash);
	object.insert(QStringLiteral("isInstrument"), isInstrument);
	object.insert(QStringLiteral("error"), error);

	QJsonArray classArray;
	for (const auto& item : classes)
	{
		QJsonObject classObject;
		classObject.insert(QStringLiteral("id"), item.id);
		classObject.insert(QStringLiteral("name"), item.name);
		classObject.insert(QStringLiteral("isInstrument"), item.isInstrument);
		classArray.append(classObject);
	}
	object.insert(QStringLiteral("classes"), classArray);

	QJsonArray parameterArray;
	for (const auto& item : parameters)
	{
		QJsonObject parameterObject;
		parameterObject.insert(QStringLiteral("id"),
			static_cast<double>(item.id));
		parameterObject.insert(QStringLiteral("name"), item.name);
		parameterObject.insert(QStringLiteral("min"), item.min);
		parameterObject.insert(QStringLiteral("max"), item.max);
		parameterObject.insert(QStringLiteral("default"), item.defaultValue);
		parameterObject.insert(QStringLiteral("steps"), item.stepCount);
		parameterArray.append(parameterObject);
	}
	object.insert(QStringLiteral("parameters"), parameterArray);
	return object;
}




PluginScanEntry PluginScanEntry::fromJson(const QJsonObject& object)
{
	PluginScanEntry entry;
	pluginScanFormatFromName(object.value(QStringLiteral("format")).toString(),
		entry.format);
	entry.path = object.value(QStringLiteral("path")).toString();
	entry.classId = object.value(QStringLiteral("classId")).toString();
	entry.uniqueId = object.value(QStringLiteral("uniqueId")).toString();
	entry.name = object.value(QStringLiteral("name")).toString();
	entry.vendor = object.value(QStringLiteral("vendor")).toString();
	entry.version = object.value(QStringLiteral("version")).toString();
	entry.mtimeMs = static_cast<qint64>(
		object.value(QStringLiteral("mtime")).toDouble());
	entry.size = static_cast<qint64>(
		object.value(QStringLiteral("size")).toDouble());
	entry.hash = object.value(QStringLiteral("hash")).toString();
	entry.isInstrument = object.value(QStringLiteral("isInstrument")).toBool();
	entry.error = object.value(QStringLiteral("error")).toString();

	for (const auto& value : object.value(QStringLiteral("classes")).toArray())
	{
		const auto item = value.toObject();
		PluginScanClass itemClass;
		itemClass.id = item.value(QStringLiteral("id")).toString();
		itemClass.name = item.value(QStringLiteral("name")).toString();
		itemClass.isInstrument =
			item.value(QStringLiteral("isInstrument")).toBool();
		entry.classes.push_back(std::move(itemClass));
	}
	for (const auto& value : object.value(QStringLiteral("parameters")).toArray())
	{
		const auto item = value.toObject();
		PluginScanParameter parameter;
		parameter.id = static_cast<uint32_t>(
			item.value(QStringLiteral("id")).toDouble());
		parameter.name = item.value(QStringLiteral("name")).toString();
		parameter.min = item.value(QStringLiteral("min")).toDouble();
		parameter.max = item.value(QStringLiteral("max")).toDouble(1.0);
		parameter.defaultValue =
			item.value(QStringLiteral("default")).toDouble();
		parameter.stepCount = item.value(QStringLiteral("steps")).toInt();
		entry.parameters.push_back(std::move(parameter));
	}
	return entry;
}

} // namespace lmms
