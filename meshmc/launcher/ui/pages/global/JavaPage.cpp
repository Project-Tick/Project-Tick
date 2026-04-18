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

#include "JavaPage.h"
#include "JavaCommon.h"
#include "ui_JavaPage.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QTabBar>
#include <QTreeWidgetItem>
#include <QDirIterator>

#include "ui/dialogs/VersionSelectDialog.h"
#ifndef MeshMC_DISABLE_JAVA_DOWNLOADER
#include "ui/dialogs/JavaDownloadDialog.h"
#endif

#include "java/JavaUtils.h"
#include "java/JavaInstallList.h"
#include "java/download/JavaRuntime.h"

#include "settings/SettingsObject.h"
#include <FileSystem.h>
#include "Application.h"
#include <sys.h>

JavaPage::JavaPage(QWidget* parent) : QWidget(parent), ui(new Ui::JavaPage)
{
	ui->setupUi(this);

	auto sysMiB = Sys::getSystemRam() / Sys::mebibyte;
	ui->maxMemSpinBox->setMaximum(sysMiB);

	// Populate vendor combobox
	auto providers = JavaDownload::JavaProviderInfo::availableProviders();
	for (const auto& provider : providers) {
		ui->javaAutoDownloadVendorBox->addItem(provider.name, provider.uid);
	}

	loadSettings();
#ifdef MeshMC_DISABLE_JAVA_DOWNLOADER
	// Hide the entire Installations tab when Java downloader is disabled
	int idx = ui->tabWidget->indexOf(ui->tabInstallations);
	if (idx != -1)
		ui->tabWidget->removeTab(idx);
	// Hide vendor selector and auto-download toggle when downloader is disabled
	ui->javaAutoDownloadCheck->setVisible(false);
	ui->labelAutoDownloadVendor->setVisible(false);
	ui->javaAutoDownloadVendorBox->setVisible(false);
#else
	refreshInstalledJavas();
#endif
}

JavaPage::~JavaPage()
{
	delete ui;
}

bool JavaPage::apply()
{
	applySettings();
	return true;
}

void JavaPage::applySettings()
{
	auto s = APPLICATION->settings();

	// Memory
	int min = ui->minMemSpinBox->value();
	int max = ui->maxMemSpinBox->value();
	if (min < max) {
		s->set("MinMemAlloc", min);
		s->set("MaxMemAlloc", max);
	} else {
		s->set("MinMemAlloc", max);
		s->set("MaxMemAlloc", min);
	}
	s->set("PermGen", ui->permGenSpinBox->value());

	// Java Settings
	s->set("JavaPath", ui->javaPathTextBox->text());
	s->set("JvmArgs", ui->jvmArgsTextBox->text());
	s->set("JavaAutoDownload", ui->javaAutoDownloadCheck->isChecked());
	s->set("JavaAutoDownloadVendor",
		   ui->javaAutoDownloadVendorBox->currentData().toString());
	JavaCommon::checkJVMArgs(s->get("JvmArgs").toString(),
							 this->parentWidget());
}
void JavaPage::loadSettings()
{
	auto s = APPLICATION->settings();
	// Memory
	int min = s->get("MinMemAlloc").toInt();
	int max = s->get("MaxMemAlloc").toInt();
	if (min < max) {
		ui->minMemSpinBox->setValue(min);
		ui->maxMemSpinBox->setValue(max);
	} else {
		ui->minMemSpinBox->setValue(max);
		ui->maxMemSpinBox->setValue(min);
	}
	ui->permGenSpinBox->setValue(s->get("PermGen").toInt());

	// Java Settings
	ui->javaPathTextBox->setText(s->get("JavaPath").toString());
	ui->jvmArgsTextBox->setText(s->get("JvmArgs").toString());

	// Auto-download
	ui->javaAutoDownloadCheck->setChecked(s->get("JavaAutoDownload").toBool());

	// Auto-download vendor
	auto currentVendor = s->get("JavaAutoDownloadVendor").toString();
	int idx = ui->javaAutoDownloadVendorBox->findData(currentVendor);
	if (idx >= 0) {
		ui->javaAutoDownloadVendorBox->setCurrentIndex(idx);
	}
}

void JavaPage::on_javaDetectBtn_clicked()
{
	JavaInstallPtr java;

	VersionSelectDialog vselect(APPLICATION->javalist().get(),
								tr("Select a Java version"), this, true);
	vselect.setResizeOn(2);
	vselect.exec();

	if (vselect.result() == QDialog::Accepted && vselect.selectedVersion()) {
		java =
			std::dynamic_pointer_cast<JavaInstall>(vselect.selectedVersion());
		ui->javaPathTextBox->setText(java->path);
	}
}

void JavaPage::on_javaBrowseBtn_clicked()
{
	QString raw_path =
		QFileDialog::getOpenFileName(this, tr("Find Java executable"));

	// do not allow current dir - it's dirty. Do not allow dirs that don't exist
	if (raw_path.isEmpty()) {
		return;
	}

	QString cooked_path = FS::NormalizePath(raw_path);
	QFileInfo javaInfo(cooked_path);
	;
	if (!javaInfo.exists() || !javaInfo.isExecutable()) {
		return;
	}
	ui->javaPathTextBox->setText(cooked_path);
}

void JavaPage::on_javaTestBtn_clicked()
{
	if (checker) {
		return;
	}
	checker.reset(new JavaCommon::TestCheck(
		this, ui->javaPathTextBox->text(), ui->jvmArgsTextBox->text(),
		ui->minMemSpinBox->value(), ui->maxMemSpinBox->value(),
		ui->permGenSpinBox->value()));
	connect(checker.get(), &JavaCommon::TestCheck::finished, this,
			&JavaPage::checkerFinished);
	checker->run();
}

void JavaPage::checkerFinished()
{
	checker.reset();
}

void JavaPage::on_javaDownloadBtn_clicked()
{
#ifndef MeshMC_DISABLE_JAVA_DOWNLOADER
	JavaDownloadDialog dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
		refreshInstalledJavas();
	}
#endif
}

void JavaPage::on_javaRefreshBtn_clicked()
{
	refreshInstalledJavas();
}

void JavaPage::on_javaRemoveBtn_clicked()
{
	auto* item = ui->installedJavaTree->currentItem();
	if (!item)
		return;

	QString path = item->text(2);
	if (path.isEmpty())
		return;

	// Find the java installation root directory (parent of bin/)
	QFileInfo fi(path);
	QDir javaDir = fi.dir(); // bin/
	javaDir.cdUp();			 // java root

	auto result = QMessageBox::question(
		this, tr("Remove Java Installation"),
		tr("Are you sure you want to remove this Java installation?\n\n%1")
			.arg(javaDir.absolutePath()),
		QMessageBox::Yes | QMessageBox::No);

	if (result != QMessageBox::Yes)
		return;

	javaDir.removeRecursively();
	refreshInstalledJavas();
}

void JavaPage::on_javaUseBtn_clicked()
{
	auto* item = ui->installedJavaTree->currentItem();
	if (!item)
		return;

	QString path = item->text(2);
	if (!path.isEmpty()) {
		ui->javaPathTextBox->setText(path);
		ui->tabWidget->setCurrentIndex(0); // Switch to Settings tab
	}
}

void JavaPage::refreshInstalledJavas()
{
	ui->installedJavaTree->clear();

	QString javaBaseDir = JavaUtils::managedJavaRoot();
	QDir baseDir(javaBaseDir);
	if (!baseDir.exists())
		return;

	// Scan for java binaries under java/{vendor}/{version}/
	QDirIterator vendorIt(javaBaseDir, QDir::Dirs | QDir::NoDotAndDotDot);
	while (vendorIt.hasNext()) {
		vendorIt.next();
		QString vendorName = vendorIt.fileName();
		QString vendorPath = vendorIt.filePath();

		QDirIterator versionIt(vendorPath, QDir::Dirs | QDir::NoDotAndDotDot);
		while (versionIt.hasNext()) {
			versionIt.next();
			QString versionPath = versionIt.filePath();

			// Look for java binary
#if defined(Q_OS_WIN)
			QString binaryName = "javaw.exe";
#else
			QString binaryName = "java";
#endif
			QDirIterator binIt(versionPath, QStringList() << binaryName,
							   QDir::Files, QDirIterator::Subdirectories);
			while (binIt.hasNext()) {
				binIt.next();
				QString javaPath = binIt.filePath();
				if (javaPath.contains("/bin/")) {
					auto* item = new QTreeWidgetItem(ui->installedJavaTree);
					item->setText(0, versionIt.fileName());
					item->setText(1, vendorName);
					item->setText(2, javaPath);
					break; // Only first binary per version dir
				}
			}
		}
	}

	ui->installedJavaTree->resizeColumnToContents(0);
	ui->installedJavaTree->resizeColumnToContents(1);
}
