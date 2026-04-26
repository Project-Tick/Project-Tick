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

#include "AccountListPage.h"
#include "ui_AccountListPage.h"

#include <QItemSelectionModel>
#include <QMenu>
#include <QInputDialog>
#include <QLineEdit>

#include <QDebug>

#include "net/NetJob.h"

#include "ui/dialogs/ProgressDialog.h"
#include "ui/dialogs/MSALoginDialog.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/SkinUploadDialog.h"

#include "tasks/Task.h"
#include "minecraft/auth/AccountTask.h"
#include "minecraft/services/SkinDelete.h"

#include "Application.h"

#include "BuildConfig.h"

AccountListPage::AccountListPage(QWidget* parent)
	: QMainWindow(parent), ui(new Ui::AccountListPage)
{
	ui->setupUi(this);
	ui->listView->setEmptyString(
		tr("Welcome!\n"
		   "If you're new here, you can click the \"Add Microsoft\" button to "
		   "add your Microsoft account."));
	ui->listView->setEmptyMode(VersionListView::String);
	ui->listView->setContextMenuPolicy(Qt::CustomContextMenu);

	m_accounts = APPLICATION->accounts();

	ui->listView->setModel(m_accounts.get());
	// Expand the account column
	ui->listView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	ui->listView->header()->setSectionResizeMode(1, QHeaderView::Stretch);
	ui->listView->header()->setSectionResizeMode(2,
												 QHeaderView::ResizeToContents);

	QItemSelectionModel* selectionModel = ui->listView->selectionModel();

	connect(selectionModel, &QItemSelectionModel::selectionChanged,
			[this](const QItemSelection& sel, const QItemSelection& dsel) {
				updateButtonStates();
			});
	connect(ui->listView, &VersionListView::customContextMenuRequested, this,
			&AccountListPage::ShowContextMenu);

	connect(m_accounts.get(), &AccountList::listChanged, this,
			&AccountListPage::listChanged);
	connect(m_accounts.get(), &AccountList::listActivityChanged, this,
			&AccountListPage::listChanged);
	connect(m_accounts.get(), &AccountList::defaultAccountChanged, this,
			&AccountListPage::listChanged);

	updateButtonStates();

	// Xbox authentication won't work without a client identifier, so disable
	// the button if it is missing
	ui->actionAddMicrosoft->setVisible(!BuildConfig.MSAClientID.isEmpty());
}

AccountListPage::~AccountListPage()
{
	delete ui;
}

void AccountListPage::ShowContextMenu(const QPoint& pos)
{
	auto menu = ui->toolBar->createContextMenu(this, tr("Context menu"));
	menu->exec(ui->listView->mapToGlobal(pos));
	delete menu;
}

void AccountListPage::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange) {
		ui->retranslateUi(this);
	}
	QMainWindow::changeEvent(event);
}

QMenu* AccountListPage::createPopupMenu()
{
	QMenu* filteredMenu = QMainWindow::createPopupMenu();
	filteredMenu->removeAction(ui->toolBar->toggleViewAction());
	return filteredMenu;
}

void AccountListPage::listChanged()
{
	updateButtonStates();
}

void AccountListPage::on_actionAddMicrosoft_triggered()
{
	if (BuildConfig.BUILD_PLATFORM == "osx64") {
		CustomMessageBox::selectable(
			this, tr("Microsoft Accounts not available"),
			tr("Microsoft accounts are only usable on macOS 10.13 or newer, "
			   "with fully updated MeshMC.\n\n"
			   "Please update both your operating system and MeshMC."),
			QMessageBox::Warning)
			->exec();
		return;
	}
	MinecraftAccountPtr account = MSALoginDialog::newAccount(
		this, tr("Log in with your Microsoft account to add it."));

	if (account) {
		m_accounts->addAccount(account);
		if (m_accounts->count() == 1) {
			m_accounts->setDefaultAccount(account);
		}
	}
}

void AccountListPage::on_actionAddOffline_triggered()
{
	// Check if at least one MSA account exists
	bool hasMSA = false;
	for (int i = 0; i < m_accounts->count(); i++) {
		if (m_accounts->at(i)->isMSA()) {
			hasMSA = true;
			break;
		}
	}
	if (!hasMSA) {
		CustomMessageBox::selectable(
			this, tr("Microsoft Account Required"),
			tr("Adding an offline account requires at least one Microsoft "
			   "account to be logged in. Please add a Microsoft account first."),
			QMessageBox::Warning)
			->exec();
		return;
	}

	bool ok = false;
	QString username = QInputDialog::getText(
		this, tr("Add Offline Account"),
		tr("Enter a username for the offline account:"),
		QLineEdit::Normal, tr("User"), &ok);
	if (!ok || username.trimmed().isEmpty()) {
		return;
	}

	auto account = MinecraftAccount::createOffline(username.trimmed());
	m_accounts->addAccount(account);
}

void AccountListPage::on_actionRemove_triggered()
{
	QModelIndexList selection =
		ui->listView->selectionModel()->selectedIndexes();
	if (selection.size() > 0) {
		QModelIndex selected = selection.first();
		m_accounts->removeAccount(selected);
	}
}

void AccountListPage::on_actionRefresh_triggered()
{
	QModelIndexList selection =
		ui->listView->selectionModel()->selectedIndexes();
	if (selection.size() > 0) {
		QModelIndex selected = selection.first();
		MinecraftAccountPtr account = selected.data(AccountList::PointerRole)
										  .value<MinecraftAccountPtr>();
		m_accounts->requestRefresh(account->internalId());
	}
}

void AccountListPage::on_actionSetDefault_triggered()
{
	QModelIndexList selection =
		ui->listView->selectionModel()->selectedIndexes();
	if (selection.size() > 0) {
		QModelIndex selected = selection.first();
		MinecraftAccountPtr account = selected.data(AccountList::PointerRole)
										  .value<MinecraftAccountPtr>();
		m_accounts->setDefaultAccount(account);
	}
}

void AccountListPage::on_actionNoDefault_triggered()
{
	m_accounts->setDefaultAccount(nullptr);
}

void AccountListPage::updateButtonStates()
{
	// If there is no selection, disable buttons that require something
	// selected.
	QModelIndexList selection =
		ui->listView->selectionModel()->selectedIndexes();
	bool hasSelection = selection.size() > 0;
	bool accountIsReady = false;
	if (hasSelection) {
		QModelIndex selected = selection.first();
		MinecraftAccountPtr account = selected.data(AccountList::PointerRole)
										  .value<MinecraftAccountPtr>();
		accountIsReady = !account->isActive();
	}
	ui->actionRemove->setEnabled(accountIsReady);
	ui->actionSetDefault->setEnabled(accountIsReady);
	ui->actionUploadSkin->setEnabled(accountIsReady);
	ui->actionDeleteSkin->setEnabled(accountIsReady);
	ui->actionRefresh->setEnabled(accountIsReady);

	// Offline account button: only enabled if at least one MSA account exists
	bool hasMSA = false;
	for (int i = 0; i < m_accounts->count(); i++) {
		if (m_accounts->at(i)->isMSA()) {
			hasMSA = true;
			break;
		}
	}
	ui->actionAddOffline->setEnabled(hasMSA);

	if (m_accounts->defaultAccount().get() == nullptr) {
		ui->actionNoDefault->setEnabled(false);
		ui->actionNoDefault->setChecked(true);
	} else {
		ui->actionNoDefault->setEnabled(true);
		ui->actionNoDefault->setChecked(false);
	}
}

void AccountListPage::on_actionUploadSkin_triggered()
{
	QModelIndexList selection =
		ui->listView->selectionModel()->selectedIndexes();
	if (selection.size() > 0) {
		QModelIndex selected = selection.first();
		MinecraftAccountPtr account = selected.data(AccountList::PointerRole)
										  .value<MinecraftAccountPtr>();
		SkinUploadDialog dialog(account, this);
		dialog.exec();
	}
}

void AccountListPage::on_actionDeleteSkin_triggered()
{
	QModelIndexList selection =
		ui->listView->selectionModel()->selectedIndexes();
	if (selection.size() <= 0)
		return;

	QModelIndex selected = selection.first();
	MinecraftAccountPtr account =
		selected.data(AccountList::PointerRole).value<MinecraftAccountPtr>();

	/* Confirm before permanently deleting the skin. */
	auto reply = QMessageBox::question(
		this, tr("Delete Skin"),
		tr("Are you sure you want to delete the skin for account \"%1\"?\n"
		   "This action cannot be undone.")
			.arg(account->profileName()),
		QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
	if (reply != QMessageBox::Yes)
		return;

	ProgressDialog prog(this);
	auto deleteSkinTask =
		std::make_shared<SkinDelete>(this, account->accessToken());
	if (prog.execWithTask((Task*)deleteSkinTask.get()) != QDialog::Accepted) {
		CustomMessageBox::selectable(this, tr("Skin Delete"),
									 tr("Failed to delete current skin!"),
									 QMessageBox::Warning)
			->exec();
		return;
	}
}
