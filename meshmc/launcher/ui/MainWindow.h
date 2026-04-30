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

#include <memory>

#include <QMainWindow>
#include <QProcess>
#include <QTimer>

#include "BaseInstance.h"
#include "minecraft/auth/MinecraftAccount.h"
#include "net/NetJob.h"
#include "updater/UpdateChecker.h"

class LaunchController;
class NewsChecker;
class NotificationChecker;
class QToolButton;
class InstanceProxyModel;
class LabeledToolButton;
class QLabel;
class MinecraftLauncher;
class BaseProfilerFactory;
class InstanceView;
class KonamiCode;
class InstanceTask;

class MainWindow : public QMainWindow
{
	Q_OBJECT

	class Ui;

  public:
	explicit MainWindow(QWidget* parent = 0);
	~MainWindow();

	bool eventFilter(QObject* obj, QEvent* ev) override;
	void closeEvent(QCloseEvent* event) override;
	void changeEvent(QEvent* event) override;

	void checkInstancePathForProblems();

	void updatesAllowedChanged(bool allowed);

	void droppedURLs(QList<QUrl> urls);
  signals:
	void isClosing();

  protected:
	QMenu* createPopupMenu() override;

  private slots:
	void onCatToggled(bool);

	void on_actionAbout_triggered();

	void on_actionPlugins_triggered();

	void on_actionMeshMCLogs_triggered();

	void on_actionAddInstance_triggered();

	void on_actionREDDIT_triggered();

	void on_actionDISCORD_triggered();

	void on_actionCopyInstance_triggered();

	void on_actionChangeInstGroup_triggered();

	void on_actionChangeInstIcon_triggered();
	void on_changeIconButton_clicked(bool)
	{
		on_actionChangeInstIcon_triggered();
	}

	void on_actionViewInstanceFolder_triggered();

	void on_actionConfig_Folder_triggered();

	void on_actionViewSelectedInstFolder_triggered();

	void on_actionViewSelectedMCFolder_triggered();

	void on_actionViewSelectedModsFolder_triggered();

	void refreshInstances();

	void on_actionViewCentralModsFolder_triggered();

	void checkForUpdates();

	void on_actionSettings_triggered();

	void on_actionInstanceSettings_triggered();

	void on_actionManageAccounts_triggered();

	void on_actionReportBug_triggered();

	void on_actionMoreNews_triggered();

	void newsButtonClicked();

	void on_actionLaunchInstance_triggered();

	void on_actionLaunchInstanceOffline_triggered();

	void on_actionDeleteInstance_triggered();

	void deleteGroup();

	void on_actionExportInstance_triggered();

	void on_actionRenameInstance_triggered();
	void on_renameButton_clicked(bool)
	{
		on_actionRenameInstance_triggered();
	}

	void on_actionEditInstance_triggered();

	void on_actionEditInstNotes_triggered();

	void on_actionMods_triggered();

	void on_actionWorlds_triggered();

	void on_actionScreenshots_triggered();

	void taskEnd();

	/**
	 * called when an icon is changed in the icon model.
	 */
	void iconUpdated(QString);

	void showInstanceContextMenu(const QPoint&);

	void updateToolsMenu();

	void instanceActivated(QModelIndex);

	void instanceChanged(const QModelIndex& current,
						 const QModelIndex& previous);

	void instanceSelectRequest(QString id);

	void instanceDataChanged(const QModelIndex& topLeft,
							 const QModelIndex& bottomRight);

	void selectionBad();

	void startTask(Task* task);

	void updateAvailable(UpdateAvailableStatus status);

	void updateNotAvailable();

	void notificationsChanged();

	void defaultAccountChanged();

	void changeActiveAccount();

	void repopulateAccountsMenu();

	void updateNewsLabel();

	/*!
	 * Stub kept for source compatibility; actual installation is delegated to
	 * the meshmc-updater binary via UpdateController.
	 */
	void downloadUpdates(UpdateAvailableStatus status);

	void konamiTriggered();

	void globalSettingsClosed();

  private:
	void retranslateUi();

	void addInstance(QString url = QString());
	void activateInstance(InstancePtr instance);
	void setCatBackground(bool enabled);
	void updateInstanceToolIcon(QString new_icon);
	void setSelectedInstanceById(const QString& id);
	void updateStatusCenter();

	void runModalTask(Task* task);
	void instanceFromInstanceTask(InstanceTask* task);
	void finalizeInstance(InstancePtr inst);

  private:
	std::unique_ptr<Ui> ui;

	// these are managed by Qt's memory management model!
	InstanceView* view = nullptr;
	InstanceProxyModel* proxymodel = nullptr;
	QToolButton* newsLabel = nullptr;
	QLabel* m_statusLeft = nullptr;
	QLabel* m_statusCenter = nullptr;
	QMenu* accountMenu = nullptr;
	QToolButton* accountMenuButton = nullptr;
	KonamiCode* secretEventFilter = nullptr;

	unique_qobject_ptr<NewsChecker> m_newsChecker;
	unique_qobject_ptr<NotificationChecker> m_notificationChecker;

	InstancePtr m_selectedInstance;
	QString m_currentInstIcon;

	// Plugin-registered instance toolbar actions
	QList<QAction*> m_pluginInstanceActions;

	// managed by the application object
	Task* m_versionLoadTask = nullptr;
};
