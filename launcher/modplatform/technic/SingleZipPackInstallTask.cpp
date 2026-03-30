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

#include "SingleZipPackInstallTask.h"

#include <QtConcurrent>

#include "MMCZip.h"
#include "TechnicPackProcessor.h"
#include "FileSystem.h"

#include "Application.h"

Technic::SingleZipPackInstallTask::SingleZipPackInstallTask(const QUrl &sourceUrl, const QString &minecraftVersion)
{
    m_sourceUrl = sourceUrl;
    m_minecraftVersion = minecraftVersion;
}

bool Technic::SingleZipPackInstallTask::abort() {
    if(m_abortable)
    {
        return m_filesNetJob->abort();
    }
    return false;
}

void Technic::SingleZipPackInstallTask::executeTask()
{
    setStatus(tr("Downloading modpack:\n%1").arg(m_sourceUrl.toString()));

    const QString path = m_sourceUrl.host() + '/' + m_sourceUrl.path();
    auto entry = APPLICATION->metacache()->resolveEntry("general", path);
    entry->setStale(true);
    m_filesNetJob = new NetJob(tr("Modpack download"), APPLICATION->network());
    m_filesNetJob->addNetAction(Net::Download::makeCached(m_sourceUrl, entry));
    m_archivePath = entry->getFullPath();
    auto job = m_filesNetJob.get();
    connect(job, &NetJob::succeeded, this, &Technic::SingleZipPackInstallTask::downloadSucceeded);
    connect(job, &NetJob::progress, this, &Technic::SingleZipPackInstallTask::downloadProgressChanged);
    connect(job, &NetJob::failed, this, &Technic::SingleZipPackInstallTask::downloadFailed);
    m_filesNetJob->start();
}

void Technic::SingleZipPackInstallTask::downloadSucceeded()
{
    m_abortable = false;

    setStatus(tr("Extracting modpack"));
    QDir extractDir(FS::PathCombine(m_stagingPath, ".minecraft"));
    qDebug() << "Attempting to create instance from" << m_archivePath;

    QString archivePath = m_archivePath;
    QString extractPath = extractDir.absolutePath();
    m_extractFuture = QtConcurrent::run(QThreadPool::globalInstance(), [archivePath, extractPath]() {
        return MMCZip::extractSubDir(archivePath, QString(""), extractPath);
    });
    connect(&m_extractFutureWatcher, &QFutureWatcher<QStringList>::finished, this, &Technic::SingleZipPackInstallTask::extractFinished);
    connect(&m_extractFutureWatcher, &QFutureWatcher<QStringList>::canceled, this, &Technic::SingleZipPackInstallTask::extractAborted);
    m_extractFutureWatcher.setFuture(m_extractFuture);
    m_filesNetJob.reset();
}

void Technic::SingleZipPackInstallTask::downloadFailed(QString reason)
{
    m_abortable = false;
    emitFailed(reason);
    m_filesNetJob.reset();
}

void Technic::SingleZipPackInstallTask::downloadProgressChanged(qint64 current, qint64 total)
{
    m_abortable = true;
    setProgress(current / 2, total);
}

void Technic::SingleZipPackInstallTask::extractFinished()
{
    if (!m_extractFuture.result())
    {
        emitFailed(tr("Failed to extract modpack"));
        return;
    }
    QDir extractDir(m_stagingPath);

    qDebug() << "Fixing permissions for extracted pack files...";
    QDirIterator it(extractDir, QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        auto filepath = it.next();
        QFileInfo file(filepath);
        auto permissions = QFile::permissions(filepath);
        auto origPermissions = permissions;
        if (file.isDir())
        {
            // Folder +rwx for current user
            permissions |= QFileDevice::Permission::ReadUser | QFileDevice::Permission::WriteUser | QFileDevice::Permission::ExeUser;
        }
        else
        {
            // File +rw for current user
            permissions |= QFileDevice::Permission::ReadUser | QFileDevice::Permission::WriteUser;
        }
        if (origPermissions != permissions)
        {
            if (!QFile::setPermissions(filepath, permissions))
            {
                logWarning(tr("Could not fix permissions for %1").arg(filepath));
            }
            else
            {
                qDebug() << "Fixed" << filepath;
            }
        }
    }

    shared_qobject_ptr<Technic::TechnicPackProcessor> packProcessor = new Technic::TechnicPackProcessor();
    connect(packProcessor.get(), &Technic::TechnicPackProcessor::succeeded, this, &Technic::SingleZipPackInstallTask::emitSucceeded);
    connect(packProcessor.get(), &Technic::TechnicPackProcessor::failed, this, &Technic::SingleZipPackInstallTask::emitFailed);
    packProcessor->run(m_globalSettings, m_instName, m_instIcon, m_stagingPath, m_minecraftVersion);
}

void Technic::SingleZipPackInstallTask::extractAborted()
{
    emitFailed(tr("Instance import has been aborted."));
}
