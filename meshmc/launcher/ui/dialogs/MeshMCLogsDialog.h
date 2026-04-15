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

#include <QDialog>
#include <QFileSystemWatcher>

namespace Ui
{
	class MeshMCLogsDialog;
}

class MeshMCLogsDialog : public QDialog
{
	Q_OBJECT

  public:
	explicit MeshMCLogsDialog(QWidget* parent = nullptr);
	~MeshMCLogsDialog();

  private slots:
	void on_selectLogBox_currentIndexChanged(int index);
	void on_btnReload_clicked();
	void on_btnCopy_clicked();
	void on_btnUpload_clicked();
	void on_btnDelete_clicked();
	void on_btnClean_clicked();
	void on_findButton_clicked();
	void onLogFileChanged(const QString& path);

  private:
	void populateLogList();
	void loadSelectedLog();
	void setControlsEnabled(bool enabled);
	QString logFilePath(const QString& name) const;
	QString logDirectory() const;

	Ui::MeshMCLogsDialog* ui;
	QFileSystemWatcher* m_liveWatcher;
	QString m_currentFile;
	bool m_watching0Log = false;
};
