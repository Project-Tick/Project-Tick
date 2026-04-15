/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * BackupPage — UI page for the BackupSystem plugin.
 * Displayed as a tab in the instance settings dialog.
 */

#pragma once

#include "plugin/sdk/mmco_sdk.h"
#include "BackupManager.h"

namespace Ui {
class BackupPage;
}

class BackupPage : public QWidget, public BasePage {
    Q_OBJECT

public:
    explicit BackupPage(InstancePtr instance, QWidget* parent = nullptr);
    ~BackupPage() override;

    QString id() const override { return "backup-system"; }
    QString displayName() const override { return tr("Backups"); }
    QIcon icon() const override;
    QString helpPage() const override { return "Instance-Backups"; }
    bool shouldDisplay() const override { return true; }

    void openedImpl() override;

private slots:
    void on_btnCreate_clicked();
    void on_btnRestore_clicked();
    void on_btnExport_clicked();
    void on_btnImport_clicked();
    void on_btnDelete_clicked();
    void onSelectionChanged();

private:
    void refreshList();
    void updateButtons();
    BackupEntry selectedEntry() const;
    QString humanFileSize(qint64 bytes) const;

    Ui::BackupPage* ui;
    InstancePtr m_instance;
    std::unique_ptr<BackupManager> m_manager;
    QList<BackupEntry> m_entries;
};
