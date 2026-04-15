/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   MeshMC - A Custom Launcher for Minecraft
 *   Copyright (C) 2026 Project Tick
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "Installer.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QThread>

// LibArchive is used for zip and tar.gz extraction.
#include <archive.h>
#include <archive_entry.h>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

Installer::Installer(QObject* parent) : QObject(parent)
{
	m_nam = new QNetworkAccessManager(this);
}

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

void Installer::start()
{
	emit progressMessage(tr("Downloading update from %1 …").arg(m_url));
	qDebug() << "Installer: downloading" << m_url;

	// Store the downloaded archive inside the install root so everything
	// stays on the same filesystem (avoids cross-device copy issues and
	// /tmp permission problems).
	const QFileInfo urlInfo(QUrl(m_url).path());
	m_tempFile = m_root + "/.meshmc-update-" + urlInfo.fileName();

	QNetworkRequest req{QUrl{m_url}};
	req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
					 QNetworkRequest::NoLessSafeRedirectPolicy);
	QNetworkReply* reply = m_nam->get(req);
	connect(reply, &QNetworkReply::downloadProgress, this,
			&Installer::onDownloadProgress);
	connect(reply, &QNetworkReply::finished, this,
			&Installer::onDownloadFinished);
}

// ---------------------------------------------------------------------------
// Private slots
// ---------------------------------------------------------------------------

void Installer::onDownloadProgress(qint64 received, qint64 total)
{
	if (total > 0) {
		const int pct = static_cast<int>((received * 100) / total);
		emit progressMessage(tr("Downloading … %1%").arg(pct));
	}
}

void Installer::onDownloadFinished()
{
	auto* reply = qobject_cast<QNetworkReply*>(sender());
	if (!reply)
		return;
	reply->deleteLater();

	if (reply->error() != QNetworkReply::NoError) {
		emit finished(false,
					  tr("Download failed: %1").arg(reply->errorString()));
		return;
	}

	// Write to temp file.
	QSaveFile file(m_tempFile);
	if (!file.open(QIODevice::WriteOnly)) {
		emit finished(false,
					  tr("Cannot write temp file: %1").arg(file.errorString()));
		return;
	}
	file.write(reply->readAll());
	if (!file.commit()) {
		emit finished(
			false, tr("Cannot commit temp file: %1").arg(file.errorString()));
		return;
	}

	emit progressMessage(tr("Download complete. Installing …"));
	qDebug() << "Installer: saved to" << m_tempFile;

	const bool ok = installArchive(m_tempFile);

	if (!ok) {
		QFile::remove(m_tempFile);
		return; // finished() already emitted inside installArchive / installExe
	}

	QFile::remove(m_tempFile);
	relaunch();
	emit finished(true, QString());
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

bool Installer::installArchive(const QString& filePath)
{
	const QString lower = filePath.toLower();

	if (lower.endsWith(".exe")) {
		return installExe(filePath);
	}

	if (lower.endsWith(".zip") || lower.endsWith(".tar.gz") ||
		lower.endsWith(".tgz")) {
		// Extract into a temp directory inside the install root so
		// everything stays on the same filesystem.
		QTemporaryDir tempDir(m_root + "/.meshmc-extract-XXXXXX");
		if (!tempDir.isValid()) {
			emit finished(
				false, tr("Cannot create temporary directory for extraction."));
			return false;
		}

		emit progressMessage(tr("Extracting archive …"));

		bool extractOk = false;
		if (lower.endsWith(".zip")) {
			extractOk = extractZip(filePath, tempDir.path());
		} else {
			extractOk = extractTarGz(filePath, tempDir.path());
		}

		if (!extractOk) {
			return false; // finished() already emitted
		}

		// Copy all extracted files into the root, skipping the updater
		// binary itself while it is running.
		emit progressMessage(tr("Installing files …"));

		// Identify the exact binary that is running right now.  We skip the
		// destination file whose absolute path matches /proc/self/exe so we
		// never try to overwrite ourselves mid-update.  This works correctly
		// whether the updater was launched directly or via a wrapper script.
		const QString selfPath =
			QFileInfo(QCoreApplication::applicationFilePath()).absoluteFilePath();

		QDir src(tempDir.path());
		// If the archive has a single top-level directory, descend into it.
		const QStringList topEntries =
			src.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
		if (topEntries.size() == 1 &&
			src.entryList(QDir::Files | QDir::NoDotAndDotDot).isEmpty()) {
			src.cd(topEntries.first());
		}

		const QDir dest(m_root);
		// QDir::System includes symlinks on Unix; we handle them explicitly.
		QDirIterator it(src.absolutePath(),
						QDir::Files | QDir::System | QDir::NoDotAndDotDot,
						QDirIterator::Subdirectories);
		while (it.hasNext()) {
			it.next();
			const QFileInfo& fi = it.fileInfo();

			const QString relPath = src.relativeFilePath(fi.absoluteFilePath());
			const QString destPath = dest.filePath(relPath);

			// Never overwrite the binary that is currently executing.
			if (QFileInfo(destPath).absoluteFilePath() == selfPath)
				continue;

			if (!QDir().mkpath(QFileInfo(destPath).absolutePath())) {
				emit finished(false, tr("Cannot create directory: %1")
										 .arg(QFileInfo(destPath).absolutePath()));
				return false;
			}

			// Remove whatever is already at the destination (file or symlink).
			if (QFileInfo(destPath).exists() || fi.isSymLink())
				QFile::remove(destPath);

			if (fi.isSymLink()) {
				// Recreate the symlink with a path relative to its own
				// directory so it stays portable after installation.
				const QString absTarget = fi.symLinkTarget();
				const QString relTarget =
					QDir(fi.absolutePath()).relativeFilePath(absTarget);
				if (!QFile::link(relTarget, destPath)) {
					qWarning() << "Installer: cannot create symlink"
							   << destPath << "->" << relTarget;
				}
				continue;
			}

			if (!QFile::copy(fi.absoluteFilePath(), destPath)) {
				emit finished(
					false, tr("Cannot copy %1 to %2.").arg(relPath, destPath));
				return false;
			}

			// Preserve executable bit on Unix.
#ifndef Q_OS_WIN
			if (fi.isExecutable()) {
				QFile(destPath).setPermissions(
					QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
					QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther |
					QFile::ExeOther);
			}
#endif
		}

		return true;
	}

	emit finished(
		false,
		tr("Unknown archive format: %1").arg(QFileInfo(filePath).suffix()));
	return false;
}

bool Installer::installExe(const QString& filePath)
{
	// For Windows NSIS / Inno Setup installers, just run the installer
	// directly. It handles file replacement and relaunching.
#ifdef Q_OS_WIN
	emit progressMessage(tr("Launching installer …"));
	const bool ok = QProcess::startDetached(filePath, QStringList());
	if (!ok) {
		emit finished(false,
					  tr("Failed to launch installer: %1").arg(filePath));
		return false;
	}
	// The installer will take care of everything; exit after launching.
	QCoreApplication::quit();
	return true;
#else
	Q_UNUSED(filePath)
	emit finished(false, tr(".exe installers are only supported on Windows."));
	return false;
#endif
}

bool Installer::extractZip(const QString& zipPath, const QString& destDir)
{
	archive* a = archive_read_new();
	archive_read_support_format_zip(a);
	archive_read_support_filter_all(a);

	if (archive_read_open_filename(a, zipPath.toLocal8Bit().constData(),
								   10240) != ARCHIVE_OK) {
		const QString err = QString::fromLocal8Bit(archive_error_string(a));
		archive_read_free(a);
		emit finished(false, tr("Cannot open zip archive: %1").arg(err));
		return false;
	}

	archive* ext = archive_write_disk_new();
	archive_write_disk_set_options(
		ext, ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL |
				 ARCHIVE_EXTRACT_FFLAGS);
	archive_write_disk_set_standard_lookup(ext);

	archive_entry* entry = nullptr;
	bool ok = true;
	QString extractError;
	while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
		const QString entryPath =
			destDir + "/" +
			QString::fromLocal8Bit(archive_entry_pathname(entry));
		archive_entry_set_pathname(entry, entryPath.toLocal8Bit().constData());

		// Ensure parent directory exists — archive_write_disk does not
		// always create missing parent directories on all platforms.
		const QString parentDir = QFileInfo(entryPath).absolutePath();
		if (!QDir().mkpath(parentDir)) {
			extractError = tr("Cannot create directory: %1").arg(parentDir);
			ok = false;
			break;
		}

		const int wh = archive_write_header(ext, entry);
		if (wh < ARCHIVE_WARN) {
			extractError = QString::fromLocal8Bit(archive_error_string(ext));
			qWarning() << "extractZip: archive_write_header failed:" << extractError;
			ok = false;
			break;
		} else if (wh != ARCHIVE_OK) {
			qWarning() << "extractZip: archive_write_header warning:"
					   << QString::fromLocal8Bit(archive_error_string(ext));
		}
		if (archive_entry_size(entry) > 0) {
			const void* buf;
			size_t size;
			la_int64_t offset;
			int rb;
			while ((rb = archive_read_data_block(a, &buf, &size, &offset)) ==
				   ARCHIVE_OK) {
				archive_write_data_block(ext, buf, size, offset);
			}
			if (rb != ARCHIVE_EOF && rb < ARCHIVE_WARN) {
				extractError = QString::fromLocal8Bit(archive_error_string(a));
				qWarning() << "extractZip: read data block failed:" << extractError;
				ok = false;
				break;
			}
		}
		archive_write_finish_entry(ext);
	}

	archive_read_close(a);
	archive_read_free(a);
	archive_write_close(ext);
	archive_write_free(ext);

	if (!ok) {
		emit finished(false, tr("Failed to extract zip archive: %1").arg(extractError));
		return false;
	}
	return true;
}

bool Installer::extractTarGz(const QString& tarPath, const QString& destDir)
{
	archive* a = archive_read_new();
	archive_read_support_format_tar(a);
	archive_read_support_format_gnutar(a);
	archive_read_support_filter_gzip(a);
	archive_read_support_filter_bzip2(a);
	archive_read_support_filter_xz(a);

	if (archive_read_open_filename(a, tarPath.toLocal8Bit().constData(),
								   10240) != ARCHIVE_OK) {
		const QString err = QString::fromLocal8Bit(archive_error_string(a));
		archive_read_free(a);
		emit finished(false, tr("Cannot open tar archive: %1").arg(err));
		return false;
	}

	archive* ext = archive_write_disk_new();
	archive_write_disk_set_options(
		ext, ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL |
				 ARCHIVE_EXTRACT_FFLAGS);
	archive_write_disk_set_standard_lookup(ext);

	archive_entry* entry = nullptr;
	bool ok = true;
	QString extractError;
	while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
		const QString entryPath =
			destDir + "/" +
			QString::fromLocal8Bit(archive_entry_pathname(entry));
		archive_entry_set_pathname(entry, entryPath.toLocal8Bit().constData());

		// Ensure parent directory exists — archive_write_disk does not
		// always create missing parent directories on all platforms.
		const QString parentDir = QFileInfo(entryPath).absolutePath();
		if (!QDir().mkpath(parentDir)) {
			extractError = tr("Cannot create directory: %1").arg(parentDir);
			ok = false;
			break;
		}

		const int wh = archive_write_header(ext, entry);
		if (wh < ARCHIVE_WARN) {
			extractError = QString::fromLocal8Bit(archive_error_string(ext));
			qWarning() << "extractTarGz: archive_write_header failed:" << extractError;
			ok = false;
			break;
		} else if (wh != ARCHIVE_OK) {
			qWarning() << "extractTarGz: archive_write_header warning:"
					   << QString::fromLocal8Bit(archive_error_string(ext));
		}
		if (archive_entry_size(entry) > 0) {
			const void* buf;
			size_t size;
			la_int64_t offset;
			int rb;
			while ((rb = archive_read_data_block(a, &buf, &size, &offset)) ==
				   ARCHIVE_OK) {
				archive_write_data_block(ext, buf, size, offset);
			}
			if (rb != ARCHIVE_EOF && rb < ARCHIVE_WARN) {
				extractError = QString::fromLocal8Bit(archive_error_string(a));
				qWarning() << "extractTarGz: read data block failed:" << extractError;
				ok = false;
				break;
			}
		}
		archive_write_finish_entry(ext);
	}

	archive_read_close(a);
	archive_read_free(a);
	archive_write_close(ext);
	archive_write_free(ext);

	if (!ok) {
		emit finished(false, tr("Failed to extract tar archive: %1").arg(extractError));
		return false;
	}
	return true;
}

void Installer::relaunch()
{
	if (m_exec.isEmpty())
		return;

	qDebug() << "Installer: relaunching" << m_exec;
	QProcess::startDetached(m_exec, QStringList());
}
