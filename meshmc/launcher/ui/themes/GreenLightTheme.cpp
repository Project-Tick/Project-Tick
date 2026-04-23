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

#include "GreenLightTheme.h"

#include <QObject>

QString GreenLightTheme::id()
{
	return "green-light";
}

QString GreenLightTheme::name()
{
	return QObject::tr("Green Light");
}

QString GreenLightTheme::tooltip()
{
	return QObject::tr("A bright Fusion-based theme with green accents");
}

bool GreenLightTheme::hasColorScheme()
{
	return true;
}

QPalette GreenLightTheme::colorScheme()
{
	QPalette palette;
	palette.setColor(QPalette::Window, QColor(255, 255, 255));
	palette.setColor(QPalette::WindowText, QColor(49, 49, 49));
	palette.setColor(QPalette::Base, QColor(250, 250, 250));
	palette.setColor(QPalette::AlternateBase, QColor(239, 240, 241));
	palette.setColor(QPalette::ToolTipBase, QColor(49, 49, 49));
	palette.setColor(QPalette::ToolTipText, QColor(239, 240, 241));
	palette.setColor(QPalette::Text, QColor(49, 49, 49));
	palette.setColor(QPalette::Button, QColor(255, 255, 255));
	palette.setColor(QPalette::ButtonText, QColor(49, 49, 49));
	palette.setColor(QPalette::BrightText, Qt::red);
	palette.setColor(QPalette::Link, QColor(37, 137, 164));
	palette.setColor(QPalette::Highlight, QColor(137, 207, 84));
	palette.setColor(QPalette::HighlightedText, QColor(239, 240, 241));
	return fadeInactive(palette, fadeAmount(), fadeColor());
}

double GreenLightTheme::fadeAmount()
{
	return 0.5;
}

QColor GreenLightTheme::fadeColor()
{
	return QColor(255, 255, 255);
}

bool GreenLightTheme::hasStyleSheet()
{
	return false;
}

QString GreenLightTheme::appStyleSheet()
{
	return QString();
}
