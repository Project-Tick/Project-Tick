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

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

#include "CrashReportDialog.h"

static QString readLogFile(const QString& logDir, const QString& baseName)
{
	QString logPath =
		QDir(logDir).absoluteFilePath(QString("%1-0.log").arg(baseName));
	QFile file(logPath);
	if (!file.open(QFile::ReadOnly | QFile::Text)) {
		qWarning() << "Could not open log file:" << logPath;
		return QString();
	}
	return QString::fromUtf8(file.readAll());
}

static QString
uploadToPasteEE(QNetworkAccessManager* nam, const QString& text,
				const QString& apiKey)
{
	QJsonObject sectionObject;
	sectionObject.insert("contents", text);
	QJsonArray sectionArray;
	sectionArray.append(sectionObject);
	QJsonObject topLevelObj;
	topLevelObj.insert("description", "MeshMC Crash Log");
	topLevelObj.insert("sections", sectionArray);
	QJsonDocument docOut;
	docOut.setObject(topLevelObj);
	QByteArray jsonContent = docOut.toJson();

	// Validate size (2MB for public, 12MB for keyed)
	int maxSize = (apiKey == "public") ? (1024 * 1024 * 2) : (1024 * 1024 * 12);
	if (jsonContent.size() > maxSize) {
		qWarning() << "Log file too large for paste.ee upload";
		return QString();
	}

	QNetworkRequest request(QUrl("https://api.paste.ee/v1/pastes"));
	request.setHeader(QNetworkRequest::UserAgentHeader,
					  "MeshMC-CrashReporter/1.0");
	request.setRawHeader("Content-Type", "application/json");
	request.setRawHeader("Content-Length",
						 QByteArray::number(jsonContent.size()));
	request.setRawHeader("X-Auth-Token", apiKey.toUtf8());

	QNetworkReply* reply = nam->post(request, jsonContent);

	// Synchronous wait
	QEventLoop loop;
	QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	// Timeout after 30 seconds
	QTimer timer;
	timer.setSingleShot(true);
	QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
	timer.start(30000);
	loop.exec();

	if (reply->error() != QNetworkReply::NoError) {
		qWarning() << "Network error during paste.ee upload:"
				   << reply->errorString();
		reply->deleteLater();
		return QString();
	}

	QByteArray data = reply->readAll();
	reply->deleteLater();

	QJsonParseError jsonError;
	QJsonDocument doc = QJsonDocument::fromJson(data, &jsonError);
	if (jsonError.error != QJsonParseError::NoError) {
		qWarning() << "JSON parse error:" << jsonError.errorString();
		return QString();
	}

	auto object = doc.object();
	if (!object.value("success").toBool()) {
		qWarning() << "paste.ee reported error:"
				   << object.value("error").toString();
		return QString();
	}

	return object.value("link").toString();
}

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);
	app.setApplicationName("meshmc-crashreporter");
	app.setApplicationVersion("1.0");

	QCommandLineParser parser;
	parser.setApplicationDescription("MeshMC Crash Reporter");
	parser.addHelpOption();

	QCommandLineOption logDirOption(
		"logdir", "Directory containing MeshMC log files.", "directory");
	parser.addOption(logDirOption);

	QCommandLineOption baseNameOption(
		"name", "Base name for log files (default: MeshMC).", "name",
		"MeshMC");
	parser.addOption(baseNameOption);

	QCommandLineOption apiKeyOption(
		"apikey", "paste.ee API key (default: public).", "key", "public");
	parser.addOption(apiKeyOption);

	parser.process(app);

	QString logDir = parser.value(logDirOption);
	if (logDir.isEmpty()) {
		logDir = QDir::currentPath();
	}

	QString baseName = parser.value(baseNameOption);
	QString apiKey = parser.value(apiKeyOption);

	// Read the crash log
	QString logContent = readLogFile(logDir, baseName);
	if (logContent.isEmpty()) {
		QMessageBox::warning(nullptr, "MeshMC Crash Reporter",
							 "Could not read MeshMC log file.\n"
							 "The crash reporter cannot proceed.");
		return 1;
	}

	// Upload to paste.ee
	QNetworkAccessManager nam;
	QString pasteLink = uploadToPasteEE(&nam, logContent, apiKey);
	if (pasteLink.isEmpty()) {
		QMessageBox::warning(
			nullptr, "MeshMC Crash Reporter",
			"Failed to upload crash log to paste.ee.\n"
			"Please manually share the log file located at:\n" +
				QDir(logDir).absoluteFilePath(
					QString("%1-0.log").arg(baseName)));
		return 1;
	}

	// Show the crash dialog
	CrashReportDialog dialog(pasteLink, logContent);
	dialog.exec();

	return 0;
}
