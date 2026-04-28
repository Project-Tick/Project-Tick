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

#include "AnalyticsWizardPage.h"
#include <Application.h>

#include <QVBoxLayout>
#include <QTextBrowser>
#include <QCheckBox>

#include <ganalytics.h>
#include <BuildConfig.h>

AnalyticsWizardPage::AnalyticsWizardPage(QWidget* parent)
	: BaseWizardPage(parent)
{
	setObjectName(QStringLiteral("analyticsPage"));
	verticalLayout_3 = new QVBoxLayout(this);
	verticalLayout_3->setObjectName(QStringLiteral("verticalLayout_3"));
	textBrowser = new QTextBrowser(this);
	textBrowser->setObjectName(QStringLiteral("textBrowser"));
	textBrowser->setAcceptRichText(false);
	textBrowser->setOpenExternalLinks(true);
	verticalLayout_3->addWidget(textBrowser);

	checkBox = new QCheckBox(this);
	checkBox->setObjectName(QStringLiteral("checkBox"));
	checkBox->setChecked(false);
	verticalLayout_3->addWidget(checkBox);
	retranslate();
}

AnalyticsWizardPage::~AnalyticsWizardPage() {}

bool AnalyticsWizardPage::validatePage()
{
	auto settings = APPLICATION->settings();
	auto analytics = APPLICATION->analytics();
	auto status = checkBox->isChecked();
	settings->set("AnalyticsSeen", analytics->version());
	settings->set("Analytics", status);
	return true;
}

void AnalyticsWizardPage::retranslate()
{
	setTitle(tr("Analytics"));
	setSubTitle(tr("We track some anonymous statistics about users."));
	textBrowser->setHtml(
		tr("<html><body>"
		   "<p>MeshMC sends anonymous usage statistics on every start of the "
		   "application. This helps us decide what platforms and issues to "
		   "focus on.</p>"
		   "<p>The data is processed by Google Analytics, see their <a "
		   "href=\"https://support.google.com/analytics/answer/"
		   "6004245?hl=en\">article on the "
		   "matter</a>.</p>"
		   "<p>The following data is collected:</p>"
		   "<ul><li>A random unique ID of the installation.<br />It is stored "
		   "in the application settings file.</li>"
		   "<li>Anonymized (partial) IP address.</li>"
		   "<li>MeshMC version.</li>"
		   "<li>Operating system name, version and architecture.</li>"
		   "<li>CPU architecture (kernel architecture on linux).</li>"
		   "<li>Size of system memory.</li>"
		   "<li>Java version, architecture and memory settings.</li></ul>"
		   "<p>If we change the tracked information, you will see this page "
		   "again.</p></body></html>"));
	checkBox->setText(tr("Enable Analytics"));
}
