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

#include <QString>
#include <QFileInfo>
#include <QDir>
#include <QSet>
#include <QDateTime>
#include "minecraft/mod/Mod.h"
#include <functional>

#include <nonstd/optional>

namespace MMCZip
{
    using FilterFunction = std::function<bool(const QString &)>;

    /**
     * Merge two zip files, using a filter function.
     * Reads entries from 'from' and writes them into the zip at 'intoPath'.
     * 'contained' tracks already-added filenames to avoid duplicates.
     */
    bool mergeZipFiles(const QString &intoPath, QFileInfo from, QSet<QString> &contained,
                       const FilterFunction filter = nullptr);

    /**
     * Take a source jar, add mods to it, resulting in target jar.
     */
    bool createModdedJar(QString sourceJarPath, QString targetJarPath, const QList<Mod>& mods);

    /**
     * Find a single file in archive by file name (not path).
     * \return the path prefix where the file is
     */
    QString findFolderOfFileInZip(const QString &zipPath, const QString & what, const QString &root = QString(""));

    /**
     * Find multiple files of the same name in archive by file name.
     * If a file is found in a path, no deeper paths are searched.
     * \return true if anything was found
     */
    bool findFilesInZip(const QString &zipPath, const QString &what,
                        QStringList &result, const QString &root = QString());

    /**
     * Compress a directory into a zip, using a filter function to exclude entries.
     */
    bool compressDir(QString zipFile, QString dir, FilterFunction excludeFilter);

    /**
     * Extract a subdirectory from an archive.
     */
    nonstd::optional<QStringList> extractSubDir(const QString &zipPath, const QString & subdir, const QString &target);

    /**
     * Extract a single file relative to the zip root.
     */
    bool extractRelFile(const QString &zipPath, const QString & file, const QString &target);

    /**
     * Extract a whole archive.
     *
     * \param fileCompressed The name of the archive.
     * \param dir The directory to extract to, the current directory if left empty.
     * \return The list of the full paths of the files extracted, empty on failure.
     */
    nonstd::optional<QStringList> extractDir(QString fileCompressed, QString dir);

    /**
     * Extract a subdirectory from an archive.
     *
     * \param fileCompressed The name of the archive.
     * \param subdir The directory within the archive to extract
     * \param dir The directory to extract to, the current directory if left empty.
     * \return The list of the full paths of the files extracted, empty on failure.
     */
    nonstd::optional<QStringList> extractDir(QString fileCompressed, QString subdir, QString dir);

    /**
     * Extract a single file from an archive into a directory.
     *
     * \param fileCompressed The name of the archive.
     * \param file The file within the archive to extract
     * \param dir The directory to extract to, the current directory if left empty.
     * \return true for success or false for failure
     */
    bool extractFile(QString fileCompressed, QString file, QString dir);

    /**
     * Read a file's contents from inside a zip archive.
     * \return the file data, or empty QByteArray on failure
     */
    QByteArray readFileFromZip(const QString &zipPath, const QString &entryName);

    /**
     * Check if a given entry path exists in a zip archive.
     */
    bool entryExists(const QString &zipPath, const QString &entryName);

    /**
     * List all entry names in a zip archive.
     */
    QStringList listEntries(const QString &zipPath);

    /**
     * List entries under a specific directory in a zip archive.
     * \param type QDir::Files, QDir::Dirs, or both
     */
    QStringList listEntries(const QString &zipPath, const QString &dirPath,
                             QDir::Filters type = QDir::Files | QDir::Dirs);

    /**
     * Get the modification time of a specific entry in a zip archive.
     */
    QDateTime getEntryModTime(const QString &zipPath, const QString &entryName);
}
