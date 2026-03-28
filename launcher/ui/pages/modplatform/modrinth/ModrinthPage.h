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

#pragma once

#include <QWidget>

#include "ui/pages/BasePage.h"
#include <Application.h>
#include "tasks/Task.h"
#include <modplatform/modrinth/ModrinthPackIndex.h>

namespace Ui
{
class ModrinthPage;
}

class NewInstanceDialog;

namespace Modrinth {
    class ListModel;
}

class ModrinthPage : public QWidget, public BasePage
{
    Q_OBJECT

public:
    explicit ModrinthPage(NewInstanceDialog *dialog, QWidget *parent = 0);
    virtual ~ModrinthPage() override;
    virtual QString displayName() const override
    {
        return tr("Modrinth");
    }
    virtual QIcon icon() const override
    {
        return APPLICATION->getThemedIcon("modrinth");
    }
    virtual QString id() const override
    {
        return "modrinth";
    }
    virtual QString helpPage() const override
    {
        return "Modrinth-platform";
    }
    virtual bool shouldDisplay() const override;

    void openedImpl() override;

    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void suggestCurrent();

private slots:
    void triggerSearch();
    void onSelectionChanged(QModelIndex first, QModelIndex second);
    void onVersionSelectionChanged(QString data);

private:
    Ui::ModrinthPage *ui = nullptr;
    NewInstanceDialog *dialog = nullptr;
    Modrinth::ListModel *listModel = nullptr;
    Modrinth::IndexedPack current;

    QString selectedVersion;
};
