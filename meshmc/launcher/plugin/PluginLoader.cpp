/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: GPL-3.0-or-later WITH LicenseRef-MeshMC-MMCO-Module-Exception-1.0
 *
 *   MeshMC - A Custom Launcher for Minecraft
 *   Copyright (C) 2026 Project Tick
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version, with the additional permission
 *   described in the MeshMC MMCO Module Exception 1.0.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 *   You should have received a copy of the MeshMC MMCO Module Exception 1.0
 *   along with this program.  If not, see <https://projecttick.org/licenses/>.
 */

#include "plugin/PluginLoader.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QDebug>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

PluginLoader::PluginLoader() = default;
PluginLoader::~PluginLoader() = default;

QStringList PluginLoader::defaultSearchPaths()
{
	QStringList paths;

	// In-tree: next to the binary
	QString appDir = QCoreApplication::applicationDirPath();
	paths << QDir(appDir).filePath("mmcmodules");

	// User-local
#ifdef Q_OS_WIN
	QString localData =
		QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
	if (!localData.isEmpty())
		paths << QDir(localData).filePath("mmcmodules");
#else
	// TODO: Edit this so that the system and user levels differentiate between them in the about dialog.
	// ~/.local/lib/mmcmodules
	QString home = QDir::homePath();
	paths << home + "/.local/lib/mmcmodules";
	paths << home + "/.local/share/MeshMC/mmcmodules";

	// System-wide
	paths << "/usr/local/lib/mmcmodules";
	paths << "/usr/lib/mmcmodules";
	paths << "/usr/local/bin/mmcmodules";
	paths << "/usr/bin/mmcmodules";
#endif

	return paths;
}

QStringList PluginLoader::searchPaths() const
{
	QStringList paths = m_extraPaths;
	paths.append(defaultSearchPaths());
	return paths;
}

void PluginLoader::addSearchPath(const QString& path)
{
	if (!path.isEmpty() && !m_extraPaths.contains(path))
		m_extraPaths.prepend(path);
}

QVector<PluginMetadata> PluginLoader::discoverModules() const
{
	QVector<PluginMetadata> result;
	QSet<QString> seen; // avoid loading the same module twice

	for (const QString& dir : searchPaths()) {
		for (auto& meta : scanDirectory(dir)) {
			QString id = meta.moduleId();
			if (seen.contains(id)) {
				qDebug() << "[PluginLoader] Skipping duplicate module" << id
						 << "from" << meta.filePath;
				if (meta.libraryHandle)
					unloadModule(meta);
				continue;
			}
			seen.insert(id);
			result.append(std::move(meta));
		}
	}

	qDebug() << "[PluginLoader] Discovered" << result.size() << "module(s)";
	return result;
}

QVector<PluginMetadata> PluginLoader::scanDirectory(const QString& dir) const
{
	QVector<PluginMetadata> result;
	QDir d(dir);
	if (!d.exists()) {
		return result;
	}

	qDebug() << "[PluginLoader] Scanning" << dir;

	QDirIterator it(dir, {"*" MMCO_EXTENSION}, QDir::Files,
					QDirIterator::NoIteratorFlags);
	while (it.hasNext()) {
		QString path = it.next();
		auto meta = loadModule(path);
		if (meta.loaded) {
			result.append(std::move(meta));
		}
	}

	return result;
}

PluginMetadata PluginLoader::loadModule(const QString& path) const
{
	PluginMetadata meta;
	meta.filePath = path;

	qDebug() << "[PluginLoader] Loading module:" << path;

	// Open the shared library.
	//
	// RTLD_NODELETE prevents the C runtime from running the module's
	// static destructors during exit().  Plugin .mmco files statically
	// link MeshMC_logic which contains the global `const Config
	// BuildConfig` — a non-trivially-destructible object.  Without
	// RTLD_NODELETE the duplicate BuildConfig inside each .mmco would
	// be destroyed at exit(), corrupting the heap because the main
	// binary's copy was already torn down ("corrupted double-linked
	// list").
#ifdef Q_OS_WIN
	HMODULE handle = LoadLibraryW(reinterpret_cast<LPCWSTR>(path.utf16()));
	if (!handle) {
		qWarning() << "[PluginLoader] Failed to load" << path
				   << "- LoadLibrary error:" << GetLastError();
		return meta;
	}
	meta.libraryHandle = reinterpret_cast<void*>(handle);
#else
	void* handle = dlopen(path.toUtf8().constData(),
						  RTLD_NOW | RTLD_LOCAL | RTLD_NODELETE);
	if (!handle) {
		qWarning() << "[PluginLoader] Failed to load" << path << "-"
				   << dlerror();
		return meta;
	}
	meta.libraryHandle = handle;
#endif

	// Resolve mmco_module_info
#ifdef Q_OS_WIN
	auto* info = reinterpret_cast<MMCOModuleInfo*>(GetProcAddress(
		static_cast<HMODULE>(meta.libraryHandle), "mmco_module_info"));
#else
	auto* info =
		reinterpret_cast<MMCOModuleInfo*>(dlsym(handle, "mmco_module_info"));
#endif

	if (!info) {
		qWarning() << "[PluginLoader]" << path
				   << "missing mmco_module_info symbol";
		unloadModule(meta);
		return meta;
	}

	// Validate magic
	if (info->magic != MMCO_MAGIC) {
		qWarning() << "[PluginLoader]" << path << "bad magic:" << Qt::hex
				   << info->magic << "(expected" << Qt::hex << MMCO_MAGIC
				   << ")";
		unloadModule(meta);
		return meta;
	}

	// Validate ABI version
	if (info->abi_version != MMCO_ABI_VERSION) {
		qWarning() << "[PluginLoader]" << path
				   << "ABI version mismatch:" << info->abi_version
				   << "(expected" << MMCO_ABI_VERSION << ")";
		unloadModule(meta);
		return meta;
	}

	meta.moduleInfo = info;
	meta.name = QString::fromUtf8(info->name ? info->name : "");
	meta.version = QString::fromUtf8(info->version ? info->version : "");
	meta.author = QString::fromUtf8(info->author ? info->author : "");
	meta.description =
		QString::fromUtf8(info->description ? info->description : "");
	meta.license = QString::fromUtf8(info->license ? info->license : "");
	meta.codeLink = QString::fromUtf8(info->code_link ? info->code_link : "");
	meta.flags = info->flags;

	// Resolve mmco_init
#ifdef Q_OS_WIN
	meta.initFunc = reinterpret_cast<PluginMetadata::InitFunc>(
		GetProcAddress(static_cast<HMODULE>(meta.libraryHandle), "mmco_init"));
	meta.unloadFunc =
		reinterpret_cast<PluginMetadata::UnloadFunc>(GetProcAddress(
			static_cast<HMODULE>(meta.libraryHandle), "mmco_unload"));
#else
	meta.initFunc =
		reinterpret_cast<PluginMetadata::InitFunc>(dlsym(handle, "mmco_init"));
	meta.unloadFunc = reinterpret_cast<PluginMetadata::UnloadFunc>(
		dlsym(handle, "mmco_unload"));
#endif

	if (!meta.initFunc) {
		qWarning() << "[PluginLoader]" << path << "missing mmco_init symbol";
		unloadModule(meta);
		return meta;
	}

	if (!meta.unloadFunc) {
		qWarning() << "[PluginLoader]" << path << "missing mmco_unload symbol";
		unloadModule(meta);
		return meta;
	}

	meta.loaded = true;
	qDebug() << "[PluginLoader] Loaded module:" << meta.name << "v"
			 << meta.version << "by" << meta.author;

	return meta;
}

void PluginLoader::unloadModule(PluginMetadata& meta)
{
	if (!meta.libraryHandle)
		return;

#ifdef Q_OS_WIN
	FreeLibrary(static_cast<HMODULE>(meta.libraryHandle));
#else
	dlclose(meta.libraryHandle);
#endif

	meta.libraryHandle = nullptr;
	meta.moduleInfo = nullptr;
	meta.initFunc = nullptr;
	meta.unloadFunc = nullptr;
	meta.loaded = false;
	meta.initialized = false;
}
