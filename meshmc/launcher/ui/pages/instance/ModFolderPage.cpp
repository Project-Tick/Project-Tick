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

#include "ModFolderPage.h"
#include "ui_ModFolderPage.h"

#include <QMessageBox>
#include <QEvent>
#include <QKeyEvent>
#include <QAbstractItemModel>
#include <QMenu>
#include <QSortFilterProxyModel>

#include "Application.h"

#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/DownloadContentDialog.h"
#include "ui/dialogs/DownloadSummaryDialog.h"
#include "ui/dialogs/ProgressDialog.h"
#include "ui/GuiUtil.h"

#include "DesktopServices.h"

#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/Mod.h"
#include "minecraft/VersionFilterData.h"
#include "minecraft/PackProfile.h"
#include "modplatform/DependencyResolver.h"
#include "modplatform/ContentDownloadTask.h"

#include "Version.h"

namespace
{
	// FIXME: wasteful
	void RemoveThePrefix(QString& string)
	{
		QRegularExpression regex(
			QStringLiteral("^(([Tt][Hh][eE])|([Tt][eE][Hh])) +"));
		string.remove(regex);
		string = string.trimmed();
	}
} // namespace

class ModSortProxy : public QSortFilterProxyModel
{
  public:
	explicit ModSortProxy(QObject* parent = 0) : QSortFilterProxyModel(parent)
	{
	}

  protected:
	bool filterAcceptsRow(int source_row,
						  const QModelIndex& source_parent) const override
	{
		ModFolderModel* model = qobject_cast<ModFolderModel*>(sourceModel());
		if (!model) {
			return false;
		}
		const auto& mod = model->at(source_row);
		if (mod.name().contains(filterRegularExpression())) {
			return true;
		}
		if (mod.description().contains(filterRegularExpression())) {
			return true;
		}
		for (auto& author : mod.authors()) {
			if (author.contains(filterRegularExpression())) {
				return true;
			}
		}
		return false;
	}

	bool lessThan(const QModelIndex& source_left,
				  const QModelIndex& source_right) const override
	{
		ModFolderModel* model = qobject_cast<ModFolderModel*>(sourceModel());
		if (!model || !source_left.isValid() || !source_right.isValid() ||
			source_left.column() != source_right.column()) {
			return QSortFilterProxyModel::lessThan(source_left, source_right);
		}

		// we are now guaranteed to have two valid indexes in the same column...
		// we love the provided invariants unconditionally and proceed.

		auto column = (ModFolderModel::Columns)source_left.column();
		bool invert = false;
		switch (column) {
			// GH-2550 - sort by enabled/disabled
			case ModFolderModel::ActiveColumn: {
				auto dataL = source_left.data(Qt::CheckStateRole).toBool();
				auto dataR = source_right.data(Qt::CheckStateRole).toBool();
				if (dataL != dataR) {
					return dataL > dataR;
				}
				// fallthrough
				invert = sortOrder() == Qt::DescendingOrder;
			}
			// GH-2722 - sort mod names in a way that discards "The" prefixes
			case ModFolderModel::NameColumn: {
				auto dataL =
					model
						->data(model->index(source_left.row(),
											ModFolderModel::NameColumn))
						.toString();
				RemoveThePrefix(dataL);
				auto dataR =
					model
						->data(model->index(source_right.row(),
											ModFolderModel::NameColumn))
						.toString();
				RemoveThePrefix(dataR);

				auto less = dataL.compare(dataR, sortCaseSensitivity());
				if (less != 0) {
					return invert ? (less > 0) : (less < 0);
				}
				// fallthrough
				invert = sortOrder() == Qt::DescendingOrder;
			}
			// GH-2762 - sort versions by parsing them as versions
			case ModFolderModel::VersionColumn: {
				auto dataL = Version(
					model
						->data(model->index(source_left.row(),
											ModFolderModel::VersionColumn))
						.toString());
				auto dataR = Version(
					model
						->data(model->index(source_right.row(),
											ModFolderModel::VersionColumn))
						.toString());
				return invert ? (dataL > dataR) : (dataL < dataR);
			}
			default: {
				return QSortFilterProxyModel::lessThan(source_left,
													   source_right);
			}
		}
	}
};

ModFolderPage::ModFolderPage(BaseInstance* inst,
							 std::shared_ptr<ModFolderModel> mods, QString id,
							 QString iconName, QString displayName,
							 QString helpPage, QWidget* parent)
	: QMainWindow(parent), ui(new Ui::ModFolderPage)
{
	ui->setupUi(this);
	ui->actionsToolbar->insertSpacer(ui->actionView_configs);

	m_inst = inst;
	on_RunningState_changed(m_inst && m_inst->isRunning());
	m_mods = mods;
	m_id = id;
	m_displayName = displayName;
	m_iconName = iconName;
	m_helpName = helpPage;
	m_fileSelectionFilter = "%1 (*.zip *.jar)";
	m_filterModel = new ModSortProxy(this);
	m_filterModel->setDynamicSortFilter(true);
	m_filterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
	m_filterModel->setSortCaseSensitivity(Qt::CaseInsensitive);
	m_filterModel->setSourceModel(m_mods.get());
	m_filterModel->setFilterKeyColumn(-1);
	ui->modTreeView->setModel(m_filterModel);
	ui->modTreeView->installEventFilter(this);
	ui->modTreeView->sortByColumn(1, Qt::AscendingOrder);
	ui->modTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui->modTreeView, &ModListView::customContextMenuRequested, this,
			&ModFolderPage::ShowContextMenu);
	connect(ui->modTreeView, &ModListView::activated, this,
			&ModFolderPage::modItemActivated);

	auto smodel = ui->modTreeView->selectionModel();
	connect(smodel, &QItemSelectionModel::currentChanged, this,
			&ModFolderPage::modCurrent);
	connect(ui->filterEdit, &QLineEdit::textChanged, this,
			&ModFolderPage::on_filterTextChanged);
	connect(m_inst, &BaseInstance::runningStatusChanged, this,
			&ModFolderPage::on_RunningState_changed);
}

void ModFolderPage::modItemActivated(const QModelIndex&)
{
	if (!m_controlsEnabled) {
		return;
	}
	auto selection = m_filterModel->mapSelectionToSource(
		ui->modTreeView->selectionModel()->selection());
	m_mods->setModStatus(selection.indexes(), ModFolderModel::Toggle);
}

QMenu* ModFolderPage::createPopupMenu()
{
	QMenu* filteredMenu = QMainWindow::createPopupMenu();
	filteredMenu->removeAction(ui->actionsToolbar->toggleViewAction());
	return filteredMenu;
}

void ModFolderPage::ShowContextMenu(const QPoint& pos)
{
	auto menu = ui->actionsToolbar->createContextMenu(this, tr("Context menu"));
	menu->exec(ui->modTreeView->mapToGlobal(pos));
	delete menu;
}

void ModFolderPage::openedImpl()
{
	m_mods->startWatching();
}

void ModFolderPage::closedImpl()
{
	m_mods->stopWatching();
}

void ModFolderPage::on_filterTextChanged(const QString& newContents)
{
	m_viewFilter = newContents;
	m_filterModel->setFilterFixedString(m_viewFilter);
}

CoreModFolderPage::CoreModFolderPage(BaseInstance* inst,
									 std::shared_ptr<ModFolderModel> mods,
									 QString id, QString iconName,
									 QString displayName, QString helpPage,
									 QWidget* parent)
	: ModFolderPage(inst, mods, id, iconName, displayName, helpPage, parent)
{
}

ModFolderPage::~ModFolderPage()
{
	m_mods->stopWatching();
	delete ui;
}

void ModFolderPage::on_RunningState_changed(bool running)
{
	if (m_controlsEnabled == !running) {
		return;
	}
	m_controlsEnabled = !running;
	ui->actionAdd->setEnabled(m_controlsEnabled);
	ui->actionDisable->setEnabled(m_controlsEnabled);
	ui->actionEnable->setEnabled(m_controlsEnabled);
	ui->actionRemove->setEnabled(m_controlsEnabled);

	// Resource packs and shader packs can be safely added while
	// Minecraft is running (reloaded via F3+T or settings menu)
	bool canDownload =
		m_controlsEnabled ||
		m_contentType == ModPlatform::ContentType::ResourcePack ||
		m_contentType == ModPlatform::ContentType::ShaderPack;
	ui->actionDownload->setEnabled(canDownload);
	ui->actionAdd->setEnabled(m_controlsEnabled || canDownload);
}

bool ModFolderPage::shouldDisplay() const
{
	return true;
}

bool CoreModFolderPage::shouldDisplay() const
{
	if (ModFolderPage::shouldDisplay()) {
		auto inst = dynamic_cast<MinecraftInstance*>(m_inst);
		if (!inst)
			return true;
		auto version = inst->getPackProfile();
		if (!version)
			return true;
		if (!version->getComponent("net.minecraftforge")) {
			return false;
		}
		if (!version->getComponent("net.minecraft")) {
			return false;
		}
		if (version->getComponent("net.minecraft")->getReleaseDateTime() <
			g_VersionFilterData.legacyCutoffDate) {
			return true;
		}
	}
	return false;
}

bool ModFolderPage::modListFilter(QKeyEvent* keyEvent)
{
	switch (keyEvent->key()) {
		case Qt::Key_Delete:
			on_actionRemove_triggered();
			return true;
		case Qt::Key_Plus:
			on_actionAdd_triggered();
			return true;
		default:
			break;
	}
	return QWidget::eventFilter(ui->modTreeView, keyEvent);
}

bool ModFolderPage::eventFilter(QObject* obj, QEvent* ev)
{
	if (ev->type() != QEvent::KeyPress) {
		return QWidget::eventFilter(obj, ev);
	}
	QKeyEvent* keyEvent = static_cast<QKeyEvent*>(ev);
	if (obj == ui->modTreeView)
		return modListFilter(keyEvent);
	return QWidget::eventFilter(obj, ev);
}

void ModFolderPage::on_actionAdd_triggered()
{
	if (!m_controlsEnabled &&
		m_contentType != ModPlatform::ContentType::ResourcePack &&
		m_contentType != ModPlatform::ContentType::ShaderPack) {
		return;
	}
	auto list = GuiUtil::BrowseForFiles(
		m_helpName,
		tr("Select %1", "Select whatever type of files the page contains. "
						"Example: 'Loader Mods'")
			.arg(m_displayName),
		m_fileSelectionFilter.arg(m_displayName),
		APPLICATION->settings()->get("CentralModsDir").toString(),
		this->parentWidget());
	if (!list.empty()) {
		for (auto filename : list) {
			m_mods->installMod(filename);
		}
	}
}

void ModFolderPage::on_actionEnable_triggered()
{
	if (!m_controlsEnabled) {
		return;
	}
	auto selection = m_filterModel->mapSelectionToSource(
		ui->modTreeView->selectionModel()->selection());
	m_mods->setModStatus(selection.indexes(), ModFolderModel::Enable);
}

void ModFolderPage::on_actionDisable_triggered()
{
	if (!m_controlsEnabled) {
		return;
	}
	auto selection = m_filterModel->mapSelectionToSource(
		ui->modTreeView->selectionModel()->selection());
	m_mods->setModStatus(selection.indexes(), ModFolderModel::Disable);
}

void ModFolderPage::on_actionRemove_triggered()
{
	if (!m_controlsEnabled) {
		return;
	}
	auto selection = m_filterModel->mapSelectionToSource(
		ui->modTreeView->selectionModel()->selection());
	m_mods->deleteMods(selection.indexes());
}

void ModFolderPage::on_actionView_configs_triggered()
{
	DesktopServices::openDirectory(m_inst->instanceConfigFolder(), true);
}

void ModFolderPage::on_actionView_Folder_triggered()
{
	DesktopServices::openDirectory(m_mods->dir().absolutePath(), true);
}

void ModFolderPage::modCurrent(const QModelIndex& current,
							   const QModelIndex& previous)
{
	if (!current.isValid()) {
		ui->frame->clear();
		return;
	}
	auto sourceCurrent = m_filterModel->mapToSource(current);
	int row = sourceCurrent.row();
	Mod& m = m_mods->operator[](row);
	ui->frame->updateWithMod(m);
}

void ModFolderPage::on_actionDownload_triggered()
{
	if (!m_controlsEnabled &&
		m_contentType != ModPlatform::ContentType::ResourcePack &&
		m_contentType != ModPlatform::ContentType::ShaderPack) {
		return;
	}

	auto* mcInst = dynamic_cast<MinecraftInstance*>(m_inst);
	if (!mcInst) {
		return;
	}

	// For mods, require a mod loader to be installed
	if (m_contentType == ModPlatform::ContentType::Mod) {
		auto profile = mcInst->getPackProfile();
		bool hasLoader = false;
		if (profile) {
			hasLoader = profile->getComponent("net.minecraftforge") ||
						profile->getComponent("net.fabricmc.fabric-loader") ||
						profile->getComponent("org.quiltmc.quilt-loader") ||
						profile->getComponent("net.neoforged.neoforge");
		}
		if (!hasLoader) {
			QMessageBox::warning(
				this->parentWidget(), tr("No Mod Loader"),
				tr("No mod loader (Forge, Fabric, Quilt, or NeoForge) is "
				   "installed "
				   "in this instance. Please install a mod loader first before "
				   "downloading mods."));
			return;
		}
	}

	// Step 1: Open browse dialog
	DownloadContentDialog browseDialog(mcInst, m_contentType,
									   this->parentWidget());
	if (browseDialog.exec() != QDialog::Accepted) {
		return;
	}

	auto selectedMods = browseDialog.selectedMods();
	if (selectedMods.isEmpty()) {
		return;
	}

	// Step 2: Resolve dependencies (only for mods)
	QList<ModPlatform::DependencyInfo> dependencies;
	QList<ModPlatform::UnresolvedDep> unresolvedDeps;
	if (m_contentType == ModPlatform::ContentType::Mod) {
		auto* resolver = new DependencyResolver(
			selectedMods, browseDialog.mcVersion(), browseDialog.loaderType());

		ProgressDialog depProgress(this->parentWidget());
		depProgress.setSkipButton(true, tr("Skip"));
		if (depProgress.execWithTask(resolver) != QDialog::Accepted) {
			if (!resolver->wasSuccessful()) {
				qWarning() << "Dependency resolution failed or was skipped";
			}
		}
		dependencies = resolver->resolvedDependencies();
		unresolvedDeps = resolver->unresolvedDependencies();
		delete resolver;
	}

	// Step 3: Show summary dialog
	DownloadSummaryDialog summaryDialog(selectedMods, dependencies,
										unresolvedDeps, this->parentWidget());
	if (summaryDialog.exec() != QDialog::Accepted) {
		return;
	}

	// Step 4: Download everything
	auto downloadItems = summaryDialog.downloadItems();
	QString targetDir = m_mods->dir().absolutePath();

	auto* downloadTask = new ContentDownloadTask(downloadItems, targetDir);
	ProgressDialog downloadProgress(this->parentWidget());
	downloadProgress.execWithTask(downloadTask);

	if (downloadTask->wasSuccessful()) {
		m_mods->update();
	} else {
		QMessageBox::warning(this->parentWidget(), tr("Download Failed"),
							 tr("Some files failed to download: %1")
								 .arg(downloadTask->failReason()));
	}
	delete downloadTask;
}
