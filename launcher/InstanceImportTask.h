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

#pragma once

#include "InstanceTask.h"
#include "net/NetJob.h"
#include <QUrl>
#include <QFuture>
#include <QFutureWatcher>
#include "settings/SettingsObject.h"
#include "QObjectPtr.h"

#include <nonstd/optional>

namespace Flame
{
    class FileResolvingTask;
    struct Manifest;
}

class InstanceImportTask : public InstanceTask
{
    Q_OBJECT
public:
    explicit InstanceImportTask(const QUrl sourceUrl);

protected:
    //! Entry point for tasks.
    virtual void executeTask() override;

private:
    void processZipPack();
    void processMeshMC();
    void processFlame();
    void configureFlameInstance(Flame::Manifest &pack);
    void onFlameFileResolutionSucceeded();
    void processModrinth();
    void processTechnic();

private slots:
    void downloadSucceeded();
    void downloadFailed(QString reason);
    void downloadProgressChanged(qint64 current, qint64 total);
    void extractFinished();
    void extractAborted();

private: /* data */
    NetJob::Ptr m_filesNetJob;
    shared_qobject_ptr<Flame::FileResolvingTask> m_modIdResolver;
    QUrl m_sourceUrl;
    QString m_archivePath;
    bool m_downloadRequired = false;
    QFuture<nonstd::optional<QStringList>> m_extractFuture;
    QFutureWatcher<nonstd::optional<QStringList>> m_extractFutureWatcher;
    enum class ModpackType{
        Unknown,
        MeshMC,
        Flame,
        Modrinth,
        Technic
    } m_modpackType = ModpackType::Unknown;
};
