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

#include "LanguageSelectionWidget.h"

#include <QVBoxLayout>
#include <QTreeView>
#include <QHeaderView>
#include <QLabel>
#include "Application.h"
#include "translations/TranslationsModel.h"

LanguageSelectionWidget::LanguageSelectionWidget(QWidget* parent)
	: QWidget(parent)
{
	verticalLayout = new QVBoxLayout(this);
	verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
	languageView = new QTreeView(this);
	languageView->setObjectName(QStringLiteral("languageView"));
	languageView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	languageView->setAlternatingRowColors(true);
	languageView->setRootIsDecorated(false);
	languageView->setItemsExpandable(false);
	languageView->setWordWrap(true);
	languageView->header()->setCascadingSectionResizes(true);
	languageView->header()->setStretchLastSection(false);
	verticalLayout->addWidget(languageView);
	helpUsLabel = new QLabel(this);
	helpUsLabel->setObjectName(QStringLiteral("helpUsLabel"));
	helpUsLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
	helpUsLabel->setOpenExternalLinks(true);
	helpUsLabel->setWordWrap(true);
	verticalLayout->addWidget(helpUsLabel);

	auto translations = APPLICATION->translations();
	auto index = translations->selectedIndex();
	languageView->setModel(translations.get());
	languageView->setCurrentIndex(index);
	languageView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
	languageView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	connect(languageView->selectionModel(),
			&QItemSelectionModel::currentRowChanged, this,
			&LanguageSelectionWidget::languageRowChanged);
	verticalLayout->setContentsMargins(0, 0, 0, 0);
}

QString LanguageSelectionWidget::getSelectedLanguageKey() const
{
	auto translations = APPLICATION->translations();
	return translations->data(languageView->currentIndex(), Qt::UserRole)
		.toString();
}

void LanguageSelectionWidget::retranslate()
{
	QString text = tr("Don't see your language or the quality is poor?<br/><a "
					  "href=\"%1\">Help us with translations!</a>")
					   .arg("https://github.com/Project-Tick/Project-Tick/wiki/"
							"Translating-MeshMC");
	helpUsLabel->setText(text);
}

void LanguageSelectionWidget::languageRowChanged(const QModelIndex& current,
												 const QModelIndex& previous)
{
	if (current == previous) {
		return;
	}
	auto translations = APPLICATION->translations();
	QString key = translations->data(current, Qt::UserRole).toString();
	translations->selectLanguage(key);
	translations->updateLanguage(key);
}
