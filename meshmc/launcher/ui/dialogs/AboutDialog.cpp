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
 *
 *  This file incorporates work covered by the following copyright and
 *  permission notice:
 *
 * Copyright 2013-2021 MultiMC Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "AboutDialog.h"
#include "ui_AboutDialog.h"
#include <QIcon>
#include "Application.h"
#include "BuildConfig.h"

#include <net/NetJob.h>

#include "HoeDown.h"
#include "MMCStrings.h"
#include "plugin/PluginManager.h"

namespace
{
	// Credits
	QString getCreditsHtml()
	{
		QFile dataFile(":/documents/credits.html");
		if (!dataFile.open(QIODevice::ReadOnly)) {
			qWarning() << "Failed to open file" << dataFile.fileName()
					   << "for reading:" << dataFile.errorString();
			return {};
		}
		QString fileContent = QString::fromUtf8(dataFile.readAll());
		dataFile.close();

		return fileContent.arg(
			QObject::tr("%1 Developers").arg(BuildConfig.MESHMC_DISPLAYNAME),
			QObject::tr("MultiMC Developers"));
	}

	QString getLicenseHtml()
	{
		QFile dataFile(":/documents/COPYING.md");
		if (dataFile.open(QIODevice::ReadOnly)) {
			HoeDown hoedown;
			QString output = hoedown.process(dataFile.readAll());
			dataFile.close();
			return output;
		} else {
			qWarning() << "Failed to open file" << dataFile.fileName()
					   << "for reading:" << dataFile.errorString();
			return QString();
		}
	}

	QString getPluginsHtml()
	{
		auto* pm = APPLICATION->pluginManager();
		if (!pm || pm->moduleCount() == 0) {
			return QString("<p><i>%1</i></p>")
				.arg(QObject::tr("No plugins loaded."));
		}

		QString html;
		html += QLatin1String(
			"<style>"
			"table { width: 100%; border-collapse: collapse; }"
			"td { padding: 4px 8px; vertical-align: top; }"
			"td.label { font-weight: bold; white-space: nowrap; width: 1%; }"
			"hr { border: none; border-top: 1px solid #ccc; margin: 10px 0; }"
			"</style>");

		const auto& modules = pm->modules();
		for (int i = 0; i < modules.size(); ++i) {
			const auto& mod = modules[i];
			if (i > 0)
				html += QLatin1String("<hr>");

			html += QLatin1String("<table>");

			html +=
				QString(
					"<tr><td class=\"label\">%1</td><td><b>%2</b></td></tr>")
					.arg(QObject::tr("Name:"), mod.name.toHtmlEscaped());

			if (!mod.author.isEmpty()) {
				html +=
					QString("<tr><td class=\"label\">%1</td><td>%2</td></tr>")
						.arg(QObject::tr("Author:"),
							 mod.author.toHtmlEscaped());
			}

			if (!mod.version.isEmpty()) {
				html +=
					QString("<tr><td class=\"label\">%1</td><td>%2</td></tr>")
						.arg(QObject::tr("Version:"),
							 mod.version.toHtmlEscaped());
			}

			if (!mod.license.isEmpty()) {
				html +=
					QString("<tr><td class=\"label\">%1</td><td>%2</td></tr>")
						.arg(QObject::tr("License:"),
							 mod.license.toHtmlEscaped());
			}

			if (!mod.description.isEmpty()) {
				html +=
					QString("<tr><td class=\"label\">%1</td><td>%2</td></tr>")
						.arg(QObject::tr("Description:"),
							 mod.description.toHtmlEscaped());
			}

			if (!mod.codeLink.isEmpty()) {
				if (mod.codeLink.startsWith(QLatin1String("http://")) ||
					mod.codeLink.startsWith(QLatin1String("https://"))) {
					html += QString("<tr><td class=\"label\">%1</td><td><a "
									"href=\"%2\">%2</a></td></tr>")
								.arg(QObject::tr("Source Code:"),
									 mod.codeLink.toHtmlEscaped());
				} else {
					html +=
						QString(
							"<tr><td class=\"label\">%1</td><td>%2</td></tr>")
							.arg(QObject::tr("Source Code:"),
								 mod.codeLink.toHtmlEscaped());
				}
			}

			html += QLatin1String("</table>");
		}
		return html;
	}

} // namespace

AboutDialog::AboutDialog(QWidget* parent)
	: QDialog(parent), ui(new Ui::AboutDialog)
{
	ui->setupUi(this);

	QString launcherName = BuildConfig.MESHMC_DISPLAYNAME;

	setWindowTitle(tr("About %1").arg(launcherName));

	QString chtml = getCreditsHtml();
	ui->creditsText->setHtml(Strings::htmlListPatch(chtml));

	QString lhtml = getLicenseHtml();
	ui->licenseText->setHtml(Strings::htmlListPatch(lhtml));

	QString phtml = getPluginsHtml();
	ui->pluginsText->setHtml(phtml);

	ui->urlLabel->setOpenExternalLinks(true);

	ui->icon->setPixmap(APPLICATION->getThemedIcon("logo").pixmap(64));
	ui->title->setText(launcherName);

	ui->versionLabel->setText(BuildConfig.printableVersionString());

	if (!BuildConfig.BUILD_PLATFORM.isEmpty())
		ui->platformLabel->setText(tr("Platform") + ": " +
								   BuildConfig.BUILD_PLATFORM);
	else
		ui->platformLabel->setVisible(false);

	if (!BuildConfig.GIT_COMMIT.isEmpty())
		ui->commitLabel->setText(tr("Commit: %1").arg(BuildConfig.GIT_COMMIT));
	else
		ui->commitLabel->setVisible(false);

	if (!BuildConfig.BUILD_DATE.isEmpty())
		ui->buildDateLabel->setText(
			tr("Build date: %1").arg(BuildConfig.BUILD_DATE));
	else
		ui->buildDateLabel->setVisible(false);

	if (!BuildConfig.VERSION_CHANNEL.isEmpty())
		ui->channelLabel->setText(tr("Channel") + ": " +
								  BuildConfig.VERSION_CHANNEL);
	else
		ui->channelLabel->setVisible(false);

	QString urlText(
		"<html><head/><body><p><a href=\"%1\">%1</a></p></body></html>");
	ui->urlLabel->setText(urlText.arg(BuildConfig.MESHMC_GIT));

	QString copyText("© 2026 %1");
	ui->copyLabel->setText(copyText.arg(BuildConfig.MESHMC_COPYRIGHT));

	connect(ui->closeButton, &QPushButton::clicked, this, &AboutDialog::close);

	connect(ui->aboutQt, &QPushButton::clicked, &QApplication::aboutQt);
}

AboutDialog::~AboutDialog()
{
	delete ui;
}
