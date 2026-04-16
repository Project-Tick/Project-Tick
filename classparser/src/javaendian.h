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
#include <stdint.h>

/**
 * Swap bytes between big endian and local number representation
 */
namespace util
{
#ifdef MULTIMC_BIG_ENDIAN
	inline uint64_t bigswap(uint64_t x)
	{
		return x;
	}

	inline uint32_t bigswap(uint32_t x)
	{
		return x;
	}

	inline uint16_t bigswap(uint16_t x)
	{
		return x;
	}

#else
	inline uint64_t bigswap(uint64_t x)
	{
		return (x >> 56) | ((x << 40) & 0x00FF000000000000) |
			   ((x << 24) & 0x0000FF0000000000) |
			   ((x << 8) & 0x000000FF00000000) |
			   ((x >> 8) & 0x00000000FF000000) |
			   ((x >> 24) & 0x0000000000FF0000) |
			   ((x >> 40) & 0x000000000000FF00) | (x << 56);
	}

	inline uint32_t bigswap(uint32_t x)
	{
		return (x >> 24) | ((x << 8) & 0x00FF0000) | ((x >> 8) & 0x0000FF00) |
			   (x << 24);
	}

	inline uint16_t bigswap(uint16_t x)
	{
		return (x >> 8) | (x << 8);
	}

#endif

	inline int64_t bigswap(int64_t x)
	{
		return static_cast<int64_t>(bigswap(static_cast<uint64_t>(x)));
	}

	inline int32_t bigswap(int32_t x)
	{
		return static_cast<int32_t>(bigswap(static_cast<uint32_t>(x)));
	}

	inline int16_t bigswap(int16_t x)
	{
		return static_cast<int16_t>(bigswap(static_cast<uint16_t>(x)));
	}
} // namespace util
