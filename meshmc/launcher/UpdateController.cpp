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

#include "UpdateController.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QMessageBox>
#include <QProcess>

#include "BuildConfig.h"
#include "FileSystem.h"

UpdateController::UpdateController(QWidget* parent, const QString& root,
								   const QString& downloadUrl)
	: m_parent(parent), m_root(root), m_downloadUrl(downloadUrl)
{
}

bool UpdateController::startUpdate()
{
	// Locate the updater binary next to the running executable.
	QString updaterName = BuildConfig.MESHMC_NAME + "-updater";
#ifdef Q_OS_WIN
	updaterName += ".exe";
#endif
	const QString updaterPath =
		FS::PathCombine(QApplication::applicationDirPath(), updaterName);

	if (!QFile::exists(updaterPath)) {
		qCritical() << "UpdateController: updater binary not found at"
					<< updaterPath;
		QMessageBox::critical(
			m_parent,
			QCoreApplication::translate("UpdateController",
										"Updater Not Found"),
			QCoreApplication::translate("UpdateController",
										"The updater binary could not be found "
										"at:\n%1\n\nPlease reinstall %2.")
				.arg(updaterPath, BuildConfig.MESHMC_DISPLAYNAME));
		return false;
	}

	const QStringList args = {"--url",	m_downloadUrl,
							  "--root", m_root,
							  "--exec", QApplication::applicationFilePath()};

	qDebug() << "UpdateController: launching" << updaterPath << "with args"
			 << args;
	const bool ok = QProcess::startDetached(updaterPath, args);
	if (!ok) {
		qCritical() << "UpdateController: failed to start updater binary.";
		QMessageBox::critical(
			m_parent,
			QCoreApplication::translate("UpdateController", "Update Failed"),
			QCoreApplication::translate("UpdateController",
										"Could not launch the updater "
										"binary.\nPlease update %1 manually.")
				.arg(BuildConfig.MESHMC_DISPLAYNAME));
		return false;
	}

	return true;
}
