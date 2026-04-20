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

#include "PageDialog.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QKeyEvent>

#include "Application.h"
#include "settings/SettingsObject.h"

#include "ui/widgets/IconLabel.h"
#include "ui/widgets/PageContainer.h"

PageDialog::PageDialog(BasePageProvider* pageProvider, QString defaultId,
					   QWidget* parent)
	: QDialog(parent)
{
	setWindowModality(Qt::WindowModal);
	setWindowTitle(pageProvider->dialogTitle());
	m_container = new PageContainer(pageProvider, defaultId, this);

	QVBoxLayout* mainLayout = new QVBoxLayout;
	mainLayout->addWidget(m_container);
	mainLayout->setSpacing(0);
	mainLayout->setContentsMargins(0, 0, 0, 0);
	setLayout(mainLayout);

	QDialogButtonBox* buttons =
		new QDialogButtonBox(QDialogButtonBox::Help | QDialogButtonBox::Close);
	buttons->button(QDialogButtonBox::Close)->setDefault(true);
	buttons->setContentsMargins(6, 0, 6, 0);
	m_container->addButtons(buttons);

	connect(buttons->button(QDialogButtonBox::Close), &QPushButton::clicked,
			this, &PageDialog::close);
	connect(buttons->button(QDialogButtonBox::Help), &QPushButton::clicked,
			m_container, &PageContainer::help);

	restoreGeometry(QByteArray::fromBase64(
		APPLICATION->settings()->get("PagedGeometry").toByteArray()));
}

void PageDialog::closeEvent(QCloseEvent* event)
{
	qDebug() << "Paged dialog close requested";
	if (m_container->prepareToClose()) {
		qDebug() << "Paged dialog close approved";
		APPLICATION->settings()->set("PagedGeometry",
									 saveGeometry().toBase64());
		qDebug() << "Paged dialog geometry saved";
		QDialog::closeEvent(event);
	}
}
