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

#include <windows.h>

#include <QOperatingSystemVersion>

Sys::KernelInfo Sys::getKernelInfo()
{
	Sys::KernelInfo out;
	out.kernelType = KernelType::Windows;
	out.kernelName = "Windows";
	const auto osVersion = QOperatingSystemVersion::current();
	out.kernelMajor = osVersion.majorVersion();
	out.kernelMinor = osVersion.minorVersion();
	out.kernelPatch = osVersion.microVersion();
	out.kernelVersion = QString("%1.%2.%3")
							.arg(out.kernelMajor)
							.arg(out.kernelMinor)
							.arg(out.kernelPatch);
	return out;
}

uint64_t Sys::getSystemRam()
{
	MEMORYSTATUSEX status;
	status.dwLength = sizeof(status);
	GlobalMemoryStatusEx(&status);
	// bytes
	return (uint64_t)status.ullTotalPhys;
}

bool Sys::isSystem64bit()
{
#if defined(_WIN64)
	return true;
#elif defined(_WIN32)
	BOOL f64 = false;
	return IsWow64Process(GetCurrentProcess(), &f64) && f64;
#else
	// it's some other kind of system...
	return false;
#endif
}

bool Sys::isCPU64bit()
{
	SYSTEM_INFO info;
	ZeroMemory(&info, sizeof(SYSTEM_INFO));
	GetNativeSystemInfo(&info);
	auto arch = info.wProcessorArchitecture;
	return arch == PROCESSOR_ARCHITECTURE_AMD64 ||
		   arch == PROCESSOR_ARCHITECTURE_IA64;
}

Sys::DistributionInfo Sys::getDistributionInfo()
{
	DistributionInfo result;
	return result;
}
