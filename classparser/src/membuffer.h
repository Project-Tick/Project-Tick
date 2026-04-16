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
#include <string>
#include <vector>
#include <exception>
#include "javaendian.h"

namespace util
{
	class membuffer
	{
	  public:
		membuffer(char* buffer, std::size_t size)
		{
			current = start = buffer;
			end = start + size;
		}
		~membuffer()
		{
			// maybe? possibly? left out to avoid confusion. for now.
			// delete start;
		}
		/**
		 * Read some value. That's all ;)
		 */
		template <class T> void read(T& val)
		{
			val = *(T*)current;
			current += sizeof(T);
		}
		/**
		 * Read a big-endian number
		 * valid for 2-byte, 4-byte and 8-byte variables
		 */
		template <class T> void read_be(T& val)
		{
			val = util::bigswap(*(T*)current);
			current += sizeof(T);
		}
		/**
		 * Read a string in the format:
		 * 2B length (big endian, unsigned)
		 * length bytes data
		 */
		void read_jstr(std::string& str)
		{
			uint16_t length = 0;
			read_be(length);
			str.append(current, length);
			current += length;
		}
		/**
		 * Skip N bytes
		 */
		void skip(std::size_t N)
		{
			current += N;
		}

	  private:
		char *start, *end, *current;
	};
} // namespace util
