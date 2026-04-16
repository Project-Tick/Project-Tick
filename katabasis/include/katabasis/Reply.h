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

#include <QList>
#include <QTimer>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QByteArray>

namespace Katabasis
{

	constexpr int defaultTimeout = 30 * 1000;

	/// A network request/reply pair that can time out.
	class Reply : public QTimer
	{
		Q_OBJECT

	  public:
		Reply(QNetworkReply* reply, int timeOut = defaultTimeout,
			  QObject* parent = 0);

	  signals:
		void error(QNetworkReply::NetworkError);

	  public slots:
		/// When time out occurs, the QNetworkReply's error() signal is
		/// triggered.
		void onTimeOut();

	  public:
		QNetworkReply* reply;
		bool timedOut = false;
	};

	/// List of O2Replies.
	class ReplyList
	{
	  public:
		ReplyList()
		{
			ignoreSslErrors_ = false;
		}

		/// Destructor.
		/// Deletes all O2Reply instances in the list.
		virtual ~ReplyList();

		/// Create a new O2Reply from a QNetworkReply, and add it to this list.
		void add(QNetworkReply* reply, int timeOut = defaultTimeout);

		/// Add an O2Reply to the list, while taking ownership of it.
		void add(Reply* reply);

		/// Remove item from the list that corresponds to a QNetworkReply.
		void remove(QNetworkReply* reply);

		/// Find an O2Reply in the list, corresponding to a QNetworkReply.
		/// @return Matching O2Reply or NULL.
		Reply* find(QNetworkReply* reply);

		bool ignoreSslErrors();
		void setIgnoreSslErrors(bool ignoreSslErrors);

	  protected:
		QList<Reply*> replies_;
		bool ignoreSslErrors_;
	};

} // namespace Katabasis
