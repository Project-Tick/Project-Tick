/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "BackupPage.h"
#include "ui_BackupPage.h"

BackupPage::BackupPage(InstancePtr instance, QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::BackupPage)
    , m_instance(instance)
{
    ui->setupUi(this);

    m_manager = std::make_unique<BackupManager>(
        m_instance->id(), m_instance->instanceRoot());

    ui->backupList->header()->setStretchLastSection(false);
    ui->backupList->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->backupList->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->backupList->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    connect(ui->backupList, &QTreeWidget::itemSelectionChanged,
            this, &BackupPage::onSelectionChanged);

    refreshList();
}

BackupPage::~BackupPage()
{
    delete ui;
}

QIcon BackupPage::icon() const
{
    return APPLICATION->getThemedIcon("backup");
}

void BackupPage::openedImpl()
{
    refreshList();
}

void BackupPage::refreshList()
{
    ui->backupList->clear();
    m_entries = m_manager->listBackups();

    for (const auto& entry : m_entries) {
        auto* item = new QTreeWidgetItem(ui->backupList);
        item->setText(0, entry.name.isEmpty() ? entry.fileName : entry.name);
        item->setText(1, entry.timestamp.toString("yyyy-MM-dd HH:mm:ss"));
        item->setText(2, humanFileSize(entry.sizeBytes));
        item->setData(0, Qt::UserRole, entry.fullPath);
    }

    int count = m_entries.size();
    ui->statusLabel->setText(tr("%n backup(s)", "", count));
    updateButtons();
}

void BackupPage::updateButtons()
{
    bool hasSelection = !ui->backupList->selectedItems().isEmpty();
    ui->btnRestore->setEnabled(hasSelection);
    ui->btnExport->setEnabled(hasSelection);
    ui->btnDelete->setEnabled(hasSelection);
}

void BackupPage::onSelectionChanged()
{
    updateButtons();
}

BackupEntry BackupPage::selectedEntry() const
{
    auto items = ui->backupList->selectedItems();
    if (items.isEmpty())
        return {};

    QString path = items.first()->data(0, Qt::UserRole).toString();
    for (const auto& e : m_entries) {
        if (e.fullPath == path)
            return e;
    }
    return {};
}

void BackupPage::on_btnCreate_clicked()
{
    bool ok = false;
    QString label = QInputDialog::getText(this,
        tr("Create Backup"),
        tr("Backup label (optional):"),
        QLineEdit::Normal, {}, &ok);

    if (!ok)
        return;

    ui->statusLabel->setText(tr("Creating backup..."));
    QApplication::processEvents();

    auto entry = m_manager->createBackup(label);
    if (entry.fullPath.isEmpty()) {
        QMessageBox::warning(this, tr("Backup Failed"),
            tr("Failed to create backup. Check the logs for details."));
    } else {
        QMessageBox::information(this, tr("Backup Created"),
            tr("Backup created successfully:\n%1").arg(entry.fileName));
    }

    refreshList();
}

void BackupPage::on_btnRestore_clicked()
{
    auto entry = selectedEntry();
    if (entry.fullPath.isEmpty())
        return;

    auto ret = QMessageBox::warning(this,
        tr("Restore Backup"),
        tr("This will replace the current instance contents with the backup:\n\n"
           "%1\n\n"
           "This action cannot be undone. Continue?").arg(entry.fileName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (ret != QMessageBox::Yes)
        return;

    ui->statusLabel->setText(tr("Restoring backup..."));
    QApplication::processEvents();

    if (m_manager->restoreBackup(entry)) {
        QMessageBox::information(this, tr("Restore Complete"),
            tr("Backup restored successfully."));
    } else {
        QMessageBox::warning(this, tr("Restore Failed"),
            tr("Failed to restore backup. Check the logs for details."));
    }

    refreshList();
}

void BackupPage::on_btnExport_clicked()
{
    auto entry = selectedEntry();
    if (entry.fullPath.isEmpty())
        return;

    QString dest = QFileDialog::getSaveFileName(this,
        tr("Export Backup"), entry.fileName, tr("Zip Files (*.zip)"));

    if (dest.isEmpty())
        return;

    if (m_manager->exportBackup(entry, dest)) {
        QMessageBox::information(this, tr("Export Complete"),
            tr("Backup exported to:\n%1").arg(dest));
    } else {
        QMessageBox::warning(this, tr("Export Failed"),
            tr("Failed to export backup."));
    }
}

void BackupPage::on_btnImport_clicked()
{
    QString src = QFileDialog::getOpenFileName(this,
        tr("Import Backup"), {}, tr("Zip Files (*.zip)"));

    if (src.isEmpty())
        return;

    bool ok = false;
    QString label = QInputDialog::getText(this,
        tr("Import Backup"),
        tr("Label for imported backup (optional):"),
        QLineEdit::Normal, {}, &ok);

    if (!ok)
        return;

    auto entry = m_manager->importBackup(src, label);
    if (entry.fullPath.isEmpty()) {
        QMessageBox::warning(this, tr("Import Failed"),
            tr("Failed to import backup."));
    } else {
        QMessageBox::information(this, tr("Import Complete"),
            tr("Backup imported successfully."));
    }

    refreshList();
}

void BackupPage::on_btnDelete_clicked()
{
    auto entry = selectedEntry();
    if (entry.fullPath.isEmpty())
        return;

    auto ret = QMessageBox::question(this,
        tr("Delete Backup"),
        tr("Delete the selected backup?\n\n%1\n\nThis cannot be undone.")
            .arg(entry.fileName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (ret != QMessageBox::Yes)
        return;

    m_manager->deleteBackup(entry);
    refreshList();
}

QString BackupPage::humanFileSize(qint64 bytes) const
{
    if (bytes < 1024)
        return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QString("%1 KiB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024LL * 1024 * 1024)
        return QString("%1 MiB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    return QString("%1 GiB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}
