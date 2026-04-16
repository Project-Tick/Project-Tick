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

#include <QTimer>
#include <QNetworkReply>

#include "katabasis/Reply.h"

namespace Katabasis
{

	Reply::Reply(QNetworkReply* r, int timeOut, QObject* parent)
		: QTimer(parent), reply(r)
	{
		setSingleShot(true);
		connect(this, &Reply::timeout, this, &Reply::onTimeOut,
				Qt::QueuedConnection);
		start(timeOut);
	}

	void Reply::onTimeOut()
	{
		timedOut = true;
		reply->abort();
	}

	// ----------------------------

	ReplyList::~ReplyList()
	{
		foreach (Reply* timedReply, replies_) {
			delete timedReply;
		}
	}

	void ReplyList::add(QNetworkReply* reply, int timeOut)
	{
		if (reply && ignoreSslErrors()) {
			reply->ignoreSslErrors();
		}
		add(new Reply(reply, timeOut));
	}

	void ReplyList::add(Reply* reply)
	{
		replies_.append(reply);
	}

	void ReplyList::remove(QNetworkReply* reply)
	{
		Reply* o2Reply = find(reply);
		if (o2Reply) {
			o2Reply->stop();
			(void)replies_.removeOne(o2Reply);
		}
	}

	Reply* ReplyList::find(QNetworkReply* reply)
	{
		foreach (Reply* timedReply, replies_) {
			if (timedReply->reply == reply) {
				return timedReply;
			}
		}
		return 0;
	}

	bool ReplyList::ignoreSslErrors()
	{
		return ignoreSslErrors_;
	}

	void ReplyList::setIgnoreSslErrors(bool ignoreSslErrors)
	{
		ignoreSslErrors_ = ignoreSslErrors;
	}

} // namespace Katabasis
