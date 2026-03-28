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

#include "ModrinthPage.h"
#include "ui_ModrinthPage.h"

#include <QKeyEvent>

#include "Application.h"
#include "Json.h"
#include "ui/dialogs/NewInstanceDialog.h"
#include "InstanceImportTask.h"
#include "ModrinthModel.h"

ModrinthPage::ModrinthPage(NewInstanceDialog *dialog, QWidget *parent)
    : QWidget(parent), ui(new Ui::ModrinthPage), dialog(dialog)
{
    ui->setupUi(this);
    connect(ui->searchButton, &QPushButton::clicked, this, &ModrinthPage::triggerSearch);
    ui->searchEdit->installEventFilter(this);
    listModel = new Modrinth::ListModel(this);
    ui->packView->setModel(listModel);

    ui->versionSelectionBox->view()->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ui->versionSelectionBox->view()->parentWidget()->setMaximumHeight(300);

    ui->sortByBox->addItem(tr("Sort by relevance"));
    ui->sortByBox->addItem(tr("Sort by downloads"));
    ui->sortByBox->addItem(tr("Sort by last updated"));
    ui->sortByBox->addItem(tr("Sort by newest"));
    ui->sortByBox->addItem(tr("Sort by follows"));

    connect(ui->sortByBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ModrinthPage::triggerSearch);
    connect(ui->packView->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &ModrinthPage::onSelectionChanged);
    connect(ui->versionSelectionBox, &QComboBox::currentTextChanged,
            this, &ModrinthPage::onVersionSelectionChanged);
}

ModrinthPage::~ModrinthPage()
{
    delete ui;
}

bool ModrinthPage::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->searchEdit && event->type() == QEvent::KeyPress)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Return)
        {
            triggerSearch();
            keyEvent->accept();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

bool ModrinthPage::shouldDisplay() const
{
    return true;
}

void ModrinthPage::openedImpl()
{
    suggestCurrent();
    triggerSearch();
}

void ModrinthPage::triggerSearch()
{
    listModel->searchWithTerm(ui->searchEdit->text(), ui->sortByBox->currentIndex());
}

void ModrinthPage::onSelectionChanged(QModelIndex first, QModelIndex second)
{
    ui->versionSelectionBox->clear();

    if (!first.isValid())
    {
        if (isOpened)
        {
            dialog->setSuggestedPack();
        }
        return;
    }

    current = listModel->data(first, Qt::UserRole).value<Modrinth::IndexedPack>();
    QString text = "";
    QString name = current.name;

    if (current.slug.isEmpty())
    {
        text = name;
    }
    else
    {
        text = "<a href=\"https://modrinth.com/modpack/" + current.slug + "\">" + name + "</a>";
    }

    if (!current.author.isEmpty())
    {
        text += "<br>" + tr(" by ") + current.author;
    }
    text += "<br><br>";

    ui->packDescription->setHtml(text + current.description);

    if (isOpened)
    {
        dialog->setSuggestedPack(current.name);
    }

    if (!current.versionsLoaded)
    {
        qDebug() << "Loading Modrinth modpack versions";
        NetJob *netJob = new NetJob(
            QString("Modrinth::PackVersions(%1)").arg(current.name),
            APPLICATION->network());
        std::shared_ptr<QByteArray> versionResponse = std::make_shared<QByteArray>();
        QString projectId = current.projectId;
        netJob->addNetAction(Net::Download::makeByteArray(
            QString("https://api.modrinth.com/v2/project/%1/version?"
                    "loaders=[\"forge\",\"fabric\",\"quilt\",\"neoforge\"]")
                .arg(projectId),
            versionResponse.get()));

        QObject::connect(netJob, &NetJob::succeeded, this, [this, netJob, versionResponse]
        {
            netJob->deleteLater();
            QJsonParseError parse_error;
            QJsonDocument doc = QJsonDocument::fromJson(*versionResponse, &parse_error);
            if (parse_error.error != QJsonParseError::NoError)
            {
                qWarning() << "Error while parsing JSON response from Modrinth at "
                           << parse_error.offset << " reason: " << parse_error.errorString();
                qWarning() << *versionResponse;
                return;
            }
            QJsonArray arr = doc.array();
            try
            {
                Modrinth::loadIndexedPackVersions(current, arr);
            }
            catch (const JSONValidationError &e)
            {
                qDebug() << *versionResponse;
                qWarning() << "Error while reading Modrinth modpack version: " << e.cause();
            }

            for (auto version : current.versions)
            {
                QString label = version.versionNumber;
                if (!version.mcVersion.isEmpty())
                {
                    label += " [" + version.mcVersion + "]";
                }
                if (!version.loaders.isEmpty())
                {
                    label += " (" + version.loaders + ")";
                }
                ui->versionSelectionBox->addItem(label, QVariant(version.downloadUrl));
            }

            suggestCurrent();
        });
        QObject::connect(netJob, &NetJob::failed, this, [netJob]
        {
            netJob->deleteLater();
        });
        netJob->start();
    }
    else
    {
        for (auto version : current.versions)
        {
            QString label = version.versionNumber;
            if (!version.mcVersion.isEmpty())
            {
                label += " [" + version.mcVersion + "]";
            }
            if (!version.loaders.isEmpty())
            {
                label += " (" + version.loaders + ")";
            }
            ui->versionSelectionBox->addItem(label, QVariant(version.downloadUrl));
        }

        suggestCurrent();
    }
}

void ModrinthPage::suggestCurrent()
{
    if (!isOpened)
    {
        return;
    }

    if (selectedVersion.isEmpty())
    {
        dialog->setSuggestedPack();
        return;
    }

    dialog->setSuggestedPack(current.name, new InstanceImportTask(selectedVersion));
    QString editedLogoName;
    editedLogoName = "modrinth_" + current.slug;
    listModel->getLogo(current.slug, current.iconUrl, [this, editedLogoName](QString logo)
    {
        dialog->setSuggestedIconFromFile(logo, editedLogoName);
    });
}

void ModrinthPage::onVersionSelectionChanged(QString data)
{
    if (data.isNull() || data.isEmpty())
    {
        selectedVersion = "";
        return;
    }
    selectedVersion = ui->versionSelectionBox->currentData().toString();
    suggestCurrent();
}
