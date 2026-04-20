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

#include "NotificationDialog.h"
#include "ui_NotificationDialog.h"

#include <QTimerEvent>
#include <QStyle>

NotificationDialog::NotificationDialog(
	const NotificationChecker::NotificationEntry& entry, QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
						  Qt::WindowCloseButtonHint),
	  ui(new Ui::NotificationDialog)
{
	ui->setupUi(this);

	QStyle::StandardPixmap icon;
	switch (entry.type) {
		case NotificationChecker::NotificationEntry::Critical:
			icon = QStyle::SP_MessageBoxCritical;
			break;
		case NotificationChecker::NotificationEntry::Warning:
			icon = QStyle::SP_MessageBoxWarning;
			break;
		default:
		case NotificationChecker::NotificationEntry::Information:
			icon = QStyle::SP_MessageBoxInformation;
			break;
	}
	ui->iconLabel->setPixmap(style()->standardPixmap(icon, 0, this));
	ui->messageLabel->setText(entry.message);

	m_dontShowAgainText = tr("Don't show again");
	m_closeText = tr("Close");

	ui->dontShowAgainBtn->setText(m_dontShowAgainText +
								  QString(" (%1)").arg(m_dontShowAgainTime));
	ui->closeBtn->setText(m_closeText + QString(" (%1)").arg(m_closeTime));

	startTimer(1000);
}

NotificationDialog::~NotificationDialog()
{
	delete ui;
}

void NotificationDialog::timerEvent(QTimerEvent* event)
{
	if (m_dontShowAgainTime > 0) {
		m_dontShowAgainTime--;
		if (m_dontShowAgainTime == 0) {
			ui->dontShowAgainBtn->setText(m_dontShowAgainText);
			ui->dontShowAgainBtn->setEnabled(true);
		} else {
			ui->dontShowAgainBtn->setText(
				m_dontShowAgainText +
				QString(" (%1)").arg(m_dontShowAgainTime));
		}
	}
	if (m_closeTime > 0) {
		m_closeTime--;
		if (m_closeTime == 0) {
			ui->closeBtn->setText(m_closeText);
			ui->closeBtn->setEnabled(true);
		} else {
			ui->closeBtn->setText(m_closeText +
								  QString(" (%1)").arg(m_closeTime));
		}
	}

	if (m_closeTime == 0 && m_dontShowAgainTime == 0) {
		killTimer(event->timerId());
	}
}

void NotificationDialog::on_dontShowAgainBtn_clicked()
{
	done(DontShowAgain);
}
void NotificationDialog::on_closeBtn_clicked()
{
	done(Normal);
}
