/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: GPL-3.0-or-later WITH LicenseRef-MeshMC-MMCO-Module-Exception-1.0
 *
 *   MeshMC - A Custom Launcher for Minecraft
 *   Copyright (C) 2026 Project Tick
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version, with the additional permission
 *   described in the MeshMC MMCO Module Exception 1.0.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 *   You should have received a copy of the MeshMC MMCO Module Exception 1.0
 *   along with this program.  If not, see <https://projecttick.org/licenses/>.
 */

#pragma once
#include <QObject>
#include <BaseInstance.h>
#include <tools/BaseProfiler.h>

#include "minecraft/launch/MinecraftServerTarget.h"
#include "minecraft/auth/MinecraftAccount.h"

class InstanceWindow;
class LaunchController : public Task
{
	Q_OBJECT
  public:
	void executeTask() override;

	LaunchController(QObject* parent = nullptr);
	virtual ~LaunchController() {};

	void setInstance(InstancePtr instance)
	{
		m_instance = instance;
	}

	InstancePtr instance()
	{
		return m_instance;
	}

	void setOnline(bool online)
	{
		m_online = online;
	}

	void setProfiler(BaseProfilerFactory* profiler)
	{
		m_profiler = profiler;
	}

	void setParentWidget(QWidget* widget)
	{
		m_parentWidget = widget;
	}

	void setServerToJoin(MinecraftServerTargetPtr serverToJoin)
	{
		m_serverToJoin = std::move(serverToJoin);
	}

	void setAccountToUse(MinecraftAccountPtr accountToUse)
	{
		m_accountToUse = std::move(accountToUse);
	}

	QString id()
	{
		return m_instance->id();
	}

	bool abort() override;

  private:
	void login();
	void launchInstance();
	void decideAccount();

  private slots:
	void readyForLaunch();

	void onSucceeded();
	void onFailed(QString reason);
	void onProgressRequested(Task* task);

  private:
	BaseProfilerFactory* m_profiler = nullptr;
	bool m_online = true;
	bool m_demoMode = false;
	QString m_demoUsername;
	InstancePtr m_instance;
	QWidget* m_parentWidget = nullptr;
	InstanceWindow* m_console = nullptr;
	MinecraftAccountPtr m_accountToUse = nullptr;
	AuthSessionPtr m_session;
	shared_qobject_ptr<LaunchTask> m_launcher;
	MinecraftServerTargetPtr m_serverToJoin;
};
