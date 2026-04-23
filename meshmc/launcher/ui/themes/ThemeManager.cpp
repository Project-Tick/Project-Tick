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

#include "ThemeManager.h"
#include "ITheme.h"
#include "SystemTheme.h"
#include "DarkTheme.h"
#include "BrightTheme.h"
#include "GreenDarkTheme.h"
#include "GreenLightTheme.h"
#include "CustomTheme.h"
#include "CatPack.h"

#include "Application.h"
#include "Exception.h"
#include <QApplication>
#include <QDebug>
#include <QDirIterator>
#include <QImageReader>
#include <QSet>
#include <QStyle>
#include <QStyleFactory>
#include <QSysInfo>
#include <xdgicon.h>

ThemeManager::ThemeManager()
{
	const auto& style = QApplication::style();
	m_defaultStyle = style->objectName();
	m_defaultPalette = QApplication::palette();

	// Default "System" theme
	addTheme(
		std::make_unique<SystemTheme>(m_defaultStyle, m_defaultPalette, true));

	// Built-in Fusion themes
	auto darkTheme = new DarkTheme();
	addTheme(std::unique_ptr<ITheme>(darkTheme));
	addTheme(std::make_unique<BrightTheme>());
	addTheme(std::make_unique<GreenDarkTheme>());
	addTheme(std::make_unique<GreenLightTheme>());

	// System widget themes from QStyleFactory
	QStringList styles = QStyleFactory::keys();
	for (auto& st : styles) {
#ifdef Q_OS_WINDOWS
		if (QSysInfo::productVersion() != "11" && st == "windows11") {
			continue;
		}
#endif
		addTheme(std::make_unique<SystemTheme>(st, m_defaultPalette, false));
	}

	// Custom theme
	addTheme(std::make_unique<CustomTheme>(darkTheme, "custom"));

	initIconThemes();
	initializeCatPacks();
}

void ThemeManager::addTheme(std::unique_ptr<ITheme> theme)
{
	QString id = theme->id();
	m_themes.insert(std::make_pair(id, std::move(theme)));
}

ITheme* ThemeManager::getTheme(const QString& id)
{
	auto it = m_themes.find(id);
	if (it != m_themes.end()) {
		return it->second.get();
	}
	return nullptr;
}

void ThemeManager::setApplicationTheme(const QString& id, bool initial)
{
	auto theme = getTheme(id);
	if (theme) {
		theme->apply(initial);
	} else {
		qWarning() << "Tried to set invalid theme:" << id;
	}
}

void ThemeManager::setIconTheme(const QString& name)
{
	XdgIcon::setThemeName(name);
}

void ThemeManager::applyCurrentlySelectedTheme(bool initial)
{
	auto settings = APPLICATION->settings();

	// Apply widget theme first (sets palette)
	auto applicationTheme = settings->get("ApplicationTheme").toString();
	if (applicationTheme.isEmpty()) {
		applicationTheme = "system";
	}
	setApplicationTheme(applicationTheme, initial);

	// Auto-resolve icon variant based on the now-active palette brightness
	auto iconTheme = settings->get("IconTheme").toString();
	if (!iconTheme.isEmpty()) {
		auto resolved = bestIconThemeForPalette(iconTheme);
		if (resolved != iconTheme) {
			settings->set("IconTheme", resolved);
		}
		setIconTheme(resolved);
	}
}

std::vector<ITheme*> ThemeManager::allThemes()
{
	std::vector<ITheme*> ret;
	for (auto& pair : m_themes) {
		ret.push_back(pair.second.get());
	}
	return ret;
}

QStringList ThemeManager::families()
{
	QStringList ret;
	QSet<QString> seen;
	for (auto& pair : m_themes) {
		QString fam = pair.second->family();
		if (!seen.contains(fam)) {
			seen.insert(fam);
			ret.append(fam);
		}
	}
	return ret;
}

std::vector<ITheme*> ThemeManager::themesInFamily(const QString& family)
{
	std::vector<ITheme*> ret;
	for (auto& pair : m_themes) {
		if (pair.second->family() == family) {
			ret.push_back(pair.second.get());
		}
	}
	return ret;
}

QList<IconThemeEntry> ThemeManager::iconThemes() const
{
	return m_iconThemes;
}

QStringList ThemeManager::iconThemeFamilies() const
{
	QStringList ret;
	QSet<QString> seen;
	for (const auto& entry : m_iconThemes) {
		const QString& fam = entry.family;
		if (!seen.contains(fam)) {
			seen.insert(fam);
			ret.append(fam);
		}
	}
	return ret;
}

QList<IconThemeEntry>
ThemeManager::iconThemesInFamily(const QString& family) const
{
	QList<IconThemeEntry> ret;
	for (const auto& entry : m_iconThemes) {
		if (entry.family == family) {
			ret.append(entry);
		}
	}
	return ret;
}

QString ThemeManager::resolveIconTheme(const QString& family) const
{
	auto entries = iconThemesInFamily(family);
	if (entries.size() <= 1) {
		return entries.isEmpty() ? QString() : entries[0].id;
	}

	// Check if family has variants
	bool hasVariants = false;
	for (const auto& entry : entries) {
		if (!entry.variant.isEmpty()) {
			hasVariants = true;
			break;
		}
	}

	if (!hasVariants) {
		return entries[0].id;
	}

	// Auto-detect based on current palette brightness
	auto windowColor = QApplication::palette().color(QPalette::Window);
	bool isDark = windowColor.lightnessF() < 0.5;

	for (const auto& entry : entries) {
		QString v = entry.variant.toLower();
		if (isDark && v == "dark")
			return entry.id;
		if (!isDark && v == "light")
			return entry.id;
	}

	return entries[0].id;
}

QString
ThemeManager::bestIconThemeForPalette(const QString& currentIconId) const
{
	// Find the family of the current icon theme
	QString family;
	for (const auto& entry : m_iconThemes) {
		if (entry.id == currentIconId) {
			family = entry.family;
			break;
		}
	}

	if (family.isEmpty()) {
		return currentIconId;
	}

	// Resolve the best variant for that family based on current palette
	QString resolved = resolveIconTheme(family);
	return resolved.isEmpty() ? currentIconId : resolved;
}

void ThemeManager::initIconThemes()
{
	m_iconThemes = {
		{"pe_colored", QObject::tr("Default"), QObject::tr("Default"),
		 QString()},
		{"multimc", QStringLiteral("MultiMC"), QStringLiteral("MultiMC"),
		 QString()},
		{"pe_dark", QObject::tr("Simple (Dark Icons)"),
		 QObject::tr("Simple (Dark Icons)"), QString()},
		{"pe_light", QObject::tr("Simple (Light Icons)"),
		 QObject::tr("Simple (Light Icons)"), QString()},
		{"pe_blue", QObject::tr("Simple (Blue Icons)"),
		 QObject::tr("Simple (Blue Icons)"), QString()},
		{"pe_colored", QObject::tr("Simple (Colored Icons)"),
		 QObject::tr("Simple (Colored Icons)"), QString()},
		{"OSX", QStringLiteral("OSX"), QStringLiteral("OSX"), QString()},
		{"iOS", QStringLiteral("iOS"), QStringLiteral("iOS"), QString()},
		{"flat", QObject::tr("Flat (Light)"), QStringLiteral("Flat"),
		 QObject::tr("Light")},
		{"flat_white", QObject::tr("Flat (Dark)"), QStringLiteral("Flat"),
		 QObject::tr("Dark")},
		{"breeze_dark", QObject::tr("Breeze (Dark)"), QStringLiteral("Breeze"),
		 QObject::tr("Dark")},
		{"breeze_light", QObject::tr("Breeze (Light)"),
		 QStringLiteral("Breeze"), QObject::tr("Light")},
		{"custom", QObject::tr("Custom"), QObject::tr("Custom"), QString()},
	};
}

void ThemeManager::initializeCatPacks()
{
	QList<std::pair<QString, QString>> defaultCats{
		{"kitteh", QObject::tr("Background Cat (from MultiMC)")},
		{"rory", QObject::tr("Rory ID 11 (drawn by Ashtaka)")},
		{"rory-flat",
		 QObject::tr("Rory ID 11 (flat edition, drawn by Ashtaka)")},
		{"teawie", QObject::tr("Teawie (drawn by SympathyTea)")}};

	for (const auto& [id, name] : defaultCats) {
		addCatPack(std::make_unique<BasicCatPack>(id, name));
	}

	// Create catpacks folder in data directory
	m_catPacksFolder = QDir("catpacks");
	if (!m_catPacksFolder.mkpath("."))
		qWarning() << "Couldn't create catpacks folder";

	QStringList supportedImageFormats;
	for (const auto& format : QImageReader::supportedImageFormats()) {
		supportedImageFormats.append("*." + format);
	}

	auto loadFiles = [this, &supportedImageFormats](const QDir& dir) {
		QDirIterator it(dir.absolutePath(), supportedImageFormats, QDir::Files);
		while (it.hasNext()) {
			QFileInfo info(it.next());
			addCatPack(std::make_unique<FileCatPack>(info));
		}
	};

	// Load image files in catpacks folder root
	loadFiles(m_catPacksFolder);

	// Load subdirectories
	QDirIterator directoryIterator(m_catPacksFolder.path(),
								   QDir::Dirs | QDir::NoDotAndDotDot);
	while (directoryIterator.hasNext()) {
		QDir dir(directoryIterator.next());
		QFileInfo manifest(dir.absoluteFilePath("catpack.json"));

		if (manifest.isFile()) {
			try {
				addCatPack(std::make_unique<JsonCatPack>(manifest));
			} catch (const Exception& e) {
				qWarning() << "Couldn't load catpack json:" << e.cause();
			}
		} else {
			loadFiles(dir);
		}
	}
}

void ThemeManager::addCatPack(std::unique_ptr<CatPack> catPack)
{
	QString id = catPack->id();
	if (m_catPacks.find(id) == m_catPacks.end())
		m_catPacks.emplace(id, std::move(catPack));
	else
		qWarning() << "CatPack" << id << "not added to prevent id duplication";
}

QString ThemeManager::getCatPack(const QString& catName)
{
	QString id = catName.isEmpty()
					 ? APPLICATION->settings()->get("BackgroundCat").toString()
					 : catName;

	auto it = m_catPacks.find(id);
	if (it != m_catPacks.end())
		return it->second->path();

	// Fallback to first available
	if (!m_catPacks.empty())
		return m_catPacks.begin()->second->path();

	return QString();
}

QList<CatPack*> ThemeManager::getValidCatPacks()
{
	QList<CatPack*> ret;
	ret.reserve(m_catPacks.size());
	for (auto& [id, pack] : m_catPacks) {
		ret.append(pack.get());
	}
	return ret;
}

QDir ThemeManager::getCatPacksFolder()
{
	return m_catPacksFolder;
}
