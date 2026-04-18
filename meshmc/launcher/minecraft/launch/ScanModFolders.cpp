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
 *
 *  This file incorporates work covered by the following copyright and
 *  permission notice:
 *
 * Copyright 2013-2021 MultiMC Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ScanModFolders.h"
#include "launch/LaunchTask.h"
#include "MMCZip.h"
#include "minecraft/OpSys.h"
#include "FileSystem.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/mod/ModFolderModel.h"

void ScanModFolders::executeTask()
{
	auto m_inst =
		std::dynamic_pointer_cast<MinecraftInstance>(m_parent->instance());

	auto loaders = m_inst->loaderModList();
	m_modsConnection = connect(loaders.get(), &ModFolderModel::updateFinished, this,
			&ScanModFolders::modsDone);
	if (!loaders->update()) {
		m_modsDone = true;
	}

	auto cores = m_inst->coreModList();
	m_coreModsConnection = connect(cores.get(), &ModFolderModel::updateFinished, this,
			&ScanModFolders::coreModsDone);
	if (!cores->update()) {
		m_coreModsDone = true;
	}
	checkDone();
}

void ScanModFolders::modsDone()
{
	m_modsDone = true;
	checkDone();
}

void ScanModFolders::coreModsDone()
{
	m_coreModsDone = true;
	checkDone();
}

void ScanModFolders::checkDone()
{
	if (m_modsDone && m_coreModsDone) {
		disconnect(m_modsConnection);
		disconnect(m_coreModsConnection);
		emitSucceeded();
	}
}
