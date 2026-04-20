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

/**
 * \brief The Config class holds all the build-time information passed from the
 * build system.
 */
class Config
{
  public:
	Config();
	QString MESHMC_NAME;
	QString MESHMC_BINARY;
	QString MESHMC_DISPLAYNAME;
	QString MESHMC_COPYRIGHT;
	QString MESHMC_DOMAIN;
	QString MESHMC_CONFIGFILE;
	QString MESHMC_GIT;

	/// The major version number.
	int VERSION_MAJOR;
	/// The minor version number.
	int VERSION_MINOR;
	/// The hotfix number.
	int VERSION_HOTFIX;
	/// The build number.
	int VERSION_BUILD;

	/**
	 * The version channel
	 * This is used by the updater to determine what channel the current version
	 * came from.
	 */
	QString VERSION_CHANNEL;

	bool UPDATER_ENABLED = false;

	/// A short string identifying this build's platform. For example, "lin64"
	/// or "win32".
	QString BUILD_PLATFORM;

	/// URL for the updater's channel (legacy, unused)
	QString UPDATER_BASE;

	/// RSS feed URL for the new updater (projt: namespace).
	QString UPDATER_FEED_URL;

	/// GitHub releases API URL for update verification.
	QString UPDATER_GITHUB_API_URL;

	/// A string containing the build timestamp
	QString BUILD_DATE;

	/// User-Agent to use.
	QString USER_AGENT;

	/// User-Agent to use for uncached requests.
	QString USER_AGENT_UNCACHED;

	/// A short string identifying this build's valid artifacts int he updater.
	/// For example, "lin64" or "win32".
	QString BUILD_ARTIFACT;

	/// Compiler name
	QString COMPILER_NAME;

	/// Compiler version
	QString COMPILER_VERSION;

	/// Target system name (e.g. "Linux", "Windows")
	QString COMPILER_TARGET_SYSTEM;

	/// Target system version
	QString COMPILER_TARGET_SYSTEM_VERSION;

	/// Target system processor (e.g. "x86_64")
	QString COMPILER_TARGET_SYSTEM_PROCESSOR;

	/// Google analytics ID
	QString ANALYTICS_ID;

	/// Google Analytics 4 API secret
	QString ANALYTICS_SECRET;

	/// URL for notifications
	QString NOTIFICATION_URL;

	/// Used for matching notifications
	QString FULL_VERSION_STR;

	/// The git commit hash of this build
	QString GIT_COMMIT;

	/// The git refspec of this build
	QString GIT_REFSPEC;

	/// The exact git tag of this build, if any
	QString GIT_TAG;

	/// This is printed on start to standard output
	QString VERSION_STR;

	/**
	 * This is used to fetch the news RSS feed.
	 * It defaults in CMakeLists.txt to "https://projecttick.org/rss.xml"
	 */
	QString NEWS_RSS_URL;

	QString MSAClientID;

	/**
	 * API key you can get from paste.ee when you register an account
	 */
	QString PASTE_EE_KEY;

	/**
	 * Client ID you can get from Imgur when you register an application
	 */
	QString IMGUR_CLIENT_ID;

	/**
	 * Metadata repository URL prefix
	 */
	QString META_URL;

	/**
	 * API key for the CurseForge API
	 */
	QString CURSEFORGE_API_KEY;

	QString BUG_TRACKER_URL;
	QString DISCORD_URL;
	QString SUBREDDIT_URL;

	QString RESOURCE_BASE = "https://resources.download.minecraft.net/";
	QString LIBRARY_BASE = "https://libraries.minecraft.net/";
	QString IMGUR_BASE_URL = "https://api.imgur.com/3/";
	QString FMLLIBS_BASE_URL = "https://files.projecttick.org/fmllibs/";
	QString TRANSLATIONS_BASE_URL = "https://i18n.projecttick.org/";

	QString MODPACKSCH_API_BASE_URL = "https://api.modpacks.ch/";

	QString LEGACY_FTB_CDN_BASE_URL = "https://dist.creeper.host/FTB2/";

	QString ATL_DOWNLOAD_SERVER_URL =
		"https://download.nodecdn.net/containers/atl/";

	/**
	 * \brief Converts the Version to a string.
	 * \return The version number in string format (major.minor.revision.build).
	 */
	QString printableVersionString() const;

	/**
	 * \brief Compiler ID String
	 * \return a string of the form "Name - Version"  of just "Name" if the
	 * version is empty
	 */
	QString compilerID() const;

	/**
	 * \brief System ID String
	 * \return a string of the form "OS Verison Processor"
	 */
	QString systemID() const;
};

extern const Config BuildConfig;
