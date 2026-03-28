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

#include "ModrinthPackManifest.h"

#include "Json.h"

static void loadFile(Modrinth::File &f, QJsonObject &fileObj)
{
    f.path = Json::requireString(fileObj, "path");

    auto downloads = Json::requireArray(fileObj, "downloads");
    if (!downloads.isEmpty())
    {
        f.downloadUrl = QUrl(downloads.first().toString());
    }

    auto hashes = Json::ensureObject(fileObj, "hashes");
    f.sha1 = Json::ensureString(hashes, "sha1", "");
    f.sha512 = Json::ensureString(hashes, "sha512", "");

    f.fileSize = Json::ensureInteger(fileObj, "fileSize", 0);
}

static void loadDependencies(Modrinth::Manifest &m, QJsonObject &deps)
{
    m.minecraftVersion = Json::ensureString(deps, "minecraft", "");
    m.forgeVersion = Json::ensureString(deps, "forge", "");
    m.fabricVersion = Json::ensureString(deps, "fabric-loader", "");
    m.quiltVersion = Json::ensureString(deps, "quilt-loader", "");
    m.neoForgeVersion = Json::ensureString(deps, "neoforge", "");
}

void Modrinth::loadManifest(Modrinth::Manifest &m, const QString &filepath)
{
    auto doc = Json::requireDocument(filepath);
    auto obj = Json::requireObject(doc);

    m.formatVersion = Json::requireInteger(obj, "formatVersion");
    if (m.formatVersion != 1)
    {
        throw JSONValidationError(
            QString("Unsupported Modrinth modpack format version: %1")
                .arg(m.formatVersion));
    }

    m.game = Json::requireString(obj, "game");
    if (m.game != "minecraft")
    {
        throw JSONValidationError(
            QString("Unsupported game in Modrinth modpack: %1").arg(m.game));
    }

    m.versionId = Json::ensureString(obj, "versionId", "");
    m.name = Json::ensureString(obj, "name", "Unnamed");
    m.summary = Json::ensureString(obj, "summary", "");

    auto files = Json::requireArray(obj, "files");
    for (auto fileRaw : files)
    {
        auto fileObj = Json::requireObject(fileRaw);
        Modrinth::File file;
        loadFile(file, fileObj);
        m.files.append(file);
    }

    auto deps = Json::ensureObject(obj, "dependencies");
    if (!deps.isEmpty())
    {
        loadDependencies(m, deps);
    }
}
