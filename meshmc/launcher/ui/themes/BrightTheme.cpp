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

#include "BrightTheme.h"

#include <QObject>

QString BrightTheme::id()
{
	return "bright";
}

QString BrightTheme::name()
{
	return QObject::tr("Bright");
}

QString BrightTheme::tooltip()
{
	return QObject::tr("A bright Fusion-based theme with blue accents");
}

bool BrightTheme::hasColorScheme()
{
	return true;
}

QPalette BrightTheme::colorScheme()
{
	QPalette brightPalette;
	brightPalette.setColor(QPalette::Window, QColor(239, 240, 241));
	brightPalette.setColor(QPalette::WindowText, QColor(49, 54, 59));
	brightPalette.setColor(QPalette::Base, QColor(252, 252, 252));
	brightPalette.setColor(QPalette::AlternateBase, QColor(239, 240, 241));
	brightPalette.setColor(QPalette::ToolTipBase, QColor(49, 54, 59));
	brightPalette.setColor(QPalette::ToolTipText, QColor(239, 240, 241));
	brightPalette.setColor(QPalette::Text, QColor(49, 54, 59));
	brightPalette.setColor(QPalette::Button, QColor(239, 240, 241));
	brightPalette.setColor(QPalette::ButtonText, QColor(49, 54, 59));
	brightPalette.setColor(QPalette::BrightText, Qt::red);
	brightPalette.setColor(QPalette::Link, QColor(41, 128, 185));
	brightPalette.setColor(QPalette::Highlight, QColor(61, 174, 233));
	brightPalette.setColor(QPalette::HighlightedText, QColor(239, 240, 241));
	return fadeInactive(brightPalette, fadeAmount(), fadeColor());
}

double BrightTheme::fadeAmount()
{
	return 0.5;
}

QColor BrightTheme::fadeColor()
{
	return QColor(239, 240, 241);
}

bool BrightTheme::hasStyleSheet()
{
	return false;
}

QString BrightTheme::appStyleSheet()
{
	return QString();
}
