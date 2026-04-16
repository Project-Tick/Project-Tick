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

#include <sys/utsname.h>

#include <QString>
#include <QStringList>
#include <QDebug>

Sys::KernelInfo Sys::getKernelInfo()
{
	Sys::KernelInfo out;
	struct utsname buf;
	uname(&buf);
	out.kernelType = KernelType::Darwin;
	out.kernelName = buf.sysname;
	QString release = out.kernelVersion = buf.release;

	// TODO: figure out how to detect cursed-ness (macOS emulated on linux via
	// mad hacks and so on)
	out.isCursed = false;

	out.kernelMajor = 0;
	out.kernelMinor = 0;
	out.kernelPatch = 0;
	auto sections = release.split('-');
	if (sections.size() >= 1) {
		auto versionParts = sections[0].split('.');
		if (versionParts.size() >= 3) {
			out.kernelMajor = versionParts[0].toInt();
			out.kernelMinor = versionParts[1].toInt();
			out.kernelPatch = versionParts[2].toInt();
		} else {
			qWarning() << "Not enough version numbers in " << sections[0]
					   << " found " << versionParts.size();
		}
	} else {
		qWarning() << "Not enough '-' sections in " << release << " found "
				   << sections.size();
	}
	return out;
}

#include <sys/sysctl.h>

uint64_t Sys::getSystemRam()
{
	uint64_t memsize;
	size_t memsizesize = sizeof(memsize);
	if (!sysctlbyname("hw.memsize", &memsize, &memsizesize, NULL, 0)) {
		return memsize;
	} else {
		return 0;
	}
}

bool Sys::isCPU64bit()
{
	// not even going to pretend I'm going to support anything else
	return true;
}

bool Sys::isSystem64bit()
{
	// yep. maybe when we have 128bit CPUs on consumer devices.
	return true;
}

Sys::DistributionInfo Sys::getDistributionInfo()
{
	DistributionInfo result;
	return result;
}
