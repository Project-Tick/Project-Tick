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

#include "FlameModel.h"
#include "Application.h"
#include <Json.h>

#include <MMCStrings.h>
#include <Version.h>

#include <QtMath>
#include <QLabel>

#include <RWStorage.h>

namespace Flame
{

	ListModel::ListModel(QObject* parent) : QAbstractListModel(parent) {}

	ListModel::~ListModel() {}

	int ListModel::rowCount(const QModelIndex& parent) const
	{
		return modpacks.size();
	}

	int ListModel::columnCount(const QModelIndex& parent) const
	{
		return 1;
	}

	QVariant ListModel::data(const QModelIndex& index, int role) const
	{
		int pos = index.row();
		if (pos >= modpacks.size() || pos < 0 || !index.isValid()) {
			return QString("INVALID INDEX %1").arg(pos);
		}

		IndexedPack pack = modpacks.at(pos);
		if (role == Qt::DisplayRole) {
			return pack.name;
		} else if (role == Qt::ToolTipRole) {
			if (pack.description.length() > 100) {
				// some magic to prevent to long tooltips and replace html
				// linebreaks
				QString edit = pack.description.left(97);
				edit = edit.left(edit.lastIndexOf("<br>"))
						   .left(edit.lastIndexOf(" "))
						   .append("...");
				return edit;
			}
			return pack.description;
		} else if (role == Qt::DecorationRole) {
			if (m_logoMap.contains(pack.logoName)) {
				return (m_logoMap.value(pack.logoName));
			}
			QIcon icon = APPLICATION->getThemedIcon("screenshot-placeholder");
			((ListModel*)this)->requestLogo(pack.logoName, pack.logoUrl);
			return icon;
		} else if (role == Qt::UserRole) {
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
		for (int i = 0; i < modpacks.size(); i++) {
			if (modpacks[i].logoName == logo) {
				emit dataChanged(createIndex(i, 0), createIndex(i, 0),
								 {Qt::DecorationRole});
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
		if (url.isEmpty()) {
			return;
		}
		if (m_loadingLogos.contains(logo) || m_failedLogos.contains(logo)) {
			return;
		}

		MetaEntryPtr entry = APPLICATION->metacache()->resolveEntry(
			"FlamePacks", QString("logos/%1").arg(logo.section(".", 0, 0)));
		NetJob* job = new NetJob(QString("Flame Icon Download %1").arg(logo),
								 APPLICATION->network());
		job->addNetAction(Net::Download::makeCached(QUrl(url), entry));

		auto fullPath = entry->getFullPath();
		QObject::connect(job, &NetJob::succeeded, this, [this, logo, fullPath] {
			emit logoLoaded(logo, QIcon(fullPath));
			if (waitingCallbacks.contains(logo)) {
				waitingCallbacks.value(logo)(fullPath);
			}
		});

		QObject::connect(job, &NetJob::failed, this,
						 [this, logo] { emit logoFailed(logo); });

		job->start();

		m_loadingLogos.append(logo);
	}

	void ListModel::getLogo(const QString& logo, const QString& logoUrl,
							LogoCallback callback)
	{
		if (m_logoMap.contains(logo)) {
			callback(APPLICATION->metacache()
						 ->resolveEntry(
							 "FlamePacks",
							 QString("logos/%1").arg(logo.section(".", 0, 0)))
						 ->getFullPath());
		} else {
			requestLogo(logo, logoUrl);
		}
	}

	Qt::ItemFlags ListModel::flags(const QModelIndex& index) const
	{
		return QAbstractListModel::flags(index);
	}

	bool ListModel::canFetchMore(const QModelIndex& parent) const
	{
		return searchState == CanPossiblyFetchMore;
	}

	void ListModel::fetchMore(const QModelIndex& parent)
	{
		if (parent.isValid())
			return;
		if (nextSearchOffset == 0) {
			qWarning() << "fetchMore with 0 offset is wrong...";
			return;
		}
		performPaginatedSearch();
	}

	void ListModel::performPaginatedSearch()
	{
		// API v1 sort fields (1-indexed): 1=Featured, 2=Popularity,
		// 3=LastUpdated, 4=Name, 5=Author, 6=TotalDownloads Use desc for
		// Featured/Popularity/LastUpdated/TotalDownloads, asc for Name/Author
		// (A-Z)
		static const char* sortOrders[] = {"desc", "desc", "desc",
										   "asc",  "asc",  "desc"};
		const char* sortOrder = (currentSort >= 0 && currentSort < 6)
									? sortOrders[currentSort]
									: "desc";

		NetJob* netJob = new NetJob("Flame::Search", APPLICATION->network());
		auto searchUrl = QString("https://api.curseforge.com/v1/mods/search?"
								 "gameId=432&"
								 "classId=4471&"
								 "index=%1&"
								 "pageSize=25&"
								 "searchFilter=%2&"
								 "sortField=%3&"
								 "sortOrder=%4")
							 .arg(nextSearchOffset)
							 .arg(currentSearchTerm)
							 .arg(currentSort + 1)
							 .arg(sortOrder);
		netJob->addNetAction(
			Net::Download::makeByteArray(QUrl(searchUrl), &response));
		jobPtr = netJob;
		jobPtr->start();
		QObject::connect(netJob, &NetJob::succeeded, this,
						 &ListModel::searchRequestFinished);
		QObject::connect(netJob, &NetJob::failed, this,
						 &ListModel::searchRequestFailed);
	}

	void ListModel::searchWithTerm(const QString& term, int sort)
	{
		if (currentSearchTerm == term &&
			currentSearchTerm.isNull() == term.isNull() &&
			currentSort == sort) {
			return;
		}
		currentSearchTerm = term;
		currentSort = sort;
		if (jobPtr) {
			jobPtr->abort();
			searchState = ResetRequested;
			return;
		} else {
			beginResetModel();
			modpacks.clear();
			endResetModel();
			searchState = None;
		}
		nextSearchOffset = 0;
		performPaginatedSearch();
	}

	void Flame::ListModel::searchRequestFinished()
	{
		jobPtr.reset();

		QJsonParseError parse_error;
		QJsonDocument doc = QJsonDocument::fromJson(response, &parse_error);
		if (parse_error.error != QJsonParseError::NoError) {
			qWarning()
				<< "Error while parsing JSON response from CurseForge at "
				<< parse_error.offset
				<< " reason: " << parse_error.errorString();
			qWarning() << response;
			return;
		}

		QList<Flame::IndexedPack> newList;
		// CurseForge API v1 wraps results in {"data": [...], "pagination":
		// {...}}
		QJsonArray packs;
		if (doc.isObject() && doc.object().contains("data")) {
			packs = doc.object().value("data").toArray();
			qDebug() << "CurseForge: parsed" << packs.size()
					 << "packs from 'data' key";
		} else {
			packs = doc.array();
			qDebug() << "CurseForge: parsed" << packs.size()
					 << "packs from root array";
		}
		qDebug() << "CurseForge raw response (first 500 chars):"
				 << response.left(500);
		for (auto packRaw : packs) {
			auto packObj = packRaw.toObject();

			Flame::IndexedPack pack;
			try {
				Flame::loadIndexedPack(pack, packObj);
				newList.append(pack);
			} catch (const JSONValidationError& e) {
				qWarning() << "Error while loading pack from CurseForge: "
						   << e.cause();
				continue;
			}
		}
		if (packs.size() < 25) {
			searchState = Finished;
		} else {
			nextSearchOffset += 25;
			searchState = CanPossiblyFetchMore;
		}
		beginInsertRows(QModelIndex(), modpacks.size(),
						modpacks.size() + newList.size() - 1);
		modpacks.append(newList);
		endInsertRows();
	}

	void Flame::ListModel::searchRequestFailed(QString reason)
	{
		jobPtr.reset();

		if (searchState == ResetRequested) {
			beginResetModel();
			modpacks.clear();
			endResetModel();

			nextSearchOffset = 0;
			performPaginatedSearch();
		} else {
			searchState = Finished;
		}
	}

} // namespace Flame
