/*
 * Vst3Module.cpp - loader for VST3 module bundles
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

#include "Vst3Module.h"

#include <system_error>


#include "travesty/base.h"

#if defined(Q_OS_WIN)
#	include <windows.h>
#elif defined(Q_OS_MACOS)
#	include <CoreFoundation/CoreFoundation.h>
#else
#	include <dlfcn.h>
#endif

namespace fs = std::filesystem;

namespace
{

QString architectureName()
{
#if defined(Q_OS_WIN)
#	if defined(_M_X64) || defined(__x86_64__)
	return QStringLiteral("x86_64-win");
#	elif defined(_M_ARM64) || defined(__aarch64__)
	return QStringLiteral("aarch64-win");
#	elif defined(_M_IX86) || defined(__i386__)
	return QStringLiteral("x86-win");
#	else
	return QStringLiteral("unknown-win");
#	endif
#elif defined(Q_OS_MACOS)
	return QStringLiteral("MacOS");
#else
#	if defined(__x86_64__)
	return QStringLiteral("x86_64-linux");
#	elif defined(__aarch64__)
	return QStringLiteral("aarch64-linux");
#	elif defined(__i386__)
	return QStringLiteral("i386-linux");
#	elif defined(__arm__)
	return QStringLiteral("armv7l-linux");
#	elif defined(__riscv) && __riscv_xlen == 64
	return QStringLiteral("riscv64-linux");
#	else
	return QStringLiteral("unknown-linux");
#	endif
#endif
}

QString binarySuffix()
{
#if defined(Q_OS_WIN)
	return QStringLiteral(".vst3");
#elif defined(Q_OS_MACOS)
	return QString();
#else
	return QStringLiteral(".so");
#endif
}

void setError(QString* error, const QString& message)
{
	if (error != nullptr)
	{
		*error = message;
	}
}

} // namespace

namespace lmms
{

Vst3Module::~Vst3Module()
{
	closeModule();
}

std::filesystem::path Vst3Module::resolveBundleBinary(
	const std::filesystem::path& bundlePath)
{
	std::error_code ec;
	if (fs::is_regular_file(bundlePath, ec))
	{
		return bundlePath;
	}
	if (!fs::is_directory(bundlePath, ec))
	{
		return {};
	}

	const auto bundleName = bundlePath.stem().string();
#if defined(Q_OS_MACOS)
	const auto candidate = bundlePath / "Contents" / "MacOS" / bundleName;
#else
	const auto contentDir = bundlePath / "Contents" / architectureName().toStdString();
	const auto candidate = contentDir / (bundleName + binarySuffix().toStdString());
#endif

	if (fs::is_regular_file(candidate, ec))
	{
		return candidate;
	}

	// Some bundles do not follow the recommended executable name exactly.
	// Accept the first matching binary in the architecture directory.
	const auto searchDir = candidate.parent_path();
	fs::directory_iterator it(searchDir, fs::directory_options::skip_permission_denied, ec);
	if (ec)
	{
		return {};
	}
	for (const auto& entry : it)
	{
		std::error_code entryError;
		if (entry.is_regular_file(entryError) && !entryError)
		{
			const auto extension = entry.path().extension().string();
			if (extension.empty() || extension == ".so" || extension == ".vst3"
				|| extension == ".dll")
			{
				return entry.path();
			}
		}
	}
	return {};
}

bool Vst3Module::load(const std::filesystem::path& bundlePath, QString* error)
{
	unload();

	m_bundlePath = bundlePath;
	m_binaryPath = resolveBundleBinary(bundlePath);
	if (m_binaryPath.empty())
	{
		setError(error, QStringLiteral("No VST3 module binary found in %1")
			.arg(QString::fromStdString(bundlePath.string())));
		return false;
	}

#if defined(Q_OS_WIN)
	m_module = LoadLibraryW(m_binaryPath.wstring().c_str());
	if (m_module == nullptr)
	{
		setError(error, QStringLiteral("Failed to load %1 (error %2)")
			.arg(QString::fromStdWString(m_binaryPath.wstring()))
			.arg(static_cast<qulonglong>(GetLastError())));
		return false;
	}
	auto entry = reinterpret_cast<v3_result (*)()>(
		GetProcAddress(static_cast<HMODULE>(m_module), "InitDll"));
	auto exitFn = reinterpret_cast<void (*)()>(
		GetProcAddress(static_cast<HMODULE>(m_module), "ExitDll"));
	auto getFactory = reinterpret_cast<v3_plugin_factory** (*)()>(
		GetProcAddress(static_cast<HMODULE>(m_module), "GetPluginFactory"));
#elif defined(Q_OS_MACOS)
	const auto binaryPath = m_binaryPath.string();
	CFURLRef url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
		reinterpret_cast<const UInt8*>(binaryPath.c_str()),
		static_cast<CFIndex>(binaryPath.size()), false);
	CFBundleRef bundle = url != nullptr
		? CFBundleCreate(kCFAllocatorDefault, url)
		: nullptr;
	if (url != nullptr)
	{
		CFRelease(url);
	}
	if (bundle == nullptr || !CFBundleLoadExecutable(bundle))
	{
		if (bundle != nullptr)
		{
			CFRelease(bundle);
		}
		setError(error, QStringLiteral("Failed to load %1")
			.arg(QString::fromStdString(binaryPath)));
		return false;
	}
	m_module = bundle;
	auto entry = reinterpret_cast<v3_result (*)(void*)>(
		CFBundleGetFunctionPointerForName(bundle, CFSTR("bundleEntry")));
	auto exitFn = reinterpret_cast<void (*)()>(
		CFBundleGetFunctionPointerForName(bundle, CFSTR("bundleExit")));
	auto getFactory = reinterpret_cast<v3_plugin_factory** (*)()>(
		CFBundleGetFunctionPointerForName(bundle, CFSTR("GetPluginFactory")));
#else
	m_module = dlopen(m_binaryPath.c_str(), RTLD_NOW | RTLD_LOCAL);
	if (m_module == nullptr)
	{
		const char* dlError = dlerror();
		setError(error, QStringLiteral("Failed to load %1: %2")
			.arg(QString::fromStdString(m_binaryPath.string()),
				QString::fromLocal8Bit(dlError != nullptr ? dlError : "unknown error")));
		return false;
	}
	auto entry = reinterpret_cast<v3_result (*)(void*)>(dlsym(m_module, "ModuleEntry"));
	auto exitFn = reinterpret_cast<void (*)()>(dlsym(m_module, "ModuleExit"));
	auto getFactory = reinterpret_cast<v3_plugin_factory** (*)()>(
		dlsym(m_module, "GetPluginFactory"));
#endif

	if (entry == nullptr || getFactory == nullptr)
	{
		setError(error, QStringLiteral("%1 is not a valid VST3 module")
			.arg(QString::fromStdString(m_binaryPath.string())));
		closeModule();
		return false;
	}

#if defined(Q_OS_WIN)
	entry();
#elif defined(Q_OS_MACOS)
	entry(m_module);
#else
	entry(m_module);
#endif
	// Only call the exit symbol after a successful entry call.
	m_exit = exitFn;

	m_factory = getFactory();
	if (m_factory == nullptr)
	{
		setError(error, QStringLiteral("%1 did not provide a plugin factory")
			.arg(QString::fromStdString(m_binaryPath.string())));
		closeModule();
		return false;
	}

	if (v3_cpp_obj_query_interface(m_factory, v3_plugin_factory_2_iid, &m_factory2) != V3_OK)
	{
		m_factory2 = nullptr;
	}
	if (m_factory2 != nullptr
		&& v3_cpp_obj_query_interface(m_factory2, v3_plugin_factory_3_iid, &m_factory3) != V3_OK)
	{
		m_factory3 = nullptr;
	}

	return true;
}

void Vst3Module::unload()
{
	// Factories are released by the caller before the module is unloaded.
	m_factory = nullptr;
	m_factory2 = nullptr;
	m_factory3 = nullptr;
	closeModule();
	m_bundlePath.clear();
	m_binaryPath.clear();
}

void Vst3Module::closeModule()
{
	if (m_module == nullptr)
	{
		return;
	}
	if (m_exit != nullptr)
	{
		m_exit();
		m_exit = nullptr;
	}
#if defined(Q_OS_WIN)
	FreeLibrary(static_cast<HMODULE>(m_module));
#elif defined(Q_OS_MACOS)
	auto* const bundle = static_cast<CFBundleRef>(m_module);
	CFBundleUnloadExecutable(bundle);
	CFRelease(bundle);
#else
	dlclose(m_module);
#endif
	m_module = nullptr;
}

} // namespace lmms
