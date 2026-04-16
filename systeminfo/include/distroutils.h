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

#include "sys.h"
#include <QString>

namespace Sys
{
	struct LsbInfo {
		QString distributor;
		QString version;
		QString description;
		QString codename;
	};

	bool main_lsb_info(LsbInfo& out);
	bool fallback_lsb_info(Sys::LsbInfo& out);
	void lsb_postprocess(Sys::LsbInfo& lsb, Sys::DistributionInfo& out);
	Sys::DistributionInfo read_lsb_release();

	QString _extract_distribution(const QString& x);
	QString _extract_version(const QString& x);
	Sys::DistributionInfo read_legacy_release();

	Sys::DistributionInfo read_os_release();
} // namespace Sys
