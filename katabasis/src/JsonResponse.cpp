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

#include "JsonResponse.h"

#include <QByteArray>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

namespace Katabasis
{

	QVariantMap parseJsonResponse(const QByteArray& data)
	{
		QJsonParseError err;
		QJsonDocument doc = QJsonDocument::fromJson(data, &err);
		if (err.error != QJsonParseError::NoError) {
			qWarning() << "parseTokenResponse: Failed to parse token response "
						  "due to err:"
					   << err.errorString();
			return QVariantMap();
		}

		if (!doc.isObject()) {
			qWarning() << "parseTokenResponse: Token response is not an object";
			return QVariantMap();
		}

		return doc.object().toVariantMap();
	}

} // namespace Katabasis
