/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "FilelinkPage.h"

FilelinkPage::FilelinkPage(InstancePtr inst, FilelinkManager* mgr)
	: m_instance(inst), m_manager(mgr)
{
	ui.setupUi(this);
	refreshTable();
}

QIcon FilelinkPage::icon() const
{
	return QIcon::fromTheme("emblem-symbolic-link",
							QIcon(":/icons/toolbar/link"));
}

void FilelinkPage::refreshTable()
{
	ui.linkTreeWidget->clear();

	auto links = m_manager->linksForInstance(m_instance->id());
	for (const auto& e : links) {
		auto* item = new QTreeWidgetItem(ui.linkTreeWidget);
		item->setText(0, e.fileName);
		item->setText(1, e.subDir);

		// Show direction relative to this instance
		if (e.targetInstance == m_instance->id()) {
			QString srcName = e.sourceInstance;
			auto srcInst =
				APPLICATION->instances()->getInstanceById(e.sourceInstance);
			if (srcInst)
				srcName = srcInst->name();
			item->setText(2, tr("From: %1").arg(srcName));
		} else {
			QString tgtName = e.targetInstance;
			auto tgtInst =
				APPLICATION->instances()->getInstanceById(e.targetInstance);
			if (tgtInst)
				tgtName = tgtInst->name();
			item->setText(2, tr("To: %1").arg(tgtName));
		}

		item->setText(3, QLocale().toString(e.linkedAt, QLocale::ShortFormat));
		item->setData(0, Qt::UserRole, e.targetPath);
	}

	ui.statusLabel->setText(tr("%n linked file(s)", "", links.size()));
}

void FilelinkPage::on_linkButton_clicked()
{
	// Build a list of other instances to choose from
	auto instList = APPLICATION->instances();
	QStringList names;
	QStringList ids;
	for (int i = 0; i < instList->count(); i++) {
		auto inst = instList->at(i);
		if (inst && inst->id() != m_instance->id()) {
			names << inst->name();
			ids << inst->id();
		}
	}

	if (names.isEmpty()) {
		QMessageBox::information(
			this, tr("Filelink"),
			tr("No other instances available to link from."));
		return;
	}

	// Simple selection: pick instance via input dialog
	bool ok = false;
	QString chosen = QInputDialog::getItem(this, tr("Link from instance"),
										   tr("Select the source instance:"),
										   names, 0, false, &ok);
	if (!ok || chosen.isEmpty())
		return;

	int idx = names.indexOf(chosen);
	if (idx < 0)
		return;

	QString sourceId = ids[idx];
	auto sourceInst = instList->getInstanceById(sourceId);
	if (!sourceInst)
		return;

	// Pick which sub-directory to link
	QStringList subDirs = {"mods", "resourcepacks", "shaderpacks"};
	QString subDir = QInputDialog::getItem(this, tr("Select folder to link"),
										   tr("Which folder should be linked?"),
										   subDirs, 0, false, &ok);
	if (!ok || subDir.isEmpty())
		return;

	QString srcDir =
		QDir(sourceInst->instanceRoot()).filePath(".minecraft/" + subDir);
	QString dstDir =
		QDir(m_instance->instanceRoot()).filePath(".minecraft/" + subDir);

	int count = m_manager->linkDirectory(sourceId, srcDir, m_instance->id(),
										 dstDir, subDir);

	if (count < 0) {
		QMessageBox::warning(this, tr("Filelink"),
							 tr("Failed to link files. The source folder may "
								"not exist."));
	} else {
		QMessageBox::information(
			this, tr("Filelink"),
			tr("Successfully linked %n file(s).", "", count));
	}

	refreshTable();
}

void FilelinkPage::on_unlinkButton_clicked()
{
	auto* item = ui.linkTreeWidget->currentItem();
	if (!item)
		return;

	QString targetPath = item->data(0, Qt::UserRole).toString();
	if (targetPath.isEmpty())
		return;

	auto answer = QMessageBox::question(
		this, tr("Unlink file"),
		tr("Remove the link for '%1'?\n\nThe linked file will be deleted.")
			.arg(item->text(0)));

	if (answer != QMessageBox::Yes)
		return;

	m_manager->unlinkFile(m_instance->id(), targetPath);
	refreshTable();
}

void FilelinkPage::on_verifyButton_clicked()
{
	auto broken = m_manager->verifyLinks(m_instance->id());
	if (broken.isEmpty()) {
		QMessageBox::information(this, tr("Filelink"),
								 tr("All links are intact."));
	} else {
		QMessageBox::warning(
			this, tr("Filelink"),
			tr("%n broken link(s) found:\n\n%1", "", broken.size())
				.arg(broken.join("\n")));
	}
}
