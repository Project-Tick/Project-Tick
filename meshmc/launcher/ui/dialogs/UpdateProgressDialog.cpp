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

#include "UpdateProgressDialog.h"

#include <QVBoxLayout>

UpdateProgressDialog::UpdateProgressDialog(QWidget* parent)
	: QDialog(parent)
{
	setWindowTitle(tr("MeshMC Update"));
	setMinimumSize(500, 400);
	setModal(true);

	auto* layout = new QVBoxLayout(this);

	m_statusLabel = new QLabel(tr("Checking for updates..."), this);
	m_statusLabel->setAlignment(Qt::AlignCenter);
	QFont font = m_statusLabel->font();
	font.setPointSize(12);
	font.setBold(true);
	m_statusLabel->setFont(font);
	layout->addWidget(m_statusLabel);

	m_progressBar = new QProgressBar(this);
	m_progressBar->setRange(0, 0); // indeterminate by default
	layout->addWidget(m_progressBar);

	m_logView = new QPlainTextEdit(this);
	m_logView->setReadOnly(true);
	m_logView->setMaximumBlockCount(1000);
	QString fontFamily = "monospace";
	m_logView->document()->setDefaultFont(QFont(fontFamily, 10));
	layout->addWidget(m_logView);

	m_closeButton = new QPushButton(tr("Cancel"), this);
	connect(m_closeButton, &QPushButton::clicked, this, &QDialog::reject);
	layout->addWidget(m_closeButton);
}

void UpdateProgressDialog::setStatus(const QString& status)
{
	m_statusLabel->setText(status);
	appendLog(status);
}

void UpdateProgressDialog::appendLog(const QString& line)
{
	m_logView->appendPlainText(line);
}

void UpdateProgressDialog::setProgress(int value, int maximum)
{
	m_progressBar->setRange(0, maximum);
	m_progressBar->setValue(value);
}

void UpdateProgressDialog::setFinished(bool success, const QString& message)
{
	m_statusLabel->setText(message);
	m_progressBar->setRange(0, 100);
	m_progressBar->setValue(success ? 100 : 0);
	m_closeButton->setText(tr("Close"));
	appendLog(message);
}
