/*
 * Vst3SubPluginFeatures.h - sub-plugin features for VST3 classes
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

#ifndef LMMS_VST3_SUBPLUGIN_FEATURES_H
#define LMMS_VST3_SUBPLUGIN_FEATURES_H

#include "Plugin.h"
#include "vst3base_export.h"

namespace lmms
{

/**
 * Exposes the classes found by Vst3List to the plugin browser and the
 * effect selection dialog. The VST3 instrument and effect plugins use this
 * while they are being introduced.
 */
class VST3BASE_EXPORT Vst3SubPluginFeatures
	: public Plugin::Descriptor::SubPluginFeatures
{
public:
	explicit Vst3SubPluginFeatures(Plugin::Type type);

	void fillDescriptionWidget(QWidget* parent, const Key* key) const override;

	void listSubPluginKeys(const Plugin::Descriptor* desc, KeyList& keyList) const override;

	bool canRescan() const override;
	void rescan() override;

	QString displayName(const Key& key) const override;
	QString description(const Key& key) const override;

private:
	Plugin::Type m_pluginType;
};

} // namespace lmms

#endif // LMMS_VST3_SUBPLUGIN_FEATURES_H
