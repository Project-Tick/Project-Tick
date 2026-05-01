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

#include "PluginsDialog.h"
#include "ui_PluginsDialog.h"
#include <QIcon>
#include "Application.h"
#include "BuildConfig.h"
#include "plugin/PluginManager.h"

namespace {

	QString getPluginsHtml()
	{
		auto* pm = APPLICATION->pluginManager();

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

} //namespace

PluginsDialog::PluginsDialog(QWidget* parent)
	: QDialog(parent), ui(new Ui::PluginsDialog)
{
	ui->setupUi(this);

	QString phtml = getPluginsHtml();
	ui->pluginsText->setHtml(phtml);

	connect(ui->closeButton, &QPushButton::clicked, this, &PluginsDialog::close);
}

PluginsDialog::~PluginsDialog()
{
	delete ui;
}
