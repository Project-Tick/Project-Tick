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

#include "MeshMCLogsDialog.h"
#include "ui_MeshMCLogsDialog.h"

#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QShortcut>

#include "Application.h"
#include "BuildConfig.h"
#include "ui/GuiUtil.h"

MeshMCLogsDialog::MeshMCLogsDialog(QWidget* parent)
	: QDialog(parent), ui(new Ui::MeshMCLogsDialog),
	  m_liveWatcher(new QFileSystemWatcher(this))
{
	ui->setupUi(this);
	setWindowTitle(tr("MeshMC Logs"));
	resize(750, 550);

	connect(m_liveWatcher, &QFileSystemWatcher::fileChanged, this,
			&MeshMCLogsDialog::onLogFileChanged);

	auto findShortcut = new QShortcut(QKeySequence(QKeySequence::Find), this);
	connect(findShortcut, &QShortcut::activated, this, [this]() {
		ui->searchBar->setFocus();
	});

	connect(ui->searchBar, &QLineEdit::returnPressed, this,
			&MeshMCLogsDialog::on_findButton_clicked);

	populateLogList();
}

MeshMCLogsDialog::~MeshMCLogsDialog()
{
	delete ui;
}

QString MeshMCLogsDialog::logDirectory() const
{
	return QDir::currentPath();
}

QString MeshMCLogsDialog::logFilePath(const QString& name) const
{
	return QDir(logDirectory()).absoluteFilePath(name);
}

void MeshMCLogsDialog::populateLogList()
{
	ui->selectLogBox->clear();

	// Stop watching old file
	if (m_watching0Log) {
		auto watched = m_liveWatcher->files();
		if (!watched.isEmpty())
			m_liveWatcher->removePaths(watched);
		m_watching0Log = false;
	}

	QString baseName = BuildConfig.MESHMC_NAME;
	QDir dir(logDirectory());
	QStringList logFiles;

	// MeshMC-0.log through MeshMC-4.log
	for (int i = 0; i <= 4; i++) {
		QString fileName = QString("%1-%2.log").arg(baseName).arg(i);
		if (dir.exists(fileName)) {
			logFiles.append(fileName);
		}
	}

	if (logFiles.isEmpty()) {
		setControlsEnabled(false);
		return;
	}

	ui->selectLogBox->addItems(logFiles);

	if (!m_currentFile.isEmpty()) {
		int idx = ui->selectLogBox->findText(m_currentFile);
		if (idx != -1)
			ui->selectLogBox->setCurrentIndex(idx);
		else
			ui->selectLogBox->setCurrentIndex(0);
	} else {
		ui->selectLogBox->setCurrentIndex(0);
	}
}

void MeshMCLogsDialog::on_selectLogBox_currentIndexChanged(int index)
{
	// Stop watching previous file
	if (m_watching0Log) {
		auto watched = m_liveWatcher->files();
		if (!watched.isEmpty())
			m_liveWatcher->removePaths(watched);
		m_watching0Log = false;
	}

	if (index < 0) {
		m_currentFile.clear();
		ui->textBrowser->clear();
		setControlsEnabled(false);
		return;
	}

	m_currentFile = ui->selectLogBox->itemText(index);
	setControlsEnabled(true);
	loadSelectedLog();

	// Watch MeshMC-0.log for live updates
	QString baseName = BuildConfig.MESHMC_NAME;
	if (m_currentFile == QString("%1-0.log").arg(baseName)) {
		QString fullPath = logFilePath(m_currentFile);
		m_liveWatcher->addPath(fullPath);
		m_watching0Log = true;
	}
}

void MeshMCLogsDialog::loadSelectedLog()
{
	if (m_currentFile.isEmpty()) {
		setControlsEnabled(false);
		return;
	}

	QString fullPath = logFilePath(m_currentFile);
	QFile file(fullPath);
	if (!file.open(QFile::ReadOnly | QFile::Text)) {
		QMessageBox::critical(
			this, tr("Error"),
			tr("Unable to open %1 for reading: %2")
				.arg(m_currentFile, file.errorString()));
		setControlsEnabled(false);
		return;
	}

	if (file.size() > (1024ll * 1024ll * 12ll)) {
		ui->textBrowser->setPlainText(
			tr("The file (%1) is too big. You may want to open it in a viewer "
			   "optimized for large files.")
				.arg(m_currentFile));
		return;
	}

	QString content = QString::fromUtf8(file.readAll());

	QString fontFamily =
		APPLICATION->settings()->get("ConsoleFont").toString();
	bool conversionOk = false;
	int fontSize =
		APPLICATION->settings()->get("ConsoleFontSize").toInt(&conversionOk);
	if (!conversionOk) {
		fontSize = 11;
	}
	QTextDocument* doc = ui->textBrowser->document();
	doc->setDefaultFont(QFont(fontFamily, fontSize));
	ui->textBrowser->setPlainText(content);

	// Scroll to bottom for 0.log (live log)
	QString baseName = BuildConfig.MESHMC_NAME;
	if (m_currentFile == QString("%1-0.log").arg(baseName)) {
		ui->textBrowser->moveCursor(QTextCursor::End);
		ui->textBrowser->ensureCursorVisible();
	}
}

void MeshMCLogsDialog::on_btnReload_clicked()
{
	populateLogList();
	loadSelectedLog();
}

void MeshMCLogsDialog::on_btnCopy_clicked()
{
	GuiUtil::setClipboardText(ui->textBrowser->toPlainText());
}

void MeshMCLogsDialog::on_btnUpload_clicked()
{
	GuiUtil::uploadPaste(ui->textBrowser->toPlainText(), this);
}

void MeshMCLogsDialog::on_btnDelete_clicked()
{
	if (m_currentFile.isEmpty()) {
		return;
	}
	if (QMessageBox::question(
			this, tr("Delete"),
			tr("Do you really want to delete %1?").arg(m_currentFile),
			QMessageBox::Yes, QMessageBox::No) == QMessageBox::No) {
		return;
	}
	QFile file(logFilePath(m_currentFile));
	if (!file.remove()) {
		QMessageBox::critical(
			this, tr("Error"),
			tr("Unable to delete %1: %2")
				.arg(m_currentFile, file.errorString()));
	}
	m_currentFile.clear();
	populateLogList();
}

void MeshMCLogsDialog::on_btnClean_clicked()
{
	QString baseName = BuildConfig.MESHMC_NAME;
	QStringList toDelete;
	QDir dir(logDirectory());

	for (int i = 0; i <= 4; i++) {
		QString fileName = QString("%1-%2.log").arg(baseName).arg(i);
		if (dir.exists(fileName)) {
			toDelete.append(fileName);
		}
	}

	if (toDelete.isEmpty())
		return;

	QMessageBox* messageBox = new QMessageBox(this);
	messageBox->setWindowTitle(tr("Clean up"));
	messageBox->setText(
		tr("Do you really want to delete all log files?\n%1")
			.arg(toDelete.join('\n')));
	messageBox->setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
	messageBox->setDefaultButton(QMessageBox::Ok);
	messageBox->setIcon(QMessageBox::Question);

	if (messageBox->exec() != QMessageBox::Ok) {
		return;
	}

	QStringList failed;
	for (const auto& item : toDelete) {
		QFile file(logFilePath(item));
		if (!file.remove()) {
			failed.push_back(item);
		}
	}

	if (!failed.empty()) {
		QMessageBox::critical(
			this, tr("Error"),
			tr("Couldn't delete some files:\n%1").arg(failed.join('\n')));
	}

	m_currentFile.clear();
	populateLogList();
}

void MeshMCLogsDialog::on_findButton_clicked()
{
	auto modifiers = QApplication::keyboardModifiers();
	bool reverse = modifiers & Qt::ShiftModifier;
	ui->textBrowser->find(ui->searchBar->text(),
						  reverse ? QTextDocument::FindFlag::FindBackward
								  : QTextDocument::FindFlag(0));
}

void MeshMCLogsDialog::onLogFileChanged(const QString& path)
{
	Q_UNUSED(path)
	// Reload current log when 0.log changes
	if (m_watching0Log) {
		loadSelectedLog();
		// QFileSystemWatcher may stop watching after a file is modified
		// (rewritten), re-add it
		QString baseName = BuildConfig.MESHMC_NAME;
		QString fullPath = logFilePath(QString("%1-0.log").arg(baseName));
		if (QFile::exists(fullPath)) {
			auto watched = m_liveWatcher->files();
			if (!watched.contains(fullPath)) {
				m_liveWatcher->addPath(fullPath);
			}
		}
	}
}

void MeshMCLogsDialog::setControlsEnabled(bool enabled)
{
	ui->btnReload->setEnabled(true); // always allow reload
	ui->btnCopy->setEnabled(enabled);
	ui->btnUpload->setEnabled(enabled);
	ui->btnDelete->setEnabled(enabled);
	ui->btnClean->setEnabled(enabled);
	ui->textBrowser->setEnabled(enabled);
}
