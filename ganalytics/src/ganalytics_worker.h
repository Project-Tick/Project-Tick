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

#include <QJsonObject>
#include <QDateTime>
#include <QTimer>
#include <QNetworkRequest>
#include <QQueue>

struct QueryBuffer {
	QJsonObject payload;
	QDateTime time;
};

class GAnalyticsWorker : public QObject
{
	Q_OBJECT

  public:
	explicit GAnalyticsWorker(GAnalytics* parent = 0);

	GAnalytics* q;

	QNetworkAccessManager* networkManager = nullptr;

	QQueue<QueryBuffer> m_messageQueue;
	QTimer m_timer;
	QNetworkRequest m_request;
	GAnalytics::LogLevel m_logLevel;

	QString m_trackingID;
	QString m_clientID;
	QString m_userID;
	QString m_appName;
	QString m_appVersion;
	QString m_language;
	QString m_screenResolution;
	QString m_viewportSize;

	QString m_measurementId;
	QString m_apiSecret;
	QString m_sessionId;

	bool m_debugMode = false;
	bool m_anonymizeIPs = false;
	bool m_isEnabled = false;
	int m_timerInterval = 30000;
	int m_version = 0;

	const static int fourHours = 4 * 60 * 60 * 1000;
	const static QLatin1String dateTimeFormat;

  public:
	void logMessage(GAnalytics::LogLevel level, const QString& message);

	QJsonObject buildBasePayload();
	QUrl buildRequestUrl();
	QString getScreenResolution();
	QString getUserAgent();
	QList<QString> persistMessageQueue();
	void readMessagesFromFile(const QList<QString>& dataList);

	void enqueuePayload(const QJsonObject& payload);
	void setIsSending(bool doSend);
	void enable(bool state);

  public slots:
	void postMessage();
	void postMessageFinished();
};
