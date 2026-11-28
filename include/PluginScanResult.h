/*
 * PluginScanResult.h - format-neutral plugin scan result
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

#ifndef LMMS_PLUGIN_SCAN_RESULT_H
#define LMMS_PLUGIN_SCAN_RESULT_H

#include <cstdint>
#include <vector>

#include <QJsonObject>
#include <QString>

namespace lmms
{

enum class PluginScanFormat
{
	Vst2,
	Vst3,
	Lv2,
};

/*! One plugin class inside a bundle or file. VST2/LV2 usually have one. */
struct PluginScanClass
{
	QString id;
	QString name;
	bool isInstrument = false;
};

/*! Parameter metadata discovered while scanning, when the format exposes it. */
struct PluginScanParameter
{
	uint32_t id = 0;
	QString name;
	double min = 0.0;
	double max = 1.0;
	double defaultValue = 0.0;
	int stepCount = 0;
};

/*! Common result for one scanned file or bundle.
 *
 *  The format adapters keep their own runtime metadata; this type is the
 *  serialized contract shared by the scanner processes and the plugin
 *  browser caches.
 */
struct PluginScanEntry
{
	PluginScanFormat format = PluginScanFormat::Vst2;
	QString path;
	QString classId;
	QString uniqueId;
	QString name;
	QString vendor;
	QString product;
	QString version;
	qint64 mtimeMs = 0;
	qint64 size = 0;
	QString hash;
	bool isInstrument = false;
	bool isEffect = false;
	std::vector<PluginScanClass> classes;
	std::vector<PluginScanParameter> parameters;
	//! Empty on success; otherwise the scan/load failure reason.
	QString error;

	QJsonObject toJson() const;
	static PluginScanEntry fromJson(const QJsonObject& object);
};

QString pluginScanFormatName(PluginScanFormat format);
bool pluginScanFormatFromName(const QString& name, PluginScanFormat& format);

} // namespace lmms

#endif // LMMS_PLUGIN_SCAN_RESULT_H
