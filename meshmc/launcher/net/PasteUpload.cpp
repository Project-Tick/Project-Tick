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

#include "PasteUpload.h"
#include "BuildConfig.h"
#include "Application.h"

#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>

PasteUpload::PasteUpload(QWidget* window, QString text, QString key)
	: m_window(window)
{
	m_key = key;
	QByteArray temp;
	QJsonObject topLevelObj;
	QJsonObject sectionObject;
	sectionObject.insert("contents", text);
	QJsonArray sectionArray;
	sectionArray.append(sectionObject);
	topLevelObj.insert("description", "Log Upload");
	topLevelObj.insert("sections", sectionArray);
	QJsonDocument docOut;
	docOut.setObject(topLevelObj);
	m_jsonContent = docOut.toJson();
}

PasteUpload::~PasteUpload() {}

bool PasteUpload::validateText()
{
	return m_jsonContent.size() <= maxSize();
}

void PasteUpload::executeTask()
{
	QNetworkRequest request(QUrl("https://api.paste.ee/v1/pastes"));
	request.setHeader(QNetworkRequest::UserAgentHeader,
					  BuildConfig.USER_AGENT_UNCACHED);

	request.setRawHeader("Content-Type", "application/json");
	request.setRawHeader("Content-Length",
						 QByteArray::number(m_jsonContent.size()));
	request.setRawHeader("X-Auth-Token", m_key.toStdString().c_str());

	QNetworkReply* rep = APPLICATION->network()->post(request, m_jsonContent);

	m_reply = std::shared_ptr<QNetworkReply>(rep);
	setStatus(tr("Uploading to paste.ee"));
	connect(rep, &QNetworkReply::uploadProgress, this, &Task::setProgress);
	connect(rep, &QNetworkReply::errorOccurred, this,
			&PasteUpload::downloadError);
	connect(rep, &QNetworkReply::finished, this, &PasteUpload::downloadFinished);
}

void PasteUpload::downloadError(QNetworkReply::NetworkError error)
{
	// error happened during download.
	qCritical() << "Network error: " << error;
	emitFailed(m_reply->errorString());
}

void PasteUpload::downloadFinished()
{
	QByteArray data = m_reply->readAll();
	// if the download succeeded
	if (m_reply->error() == QNetworkReply::NetworkError::NoError) {
		m_reply.reset();
		QJsonParseError jsonError;
		QJsonDocument doc = QJsonDocument::fromJson(data, &jsonError);
		if (jsonError.error != QJsonParseError::NoError) {
			emitFailed(jsonError.errorString());
			return;
		}
		if (!parseResult(doc)) {
			emitFailed(tr("paste.ee returned an error. Please consult the logs "
						  "for more information"));
			return;
		}
	}
	// else the download failed
	else {
		emitFailed(QString("Network error: %1").arg(m_reply->errorString()));
		m_reply.reset();
		return;
	}
	emitSucceeded();
}

bool PasteUpload::parseResult(QJsonDocument doc)
{
	auto object = doc.object();
	auto status = object.value("success").toBool();
	if (!status) {
		qCritical() << "paste.ee reported error:"
					<< QString(object.value("error").toString());
		return false;
	}
	m_pasteLink = object.value("link").toString();
	m_pasteID = object.value("id").toString();
	qDebug() << m_pasteLink;
	return true;
}
