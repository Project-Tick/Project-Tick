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
#include "BaseInstance.h"
#include "launch/LaunchTask.h"

class NullInstance : public BaseInstance
{
	Q_OBJECT
  public:
	NullInstance(SettingsObjectPtr globalSettings, SettingsObjectPtr settings,
				 const QString& rootDir)
		: BaseInstance(globalSettings, settings, rootDir)
	{
		setVersionBroken(true);
	}
	virtual ~NullInstance() {};
	void saveNow() override {}
	QString getStatusbarDescription() override
	{
		return tr("Unknown instance type");
	};
	QSet<QString> traits() const override
	{
		return {};
	};
	QString instanceConfigFolder() const override
	{
		return instanceRoot();
	};
	shared_qobject_ptr<LaunchTask>
	createLaunchTask(AuthSessionPtr, MinecraftServerTargetPtr) override
	{
		return nullptr;
	}
	shared_qobject_ptr<Task> createUpdateTask(Net::Mode) override
	{
		return nullptr;
	}
	QProcessEnvironment createEnvironment() override
	{
		return QProcessEnvironment();
	}
	QMap<QString, QString> getVariables() const override
	{
		return QMap<QString, QString>();
	}
	IPathMatcher::Ptr getLogFileMatcher() override
	{
		return nullptr;
	}
	QString getLogFileRoot() override
	{
		return instanceRoot();
	}
	QString typeName() const override
	{
		return "Null";
	}
	bool canExport() const override
	{
		return false;
	}
	bool canEdit() const override
	{
		return false;
	}
	bool canLaunch() const override
	{
		return false;
	}
	QStringList
	verboseDescription(AuthSessionPtr,
					   MinecraftServerTargetPtr) override
	{
		QStringList out;
		out << "Null instance - placeholder.";
		return out;
	}
	QString modsRoot() const override
	{
		return QString();
	}
};
