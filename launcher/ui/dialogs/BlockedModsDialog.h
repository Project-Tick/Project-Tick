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
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QVBoxLayout>

struct BlockedMod {
    int projectId;
    int fileId;
    QString fileName;
    QString targetPath;
    bool found = false;
};

class BlockedModsDialog : public QDialog {
    Q_OBJECT

public:
    explicit BlockedModsDialog(QWidget *parent, const QString &title, const QString &text,
                               QList<BlockedMod> &mods);

    /// Returns the list of mods with updated `found` status
    QList<BlockedMod> &resultMods() { return m_mods; }

private slots:
    void onDownloadDirChanged(const QString &path);
    void openModDownload(int index);

private:
    void scanDownloadsFolder();
    void updateModStatus();
    void setupWatch();

    QList<BlockedMod> &m_mods;
    QString m_downloadDir;
    QFileSystemWatcher m_watcher;

    struct ModRow {
        QLabel *nameLabel;
        QLabel *statusLabel;
        QPushButton *downloadButton;
    };
    QList<ModRow> m_rows;

    QDialogButtonBox *m_buttons;
};
