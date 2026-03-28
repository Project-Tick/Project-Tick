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

#include "ModrinthPackIndex.h"

#include "Json.h"

void Modrinth::loadIndexedPack(Modrinth::IndexedPack &pack, QJsonObject &obj)
{
    pack.projectId = Json::ensureString(obj, "project_id", "");
    if (pack.projectId.isEmpty())
    {
        pack.projectId = Json::requireString(obj, "id");
    }
    pack.slug = Json::ensureString(obj, "slug", "");
    pack.name = Json::requireString(obj, "title");
    pack.description = Json::ensureString(obj, "description", "");
    pack.author = Json::ensureString(obj, "author", "");
    pack.downloads = Json::ensureInteger(obj, "downloads", 0);

    pack.iconUrl = Json::ensureString(obj, "icon_url", "");
}

void Modrinth::loadIndexedPackVersions(Modrinth::IndexedPack &pack, QJsonArray &arr)
{
    pack.versions.clear();
    for (auto versionRaw : arr)
    {
        auto obj = versionRaw.toObject();
        Modrinth::IndexedVersion version;
        version.id = Json::requireString(obj, "id");
        version.projectId = Json::ensureString(obj, "project_id", pack.projectId);
        version.name = Json::ensureString(obj, "name", "");
        version.versionNumber = Json::requireString(obj, "version_number");

        auto gameVersions = Json::ensureArray(obj, "game_versions");
        if (!gameVersions.isEmpty())
        {
            version.mcVersion = gameVersions.first().toString();
        }

        auto loaders = Json::ensureArray(obj, "loaders");
        QStringList loaderList;
        for (auto loader : loaders)
        {
            loaderList.append(loader.toString());
        }
        version.loaders = loaderList.join(", ");

        auto files = Json::ensureArray(obj, "files");
        for (auto fileRaw : files)
        {
            auto fileObj = fileRaw.toObject();
            bool primary = Json::ensureBoolean(fileObj, "primary", false);
            if (primary || files.size() == 1)
            {
                version.downloadUrl = Json::ensureString(fileObj, "url", "");
                version.downloadSize = Json::ensureInteger(fileObj, "size", 0);
                auto hashes = Json::ensureObject(fileObj, "hashes");
                version.sha1 = Json::ensureString(hashes, "sha1", "");
                break;
            }
        }

        if (!version.downloadUrl.isEmpty())
        {
            pack.versions.append(version);
        }
    }
    pack.versionsLoaded = true;
}
