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

#include <QString>
#include <QUrl>
#include <QVector>

namespace Modrinth {

struct File {
    QString path;
    QUrl downloadUrl;
    QString sha1;
    QString sha512;
    int fileSize = 0;
};

struct Dependency {
    QString versionId;
    QString projectId;
    QString fileName;
};

struct Manifest {
    int formatVersion = 0;
    QString game;
    QString versionId;
    QString name;
    QString summary;
    QVector<Modrinth::File> files;

    QString minecraftVersion;
    QString forgeVersion;
    QString fabricVersion;
    QString quiltVersion;
    QString neoForgeVersion;
};

void loadManifest(Modrinth::Manifest &m, const QString &filepath);

}
