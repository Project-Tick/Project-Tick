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

#include "GetSkinStep.h"

#include <QNetworkRequest>

#include "minecraft/auth/AuthRequest.h"
#include "minecraft/auth/Parsers.h"

GetSkinStep::GetSkinStep(AccountData* data) : AuthStep(data) {}

GetSkinStep::~GetSkinStep() noexcept = default;

QString GetSkinStep::describe()
{
	return tr("Getting skin.");
}

void GetSkinStep::perform()
{
	auto url = QUrl(m_data->minecraftProfile.skin.url);
	QNetworkRequest request = QNetworkRequest(url);
	AuthRequest* requestor = new AuthRequest(this);
	connect(requestor, &AuthRequest::finished, this,
			&GetSkinStep::onRequestDone);
	requestor->get(request);
}

void GetSkinStep::rehydrate()
{
	// NOOP, for now.
	// TODO: Make the most of this space.
}

void GetSkinStep::onRequestDone(QNetworkReply::NetworkError error,
								QByteArray data,
								QList<QNetworkReply::RawHeaderPair>)
{
	auto requestor = qobject_cast<AuthRequest*>(QObject::sender());
	requestor->deleteLater();

	if (error == QNetworkReply::NoError) {
		m_data->minecraftProfile.skin.data = data;
	}
	emit finished(AccountTaskState::STATE_SUCCEEDED, tr("Got skin"));
}
