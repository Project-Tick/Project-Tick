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

#include "GreenDarkTheme.h"

#include <QObject>

QString GreenDarkTheme::id()
{
	return "green-dark";
}

QString GreenDarkTheme::name()
{
	return QObject::tr("Green Dark");
}

QString GreenDarkTheme::tooltip()
{
	return QObject::tr("A dark Fusion-based theme with green accents");
}

bool GreenDarkTheme::hasColorScheme()
{
	return true;
}

QPalette GreenDarkTheme::colorScheme()
{
	QPalette palette;
	palette.setColor(QPalette::Window, QColor(49, 49, 49));
	palette.setColor(QPalette::WindowText, Qt::white);
	palette.setColor(QPalette::Base, QColor(34, 34, 34));
	palette.setColor(QPalette::AlternateBase, QColor(49, 49, 49));
	palette.setColor(QPalette::ToolTipBase, Qt::white);
	palette.setColor(QPalette::ToolTipText, Qt::white);
	palette.setColor(QPalette::Text, Qt::white);
	palette.setColor(QPalette::Button, QColor(49, 49, 49));
	palette.setColor(QPalette::ButtonText, Qt::white);
	palette.setColor(QPalette::BrightText, Qt::red);
	palette.setColor(QPalette::Link, QColor(47, 163, 198));
	palette.setColor(QPalette::Highlight, QColor(150, 219, 89));
	palette.setColor(QPalette::HighlightedText, Qt::black);
	palette.setColor(QPalette::PlaceholderText, Qt::darkGray);
	return fadeInactive(palette, fadeAmount(), fadeColor());
}

double GreenDarkTheme::fadeAmount()
{
	return 0.5;
}

QColor GreenDarkTheme::fadeColor()
{
	return QColor(49, 49, 49);
}

bool GreenDarkTheme::hasStyleSheet()
{
	return true;
}

QString GreenDarkTheme::appStyleSheet()
{
	return "QToolTip { color: #ffffff; background-color: #2fa3c6; border: 1px "
		   "solid white; }";
}
