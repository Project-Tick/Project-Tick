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

#include <QMainWindow>

#include "minecraft/MinecraftInstance.h"
#include "ui/pages/BasePage.h"
#include "modplatform/ContentType.h"

#include <Application.h>

class ModFolderModel;
namespace Ui
{
	class ModFolderPage;
}

class ModFolderPage : public QMainWindow, public BasePage
{
	Q_OBJECT

  public:
	explicit ModFolderPage(BaseInstance* inst,
						   std::shared_ptr<ModFolderModel> mods, QString id,
						   QString iconName, QString displayName,
						   QString helpPage = "", QWidget* parent = 0);
	virtual ~ModFolderPage();

	void setFilter(const QString& filter)
	{
		m_fileSelectionFilter = filter;
	}

	void setContentType(ModPlatform::ContentType type)
	{
		m_contentType = type;
	}

	virtual QString displayName() const override
	{
		return m_displayName;
	}
	virtual QIcon icon() const override
	{
		return APPLICATION->getThemedIcon(m_iconName);
	}
	virtual QString id() const override
	{
		return m_id;
	}
	virtual QString helpPage() const override
	{
		return m_helpName;
	}
	virtual bool shouldDisplay() const override;

	virtual void openedImpl() override;
	virtual void closedImpl() override;

  protected:
	bool eventFilter(QObject* obj, QEvent* ev) override;
	bool modListFilter(QKeyEvent* ev);
	QMenu* createPopupMenu() override;

  protected:
	BaseInstance* m_inst = nullptr;

  protected:
	Ui::ModFolderPage* ui = nullptr;
	std::shared_ptr<ModFolderModel> m_mods;
	QSortFilterProxyModel* m_filterModel = nullptr;
	QString m_iconName;
	QString m_id;
	QString m_displayName;
	QString m_helpName;
	QString m_fileSelectionFilter;
	QString m_viewFilter;
	bool m_controlsEnabled = true;
	ModPlatform::ContentType m_contentType = ModPlatform::ContentType::Mod;

  public slots:
	void modCurrent(const QModelIndex& current, const QModelIndex& previous);

  private slots:
	void modItemActivated(const QModelIndex& index);
	void on_filterTextChanged(const QString& newContents);
	void runningStateChanged(bool running);
	void on_actionAdd_triggered();
	void on_actionRemove_triggered();
	void on_actionEnable_triggered();
	void on_actionDisable_triggered();
	void on_actionView_Folder_triggered();
	void on_actionView_configs_triggered();
	void on_actionDownload_triggered();
	void ShowContextMenu(const QPoint& pos);
};

class CoreModFolderPage : public ModFolderPage
{
  public:
	explicit CoreModFolderPage(BaseInstance* inst,
							   std::shared_ptr<ModFolderModel> mods, QString id,
							   QString iconName, QString displayName,
							   QString helpPage = "", QWidget* parent = 0);
	virtual ~CoreModFolderPage() {}
	virtual bool shouldDisplay() const;
};
