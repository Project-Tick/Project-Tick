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

#pragma once

#include <QString>

class QWidget;

/*!
 * UpdateController launches the separate meshmc-updater binary and then
 * requests the main application to quit so the updater can proceed.
 *
 * The updater binary is located next to the running executable
 * (QApplication::applicationDirPath()).  It receives:
 *   --url  <download_url>   - artifact to download and install
 *   --root <root_path>      - installation root (prefix directory)
 *   --exec <app_binary>     - path to re-launch after the update completes
 */
class UpdateController
{
  public:
	UpdateController(QWidget* parent, const QString& root,
					 const QString& downloadUrl);

	/*!
	 * Locates the meshmc-updater binary next to the running executable,
	 * launches it with the required arguments, and returns true on success.
	 * The caller is responsible for quitting the main application afterwards.
	 */
	bool startUpdate();

  private:
	QWidget* m_parent;
	QString m_root;
	QString m_downloadUrl;
};
