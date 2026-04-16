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

#include "GZip.h"
#include <neozip.h>
#include <QByteArray>

bool GZip::unzip(const QByteArray& compressedBytes,
				 QByteArray& uncompressedBytes)
{
	if (compressedBytes.size() == 0) {
		uncompressedBytes = compressedBytes;
		return true;
	}

	unsigned uncompLength = compressedBytes.size();
	uncompressedBytes.clear();
	uncompressedBytes.resize(uncompLength);

	zng_stream strm;
	memset(&strm, 0, sizeof(strm));
	strm.next_in = (const uint8_t*)compressedBytes.data();
	strm.avail_in = compressedBytes.size();

	bool done = false;

	if (zng_inflateInit2(&strm, (16 + MAX_WBITS)) != Z_OK) {
		return false;
	}

	int err = Z_OK;

	while (!done) {
		// If our output buffer is too small
		if (strm.total_out >= uncompLength) {
			uncompressedBytes.resize(uncompLength * 2);
			uncompLength *= 2;
		}

		strm.next_out = (uint8_t*)(uncompressedBytes.data() + strm.total_out);
		strm.avail_out = uncompLength - strm.total_out;

		// Inflate another chunk.
		err = zng_inflate(&strm, Z_SYNC_FLUSH);
		if (err == Z_STREAM_END)
			done = true;
		else if (err != Z_OK) {
			break;
		}
	}

	if (zng_inflateEnd(&strm) != Z_OK || !done) {
		return false;
	}

	uncompressedBytes.resize(strm.total_out);
	return true;
}

bool GZip::zip(const QByteArray& uncompressedBytes, QByteArray& compressedBytes)
{
	if (uncompressedBytes.size() == 0) {
		compressedBytes = uncompressedBytes;
		return true;
	}

	unsigned compLength =
		std::min(static_cast<qsizetype>(16), uncompressedBytes.size());
	compressedBytes.clear();
	compressedBytes.resize(compLength);

	zng_stream zs;
	memset(&zs, 0, sizeof(zs));

	if (zng_deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, (16 + MAX_WBITS),
					 8, Z_DEFAULT_STRATEGY) != Z_OK) {
		return false;
	}

	zs.next_in = (const uint8_t*)uncompressedBytes.data();
	zs.avail_in = uncompressedBytes.size();

	int ret;
	compressedBytes.resize(uncompressedBytes.size());

	unsigned offset = 0;
	unsigned temp = 0;
	do {
		auto remaining = compressedBytes.size() - offset;
		if (remaining < 1) {
			compressedBytes.resize(compressedBytes.size() * 2);
		}
		zs.next_out = (uint8_t*)(compressedBytes.data() + offset);
		temp = zs.avail_out = compressedBytes.size() - offset;
		ret = zng_deflate(&zs, Z_FINISH);
		offset += temp - zs.avail_out;
	} while (ret == Z_OK);

	compressedBytes.resize(offset);

	if (zng_deflateEnd(&zs) != Z_OK) {
		return false;
	}

	if (ret != Z_STREAM_END) {
		return false;
	}
	return true;
}