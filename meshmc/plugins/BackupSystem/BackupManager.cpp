/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "BackupManager.h"

BackupManager::BackupManager(const QString& instanceId,
                             const QString& instanceRoot)
    : m_instanceId(instanceId)
    , m_instanceRoot(instanceRoot)
{
    m_backupDir = QDir(m_instanceRoot).filePath(".backups");
    ensureBackupDir();
}

QString BackupManager::backupDir() const
{
    return m_backupDir;
}

void BackupManager::ensureBackupDir()
{
    QDir().mkpath(m_backupDir);
}

QList<BackupEntry> BackupManager::listBackups() const
{
    QList<BackupEntry> result;

    QDir dir(m_backupDir);
    if (!dir.exists())
        return result;

    const auto files = dir.entryInfoList({"*.zip"}, QDir::Files, QDir::Time);
    for (const auto& fi : files) {
        result.append(entryFromFile(fi.absoluteFilePath()));
    }

    return result;
}

BackupEntry BackupManager::createBackup(const QString& label)
{
    ensureBackupDir();

    QString fileName = generateFileName(label);
    QString zipPath = QDir(m_backupDir).filePath(fileName);

    qDebug() << "[BackupSystem] Creating backup:" << zipPath
             << "from" << m_instanceRoot;

    bool ok = MMCZip::compressDir(zipPath, m_instanceRoot,
                                  [this](const QString& entry) -> bool {
                                      return entry.startsWith(".backups/") ||
                                             entry.startsWith(".backups\\");
                                  });

    if (!ok) {
        qWarning() << "[BackupSystem] Failed to create backup:" << zipPath;
        QFile::remove(zipPath);
        return {};
    }

    qDebug() << "[BackupSystem] Backup created successfully:" << zipPath;
    return entryFromFile(zipPath);
}

bool BackupManager::restoreBackup(const BackupEntry& entry)
{
    if (!QFile::exists(entry.fullPath)) {
        qWarning() << "[BackupSystem] Backup file not found:" << entry.fullPath;
        return false;
    }

    qDebug() << "[BackupSystem] Restoring backup:" << entry.fullPath
             << "to" << m_instanceRoot;

    QDir instDir(m_instanceRoot);
    const auto entries = instDir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
    for (const auto& fi : entries) {
        if (fi.fileName() == ".backups")
            continue;

        if (fi.isDir()) {
            QDir(fi.absoluteFilePath()).removeRecursively();
        } else {
            QFile::remove(fi.absoluteFilePath());
        }
    }

    auto result = MMCZip::extractDir(entry.fullPath, m_instanceRoot);
    if (!result.has_value()) {
        qWarning() << "[BackupSystem] Failed to extract backup:" << entry.fullPath;
        return false;
    }

    qDebug() << "[BackupSystem] Restore complete.";
    return true;
}

bool BackupManager::exportBackup(const BackupEntry& entry, const QString& destPath)
{
    if (!QFile::exists(entry.fullPath))
        return false;

    QDir().mkpath(QFileInfo(destPath).absolutePath());
    return QFile::copy(entry.fullPath, destPath);
}

BackupEntry BackupManager::importBackup(const QString& srcZipPath, const QString& label)
{
    if (!QFile::exists(srcZipPath))
        return {};

    ensureBackupDir();

    QString fileName = generateFileName(label);
    QString destPath = QDir(m_backupDir).filePath(fileName);

    if (!QFile::copy(srcZipPath, destPath))
        return {};

    qDebug() << "[BackupSystem] Imported backup:" << srcZipPath << "as" << destPath;
    return entryFromFile(destPath);
}

bool BackupManager::deleteBackup(const BackupEntry& entry)
{
    return QFile::remove(entry.fullPath);
}

QString BackupManager::generateFileName(const QString& label) const
{
    QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    if (label.isEmpty()) {
        return ts + ".zip";
    }
    QString safe = label;
    safe.replace(QRegularExpression("[^a-zA-Z0-9_-]"), "_");
    safe.truncate(64);
    return ts + "_" + safe + ".zip";
}

BackupEntry BackupManager::entryFromFile(const QString& filePath) const
{
    QFileInfo fi(filePath);
    BackupEntry entry;
    entry.fileName = fi.fileName();
    entry.fullPath = fi.absoluteFilePath();
    entry.sizeBytes = fi.size();
    entry.timestamp = fi.birthTime().isValid() ? fi.birthTime() : fi.lastModified();

    QString base = fi.completeBaseName();
    int thirdUs = base.indexOf('_', 20);
    if (thirdUs > 0 && thirdUs < base.size() - 1) {
        entry.name = base.mid(thirdUs + 1).replace('_', ' ');
    } else {
        entry.name = base;
    }

    return entry;
}
