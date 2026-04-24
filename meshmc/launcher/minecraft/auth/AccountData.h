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
#include <QByteArray>
#include <QVector>
#include <katabasis/Bits.h>
#include <QJsonObject>

struct Skin {
	QString id;
	QString url;
	QString variant;

	QByteArray data;
};

struct Cape {
	QString id;
	QString url;
	QString alias;

	QByteArray data;
};

struct MinecraftEntitlement {
	bool ownsMinecraft = false;
	bool canPlayMinecraft = false;
	Katabasis::Validity validity = Katabasis::Validity::None;
};

struct MinecraftProfile {
	QString id;
	QString name;
	Skin skin;
	QString currentCape;
	QMap<QString, Cape> capes;
	Katabasis::Validity validity = Katabasis::Validity::None;
};

enum class AccountType { MSA, Offline };

enum class AccountState {
	Unchecked,
	Offline,
	Working,
	Online,
	Errored,
	Expired,
	Gone
};

struct AccountData {
	QJsonObject saveState() const;
	bool resumeStateFromV3(QJsonObject data);

	//! Gamertag for MSA
	QString accountDisplayString() const;

	//! Yggdrasil access token, as passed to the game.
	QString accessToken() const;

	QString profileId() const;
	QString profileName() const;

	QString lastError() const;

	AccountType type = AccountType::MSA;

	QString offlineUsername;

	Katabasis::Token msaToken;
	Katabasis::Token userToken;
	Katabasis::Token xboxApiToken;
	Katabasis::Token mojangservicesToken;

	Katabasis::Token yggdrasilToken;
	MinecraftProfile minecraftProfile;
	MinecraftEntitlement minecraftEntitlement;
	Katabasis::Validity validity_ = Katabasis::Validity::None;

	// runtime only information (not saved with the account)
	QString internalId;
	QString errorString;
	AccountState accountState = AccountState::Unchecked;
};
