/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * BackupManager — Core backup logic for the BackupSystem plugin.
 * Handles creating, restoring, exporting, and importing instance backups.
 */

#pragma once

#include "plugin/sdk/mmco_sdk.h"

struct BackupEntry {
    QString name;           // human-readable label
    QString fileName;       // zip file name (e.g. "2026-01-15_14-30-00.zip")
    QString fullPath;       // absolute path to the backup zip
    QDateTime timestamp;    // when the backup was created
    qint64 sizeBytes;       // file size
};

class BackupManager {
public:
    explicit BackupManager(const QString& instanceId,
                           const QString& instanceRoot);

    QString backupDir() const;
    QList<BackupEntry> listBackups() const;
    BackupEntry createBackup(const QString& label = {});
    bool restoreBackup(const BackupEntry& entry);
    bool exportBackup(const BackupEntry& entry, const QString& destPath);
    BackupEntry importBackup(const QString& srcZipPath, const QString& label = {});
    bool deleteBackup(const BackupEntry& entry);

private:
    void ensureBackupDir();
    QString generateFileName(const QString& label) const;
    BackupEntry entryFromFile(const QString& filePath) const;

    QString m_instanceId;
    QString m_instanceRoot;
    QString m_backupDir;
};
