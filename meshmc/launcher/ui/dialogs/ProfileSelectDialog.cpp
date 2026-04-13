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

#include "ProfileSelectDialog.h"
#include "ui_ProfileSelectDialog.h"

#include <QItemSelectionModel>
#include <QDebug>

#include "SkinUtils.h"
#include "Application.h"

#include "ui/dialogs/ProgressDialog.h"

ProfileSelectDialog::ProfileSelectDialog(const QString& message, int flags,
										 QWidget* parent)
	: QDialog(parent), ui(new Ui::ProfileSelectDialog)
{
	ui->setupUi(this);

	m_accounts = APPLICATION->accounts();
	auto view = ui->listView;
	// view->setModel(m_accounts.get());
	// view->hideColumn(AccountList::ActiveColumn);
	view->setColumnCount(1);
	view->setRootIsDecorated(false);
	// FIXME: use a real model, not this
	if (QTreeWidgetItem* header = view->headerItem()) {
		header->setText(0, tr("Name"));
	} else {
		view->setHeaderLabel(tr("Name"));
	}
	QList<QTreeWidgetItem*> items;
	for (int i = 0; i < m_accounts->count(); i++) {
		MinecraftAccountPtr account = m_accounts->at(i);
		QString profileLabel;
		if (account->isInUse()) {
			profileLabel = tr("%1 (in use)").arg(account->profileName());
		} else {
			profileLabel = account->profileName();
		}
		auto item = new QTreeWidgetItem(view);
		item->setText(0, profileLabel);
		item->setIcon(0, account->getFace());
		item->setData(0, AccountList::PointerRole,
					  QVariant::fromValue(account));
		items.append(item);
	}
	view->addTopLevelItems(items);

	// Set the message label.
	ui->msgLabel->setVisible(!message.isEmpty());
	ui->msgLabel->setText(message);

	// Flags...
	ui->globalDefaultCheck->setVisible(flags & GlobalDefaultCheckbox);
	ui->instDefaultCheck->setVisible(flags & InstanceDefaultCheckbox);
	qDebug() << flags;

	// Select the first entry in the list.
	ui->listView->setCurrentIndex(ui->listView->model()->index(0, 0));

	connect(ui->listView, &QTreeWidget::doubleClicked, this,
			&ProfileSelectDialog::on_buttonBox_accepted);
}

ProfileSelectDialog::~ProfileSelectDialog()
{
	delete ui;
}

MinecraftAccountPtr ProfileSelectDialog::selectedAccount() const
{
	return m_selected;
}

bool ProfileSelectDialog::useAsGlobalDefault() const
{
	return ui->globalDefaultCheck->isChecked();
}

bool ProfileSelectDialog::useAsInstDefaullt() const
{
	return ui->instDefaultCheck->isChecked();
}

void ProfileSelectDialog::on_buttonBox_accepted()
{
	QModelIndexList selection =
		ui->listView->selectionModel()->selectedIndexes();
	if (selection.size() > 0) {
		QModelIndex selected = selection.first();
		m_selected = selected.data(AccountList::PointerRole)
						 .value<MinecraftAccountPtr>();
	}
	close();
}

void ProfileSelectDialog::on_buttonBox_rejected()
{
	close();
}
