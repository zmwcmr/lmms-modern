/*
 * Vst3Module.h - loader for VST3 module bundles
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

#ifndef LMMS_VST3_MODULE_H
#define LMMS_VST3_MODULE_H

#include <filesystem>

#include <QString>

#include "travesty/factory.h"

namespace lmms
{

/**
 * Loads a VST3 bundle with the platform's dynamic loader and exposes the
 * plugin factory. This class is intentionally only used by the scanner and
 * by the out-of-process host; the LMMS GUI process must never load a plugin
 * module directly.
 */
class Vst3Module
{
public:
	Vst3Module() = default;
	~Vst3Module();

	Vst3Module(const Vst3Module&) = delete;
	Vst3Module& operator=(const Vst3Module&) = delete;

	/**
	 * Locate the shared library inside a VST3 bundle. On Linux and Windows a
	 * bundle is a directory, on macOS it is an application bundle. A plain
	 * shared library path is returned unchanged.
	 */
	static std::filesystem::path resolveBundleBinary(
		const std::filesystem::path& bundlePath);

	bool load(const std::filesystem::path& bundlePath, QString* error = nullptr);
	void unload();

	bool isLoaded() const { return m_module != nullptr; }
	const std::filesystem::path& bundlePath() const { return m_bundlePath; }
	const std::filesystem::path& binaryPath() const { return m_binaryPath; }

	v3_plugin_factory** factory() const { return m_factory; }
	v3_plugin_factory_2** factory2() const { return m_factory2; }
	v3_plugin_factory_3** factory3() const { return m_factory3; }

private:
	void* m_module = nullptr;
	void (*m_exit)() = nullptr;
	std::filesystem::path m_bundlePath;
	std::filesystem::path m_binaryPath;

	v3_plugin_factory** m_factory = nullptr;
	v3_plugin_factory_2** m_factory2 = nullptr;
	v3_plugin_factory_3** m_factory3 = nullptr;

	//! Called from the destructor; does not release the factories because
	//! their owner is the caller.
	void closeModule();
};

} // namespace lmms

#endif // LMMS_VST3_MODULE_H
