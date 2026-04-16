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

#pragma once

#include <QByteArray>
#include <QMap>
#include <QNetworkRequest>
#include <QObject>
#include <QString>
#include <QTimer>

class QNetworkAccessManager;

namespace Katabasis
{

	/// Poll an authorization server for token
	class PollServer : public QObject
	{
		Q_OBJECT

	  public:
		explicit PollServer(QNetworkAccessManager* manager,
							const QNetworkRequest& request,
							const QByteArray& payload, int expiresIn,
							QObject* parent = 0);

		/// Seconds to wait between polling requests
		Q_PROPERTY(int interval READ interval WRITE setInterval)
		int interval() const;
		void setInterval(int interval);

	  signals:
		void verificationReceived(QMap<QString, QString>);
		void serverClosed(bool); // whether it has found parameters

	  public slots:
		void startPolling();

	  protected slots:
		void onPollTimeout();
		void onExpiration();
		void onReplyFinished();

	  protected:
		QNetworkAccessManager* manager_;
		const QNetworkRequest request_;
		const QByteArray payload_;
		const int expiresIn_;
		QTimer expirationTimer;
		QTimer pollTimer;
	};

} // namespace Katabasis
