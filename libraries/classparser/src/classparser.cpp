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
#include "classfile.h"
#include "classparser.h"

#include <QFile>
#include <archive.h>
#include <archive_entry.h>
#include <QDebug>

namespace classparser
{

QString GetMinecraftJarVersion(QString jarName)
{
    QString version;

    // check if minecraft.jar exists
    QFile jar(jarName);
    if (!jar.exists())
        return version;

    // open jar with libarchive
    struct archive *a = archive_read_new();
    archive_read_support_format_zip(a);
    if (archive_read_open_filename(a, jarName.toUtf8().constData(), 10240) != ARCHIVE_OK) {
        archive_read_free(a);
        return version;
    }

    // find and read Minecraft.class
    QByteArray classData;
    struct archive_entry *entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        QString name = QString::fromUtf8(archive_entry_pathname(entry));
        if (name == "net/minecraft/client/Minecraft.class") {
            la_int64_t sz = archive_entry_size(entry);
            if (sz > 0) {
                classData.resize(sz);
                archive_read_data(a, classData.data(), sz);
            } else {
                char buf[8192];
                la_ssize_t r;
                while ((r = archive_read_data(a, buf, sizeof(buf))) > 0)
                    classData.append(buf, r);
            }
            break;
        }
        archive_read_data_skip(a);
    }
    archive_read_free(a);

    if (classData.isEmpty())
        return version;

    // parse Minecraft.class
    try
    {
        char *temp = classData.data();
        qint64 size = classData.size();
        java::classfile MinecraftClass(temp, size);
        java::constant_pool constants = MinecraftClass.constants;
        for (java::constant_pool::container_type::const_iterator iter = constants.begin();
             iter != constants.end(); iter++)
        {
            const java::constant &constant = *iter;
            if (constant.type != java::constant_type_t::j_string_data)
                continue;
            const std::string &str = constant.str_data;
            qDebug() << QString::fromStdString(str);
            if (str.compare(0, 20, "Minecraft Minecraft ") == 0)
            {
                version = str.substr(20).data();
                break;
            }
        }
    }
    catch (const java::classfile_exception &) { }

    return version;
}
}
