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

#include <QObject>
#include <QList>
#include <QUrl>

#include "net/NetJob.h"
#include "tasks/Task.h"
#include "minecraft/VersionFilterData.h"

class MinecraftVersion;
class MinecraftInstance;

class MinecraftUpdate : public Task
{
    Q_OBJECT
public:
    explicit MinecraftUpdate(MinecraftInstance *inst, QObject *parent = 0);
    virtual ~MinecraftUpdate() {};

    void executeTask() override;
    bool canAbort() const override;

private
slots:
    bool abort() override;
    void subtaskSucceeded();
    void subtaskFailed(QString error);

private:
    void next();

private:
    MinecraftInstance *m_inst = nullptr;
    QList<std::shared_ptr<Task>> m_tasks;
    QString m_preFailure;
    int m_currentTask = -1;
    bool m_abort = false;
    bool m_failed_out_of_order = false;
    QString m_fail_reason;
};
