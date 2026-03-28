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

#include "ModrinthModel.h"

#include "Application.h"
#include "Json.h"

#include <QtMath>

namespace Modrinth {

ListModel::ListModel(QObject *parent) : QAbstractListModel(parent)
{
}

ListModel::~ListModel()
{
}

int ListModel::rowCount(const QModelIndex &parent) const
{
    return modpacks.size();
}

int ListModel::columnCount(const QModelIndex &parent) const
{
    return 1;
}

QVariant ListModel::data(const QModelIndex &index, int role) const
{
    int pos = index.row();
    if (pos >= modpacks.size() || pos < 0 || !index.isValid())
    {
        return QString("INVALID INDEX %1").arg(pos);
    }

    IndexedPack pack = modpacks.at(pos);
    if (role == Qt::DisplayRole)
    {
        return pack.name;
    }
    else if (role == Qt::ToolTipRole)
    {
        if (pack.description.length() > 100)
        {
            QString edit = pack.description.left(97);
            edit = edit.left(edit.lastIndexOf(" ")).append("...");
            return edit;
        }
        return pack.description;
    }
    else if (role == Qt::DecorationRole)
    {
        if (m_logoMap.contains(pack.slug))
        {
            return (m_logoMap.value(pack.slug));
        }
        QIcon icon = APPLICATION->getThemedIcon("screenshot-placeholder");
        ((ListModel *)this)->requestLogo(pack.slug, pack.iconUrl);
        return icon;
    }
    else if (role == Qt::UserRole)
    {
        QVariant v;
        v.setValue(pack);
        return v;
    }

    return QVariant();
}

void ListModel::logoLoaded(QString logo, QIcon out)
{
    m_loadingLogos.removeAll(logo);
    m_logoMap.insert(logo, out);
    for (int i = 0; i < modpacks.size(); i++)
    {
        if (modpacks[i].slug == logo)
        {
            emit dataChanged(createIndex(i, 0), createIndex(i, 0), {Qt::DecorationRole});
        }
    }
}

void ListModel::logoFailed(QString logo)
{
    m_failedLogos.append(logo);
    m_loadingLogos.removeAll(logo);
}

void ListModel::requestLogo(QString logo, QString url)
{
    if (m_loadingLogos.contains(logo) || m_failedLogos.contains(logo))
    {
        return;
    }

    MetaEntryPtr entry = APPLICATION->metacache()->resolveEntry("ModrinthPacks", QString("logos/%1").arg(logo));
    NetJob *job = new NetJob(QString("Modrinth Icon Download %1").arg(logo), APPLICATION->network());
    job->addNetAction(Net::Download::makeCached(QUrl(url), entry));

    auto fullPath = entry->getFullPath();
    QObject::connect(job, &NetJob::succeeded, this, [this, job, logo, fullPath]
    {
        job->deleteLater();
        emit logoLoaded(logo, QIcon(fullPath));
        if (waitingCallbacks.contains(logo))
        {
            waitingCallbacks.value(logo)(fullPath);
        }
    });

    QObject::connect(job, &NetJob::failed, this, [this, job, logo]
    {
        job->deleteLater();
        emit logoFailed(logo);
    });

    job->start();

    m_loadingLogos.append(logo);
}

void ListModel::getLogo(const QString &logo, const QString &logoUrl, LogoCallback callback)
{
    if (m_logoMap.contains(logo))
    {
        callback(APPLICATION->metacache()->resolveEntry("ModrinthPacks", QString("logos/%1").arg(logo))->getFullPath());
    }
    else
    {
        requestLogo(logo, logoUrl);
    }
}

Qt::ItemFlags ListModel::flags(const QModelIndex &index) const
{
    return QAbstractListModel::flags(index);
}

bool ListModel::canFetchMore(const QModelIndex &parent) const
{
    return searchState == CanPossiblyFetchMore;
}

void ListModel::fetchMore(const QModelIndex &parent)
{
    if (parent.isValid())
    {
        return;
    }
    if (nextSearchOffset == 0)
    {
        qWarning() << "fetchMore with 0 offset is wrong...";
        return;
    }
    performPaginatedSearch();
}

void ListModel::performPaginatedSearch()
{
    static const char *sortFields[] = { "relevance", "downloads", "updated", "newest", "follows" };
    int sortIndex = (currentSort >= 0 && currentSort < 5) ? currentSort : 0;

    NetJob *netJob = new NetJob("Modrinth::Search", APPLICATION->network());
    auto searchUrl = QString(
        "https://api.modrinth.com/v2/search?"
        "query=%1&"
        "facets=[[\"project_type:modpack\"]]&"
        "index=%2&"
        "offset=%3&"
        "limit=20"
    ).arg(currentSearchTerm, sortFields[sortIndex]).arg(nextSearchOffset);
    netJob->addNetAction(Net::Download::makeByteArray(QUrl(searchUrl), &response));
    jobPtr = netJob;
    jobPtr->start();
    QObject::connect(netJob, &NetJob::succeeded, this, &ListModel::searchRequestFinished);
    QObject::connect(netJob, &NetJob::failed, this, &ListModel::searchRequestFailed);
}

void ListModel::searchWithTerm(const QString &term, int sort)
{
    if (currentSearchTerm == term && currentSearchTerm.isNull() == term.isNull() && currentSort == sort)
    {
        return;
    }
    currentSearchTerm = term;
    currentSort = sort;
    if (jobPtr)
    {
        jobPtr->abort();
        searchState = ResetRequested;
        return;
    }
    else
    {
        beginResetModel();
        modpacks.clear();
        endResetModel();
        searchState = None;
    }
    nextSearchOffset = 0;
    performPaginatedSearch();
}

void ListModel::searchRequestFinished()
{
    jobPtr.reset();

    QJsonParseError parse_error;
    QJsonDocument doc = QJsonDocument::fromJson(response, &parse_error);
    if (parse_error.error != QJsonParseError::NoError)
    {
        qWarning() << "Error while parsing JSON response from Modrinth at " << parse_error.offset
                    << " reason: " << parse_error.errorString();
        qWarning() << response;
        return;
    }

    QList<Modrinth::IndexedPack> newList;
    auto obj = doc.object();
    auto hits = Json::ensureArray(obj, "hits");
    for (auto packRaw : hits)
    {
        auto packObj = packRaw.toObject();
        Modrinth::IndexedPack pack;
        try
        {
            Modrinth::loadIndexedPack(pack, packObj);
            newList.append(pack);
        }
        catch (const JSONValidationError &e)
        {
            qWarning() << "Error while loading pack from Modrinth: " << e.cause();
            continue;
        }
    }

    int totalHits = Json::ensureInteger(obj, "total_hits", 0);
    if (newList.size() < 20 || (nextSearchOffset + newList.size()) >= totalHits)
    {
        searchState = Finished;
    }
    else
    {
        nextSearchOffset += 20;
        searchState = CanPossiblyFetchMore;
    }

    beginInsertRows(QModelIndex(), modpacks.size(), modpacks.size() + newList.size() - 1);
    modpacks.append(newList);
    endInsertRows();
}

void ListModel::searchRequestFailed(QString reason)
{
    jobPtr.reset();

    if (searchState == ResetRequested)
    {
        beginResetModel();
        modpacks.clear();
        endResetModel();

        nextSearchOffset = 0;
        performPaginatedSearch();
    }
    else
    {
        searchState = Finished;
    }
}

}
