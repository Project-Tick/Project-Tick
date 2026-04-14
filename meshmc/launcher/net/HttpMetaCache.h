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

#pragma once
#include <QString>
#include <QMap>
#include <qtimer.h>
#include <memory>

class HttpMetaCache;

class MetaEntry
{
	friend class HttpMetaCache;

  protected:
	MetaEntry() {}

  public:
	bool isStale()
	{
		return stale;
	}
	void setStale(bool isStale)
	{
		stale = isStale;
	}
	QString getFullPath();
	QString getRemoteChangedTimestamp()
	{
		return remote_changed_timestamp;
	}
	void setRemoteChangedTimestamp(QString remoteChangedTimestamp)
	{
		remote_changed_timestamp = remoteChangedTimestamp;
	}
	void setLocalChangedTimestamp(qint64 timestamp)
	{
		local_changed_timestamp = timestamp;
	}
	QString getETag()
	{
		return etag;
	}
	void setETag(QString eTag)
	{
		etag = eTag;
	}
	QString getMD5Sum()
	{
		return md5sum;
	}
	void setMD5Sum(QString md5Sum)
	{
		md5sum = md5Sum;
	}

  protected:
	QString baseId;
	QString basePath;
	QString relativePath;
	QString md5sum;
	QString etag;
	qint64 local_changed_timestamp = 0;
	QString remote_changed_timestamp; // QString for now, RFC 2822 encoded time
	bool stale = true;
};

typedef std::shared_ptr<MetaEntry> MetaEntryPtr;

class HttpMetaCache : public QObject
{
	Q_OBJECT
  public:
	// supply path to the cache index file
	HttpMetaCache(QString path = QString());
	~HttpMetaCache();

	// get the entry solely from the cache
	// you probably don't want this, unless you have some specific caching
	// needs.
	MetaEntryPtr getEntry(QString base, QString resource_path);

	// get the entry from cache and verify that it isn't stale (within reason)
	MetaEntryPtr resolveEntry(QString base, QString resource_path,
							  QString expected_etag = QString());

	// add a previously resolved stale entry
	bool updateEntry(MetaEntryPtr stale_entry);

	// evict selected entry from cache
	bool evictEntry(MetaEntryPtr entry);

	void addBase(QString base, QString base_root);

	// (re)start a timer that calls SaveNow later.
	void SaveEventually();
	void Load();
	QString getBasePath(QString base);
  public slots:
	void SaveNow();

  private:
	// create a new stale entry, given the parameters
	MetaEntryPtr staleEntry(QString base, QString resource_path);
	struct EntryMap {
		QString base_path;
		QMap<QString, MetaEntryPtr> entry_list;
	};
	QMap<QString, EntryMap> m_entries;
	QString m_index_file;
	QTimer saveBatchingTimer;
};
