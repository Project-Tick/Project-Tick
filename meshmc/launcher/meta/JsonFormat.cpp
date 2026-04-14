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
 * Copyright 2015-2021 MultiMC Contributors
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

#include "JsonFormat.h"

// FIXME: remove this from here... somehow
#include "minecraft/OneSixVersionFormat.h"
#include "Json.h"

#include "Index.h"
#include "Version.h"
#include "VersionList.h"

using namespace Json;

namespace Meta
{

	MetadataVersion currentFormatVersion()
	{
		return MetadataVersion::InitialRelease;
	}

	// Index
	static std::shared_ptr<Index> parseIndexInternal(const QJsonObject& obj)
	{
		const QVector<QJsonObject> objects =
			requireIsArrayOf<QJsonObject>(obj, "packages");
		QVector<VersionListPtr> lists;
		lists.reserve(static_cast<int>(objects.size()));
		std::transform(objects.begin(), objects.end(),
					   std::back_inserter(lists), [](const QJsonObject& obj) {
						   VersionListPtr list = std::make_shared<VersionList>(
							   requireString(obj, "uid"));
						   list->setName(ensureString(obj, "name", QString()));
						   return list;
					   });
		return std::make_shared<Index>(lists);
	}

	// Version
	static VersionPtr parseCommonVersion(const QString& uid,
										 const QJsonObject& obj)
	{
		VersionPtr version =
			std::make_shared<Version>(uid, requireString(obj, "version"));
		version->setTime(QDateTime::fromString(
							 requireString(obj, "releaseTime"), Qt::ISODate)
							 .toMSecsSinceEpoch() /
						 1000);
		version->setType(ensureString(obj, "type", QString()));
		version->setRecommended(
			ensureBoolean(obj, QString("recommended"), false));
		version->setVolatile(ensureBoolean(obj, QString("volatile"), false));
		RequireSet requirements, conflicts;
		parseRequires(obj, &requirements, "requires");
		parseRequires(obj, &conflicts, "conflicts");
		version->setRequires(requirements, conflicts);
		return version;
	}

	static std::shared_ptr<Version> parseVersionInternal(const QJsonObject& obj)
	{
		VersionPtr version = parseCommonVersion(requireString(obj, "uid"), obj);

		version->setData(OneSixVersionFormat::versionFileFromJson(
			QJsonDocument(obj),
			QString("%1/%2.json").arg(version->uid(), version->version()),
			obj.contains("order")));
		return version;
	}

	// Version list / package
	static std::shared_ptr<VersionList>
	parseVersionListInternal(const QJsonObject& obj)
	{
		const QString uid = requireString(obj, "uid");

		const QVector<QJsonObject> versionsRaw =
			requireIsArrayOf<QJsonObject>(obj, "versions");
		QVector<VersionPtr> versions;
		versions.reserve(static_cast<int>(versionsRaw.size()));
		std::transform(versionsRaw.begin(), versionsRaw.end(),
					   std::back_inserter(versions),
					   [uid](const QJsonObject& vObj) {
						   auto version = parseCommonVersion(uid, vObj);
						   version->setProvidesRecommendations();
						   return version;
					   });

		VersionListPtr list = std::make_shared<VersionList>(uid);
		list->setName(ensureString(obj, "name", QString()));
		list->setVersions(versions);
		return list;
	}

	MetadataVersion parseFormatVersion(const QJsonObject& obj, bool required)
	{
		if (!obj.contains("formatVersion")) {
			if (required) {
				return MetadataVersion::Invalid;
			}
			return MetadataVersion::InitialRelease;
		}
		if (!obj.value("formatVersion").isDouble()) {
			return MetadataVersion::Invalid;
		}
		switch (obj.value("formatVersion").toInt()) {
			case 0:
			case 1:
				return MetadataVersion::InitialRelease;
			default:
				return MetadataVersion::Invalid;
		}
	}

	void serializeFormatVersion(QJsonObject& obj, Meta::MetadataVersion version)
	{
		if (version == MetadataVersion::Invalid) {
			return;
		}
		obj.insert("formatVersion", int(version));
	}

	void parseIndex(const QJsonObject& obj, Index* ptr)
	{
		const MetadataVersion version = parseFormatVersion(obj);
		switch (version) {
			case MetadataVersion::InitialRelease:
				ptr->merge(parseIndexInternal(obj));
				break;
			case MetadataVersion::Invalid:
				throw ParseException(QObject::tr("Unknown format version!"));
		}
	}

	void parseVersionList(const QJsonObject& obj, VersionList* ptr)
	{
		const MetadataVersion version = parseFormatVersion(obj);
		switch (version) {
			case MetadataVersion::InitialRelease:
				ptr->merge(parseVersionListInternal(obj));
				break;
			case MetadataVersion::Invalid:
				throw ParseException(QObject::tr("Unknown format version!"));
		}
	}

	void parseVersion(const QJsonObject& obj, Version* ptr)
	{
		const MetadataVersion version = parseFormatVersion(obj);
		switch (version) {
			case MetadataVersion::InitialRelease:
				ptr->merge(parseVersionInternal(obj));
				break;
			case MetadataVersion::Invalid:
				throw ParseException(QObject::tr("Unknown format version!"));
		}
	}

	/*
	[
	{"uid":"foo", "equals":"version"}
	]
	*/
	void parseRequires(const QJsonObject& obj, RequireSet* ptr,
					   const char* keyName)
	{
		if (obj.contains(keyName)) {
			QSet<QString> requirements;
			auto reqArray = requireArray(obj, keyName);
			auto iter = reqArray.begin();
			while (iter != reqArray.end()) {
				auto reqObject = requireObject(*iter);
				auto uid = requireString(reqObject, "uid");
				auto equals = ensureString(reqObject, "equals", QString());
				auto suggests = ensureString(reqObject, "suggests", QString());
				ptr->insert({uid, equals, suggests});
				iter++;
			}
		}
	}
	void serializeRequires(QJsonObject& obj, RequireSet* ptr,
						   const char* keyName)
	{
		if (!ptr || ptr->empty()) {
			return;
		}
		QJsonArray arrOut;
		for (auto& iter : *ptr) {
			QJsonObject reqOut;
			reqOut.insert("uid", iter.uid);
			if (!iter.equalsVersion.isEmpty()) {
				reqOut.insert("equals", iter.equalsVersion);
			}
			if (!iter.suggests.isEmpty()) {
				reqOut.insert("suggests", iter.suggests);
			}
			arrOut.append(reqOut);
		}
		obj.insert(keyName, arrOut);
	}

} // namespace Meta
