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

#include <RWStorage.h>

#include <QAbstractListModel>
#include <QIcon>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>

#include <functional>
#include <net/NetJob.h>

#include <modplatform/modrinth/ModrinthPackIndex.h>

namespace Modrinth {

typedef QMap<QString, QIcon> LogoMap;
typedef std::function<void(QString)> LogoCallback;

class ListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit ListModel(QObject *parent);
    virtual ~ListModel() override;

    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool canFetchMore(const QModelIndex &parent) const override;
    void fetchMore(const QModelIndex &parent) override;

    void getLogo(const QString &logo, const QString &logoUrl, LogoCallback callback);
    void searchWithTerm(const QString &term, const int sort);

private slots:
    void performPaginatedSearch();

    void logoFailed(QString logo);
    void logoLoaded(QString logo, QIcon out);

    void searchRequestFinished();
    void searchRequestFailed(QString reason);

private:
    void requestLogo(QString file, QString url);

private:
    QList<IndexedPack> modpacks;
    QStringList m_failedLogos;
    QStringList m_loadingLogos;
    LogoMap m_logoMap;
    QMap<QString, LogoCallback> waitingCallbacks;

    QString currentSearchTerm;
    int currentSort = 0;
    int nextSearchOffset = 0;
    enum SearchState {
        None,
        CanPossiblyFetchMore,
        ResetRequested,
        Finished
    } searchState = None;
    NetJob::Ptr jobPtr;
    QByteArray response;
};

}
