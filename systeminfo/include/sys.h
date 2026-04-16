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

#pragma once
#include <QString>

namespace Sys
{
	const uint64_t mebibyte = 1024ull * 1024ull;

	enum class KernelType { Undetermined, Windows, Darwin, Linux };

	struct KernelInfo {
		QString kernelName;
		QString kernelVersion;

		KernelType kernelType = KernelType::Undetermined;
		int kernelMajor = 0;
		int kernelMinor = 0;
		int kernelPatch = 0;
		bool isCursed = false;
	};

	KernelInfo getKernelInfo();

	struct DistributionInfo {
		DistributionInfo operator+(const DistributionInfo& rhs) const
		{
			DistributionInfo out;
			if (!distributionName.isEmpty()) {
				out.distributionName = distributionName;
			} else {
				out.distributionName = rhs.distributionName;
			}
			if (!distributionVersion.isEmpty()) {
				out.distributionVersion = distributionVersion;
			} else {
				out.distributionVersion = rhs.distributionVersion;
			}
			return out;
		}
		QString distributionName;
		QString distributionVersion;
	};

	DistributionInfo getDistributionInfo();

	uint64_t getSystemRam();

	bool isSystem64bit();

	bool isCPU64bit();
} // namespace Sys
