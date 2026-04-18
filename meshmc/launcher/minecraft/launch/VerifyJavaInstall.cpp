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

#include "VerifyJavaInstall.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QRegularExpression>

#include <launch/LaunchTask.h>
#include <minecraft/MinecraftInstance.h>
#include <minecraft/PackProfile.h>
#include <minecraft/VersionFilterData.h>

#include "Application.h"
#include "FileSystem.h"
#include "Json.h"
#include "java/JavaUtils.h"

#ifndef MeshMC_DISABLE_JAVA_DOWNLOADER
#include "BuildConfig.h"
#include "net/Download.h"
#endif

#ifdef major
#undef major
#endif
#ifdef minor
#undef minor
#endif

namespace
{
	std::optional<JavaVersion> probeJavaVersion(const QString& javaPath)
	{
		const auto checkerJar =
			FS::PathCombine(APPLICATION->getJarsPath(), "JavaCheck.jar");
		if (!QFileInfo::exists(checkerJar)) {
			return std::nullopt;
		}

		QProcess process;
		process.setProgram(javaPath);
		process.setArguments({"-jar", checkerJar});
		process.setProcessEnvironment(CleanEnviroment());
		process.setProcessChannelMode(QProcess::SeparateChannels);
		process.start();

		if (!process.waitForFinished(15000) ||
			process.exitStatus() != QProcess::NormalExit ||
			process.exitCode() != 0) {
			return std::nullopt;
		}

		const auto stdoutData =
			QString::fromLocal8Bit(process.readAllStandardOutput());
		const auto lines = stdoutData.split('\n', Qt::SkipEmptyParts);
		for (const auto& rawLine : lines) {
			const auto line = rawLine.trimmed();
			if (!line.startsWith("java.version=")) {
				continue;
			}
			return JavaVersion(line.mid(QString("java.version=").size()));
		}

		return std::nullopt;
	}
} // namespace

int VerifyJavaInstall::determineRequiredJavaMajor() const
{
	auto m_inst =
		std::dynamic_pointer_cast<MinecraftInstance>(m_parent->instance());
	auto minecraftComponent =
		m_inst->getPackProfile()->getComponent("net.minecraft");

	if (minecraftComponent->getReleaseDateTime() >=
		g_VersionFilterData.java25BeginsDate)
		return 25;
	if (minecraftComponent->getReleaseDateTime() >=
		g_VersionFilterData.java21BeginsDate)
		return 21;
	if (minecraftComponent->getReleaseDateTime() >=
		g_VersionFilterData.java17BeginsDate)
		return 17;
	if (minecraftComponent->getReleaseDateTime() >=
		g_VersionFilterData.java16BeginsDate)
		return 16;
	if (minecraftComponent->getReleaseDateTime() >=
		g_VersionFilterData.java8BeginsDate)
		return 8;
	return 0;
}

QString VerifyJavaInstall::javaInstallDir() const
{
	return JavaUtils::managedJavaRoot();
}

QString VerifyJavaInstall::findInstalledJava(int requiredMajor) const
{
	JavaUtils javaUtils;
	QList<QString> systemJavas = javaUtils.FindJavaPaths();
	QSet<QString> seenPaths;
	for (const QString& javaPath : systemJavas) {
		QString resolved = FS::ResolveExecutable(javaPath);
		if (resolved.isEmpty() || seenPaths.contains(resolved))
			continue;

		seenPaths.insert(resolved);
		const auto version = probeJavaVersion(resolved);
		if (version.has_value() && version->major() >= requiredMajor) {
			return resolved;
		}
	}

	return {};
}

void VerifyJavaInstall::executeTask()
{
	auto m_inst =
		std::dynamic_pointer_cast<MinecraftInstance>(m_parent->instance());

	auto javaVersion = m_inst->getJavaVersion();
	int requiredMajor = determineRequiredJavaMajor();

	// No Java requirement or already met
	if (requiredMajor == 0 || javaVersion.major() >= requiredMajor) {
		emitSucceeded();
		return;
	}

	// Java version insufficient — try to find an already-downloaded one
	emit logLine(
		tr("Current Java version %1 does not meet the requirement of Java %2.")
			.arg(javaVersion.toString())
			.arg(requiredMajor),
		MessageLevel::Warning);

	QString existingJava = findInstalledJava(requiredMajor);
	if (!existingJava.isEmpty()) {
		emit logLine(tr("Found installed Java %1 at: %2")
						 .arg(requiredMajor)
						 .arg(existingJava),
					 MessageLevel::MeshMC);
#ifndef MeshMC_DISABLE_JAVA_DOWNLOADER
		setJavaPathAndSucceed(existingJava);
#else
		m_inst->settings()->set("OverrideJavaLocation", true);
		m_inst->settings()->set("JavaPath", existingJava);
		emit logLine(tr("Java path set to: %1").arg(existingJava),
					 MessageLevel::MeshMC);
		emitSucceeded();
#endif
		return;
	}

#ifndef MeshMC_DISABLE_JAVA_DOWNLOADER
	// Check runtime setting for auto-download
	if (!m_inst->settings()->get("JavaAutoDownload").toBool()) {
		emitFailed(
			tr("Java %1 is required but not installed. Automatic Java download is disabled.")
				.arg(requiredMajor));
		return;
	}
	// Not found — auto-download
	emit logLine(
		tr("No installed Java %1 found. Downloading...").arg(requiredMajor),
		MessageLevel::MeshMC);
	autoDownloadJava(requiredMajor);
#else
	emitFailed(
		tr("Java %1 is required but not installed. Please install it manually.")
			.arg(requiredMajor));
#endif
}

#ifndef MeshMC_DISABLE_JAVA_DOWNLOADER
void VerifyJavaInstall::autoDownloadJava(int requiredMajor)
{
	// Get vendor from instance settings (falls through to global if not overridden)
	auto m_inst =
		std::dynamic_pointer_cast<MinecraftInstance>(m_parent->instance());
	m_preferredVendor =
		m_inst->settings()->get("JavaAutoDownloadVendor").toString();
	if (m_preferredVendor.isEmpty()) {
		m_preferredVendor = "net.minecraft.java";
	}
	emit logLine(
		tr("Using Java vendor: %1").arg(m_preferredVendor),
		MessageLevel::MeshMC);
	fetchVersionList(requiredMajor);
}

void VerifyJavaInstall::fetchVersionList(int requiredMajor)
{
	m_fetchData.clear();
	QString uid = m_preferredVendor;
	QString url = QString("%1%2/index.json").arg(BuildConfig.META_URL, uid);

	m_fetchJob = new NetJob(tr("Fetch Java versions"), APPLICATION->network());
	auto dl = Net::Download::makeByteArray(QUrl(url), &m_fetchData);
	m_fetchJob->addNetAction(dl);

	connect(
		m_fetchJob.get(), &NetJob::succeeded, this,
		[this, uid, requiredMajor]() {
			m_fetchJob.reset();

			QJsonDocument doc;
			try {
				doc = Json::requireDocument(m_fetchData);
			} catch (const Exception& e) {
				emitFailed(
					tr("Failed to parse Java version list from meta server: %1")
						.arg(e.cause()));
				return;
			}
			if (!doc.isObject()) {
				emitFailed(
					tr("Failed to parse Java version list from meta server."));
				return;
			}

			auto versions = JavaDownload::parseVersionIndex(doc.object(), uid);

			// Find the matching version (e.g., "java25" for requiredMajor=25)
			QString targetVersionId = QString("java%1").arg(requiredMajor);
			bool found = false;
			for (const auto& ver : versions) {
				if (ver.versionId == targetVersionId) {
					found = true;
					fetchRuntimes(ver.versionId, requiredMajor);
					return;
				}
			}

			if (!found) {
				emitFailed(tr("Java %1 is not available for download from the "
							  "selected vendor. Please install it manually or "
							  "try a different vendor in settings.")
							   .arg(requiredMajor));
			}
		});

	connect(m_fetchJob.get(), &NetJob::failed, this,
			[this, requiredMajor](QString reason) {
				emitFailed(tr("Failed to fetch Java version list: %1. Please "
							  "install Java %2 manually.")
							   .arg(reason)
							   .arg(requiredMajor));
				m_fetchJob.reset();
			});

	m_fetchJob->start();
}

void VerifyJavaInstall::fetchRuntimes(const QString& versionId,
									  int requiredMajor)
{
	m_fetchData.clear();
	QString uid = m_preferredVendor;
	QString url =
		QString("%1%2/%3.json").arg(BuildConfig.META_URL, uid, versionId);

	m_fetchJob =
		new NetJob(tr("Fetch Java runtime details"), APPLICATION->network());
	auto dl = Net::Download::makeByteArray(QUrl(url), &m_fetchData);
	m_fetchJob->addNetAction(dl);

	connect(m_fetchJob.get(), &NetJob::succeeded, this,
			[this, requiredMajor]() {
				auto fetchJob = std::move(m_fetchJob);

				QJsonDocument doc;
				try {
					doc = Json::requireDocument(m_fetchData);
				} catch (const Exception& e) {
					emitFailed(tr("Failed to parse Java runtime details: %1")
								   .arg(e.cause()));
					return;
				}
				if (!doc.isObject()) {
					emitFailed(tr("Failed to parse Java runtime details."));
					return;
				}

				auto allRuntimes = JavaDownload::parseRuntimes(doc.object());
				QString myOS = JavaDownload::currentRuntimeOS();

				// Filter for current platform
				for (const auto& rt : allRuntimes) {
					if (rt.runtimeOS == myOS) {
						emit logLine(tr("Downloading %1 (%2)...")
										 .arg(rt.name, rt.version.toString()),
									 MessageLevel::MeshMC);
						startDownload(rt, requiredMajor);
						return;
					}
				}

				emitFailed(tr("No Java %1 download available for your platform "
							  "(%2). Please install it manually.")
							   .arg(requiredMajor)
							   .arg(myOS));
			});

	connect(m_fetchJob.get(), &NetJob::failed, this, [this](QString reason) {
		emitFailed(tr("Failed to fetch Java runtime details: %1").arg(reason));
		m_fetchJob.reset();
	});

	m_fetchJob->start();
}

void VerifyJavaInstall::startDownload(const JavaDownload::RuntimeEntry& runtime,
									  int requiredMajor)
{
	QString dirName =
		QString("%1-%2").arg(runtime.name, runtime.version.toString());
	QString targetDir =
		FS::PathCombine(javaInstallDir(), runtime.vendor, dirName);

	m_downloadTask =
		std::make_unique<JavaDownloadTask>(runtime, targetDir, this);

	connect(m_downloadTask.get(), &Task::succeeded, this, [this]() {
		QString javaPath = m_downloadTask->installedJavaPath();

		if (javaPath.isEmpty()) {
			emitFailed(
				tr("Java was downloaded but the binary could not be found."));
			return;
		}

		emit logLine(tr("Java downloaded and installed at: %1").arg(javaPath),
					 MessageLevel::MeshMC);
		setJavaPathAndSucceed(javaPath);
	});

	connect(m_downloadTask.get(), &Task::failed, this,
			[this, requiredMajor](const QString& reason) {
				emitFailed(tr("Failed to download Java %1: %2")
							   .arg(requiredMajor)
							   .arg(reason));
				m_downloadTask.reset();
			});

	connect(m_downloadTask.get(), &Task::status, this,
			[this](const QString& status) {
				emit logLine(status, MessageLevel::MeshMC);
			});

	m_downloadTask->start();
}

void VerifyJavaInstall::setJavaPathAndSucceed(const QString& javaPath)
{
	auto m_inst =
		std::dynamic_pointer_cast<MinecraftInstance>(m_parent->instance());
	// Set Java path override on the instance only, not globally
	m_inst->settings()->set("OverrideJavaLocation", true);
	m_inst->settings()->set("JavaPath", javaPath);
	emit logLine(tr("Java path set to: %1").arg(javaPath),
				 MessageLevel::MeshMC);
	emitSucceeded();
}
#endif
