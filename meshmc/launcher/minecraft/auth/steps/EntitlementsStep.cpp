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

#include "EntitlementsStep.h"

#include <QNetworkRequest>
#include <QUuid>

#include "minecraft/auth/AuthRequest.h"
#include "minecraft/auth/Parsers.h"

EntitlementsStep::EntitlementsStep(AccountData* data) : AuthStep(data) {}

EntitlementsStep::~EntitlementsStep() noexcept = default;

QString EntitlementsStep::describe()
{
	return tr("Determining game ownership.");
}

void EntitlementsStep::perform()
{
	auto uuid = QUuid::createUuid();
	m_entitlementsRequestId = uuid.toString().remove('{').remove('}');
	auto url =
		"https://api.minecraftservices.com/entitlements/license?requestId=" +
		m_entitlementsRequestId;
	QNetworkRequest request = QNetworkRequest(url);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	request.setRawHeader("Accept", "application/json");
	request.setRawHeader(
		"Authorization",
		QString("Bearer %1").arg(m_data->yggdrasilToken.token).toUtf8());
	AuthRequest* requestor = new AuthRequest(this);
	connect(requestor, &AuthRequest::finished, this,
			&EntitlementsStep::onRequestDone);
	requestor->get(request);
	qDebug() << "Getting entitlements...";
}

void EntitlementsStep::rehydrate()
{
	// NOOP, for now. We only save bools and there's nothing to check.
}

void EntitlementsStep::onRequestDone(QNetworkReply::NetworkError,
									 QByteArray data,
									 QList<QNetworkReply::RawHeaderPair>)
{
	auto requestor = qobject_cast<AuthRequest*>(QObject::sender());
	requestor->deleteLater();

#ifndef NDEBUG
	qDebug() << data;
#endif

	// TODO: check presence of same entitlementsRequestId?
	// TODO: validate JWTs?
	Parsers::parseMinecraftEntitlements(data, m_data->minecraftEntitlement);

	emit finished(AccountTaskState::STATE_WORKING, tr("Got entitlements"));
}
