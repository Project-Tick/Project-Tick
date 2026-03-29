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

#include "MCEditTool.h"

#include <QDir>
#include <QProcess>
#include <QUrl>

#include "settings/SettingsObject.h"
#include "BaseInstance.h"
#include "minecraft/MinecraftInstance.h"

MCEditTool::MCEditTool(SettingsObjectPtr settings)
{
    settings->registerSetting("MCEditPath");
    m_settings = settings;
}

void MCEditTool::setPath(QString& path)
{
    m_settings->set("MCEditPath", path);
}

QString MCEditTool::path() const
{
    return m_settings->get("MCEditPath").toString();
}

bool MCEditTool::check(const QString& toolPath, QString& error)
{
    if (toolPath.isEmpty())
    {
        error = QObject::tr("Path is empty");
        return false;
    }
    const QDir dir(toolPath);
    if (!dir.exists())
    {
        error = QObject::tr("Path does not exist");
        return false;
    }
    if (!dir.exists("mcedit.sh") && !dir.exists("mcedit.py") && !dir.exists("mcedit.exe") && !dir.exists("Contents") && !dir.exists("mcedit2.exe"))
    {
        error = QObject::tr("Path does not seem to be a MCEdit path");
        return false;
    }
    return true;
}

QString MCEditTool::getProgramPath()
{
#ifdef Q_OS_MACOS
    return path();
#else
    const QString mceditPath = path();
    QDir mceditDir(mceditPath);
#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
    if (mceditDir.exists("mcedit.sh"))
    {
        return mceditDir.absoluteFilePath("mcedit.sh");
    }
    else if (mceditDir.exists("mcedit.py"))
    {
        return mceditDir.absoluteFilePath("mcedit.py");
    }
    return QString();
#elif defined(Q_OS_WIN32)
    if (mceditDir.exists("mcedit.exe"))
    {
        return mceditDir.absoluteFilePath("mcedit.exe");
    }
    else if (mceditDir.exists("mcedit2.exe"))
    {
        return mceditDir.absoluteFilePath("mcedit2.exe");
    }
    return QString();
#endif
#endif
}
