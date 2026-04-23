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
 *
 *  This file incorporates work covered by the following copyright and
 *  permission notice:
 *
 * Copyright 2013-2021 MultiMC Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "InstanceImportTask.h"
#include "BaseInstance.h"
#include "FileSystem.h"
#include "Application.h"
#include "MMCZip.h"
#include "NullInstance.h"
#include "settings/INISettingsObject.h"
#include "icons/IconUtils.h"
#include <QRegularExpression>
#include <QtConcurrentRun>

// FIXME: this does not belong here, it's Minecraft/Flame specific
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "modplatform/flame/FileResolvingTask.h"
#include "modplatform/flame/PackManifest.h"
#include "modplatform/modrinth/ModrinthPackManifest.h"
#include "Json.h"
#include "modplatform/technic/TechnicPackProcessor.h"

#include "icons/IconList.h"
#include "Application.h"
#include "ui/dialogs/BlockedModsDialog.h"

#include <QDir>
#include <QStandardPaths>

InstanceImportTask::InstanceImportTask(const QUrl sourceUrl)
{
	m_sourceUrl = sourceUrl;
}

void InstanceImportTask::executeTask()
{
	if (m_sourceUrl.isLocalFile()) {
		m_archivePath = m_sourceUrl.toLocalFile();
		processZipPack();
	} else {
		setStatus(tr("Downloading modpack:\n%1").arg(m_sourceUrl.toString()));
		m_downloadRequired = true;

		const QString path = m_sourceUrl.host() + '/' + m_sourceUrl.path();
		auto entry = APPLICATION->metacache()->resolveEntry("general", path);
		entry->setStale(true);
		m_filesNetJob =
			new NetJob(tr("Modpack download"), APPLICATION->network());
		m_filesNetJob->addNetAction(
			Net::Download::makeCached(m_sourceUrl, entry));
		m_archivePath = entry->getFullPath();
		auto job = m_filesNetJob.get();
		connect(job, &NetJob::succeeded, this,
				&InstanceImportTask::downloadSucceeded);
		connect(job, &NetJob::progress, this,
				&InstanceImportTask::downloadProgressChanged);
		connect(job, &NetJob::failed, this,
				&InstanceImportTask::downloadFailed);
		m_filesNetJob->start();
	}
}

void InstanceImportTask::downloadSucceeded()
{
	processZipPack();
	m_filesNetJob.reset();
}

void InstanceImportTask::downloadFailed(QString reason)
{
	emitFailed(reason);
	m_filesNetJob.reset();
}

void InstanceImportTask::downloadProgressChanged(qint64 current, qint64 total)
{
	setProgress(current / 2, total);
}

void InstanceImportTask::processZipPack()
{
	setStatus(tr("Inspecting modpack archive"));
	qDebug() << "Detecting modpack type for" << m_archivePath;

	// Run ALL archive scanning in a background thread so the UI stays
	// responsive. We also do a single listEntries() pass instead of
	// calling findFolderOfFileInZip() / entryExists() separately (each of
	// which would re-open the archive, and findFolderOfFileInZip is
	// recursive so it opens the archive once per directory level).
	const QString archivePath = m_archivePath;
	const QString stagingPath = m_stagingPath;

	m_detectFuture = QtConcurrent::run([archivePath, stagingPath]() {
		DetectResult result;
		result.extractTarget = stagingPath;

		// Single archive open + single linear scan over all entries.
		const QStringList entries = MMCZip::listEntries(archivePath);

		// Helper: find the folder prefix (e.g. "root/") for the first
		// entry whose filename component equals `needle`.
		// Returns "" (empty, non-null) if found at root.
		// Returns QString() (null) if not found.
		auto findFolder = [&entries](const QString& needle) -> QString {
			for (const QString& e : entries) {
				if (e == needle)
					return QLatin1String("");  // found at root
				if (e.endsWith(QLatin1Char('/') + needle)) {
					return e.left(e.lastIndexOf(QLatin1Char('/')) + 1);
				}
			}
			return {};  // null → not found
		};

		result.mmcRoot = findFolder(QStringLiteral("instance.cfg"));
		result.modrinthRoot =
			findFolder(QStringLiteral("modrinth.index.json"));
		result.flameRoot = findFolder(QStringLiteral("manifest.json"));
		result.technicFound =
			entries.contains(QStringLiteral("bin/modpack.jar")) ||
			entries.contains(QStringLiteral("bin/version.json")) ||
			entries.contains(QStringLiteral("bin/modpack.jar/")) ||
			entries.contains(QStringLiteral("bin/version.json/"));

		// For Technic, create the target subdir now (background thread is
		// fine for filesystem ops).
		if (result.technicFound && result.mmcRoot.isNull() &&
			result.modrinthRoot.isNull()) {
			QDir extractDir(stagingPath);
			extractDir.mkpath(QStringLiteral(".minecraft"));
			result.extractTarget =
				extractDir.absoluteFilePath(QStringLiteral(".minecraft"));
		}

		return result;
	});

	connect(&m_detectFutureWatcher, &QFutureWatcher<DetectResult>::finished,
			this, &InstanceImportTask::detectFinished);
	m_detectFutureWatcher.setFuture(m_detectFuture);
}

void InstanceImportTask::detectFinished()
{
	const DetectResult r = m_detectFuture.result();

	// Determine pack type (same priority order as before).
	QString root;
	if (!r.mmcRoot.isNull()) {
		qDebug() << "MeshMC:" << r.mmcRoot;
		m_modpackType = ModpackType::MeshMC;
		root = r.mmcRoot;
	} else if (!r.modrinthRoot.isNull()) {
		qDebug() << "Modrinth:" << r.modrinthRoot;
		m_modpackType = ModpackType::Modrinth;
		root = r.modrinthRoot;
	} else if (r.technicFound) {
		qDebug() << "Technic";
		m_modpackType = ModpackType::Technic;
		// root stays empty — extractSubDir extracts everything
	} else if (!r.flameRoot.isNull()) {
		qDebug() << "Flame:" << r.flameRoot;
		m_modpackType = ModpackType::Flame;
		root = r.flameRoot;
	}

	if (m_modpackType == ModpackType::Unknown) {
		QFileInfo fi(m_archivePath);
		if (!fi.exists() || fi.size() == 0) {
			emitFailed(tr("Modpack archive is missing or empty:\n%1")
						   .arg(m_archivePath));
		} else {
			emitFailed(
				tr("Archive does not contain a recognized modpack type."));
		}
		return;
	}

	setStatus(tr("Extracting modpack"));
	const QString archivePath = m_archivePath;
	m_extractFuture =
		QtConcurrent::run(QThreadPool::globalInstance(), MMCZip::extractSubDir,
						  archivePath, root, r.extractTarget);
	connect(&m_extractFutureWatcher,
			&QFutureWatcher<nonstd::optional<QStringList>>::finished, this,
			&InstanceImportTask::extractFinished);
	connect(&m_extractFutureWatcher,
			&QFutureWatcher<nonstd::optional<QStringList>>::canceled, this,
			&InstanceImportTask::extractAborted);
	m_extractFutureWatcher.setFuture(m_extractFuture);
}

void InstanceImportTask::extractFinished()
{
	if (!m_extractFuture.result()) {
		emitFailed(tr("Failed to extract modpack"));
		return;
	}
	QDir extractDir(m_stagingPath);

	qDebug() << "Fixing permissions for extracted pack files...";
	QDirIterator it(extractDir, QDirIterator::Subdirectories);
	while (it.hasNext()) {
		auto filepath = it.next();
		QFileInfo file(filepath);
		auto permissions = QFile::permissions(filepath);
		auto origPermissions = permissions;
		if (file.isDir()) {
			// Folder +rwx for current user
			permissions |= QFileDevice::Permission::ReadUser |
						   QFileDevice::Permission::WriteUser |
						   QFileDevice::Permission::ExeUser;
		} else {
			// File +rw for current user
			permissions |= QFileDevice::Permission::ReadUser |
						   QFileDevice::Permission::WriteUser;
		}
		if (origPermissions != permissions) {
			if (!QFile::setPermissions(filepath, permissions)) {
				logWarning(
					tr("Could not fix permissions for %1").arg(filepath));
			} else {
				qDebug() << "Fixed" << filepath;
			}
		}
	}

	switch (m_modpackType) {
		case ModpackType::Flame:
			processFlame();
			return;
		case ModpackType::Modrinth:
			processModrinth();
			return;
		case ModpackType::MeshMC:
			processMeshMC();
			return;
		case ModpackType::Technic:
			processTechnic();
			return;
		case ModpackType::Unknown:
			emitFailed(
				tr("Archive does not contain a recognized modpack type."));
			return;
	}
}

void InstanceImportTask::extractAborted()
{
	emitFailed(tr("Instance import has been aborted."));
	return;
}

void InstanceImportTask::processFlame()
{
	Flame::Manifest pack;
	try {
		QString configPath = FS::PathCombine(m_stagingPath, "manifest.json");
		Flame::loadManifest(pack, configPath);
		if (!QFile::remove(configPath)) {
			qWarning() << "Could not remove manifest.json from staging";
		}
	} catch (const JSONValidationError& e) {
		emitFailed(tr("Could not understand pack manifest:\n") + e.cause());
		return;
	}
	if (!pack.overrides.isEmpty()) {
		QString overridePath = FS::PathCombine(m_stagingPath, pack.overrides);
		if (QFile::exists(overridePath)) {
			QString mcPath = FS::PathCombine(m_stagingPath, "minecraft");
			if (!QFile::rename(overridePath, mcPath)) {
				emitFailed(tr("Could not rename the overrides folder:\n") +
						   pack.overrides);
				return;
			}
		} else {
			logWarning(tr("The specified overrides folder (%1) is missing. "
						  "Maybe the modpack was already used before?")
						   .arg(pack.overrides));
		}
	}

	configureFlameInstance(pack);

	m_modIdResolver =
		new Flame::FileResolvingTask(APPLICATION->network(), pack);
	connect(m_modIdResolver.get(), &Flame::FileResolvingTask::succeeded, this,
			&InstanceImportTask::onFlameFileResolutionSucceeded);
	connect(m_modIdResolver.get(), &Flame::FileResolvingTask::failed,
			[&](QString reason) {
				m_modIdResolver.reset();
				emitFailed(tr("Unable to resolve mod IDs:\n") + reason);
			});
	connect(m_modIdResolver.get(), &Flame::FileResolvingTask::progress,
			[&](qint64 current, qint64 total) { setProgress(current, total); });
	connect(m_modIdResolver.get(), &Flame::FileResolvingTask::status,
			[&](QString status) { setStatus(status); });
	m_modIdResolver->start();
}

static QString selectFlameIcon(const QString& instIcon,
							   const Flame::Manifest& pack)
{
	if (instIcon != "default")
		return instIcon;
	if (pack.name.contains("Direwolf20"))
		return "steve";
	if (pack.name.contains("FTB") || pack.name.contains("Feed The Beast"))
		return "ftb_logo";
	// default to something other than the MeshMC default to distinguish these
	return "flame";
}

void InstanceImportTask::configureFlameInstance(Flame::Manifest& pack)
{
	const static QMap<QString, QString> forgemap = {{"1.2.5", "3.4.9.171"},
													{"1.4.2", "6.0.1.355"},
													{"1.4.7", "6.6.2.534"},
													{"1.5.2", "7.8.1.737"}};

	struct FlameLoaderMapping {
		const char* prefix;
		QString version;
		const char* componentId;
	};
	FlameLoaderMapping loaderMappings[] = {
		{"forge-", {}, "net.minecraftforge"},
		{"fabric-", {}, "net.fabricmc.fabric-loader"},
		{"neoforge-", {}, "net.neoforged"},
		{"quilt-", {}, "org.quiltmc.quilt-loader"},
	};
	for (auto& loader : pack.minecraft.modLoaders) {
		auto id = loader.id;
		bool matched = false;
		for (auto& mapping : loaderMappings) {
			if (id.startsWith(mapping.prefix)) {
				id.remove(mapping.prefix);
				mapping.version = id;
				matched = true;
				break;
			}
		}
		if (!matched) {
			logWarning(tr("Unknown mod loader in manifest: %1").arg(id));
		}
	}

	QString configPath = FS::PathCombine(m_stagingPath, "instance.cfg");
	auto instanceSettings = std::make_shared<INISettingsObject>(configPath);
	instanceSettings->registerSetting("InstanceType", "Legacy");
	instanceSettings->set("InstanceType", "OneSix");
	MinecraftInstance instance(m_globalSettings, instanceSettings,
							   m_stagingPath);
	auto mcVersion = pack.minecraft.version;
	// Hack to correct some 'special sauce'...
	if (mcVersion.endsWith('.')) {
		mcVersion.remove(QRegularExpression("[.]+$"));
		logWarning(tr("Mysterious trailing dots removed from Minecraft version "
					  "while importing pack."));
	}
	auto components = instance.getPackProfile();
	components->buildingFromScratch();
	components->setComponentVersion("net.minecraft", mcVersion, true);

	// Handle Forge "recommended" version mapping
	auto& forgeMapping = loaderMappings[0];
	if (forgeMapping.version == "recommended") {
		if (forgemap.contains(mcVersion)) {
			forgeMapping.version = forgemap[mcVersion];
		} else {
			logWarning(
				tr("Could not map recommended forge version for Minecraft %1")
					.arg(mcVersion));
		}
	}

	for (const auto& mapping : loaderMappings) {
		if (!mapping.version.isEmpty()) {
			components->setComponentVersion(mapping.componentId,
											mapping.version);
		}
	}

	instance.setIconKey(selectFlameIcon(m_instIcon, pack));

	QString jarmodsPath =
		FS::PathCombine(m_stagingPath, "minecraft", "jarmods");
	QFileInfo jarmodsInfo(jarmodsPath);
	if (jarmodsInfo.isDir()) {
		// install all the jar mods
		qDebug() << "Found jarmods:";
		QDir jarmodsDir(jarmodsPath);
		QStringList jarMods;
		for (auto info :
			 jarmodsDir.entryInfoList(QDir::NoDotAndDotDot | QDir::Files)) {
			qDebug() << info.fileName();
			jarMods.push_back(info.absoluteFilePath());
		}
		auto profile = instance.getPackProfile();
		profile->installJarMods(jarMods);
		// nuke the original files
		FS::deletePath(jarmodsPath);
	}
	instance.setName(m_instName);
}

void InstanceImportTask::onFlameFileResolutionSucceeded()
{
	auto results = m_modIdResolver->getResults();
	m_filesNetJob = new NetJob(tr("Mod download"), APPLICATION->network());

	// Collect restricted mods that need browser download
	QList<BlockedMod> blockedMods;

	for (auto result : results.files) {
		QString filename = result.fileName;
		if (!result.required) {
			filename += ".disabled";
		}

		auto relpath =
			FS::PathCombine("minecraft", result.targetFolder, filename);
		auto path = FS::PathCombine(m_stagingPath, relpath);

		switch (result.type) {
			case Flame::File::Type::Folder: {
				logWarning(
					tr("This 'Folder' may need extracting: %1").arg(relpath));
				[[fallthrough]];
			}
			case Flame::File::Type::SingleFile:
				[[fallthrough]];
			case Flame::File::Type::Mod: {
				bool isBlocked = !result.resolved || !result.url.isValid() ||
								 result.url.isEmpty();
				if (isBlocked && !result.fileName.isEmpty()) {
					blockedMods.append({result.projectId, result.fileId,
										result.fileName, path, false});
					break;
				}
				if (isBlocked) {
					logWarning(tr("Skipping mod %1 (project %2) - no download "
								  "URL and no filename available")
								   .arg(result.fileId)
								   .arg(result.projectId));
					break;
				}
				qDebug() << "Will download" << result.url << "to" << path;
				auto dl = Net::Download::makeFile(result.url, path);
				m_filesNetJob->addNetAction(dl);
				break;
			}
			case Flame::File::Type::Modpack:
				logWarning(tr("Nesting modpacks in modpacks is not "
							  "implemented, nothing was downloaded: %1")
							   .arg(relpath));
				break;
			case Flame::File::Type::Cmod2:
				[[fallthrough]];
			case Flame::File::Type::Ctoc:
				[[fallthrough]];
			case Flame::File::Type::Unknown:
				logWarning(tr("Unrecognized/unhandled PackageType for: %1")
							   .arg(relpath));
				break;
		}
	}

	// Handle restricted mods via dialog
	if (!blockedMods.isEmpty()) {
		BlockedModsDialog dlg(nullptr, tr("Restricted Mods"),
							  tr("The following mods have restricted downloads "
								 "and are not available through the API.\n"
								 "Click the Download button next to each mod "
								 "to open its download page in your browser.\n"
								 "Once all files appear in your Downloads "
								 "folder, click Continue."),
							  blockedMods);

		if (dlg.exec() == QDialog::Accepted) {
			QString downloadDir = QStandardPaths::writableLocation(
				QStandardPaths::DownloadLocation);
			for (const auto& mod : blockedMods) {
				if (mod.found) {
					QString srcPath =
						FS::PathCombine(downloadDir, mod.fileName);
					QFileInfo targetInfo(mod.targetPath);
					QDir().mkpath(targetInfo.absolutePath());

					if (QFile::copy(srcPath, mod.targetPath)) {
						qDebug() << "Copied restricted mod:" << mod.fileName;
					} else {
						logWarning(tr("Failed to copy %1 from downloads folder")
									   .arg(mod.fileName));
					}
				}
			}
		} else {
			logWarning(tr("User cancelled restricted mod downloads - %1 mod(s) "
						  "will be missing")
						   .arg(blockedMods.size()));
		}
	}

	m_modIdResolver.reset();
	connect(m_filesNetJob.get(), &NetJob::succeeded, this, [&]() {
		emitSucceeded();
		m_filesNetJob.reset();
	});
	connect(m_filesNetJob.get(), &NetJob::failed, [&](QString reason) {
		emitFailed(reason);
		m_filesNetJob.reset();
	});
	connect(m_filesNetJob.get(), &NetJob::progress,
			[&](qint64 current, qint64 total) { setProgress(current, total); });
	setStatus(tr("Downloading mods..."));
	m_filesNetJob->start();
}

static void applyModrinthOverrides(const QString& stagingPath,
								   const QString& mcPath)
{
	QString overridePath = FS::PathCombine(stagingPath, "overrides");
	QString clientOverridePath =
		FS::PathCombine(stagingPath, "client-overrides");

	if (QFile::exists(overridePath)) {
		if (!FS::copy(overridePath, mcPath)()) {
			qWarning() << "Could not apply overrides from the modpack.";
		}
		FS::deletePath(overridePath);
	}

	if (QFile::exists(clientOverridePath)) {
		if (!FS::copy(clientOverridePath, mcPath)()) {
			qWarning() << "Could not apply client-overrides from the modpack.";
		}
		FS::deletePath(clientOverridePath);
	}
}

void InstanceImportTask::processModrinth()
{
	Modrinth::Manifest pack;
	try {
		QString configPath =
			FS::PathCombine(m_stagingPath, "modrinth.index.json");
		Modrinth::loadManifest(pack, configPath);
		if (!QFile::remove(configPath)) {
			qWarning() << "Could not remove modrinth.index.json from staging";
		}
	} catch (const JSONValidationError& e) {
		emitFailed(tr("Could not understand Modrinth modpack manifest:\n") +
				   e.cause());
		return;
	}

	// Move overrides folder contents to minecraft directory
	QString mcPath = FS::PathCombine(m_stagingPath, "minecraft");

	QDir mcDir(mcPath);
	if (!mcDir.exists()) {
		mcDir.mkpath(".");
	}

	applyModrinthOverrides(m_stagingPath, mcPath);

	// Create instance config
	QString configPath = FS::PathCombine(m_stagingPath, "instance.cfg");
	auto instanceSettings = std::make_shared<INISettingsObject>(configPath);
	instanceSettings->registerSetting("InstanceType", "Legacy");
	instanceSettings->set("InstanceType", "OneSix");
	MinecraftInstance instance(m_globalSettings, instanceSettings,
							   m_stagingPath);

	auto components = instance.getPackProfile();
	components->buildingFromScratch();

	struct ModLoaderMapping {
		const QString& version;
		const char* componentId;
	};
	const ModLoaderMapping loaders[] = {
		{pack.forgeVersion, "net.minecraftforge"},
		{pack.fabricVersion, "net.fabricmc.fabric-loader"},
		{pack.quiltVersion, "org.quiltmc.quilt-loader"},
		{pack.neoForgeVersion, "net.neoforged"},
	};

	if (!pack.minecraftVersion.isEmpty()) {
		components->setComponentVersion("net.minecraft", pack.minecraftVersion,
										true);
	}
	for (const auto& loader : loaders) {
		if (!loader.version.isEmpty()) {
			components->setComponentVersion(loader.componentId, loader.version);
		}
	}

	if (m_instIcon != "default") {
		instance.setIconKey(m_instIcon);
	} else {
		instance.setIconKey("modrinth");
	}

	instance.setName(m_instName);

	// Download all mod files
	m_filesNetJob =
		new NetJob(tr("Modrinth mod download"), APPLICATION->network());
	auto minecraftDir = FS::PathCombine(m_stagingPath, "minecraft");
	auto canonicalBase = QDir(minecraftDir).canonicalPath();
	for (auto& file : pack.files) {
		if (file.path.contains("..") || QDir::isAbsolutePath(file.path)) {
			qWarning() << "Skipping potentially malicious file path:"
					   << file.path;
			continue;
		}
		auto path = FS::PathCombine(minecraftDir, file.path);
		auto canonicalDir = QFileInfo(path).absolutePath();
		if (!canonicalDir.startsWith(canonicalBase)) {
			qWarning() << "Skipping file path that escapes staging directory:"
					   << file.path;
			continue;
		}
		if (!file.downloadUrl.isValid() || file.downloadUrl.isEmpty()) {
			logWarning(
				tr("Skipping file with no download URL: %1").arg(file.path));
			continue;
		}
		qDebug() << "Will download" << file.downloadUrl << "to" << path;
		auto dl = Net::Download::makeFile(file.downloadUrl, path);
		m_filesNetJob->addNetAction(dl);
	}

	connect(m_filesNetJob.get(), &NetJob::succeeded, this, [&]() {
		emitSucceeded();
		m_filesNetJob.reset();
	});
	connect(m_filesNetJob.get(), &NetJob::failed, [&](QString reason) {
		emitFailed(reason);
		m_filesNetJob.reset();
	});
	connect(m_filesNetJob.get(), &NetJob::progress,
			[&](qint64 current, qint64 total) { setProgress(current, total); });

	setStatus(tr("Downloading mods..."));
	m_filesNetJob->start();
}

void InstanceImportTask::processTechnic()
{
	shared_qobject_ptr<Technic::TechnicPackProcessor> packProcessor =
		new Technic::TechnicPackProcessor();
	connect(packProcessor.get(), &Technic::TechnicPackProcessor::succeeded,
			this, &InstanceImportTask::emitSucceeded);
	connect(packProcessor.get(), &Technic::TechnicPackProcessor::failed, this,
			&InstanceImportTask::emitFailed);
	packProcessor->run(m_globalSettings, m_instName, m_instIcon, m_stagingPath);
}

void InstanceImportTask::processMeshMC()
{
	QString configPath = FS::PathCombine(m_stagingPath, "instance.cfg");
	auto instanceSettings = std::make_shared<INISettingsObject>(configPath);
	instanceSettings->registerSetting("InstanceType", "Legacy");

	NullInstance instance(m_globalSettings, instanceSettings, m_stagingPath);

	// reset time played on import... because packs.
	instance.resetTimePlayed();

	// Set a new name for the imported instance
	instance.setName(m_instName);

	// Use user-specified icon if available, otherwise import from the pack
	if (m_instIcon != "default") {
		instance.setIconKey(m_instIcon);
	} else {
		m_instIcon = instance.iconKey();

		auto importIconPath =
			IconUtils::findBestIconIn(instance.instanceRoot(), m_instIcon);
		if (!importIconPath.isNull() && QFile::exists(importIconPath)) {
			// import icon
			auto iconList = APPLICATION->icons();
			if (iconList->iconFileExists(m_instIcon)) {
				iconList->deleteIcon(m_instIcon);
			}
			iconList->installIcons({importIconPath});
		}
	}
	emitSucceeded();
}
