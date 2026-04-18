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

#include "JavaWizardPage.h"
#include "Application.h"

#include <QVBoxLayout>
#include <QGroupBox>
#include <QSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QFileDialog>
#include <QComboBox>

#include <sys.h>

#include "FileSystem.h"
#include "java/JavaInstall.h"
#include "java/JavaUtils.h"
#include "JavaCommon.h"
#include "java/download/JavaRuntime.h"

#include "ui/widgets/VersionSelectWidget.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/widgets/JavaSettingsWidget.h"

JavaWizardPage::JavaWizardPage(QWidget* parent) : BaseWizardPage(parent)
{
	setupUi();
}

void JavaWizardPage::setupUi()
{
	setObjectName(QStringLiteral("javaPage"));
	QVBoxLayout* layout = new QVBoxLayout(this);

	// Java auto-download vendor selection
	auto vendorGroupBox = new QGroupBox(this);
	vendorGroupBox->setObjectName(QStringLiteral("vendorGroupBox"));
	auto vendorLayout = new QHBoxLayout(vendorGroupBox);

	m_vendorLabel = new QLabel(vendorGroupBox);
	vendorLayout->addWidget(m_vendorLabel);

	m_vendorComboBox = new QComboBox(vendorGroupBox);
	m_vendorComboBox->setObjectName(QStringLiteral("vendorComboBox"));
	auto providers = JavaDownload::JavaProviderInfo::availableProviders();
	for (const auto& provider : providers) {
		m_vendorComboBox->addItem(provider.name, provider.uid);
	}
	vendorLayout->addWidget(m_vendorComboBox);

	layout->addWidget(vendorGroupBox);

	m_java_widget = new JavaSettingsWidget(this);
	layout->addWidget(m_java_widget);
	setLayout(layout);

	retranslate();
}

void JavaWizardPage::refresh()
{
	m_java_widget->refresh();
}

void JavaWizardPage::initializePage()
{
	m_java_widget->initialize();

	// Set vendor combo to current setting
	auto currentVendor =
		APPLICATION->settings()->get("JavaAutoDownloadVendor").toString();
	int idx = m_vendorComboBox->findData(currentVendor);
	if (idx >= 0) {
		m_vendorComboBox->setCurrentIndex(idx);
	}
}

bool JavaWizardPage::wantsRefreshButton()
{
	return true;
}

bool JavaWizardPage::validatePage()
{
	auto settings = APPLICATION->settings();

	// Save selected vendor
	settings->set("JavaAutoDownloadVendor",
				  m_vendorComboBox->currentData().toString());

	auto result = m_java_widget->validate();
	switch (result) {
		default:
		case JavaSettingsWidget::ValidationStatus::Bad: {
			return false;
		}
		case JavaSettingsWidget::ValidationStatus::AllOK: {
			settings->set("JavaPath", m_java_widget->javaPath());
		}
		case JavaSettingsWidget::ValidationStatus::JavaBad: {
			// Memory
			auto s = APPLICATION->settings();
			s->set("MinMemAlloc", m_java_widget->minHeapSize());
			s->set("MaxMemAlloc", m_java_widget->maxHeapSize());
			if (m_java_widget->permGenEnabled()) {
				s->set("PermGen", m_java_widget->permGenSize());
			} else {
				s->reset("PermGen");
			}
			return true;
		}
	}
}

void JavaWizardPage::retranslate()
{
	setTitle(tr("Java"));
	setSubTitle(tr(
		"You do not have a working Java set up yet or it went missing.\n"
		"Please select one of the following or browse for a java executable.\n"
		"You can also choose which vendor to use for automatic Java downloads."));
	m_vendorLabel->setText(tr("Auto-download Java from:"));
	m_java_widget->retranslate();
}
