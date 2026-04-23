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

#include "BlockedModsDialog.h"

#include <DesktopServices.h>
#include <QDir>
#include <QFont>
#include <QGridLayout>
#include <QScrollArea>
#include <QStandardPaths>
#include <QUrl>

BlockedModsDialog::BlockedModsDialog(QWidget* parent, const QString& title,
									 const QString& text,
									 QList<BlockedMod>& mods)
	: QDialog(parent), m_mods(mods)
{
	setWindowTitle(title);
	setMinimumSize(550, 300);
	resize(620, 420);
	setWindowModality(Qt::WindowModal);

	auto* mainLayout = new QVBoxLayout(this);

	// Description label at top
	auto* descLabel = new QLabel(text, this);
	descLabel->setWordWrap(true);
	mainLayout->addWidget(descLabel);

	// Scrollable area for mod list
	auto* scrollArea = new QScrollArea(this);
	scrollArea->setWidgetResizable(true);

	auto* scrollWidget = new QWidget();
	auto* grid = new QGridLayout(scrollWidget);
	grid->setColumnStretch(0, 3); // mod name
	grid->setColumnStretch(1, 1); // status
	grid->setColumnStretch(2, 0); // button

	// Header row
	auto* headerName = new QLabel(tr("<b>Mod</b>"), scrollWidget);
	auto* headerStatus = new QLabel(tr("<b>Status</b>"), scrollWidget);
	grid->addWidget(headerName, 0, 0);
	grid->addWidget(headerStatus, 0, 1);

	for (int i = 0; i < m_mods.size(); i++) {
		int row = i + 1;

		auto* nameLabel = new QLabel(m_mods[i].fileName, scrollWidget);
		nameLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

		auto* statusLabel = new QLabel(tr("Missing"), scrollWidget);
		statusLabel->setStyleSheet("color: #cc3333; font-weight: bold;");

		auto* downloadBtn = new QPushButton(tr("Download"), scrollWidget);
		connect(downloadBtn, &QPushButton::clicked, this,
				[this, i]() { openModDownload(i); });

		grid->addWidget(nameLabel, row, 0);
		grid->addWidget(statusLabel, row, 1);
		grid->addWidget(downloadBtn, row, 2);

		m_rows.append({nameLabel, statusLabel, downloadBtn});
	}

	// Add stretch at bottom of grid
	grid->setRowStretch(m_mods.size() + 1, 1);

	scrollWidget->setLayout(grid);
	scrollArea->setWidget(scrollWidget);
	mainLayout->addWidget(scrollArea, 1);

	// Button box at bottom
	m_buttons = new QDialogButtonBox(
		QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	m_buttons->button(QDialogButtonBox::Ok)->setText(tr("Continue"));
	m_buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
	connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	mainLayout->addWidget(m_buttons);

	setLayout(mainLayout);

	// Set up Downloads folder watching
	setupWatch();

	// Initial scan
	scanDownloadsFolder();
}

void BlockedModsDialog::setupWatch()
{
	m_downloadDir =
		QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
	if (!m_downloadDir.isEmpty() && QDir(m_downloadDir).exists()) {
		m_watcher.addPath(m_downloadDir);
		connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this,
				&BlockedModsDialog::onDownloadDirChanged);
	}
}

void BlockedModsDialog::onDownloadDirChanged(const QString& path)
{
	Q_UNUSED(path);
	scanDownloadsFolder();
}

void BlockedModsDialog::scanDownloadsFolder()
{
	if (m_downloadDir.isEmpty())
		return;

	QDir dir(m_downloadDir);
	QStringList files = dir.entryList(QDir::Files);

	for (int i = 0; i < m_mods.size(); i++) {
		if (!m_mods[i].found && files.contains(m_mods[i].fileName)) {
			m_mods[i].found = true;
		}
	}

	updateModStatus();
}

void BlockedModsDialog::updateModStatus()
{
	bool allFound = true;

	for (int i = 0; i < m_mods.size(); i++) {
		if (m_mods[i].found) {
			m_rows[i].statusLabel->setText(QString::fromUtf8("\u2714 ") +
										   tr("Found"));
			m_rows[i].statusLabel->setStyleSheet(
				"color: #33aa33; font-weight: bold;");
			m_rows[i].downloadButton->setEnabled(false);
		} else {
			m_rows[i].statusLabel->setText(tr("Missing"));
			m_rows[i].statusLabel->setStyleSheet(
				"color: #cc3333; font-weight: bold;");
			m_rows[i].downloadButton->setEnabled(true);
			allFound = false;
		}
	}

	m_buttons->button(QDialogButtonBox::Ok)->setEnabled(allFound);
}

void BlockedModsDialog::openModDownload(int index)
{
	if (index < 0 || index >= m_mods.size())
		return;

	const auto& mod = m_mods[index];
	QString url =
		QString("https://www.curseforge.com/api/v1/mods/%1/files/%2/download")
			.arg(mod.projectId)
			.arg(mod.fileId);
	DesktopServices::openUrl(QUrl(url));
}
