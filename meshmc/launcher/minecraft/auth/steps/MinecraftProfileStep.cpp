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

#include "MinecraftProfileStep.h"

#include <QNetworkRequest>

#include "minecraft/auth/AuthRequest.h"
#include "minecraft/auth/Parsers.h"

MinecraftProfileStep::MinecraftProfileStep(AccountData* data) : AuthStep(data)
{
}

MinecraftProfileStep::~MinecraftProfileStep() noexcept = default;

QString MinecraftProfileStep::describe()
{
	return tr("Fetching the Minecraft profile.");
}

void MinecraftProfileStep::perform()
{
	auto url = QUrl("https://api.minecraftservices.com/minecraft/profile");
	QNetworkRequest request = QNetworkRequest(url);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	request.setRawHeader(
		"Authorization",
		QString("Bearer %1").arg(m_data->yggdrasilToken.token).toUtf8());

	AuthRequest* requestor = new AuthRequest(this);
	connect(requestor, &AuthRequest::finished, this,
			&MinecraftProfileStep::onRequestDone);
	requestor->get(request);
}

void MinecraftProfileStep::rehydrate()
{
	// NOOP, for now. We only save bools and there's nothing to check.
}

void MinecraftProfileStep::onRequestDone(QNetworkReply::NetworkError error,
										 QByteArray data,
										 QList<QNetworkReply::RawHeaderPair>)
{
	auto requestor = qobject_cast<AuthRequest*>(QObject::sender());
	requestor->deleteLater();

#ifndef NDEBUG
	qDebug() << data;
#endif
	if (error == QNetworkReply::ContentNotFoundError) {
		// NOTE: Succeed even if we do not have a profile. This is a valid
		// account state.
		m_data->minecraftProfile = MinecraftProfile();
		emit finished(AccountTaskState::STATE_SUCCEEDED,
					  tr("Account has no Minecraft profile."));
		return;
	}
	if (error != QNetworkReply::NoError) {
		qWarning() << "Error getting profile:";
		qWarning() << " HTTP Status:        " << requestor->httpStatus_;
		qWarning() << " Internal error no.: " << error;
		qWarning() << " Error string:       " << requestor->errorString_;

		qWarning() << " Response:";
		qWarning() << QString::fromUtf8(data);

		emit finished(AccountTaskState::STATE_FAILED_SOFT,
					  tr("Minecraft Java profile acquisition failed."));
		return;
	}
	if (!Parsers::parseMinecraftProfile(data, m_data->minecraftProfile)) {
		m_data->minecraftProfile = MinecraftProfile();
		emit finished(
			AccountTaskState::STATE_FAILED_SOFT,
			tr("Minecraft Java profile response could not be parsed"));
		return;
	}

	emit finished(AccountTaskState::STATE_WORKING,
				  tr("Minecraft Java profile acquisition succeeded."));
}
