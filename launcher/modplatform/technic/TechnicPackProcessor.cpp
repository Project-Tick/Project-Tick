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
 * Copyright 2020-2021 MultiMC Contributors
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

#include "TechnicPackProcessor.h"

#include <FileSystem.h>
#include <Json.h>
#include <minecraft/MinecraftInstance.h>
#include <minecraft/PackProfile.h>
#include <MMCZip.h>
#include <settings/INISettingsObject.h>

#include <memory>

void Technic::TechnicPackProcessor::run(SettingsObjectPtr globalSettings, const QString &instName, const QString &instIcon, const QString &stagingPath, const QString &minecraftVersion, const bool isSolder)
{
    QString minecraftPath = FS::PathCombine(stagingPath, ".minecraft");
    QString configPath = FS::PathCombine(stagingPath, "instance.cfg");
    auto instanceSettings = std::make_shared<INISettingsObject>(configPath);
    instanceSettings->registerSetting("InstanceType", "Legacy");
    instanceSettings->set("InstanceType", "OneSix");
    MinecraftInstance instance(globalSettings, instanceSettings, stagingPath);

    instance.setName(instName);

    if (instIcon != "default")
    {
        instance.setIconKey(instIcon);
    }

    auto components = instance.getPackProfile();
    components->buildingFromScratch();

    QByteArray data;

    QString modpackJar = FS::PathCombine(minecraftPath, "bin", "modpack.jar");
    QString versionJson = FS::PathCombine(minecraftPath, "bin", "version.json");
    QString fmlMinecraftVersion;
    if (QFile::exists(modpackJar))
    {
        if (MMCZip::entryExists(modpackJar, "version.json"))
        {
            if (MMCZip::entryExists(modpackJar, "fmlversion.properties"))
            {
                QByteArray fmlVersionData = MMCZip::readFileFromZip(modpackJar, "fmlversion.properties");
                if (fmlVersionData.isEmpty())
                {
                    emit failed(tr("Unable to open \"fmlversion.properties\"!"));
                    return;
                }
                INIFile iniFile;
                iniFile.loadFile(fmlVersionData);
                // If not present, this evaluates to a null string
                fmlMinecraftVersion = iniFile["fmlbuild.mcversion"].toString();
            }
            data = MMCZip::readFileFromZip(modpackJar, "version.json");
            if (data.isEmpty())
            {
                emit failed(tr("Unable to open \"version.json\"!"));
                return;
            }
        }
        else
        {
            if (minecraftVersion.isEmpty())
                emit failed(tr("Could not find \"version.json\" inside \"bin/modpack.jar\", but minecraft version is unknown"));
            components->setComponentVersion("net.minecraft", minecraftVersion, true);
            components->installJarMods({modpackJar});

            // Forge for 1.4.7 and for 1.5.2 require extra libraries.
            // Figure out the forge version and add it as a component
            // (the code still comes from the jar mod installed above)
            if (MMCZip::entryExists(modpackJar, "forgeversion.properties"))
            {
                QByteArray forgeVersionData = MMCZip::readFileFromZip(modpackJar, "forgeversion.properties");
                if (forgeVersionData.isEmpty())
                {
                    // Really shouldn't happen, but error handling shall not be forgotten
                    emit failed(tr("Unable to open \"forgeversion.properties\""));
                    return;
                }
                INIFile iniFile;
                iniFile.loadFile(forgeVersionData);
                QString major, minor, revision, build;
                major = iniFile["forge.major.number"].toString();
                minor = iniFile["forge.minor.number"].toString();
                revision = iniFile["forge.revision.number"].toString();
                build = iniFile["forge.build.number"].toString();

                if (major.isEmpty() || minor.isEmpty() || revision.isEmpty() || build.isEmpty())
                {
                    emit failed(tr("Invalid \"forgeversion.properties\"!"));
                    return;
                }

                components->setComponentVersion("net.minecraftforge", major + '.' + minor + '.' + revision + '.' + build);
            }

            components->saveNow();
            emit succeeded();
            return;
        }
    }
    else if (QFile::exists(versionJson))
    {
        QFile file(versionJson);
        if (!file.open(QIODevice::ReadOnly))
        {
            emit failed(tr("Unable to open \"version.json\"!"));
            return;
        }
        data = file.readAll();
        file.close();
    }
    else
    {
        // This is the "Vanilla" modpack, excluded by the search code
        emit failed(tr("Unable to find a \"version.json\"!"));
        return;
    }

    try
    {
        QJsonDocument doc = Json::requireDocument(data);
        QJsonObject root = Json::requireObject(doc, "version.json");
        QString minecraftVersion = Json::ensureString(root, "inheritsFrom", QString(), "");
        if (minecraftVersion.isEmpty())
        {
            if (fmlMinecraftVersion.isEmpty())
            {
                emit failed(tr("Could not understand \"version.json\":\ninheritsFrom is missing"));
                return;
            }
            minecraftVersion = fmlMinecraftVersion;
        }
        components->setComponentVersion("net.minecraft", minecraftVersion, true);
        for (auto library: Json::ensureArray(root, "libraries", {}))
        {
            if (!library.isObject())
            {
                continue;
            }

            auto libraryObject = Json::ensureObject(library, {}, "");
            auto libraryName = Json::ensureString(libraryObject, "name", "", "");

            if (libraryName.startsWith("net.minecraftforge:forge:") && libraryName.contains('-'))
            {
                QString libraryVersion = libraryName.section(':', 2);
                if (!libraryVersion.startsWith("1.7.10-"))
                {
                    components->setComponentVersion("net.minecraftforge", libraryName.section('-', 1));
                }
                else
                {
                    // 1.7.10 versions sometimes look like 1.7.10-10.13.4.1614-1.7.10, this filters out the 10.13.4.1614 part
                    components->setComponentVersion("net.minecraftforge", libraryName.section('-', 1, 1));
                }
            }
            else if (libraryName.startsWith("net.minecraftforge:minecraftforge:"))
            {
                components->setComponentVersion("net.minecraftforge", libraryName.section(':', 2));
            }
            else if (libraryName.startsWith("net.fabricmc:fabric-loader:"))
            {
                components->setComponentVersion("net.fabricmc.fabric-loader", libraryName.section(':', 2));
            }
            else if (libraryName.startsWith("net.neoforged.fancymodloader:loader:") || libraryName.startsWith("net.neoforged:neoforge:"))
            {
                components->setComponentVersion("net.neoforged", libraryName.section(':', 2));
            }
            else if (libraryName.startsWith("org.quiltmc:quilt-loader:"))
            {
                components->setComponentVersion("org.quiltmc.quilt-loader", libraryName.section(':', 2));
            }
        }
    }
    catch (const JSONValidationError &e)
    {
        emit failed(tr("Could not understand \"version.json\":\n") + e.cause());
        return;
    }

    components->saveNow();
    emit succeeded();
}
