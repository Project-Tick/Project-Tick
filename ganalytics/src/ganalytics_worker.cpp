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

#include "ganalytics.h"
#include "ganalytics_worker.h"
#include "sys.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrlQuery>

#include <QGuiApplication>
#include <QScreen>

const QLatin1String
	GAnalyticsWorker::dateTimeFormat("yyyy,MM,dd-hh:mm::ss:zzz");

GAnalyticsWorker::GAnalyticsWorker(GAnalytics* parent)
	: QObject(parent), q(parent), m_logLevel(GAnalytics::Error)
{
	m_appName = QCoreApplication::instance()->applicationName();
	m_appVersion = QCoreApplication::instance()->applicationVersion();
	m_request.setHeader(QNetworkRequest::ContentTypeHeader,
						"application/json");
	m_request.setHeader(QNetworkRequest::UserAgentHeader, getUserAgent());

	m_language = QLocale::system().name().toLower().replace("_", "-");
	m_screenResolution = getScreenResolution();

	m_timer.setInterval(m_timerInterval);
	connect(&m_timer, &QTimer::timeout, this, &GAnalyticsWorker::postMessage);
}

void GAnalyticsWorker::enable(bool state)
{
	// state change to the same is not valid.
	if (m_isEnabled == state) {
		return;
	}

	m_isEnabled = state;
	if (m_isEnabled) {
		// enable -> start doing things :)
		m_timer.start();
	} else {
		// disable -> stop the timer
		m_timer.stop();
	}
}

void GAnalyticsWorker::logMessage(GAnalytics::LogLevel level,
								  const QString& message)
{
	if (m_logLevel > level) {
		return;
	}

	qDebug() << "[Analytics]" << message;
}

/**
 * Build the base GA4 JSON payload with client_id and optional user_id.
 * @return payload  Base QJsonObject for a GA4 Measurement Protocol request.
 */
QJsonObject GAnalyticsWorker::buildBasePayload()
{
	QJsonObject payload;
	payload["client_id"] = m_clientID;
	if (!m_userID.isEmpty()) {
		payload["user_id"] = m_userID;
	}
	return payload;
}

/**
 * Build the GA4 Measurement Protocol endpoint URL.
 * Includes measurement_id and api_secret as query parameters.
 * Uses debug endpoint when debug mode is enabled.
 * @return url  The fully constructed request URL.
 */
QUrl GAnalyticsWorker::buildRequestUrl()
{
	QUrl url;
	if (m_debugMode) {
		url.setUrl("https://www.google-analytics.com/debug/mp/collect");
	} else {
		url.setUrl("https://www.google-analytics.com/mp/collect");
	}
	QUrlQuery urlQuery;
	urlQuery.addQueryItem("measurement_id", m_measurementId);
	urlQuery.addQueryItem("api_secret", m_apiSecret);
	url.setQuery(urlQuery);
	return url;
}

/**
 * Get primary screen resolution.
 * @return      A QString like "800x600".
 */
QString GAnalyticsWorker::getScreenResolution()
{
	QScreen* screen = QGuiApplication::primaryScreen();
	QSize size = screen->size();

	return QString("%1x%2").arg(size.width()).arg(size.height());
}

/**
 * Try to gain information about the system where this application
 * is running. It needs to get the name and version of the operating
 * system, the language and screen resolution.
 * All this information will be send in POST messages.
 * @return agent        A QString with all the information formatted for a POST
 * message.
 */
QString GAnalyticsWorker::getUserAgent()
{
	return QString("%1/%2").arg(m_appName).arg(m_appVersion);
}

/**
 * The message queue contains a list of QueryBuffer object.
 * QueryBuffer holds a QUrlQuery object and a QDateTime object.
 * These both object are freed from the buffer object and
 * inserted as QString objects in a QList.
 * @return dataList     The list with concartinated queue data.
 */
QList<QString> GAnalyticsWorker::persistMessageQueue()
{
	QList<QString> dataList;
	foreach (QueryBuffer buffer, m_messageQueue) {
		QJsonDocument doc(buffer.payload);
		dataList << QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
		dataList << buffer.time.toString(dateTimeFormat);
	}
	return dataList;
}

/**
 * Reads persistent messages from a file.
 * Gets all message data as a QList<QString>.
 * Two lines in the list build a QueryBuffer object.
 */
void GAnalyticsWorker::readMessagesFromFile(const QList<QString>& dataList)
{
	QListIterator<QString> iter(dataList);
	while (iter.hasNext()) {
		QString jsonString = iter.next();
		QString dateString = iter.next();
		QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
		QDateTime dateTime = QDateTime::fromString(dateString, dateTimeFormat);
		QueryBuffer buffer;
		buffer.payload = doc.object();
		buffer.time = dateTime;
		m_messageQueue.enqueue(buffer);
	}
}

/**
 * Takes a QUrlQuery object and wrapp it together with
 * a QTime object into a QueryBuffer struct. These struct
 * will be stored in the message queue.
 */
void GAnalyticsWorker::enqueuePayload(const QJsonObject& payload)
{
	QueryBuffer buffer;
	buffer.payload = payload;
	buffer.time = QDateTime::currentDateTime();

	m_messageQueue.enqueue(buffer);
}

/**
 * This function is called by a timer interval.
 * The function tries to send a messages from the queue.
 * If message was successfully send then this function
 * will be called back to send next message.
 * If message queue contains more than one message then
 * the connection will kept open.
 * The message POST is asyncroniously when the server
 * answered a signal will be emitted.
 */
void GAnalyticsWorker::postMessage()
{
	if (m_messageQueue.isEmpty()) {
		m_timer.start();
		return;
	} else {
		m_timer.stop();
	}

	QString connection = "close";
	if (m_messageQueue.count() > 1) {
		connection = "keep-alive";
	}

	QueryBuffer buffer = m_messageQueue.head();
	QDateTime sendTime = QDateTime::currentDateTime();
	qint64 timeDiff = buffer.time.msecsTo(sendTime);

	if (timeDiff > fourHours) {
		m_messageQueue.dequeue();
		emit postMessage();
		return;
	}

	QJsonObject payload = buffer.payload;
	qint64 timestampMicros = buffer.time.toMSecsSinceEpoch() * 1000;
	payload["timestamp_micros"] = QString::number(timestampMicros);

	QJsonDocument doc(payload);
	QByteArray jsonData = doc.toJson(QJsonDocument::Compact);

	m_request.setUrl(buildRequestUrl());
	m_request.setRawHeader("Connection", connection.toUtf8());
	m_request.setHeader(QNetworkRequest::ContentLengthHeader, jsonData.length());

	logMessage(GAnalytics::Debug, "GA4 payload = " + QString::fromUtf8(jsonData));

	if (networkManager == NULL) {
		networkManager = new QNetworkAccessManager(this);
	}

	QNetworkReply* reply = networkManager->post(m_request, jsonData);
	connect(reply, &QNetworkReply::finished, this,
			&GAnalyticsWorker::postMessageFinished);
}

/**
 * NetworkAccsessManager has finished to POST a message.
 * If POST message was successfully send then the message
 * query should be removed from queue.
 * SIGNAL "postMessage" will be emitted to send next message
 * if there is any.
 * If message couldn't be send then next try is when the
 * timer emits its signal.
 */
void GAnalyticsWorker::postMessageFinished()
{
	QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());

	int httpStausCode =
		reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	if (httpStausCode < 200 || httpStausCode > 299) {
		logMessage(
			GAnalytics::Error,
			QString("Error posting message: %1").arg(reply->errorString()));

		// An error ocurred. Try sending later.
		m_timer.start();
		return;
	} else {
		logMessage(GAnalytics::Debug, "Message sent");
	}

	m_messageQueue.dequeue();
	postMessage();
	reply->deleteLater();
}
