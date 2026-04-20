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

#include "Application.h"
#include "BuildConfig.h"
#include "plugin/PluginManager.h"

#include "ui/MainWindow.h"
#include "ui/InstanceWindow.h"

#include <QWindow>

#include "ui/instanceview/AccessibleInstanceView.h"

#include "ui/pages/BasePageProvider.h"
#include "ui/pages/global/MeshMCPage.h"
#include "ui/pages/global/MinecraftPage.h"
#include "ui/pages/global/JavaPage.h"
#include "ui/pages/global/LanguagePage.h"
#include "ui/pages/global/ProxyPage.h"
#include "ui/pages/global/ExternalToolsPage.h"
#include "ui/pages/global/AccountListPage.h"
#include "ui/pages/global/PasteEEPage.h"
#include "ui/pages/global/CustomCommandsPage.h"
#include "ui/pages/global/AppearancePage.h"

#include "ui/themes/ITheme.h"
#include "ui/themes/ThemeManager.h"

#include "ui/setupwizard/SetupWizard.h"
#include "ui/setupwizard/LanguageWizardPage.h"
#include "ui/setupwizard/JavaWizardPage.h"
#include "ui/setupwizard/AnalyticsWizardPage.h"

#include "ui/dialogs/CustomMessageBox.h"

#include "ui/pagedialog/PageDialog.h"

#include "ApplicationMessage.h"

#include <iostream>

#include <QAccessible>
#include <QDir>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTranslator>
#include <QLibraryInfo>
#include <QList>
#include <QStringList>
#include <QDebug>
#include <QStyleFactory>

#include "InstanceList.h"

#include <minecraft/auth/AccountList.h>
#include "icons/IconList.h"
#include "net/HttpMetaCache.h"

#include "java/JavaUtils.h"

#include "updater/UpdateChecker.h"

#include "tools/JProfiler.h"
#include "tools/JVisualVM.h"
#include "tools/MCEditTool.h"

#include <xdgicon.h>
#include "settings/INISettingsObject.h"
#include "settings/Setting.h"

#include "translations/TranslationsModel.h"
#include "meta/Index.h"

#include <Commandline.h>
#include <FileSystem.h>
#include <DesktopServices.h>
#include <LocalPeer.h>
#include "MMCZip.h"

#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/Component.h"

#include <ganalytics.h>
#include <sys.h>
#include "MMCStrings.h"

#if defined Q_OS_WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdio>
#endif

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

static const QLatin1String liveCheckFile("live.check");

using namespace Commandline;

#define MACOS_HINT                                                             \
	"If you are on macOS Sierra, you might have to move the app to your "      \
	"/Applications or ~/Applications"                                          \
	"folder. This usually fixes the problem and you can move the application " \
	"elsewhere afterwards.\n"                                                  \
	"\n"

namespace
{
#if defined(Q_OS_MAC)
	QString macOSDataPath()
	{
		auto dataPath =
			QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
		if (!dataPath.isEmpty()) {
			return dataPath;
		}
		return FS::PathCombine(QDir::homePath(), "Library",
							   "Application Support", BuildConfig.MESHMC_NAME);
	}

	QStringList legacyMacDataPatterns()
	{
		return {"*.cfg",	   "*.ini",	 "*.json",		"*.log",	 "accounts",
				"assets",	   "cache",	 "icons",		"instances", "java",
				"libraries",   "meta",	 "metacache",	"mods",		 "patches",
				"screenshots", "themes", "translations"};
	}

	QStringList legacyMacDataEntries(const QString& legacyDataPath)
	{
		QDir legacyDir(legacyDataPath);
		QSet<QString> entries;

		for (const auto& pattern : legacyMacDataPatterns()) {
			auto matches = legacyDir.entryInfoList(
				QStringList{pattern}, QDir::NoDotAndDotDot | QDir::Dirs |
										  QDir::Files | QDir::Hidden |
										  QDir::System);
			for (const auto& match : matches) {
				entries.insert(match.fileName());
			}
		}

		return entries.values();
	}

	bool migrateLegacyMacEntry(const QString& sourcePath,
							   const QString& targetPath)
	{
		QFileInfo sourceInfo(sourcePath);
		if (!sourceInfo.exists()) {
			return true;
		}

		QFileInfo targetInfo(targetPath);
		if (sourceInfo.isDir()) {
			if (!targetInfo.exists() && QDir().rename(sourcePath, targetPath)) {
				return true;
			}
			if (!FS::ensureFolderPathExists(targetPath)) {
				return false;
			}
			if (!FS::copy(sourcePath, targetPath)()) {
				return false;
			}
			return FS::deletePath(sourcePath);
		}

		if (targetInfo.exists()) {
			return QFile::remove(sourcePath);
		}
		if (!FS::ensureFilePathExists(targetPath)) {
			return false;
		}
		if (QFile::rename(sourcePath, targetPath)) {
			return true;
		}
		if (!QFile::copy(sourcePath, targetPath)) {
			return false;
		}
		return QFile::remove(sourcePath);
	}

	bool migrateLegacyMacData(const QString& legacyDataPath,
							  const QString& dataPath)
	{
		bool migrated = true;
		for (const auto& entry : legacyMacDataEntries(legacyDataPath)) {
			const auto sourcePath = FS::PathCombine(legacyDataPath, entry);
			const auto targetPath = FS::PathCombine(dataPath, entry);
			if (!migrateLegacyMacEntry(sourcePath, targetPath)) {
				qWarning() << "Failed to migrate legacy macOS data entry"
						   << sourcePath << "to" << targetPath;
				migrated = false;
			}
		}
		return migrated;
	}
#endif
} // namespace

namespace
{
	void appDebugOutput(QtMsgType type, const QMessageLogContext& context,
						const QString& msg)
	{
		const char* levels = "DWCFIS";
		const QString format("%1 %2 %3\n");

		qint64 msecstotal = APPLICATION->timeSinceStart();
		qint64 seconds = msecstotal / 1000;
		qint64 msecs = msecstotal % 1000;
		QString foo;
		char buf[1025] = {0};
		::snprintf(buf, 1024, "%5lld.%03lld", seconds, msecs);

		QString out = format.arg(buf).arg(levels[type]).arg(msg);

		APPLICATION->logFile->write(out.toUtf8());
		APPLICATION->logFile->flush();
		QString coloredOut = QString("%1 %2%3%4%5 %6\n")
								 .arg(buf)
								 .arg(Strings::logColor(type))
								 .arg(levels[type])
								 .arg(":")
								 .arg(Strings::logColorReset())
								 .arg(msg);
		QTextStream(stderr) << coloredOut.toLocal8Bit();
		fflush(stderr);
	}

	[[maybe_unused]] QString getIdealPlatform(QString currentPlatform)
	{
		auto info = Sys::getKernelInfo();
		switch (info.kernelType) {
			case Sys::KernelType::Darwin: {
				if (info.kernelMajor >= 17) {
					// macOS 10.13 or newer
					return "osx64-5.15.2";
				} else {
					// macOS 10.12 or older
					return "osx64";
				}
			}
			case Sys::KernelType::Windows: {
				// FIXME: 5.15.2 is not stable on Windows, due to a large number
				// of completely unpredictable and hard to reproduce issues
				break;
			}
			case Sys::KernelType::Undetermined:
				[[fallthrough]];
			case Sys::KernelType::Linux: {
				break;
			}
		}
		return currentPlatform;
	}

} // namespace

Application::Application(int& argc, char** argv) : QApplication(argc, argv)
{
	initPlatform();
	if (m_status != StartingUp)
		return;

	auto args = parseCommandLine(argc, argv);
	if (m_status != StartingUp)
		return;

	QString origcwdPath, adjustedBy, dataPath;
	if (!resolveDataPath(args, dataPath, adjustedBy, origcwdPath))
		return;

	if (m_instanceIdToLaunch.isEmpty() && !m_serverToJoin.isEmpty()) {
		qWarning() << "--server can only be used in combination with --launch!";
		m_status = Application::Failed;
		return;
	}

	if (m_instanceIdToLaunch.isEmpty() && !m_profileToUse.isEmpty()) {
		qWarning()
			<< "--account can only be used in combination with --launch!";
		m_status = Application::Failed;
		return;
	}

	// CLI fast path
	// When running a CLI-only command we need almost nothing from the
	// full application stack.  Load only settings (for InstanceDir) and
	// the instance list, execute the command, then bail out.  This
	// avoids starting the network, translations, themes, plugins,
	// analytics, logging, and the GUI — which also prevents the
	// "corrupted double-linked list" crash that occurs when static
	// destructors in plugin modules run after Qt is torn down.
	if (m_cliListInstances || !m_cliInstanceInfoId.isEmpty() ||
		!m_cliExportId.isEmpty()) {
		initSettings();

		// Load instance list
		QString instDir = m_settings->get("InstanceDir").toString();
		m_instances.reset(new InstanceList(m_settings, instDir, this));
		m_instances->loadList();

		performCLIAction();
		return;
	}

	if (!initPeerInstance())
		return;
	if (!initLogging(dataPath))
		return;

	QString binPath = applicationDirPath();
	setupPaths(binPath, origcwdPath, adjustedBy);

	initSettings();

#ifndef QT_NO_ACCESSIBILITY
	QAccessible::installFactory(groupViewAccessibleFactory);
#endif /* !QT_NO_ACCESSIBILITY */

	initSubsystems();
	initAnalytics();

	// Initialize the plugin system — scans mmcmodules directories
	// and loads all discovered .mmco modules.
	// NOTE: parent is nullptr — unique_ptr is the sole owner.
	// Do NOT pass `this` as QObject parent, or the PluginManager
	// will be double-freed (once by unique_ptr, once by ~QObject).
	m_pluginManager = std::make_unique<PluginManager>(this, nullptr);
	m_pluginManager->initializeAll();

	if (createSetupWizard()) {
		return;
	}
	performMainStartupAction();
}

void Application::initAnalytics()
{
	const int analyticsVersion = 2;
	if (BuildConfig.ANALYTICS_ID.isEmpty()) {
		return;
	}

	auto analyticsSetting = m_settings->getSetting("Analytics");
	connect(analyticsSetting.get(), &Setting::SettingChanged, this,
			&Application::analyticsSettingChanged);
	QString clientID = m_settings->get("AnalyticsClientID").toString();
	if (clientID.isEmpty()) {
		clientID = QUuid::createUuid().toString();
		clientID.remove(QLatin1Char('{'));
		clientID.remove(QLatin1Char('}'));
		m_settings->set("AnalyticsClientID", clientID);
	}
	m_analytics = new GAnalytics(BuildConfig.ANALYTICS_ID, clientID,
								 analyticsVersion, this);
	m_analytics->setMeasurementId(BuildConfig.ANALYTICS_ID);
	m_analytics->setApiSecret(BuildConfig.ANALYTICS_SECRET);
	m_analytics->setLogLevel(GAnalytics::Debug);
	m_analytics->setAnonymizeIPs(true);
	// FIXME: the ganalytics library has no idea about our fancy shared
	// pointers...
	m_analytics->setNetworkAccessManager(network().get());

	if (m_settings->get("AnalyticsSeen").toInt() < m_analytics->version()) {
		qDebug() << "Analytics info not seen by user yet (or old version).";
		return;
	}
	if (!m_settings->get("Analytics").toBool()) {
		qDebug() << "Analytics disabled by user.";
		return;
	}

	m_analytics->enable();
	qDebug() << "<> Initialized analytics with tid" << BuildConfig.ANALYTICS_ID;
}

void Application::initPlatform()
{
#if defined Q_OS_WIN32
	// attach the parent console
	if (AttachConsole(ATTACH_PARENT_PROCESS)) {
		// Reopen and sync all the I/O after attaching to parent console
		if (freopen("CON", "w", stdout)) {
			std::ios_base::sync_with_stdio();
		}
		if (freopen("CON", "w", stderr)) {
			std::ios_base::sync_with_stdio();
		}
		if (freopen("CON", "r", stdin)) {
			std::cin.sync_with_stdio();
		}
		auto out = GetStdHandle(STD_OUTPUT_HANDLE);
		DWORD written;
		const char* endline = "\n";
		WriteConsole(out, endline, strlen(endline), &written, nullptr);
		consoleAttached = true;
	}
#endif
	setOrganizationName(BuildConfig.MESHMC_NAME);
	setOrganizationDomain(BuildConfig.MESHMC_DOMAIN);
	setApplicationName(BuildConfig.MESHMC_NAME);
	setApplicationDisplayName(BuildConfig.MESHMC_DISPLAYNAME);
	setApplicationVersion(BuildConfig.printableVersionString());

	startTime = QDateTime::currentDateTime();

#ifdef Q_OS_LINUX
	{
		QFile osrelease("/proc/sys/kernel/osrelease");
		if (osrelease.open(QFile::ReadOnly | QFile::Text)) {
			QTextStream in(&osrelease);
			auto contents = in.readAll();
			if (contents.contains("WSL", Qt::CaseInsensitive) ||
				contents.contains("Microsoft", Qt::CaseInsensitive)) {
				showFatalErrorMessage(
					"Unsupported system detected!",
					"Linux-on-Windows distributions are not supported.\n\n"
					"Please use the Windows binary when playing on Windows.");
				return;
			}
		}
	}
#endif

	// Don't quit on hiding the last window
	this->setQuitOnLastWindowClosed(false);
}

QHash<QString, QVariant> Application::parseCommandLine(int& argc, char** argv)
{
	QHash<QString, QVariant> args;

	Parser parser(FlagStyle::GNU, ArgumentStyle::SpaceAndEquals);

	// --help
	parser.addSwitch("help");
	parser.addShortOpt("help", 'h');
	parser.addDocumentation("help", "Display this help and exit.");
	// --version
	parser.addSwitch("version");
	parser.addShortOpt("version", 'V');
	parser.addDocumentation("version", "Display program version and exit.");
	// --dir
	parser.addOption("dir");
	parser.addShortOpt("dir", 'd');
	parser.addDocumentation(
		"dir", "Use the supplied folder as application root "
			   "instead of the binary location (use '.' for current)");
	// --launch
	parser.addOption("launch");
	parser.addShortOpt("launch", 'l');
	parser.addDocumentation("launch",
							"Launch the specified instance (by instance ID)");
	// --server
	parser.addOption("server");
	parser.addShortOpt("server", 's');
	parser.addDocumentation("server",
							"Join the specified server on launch (only valid "
							"in combination with --launch)");
	// --profile
	parser.addOption("profile");
	parser.addShortOpt("profile", 'a');
	parser.addDocumentation("profile",
							"Use the account specified by its profile name "
							"(only valid in combination with --launch)");
	// --alive
	parser.addSwitch("alive");
	parser.addDocumentation("alive", "Write a small '" + liveCheckFile +
										 "' file after MeshMC starts");
	// --import
	parser.addOption("import");
	parser.addShortOpt("import", 'I');
	parser.addDocumentation(
		"import", "Import instance from specified zip (local path or URL)");

	// --list-instances  (CLI)
	parser.addSwitch("list-instances");
	parser.addShortOpt("list-instances", 'L');
	parser.addDocumentation("list-instances", "List all instances and exit.");
	// --instance-info  (CLI)
	parser.addOption("instance-info");
	parser.addShortOpt("instance-info", 'i');
	parser.addDocumentation("instance-info",
							"Show detailed information about the specified "
							"instance (by ID) and exit.");
	// --export  (CLI)
	parser.addOption("export");
	parser.addShortOpt("export", 'E');
	parser.addDocumentation("export",
							"Export the specified instance to a zip file. "
							"Requires --output to set the destination path.");
	// --output  (CLI, used with --export)
	parser.addOption("output");
	parser.addShortOpt("output", 'o');
	parser.addDocumentation("output", "Output file path (used with --export).");

	// parse the arguments
	try {
		args = parser.parse(arguments());
	} catch (const ParsingError& e) {
		qCritical() << "CommandLineError:" << e.what();
		if (argc > 0)
			qCritical() << "Try '" << argv[0]
						<< "' -h' to get help on command line parameters.";
		m_status = Application::Failed;
		return args;
	}

	// display help and exit
	if (args["help"].toBool()) {
		QTextStream(stdout) << parser.compileHelp(arguments()[0]);
		m_status = Application::Succeeded;
		return args;
	}

	// display version and exit
	if (args["version"].toBool()) {
		QTextStream(stdout)
			<< "Version " << BuildConfig.printableVersionString() << "\n";
		QTextStream(stdout) << "Git " << BuildConfig.GIT_COMMIT << "\n";
		m_status = Application::Succeeded;
		return args;
	}

	m_instanceIdToLaunch = args["launch"].toString();
	m_serverToJoin = args["server"].toString();
	m_profileToUse = args["profile"].toString();
	m_liveCheck = args["alive"].toBool();
	m_zipToImport = args["import"].toUrl();

	m_cliListInstances = args["list-instances"].toBool();
	m_cliInstanceInfoId = args["instance-info"].toString();
	m_cliExportId = args["export"].toString();
	m_cliOutputPath = args["output"].toString();

	if (!m_cliExportId.isEmpty() && m_cliOutputPath.isEmpty()) {
		qCritical() << "--export requires --output <path>.";
		m_status = Application::Failed;
		return args;
	}

	return args;
}

bool Application::resolveDataPath(const QHash<QString, QVariant>& args,
								  QString& dataPath, QString& adjustedBy,
								  QString& origcwdPath)
{
	origcwdPath = QDir::currentPath();

	QString dirParam = args["dir"].toString();
	if (!dirParam.isEmpty()) {
		adjustedBy += "Command line " + dirParam;
		dataPath = dirParam;
	} else {
#if defined(Q_OS_MAC)
		dataPath = macOSDataPath();
		adjustedBy += "macOS application data location " + dataPath;
#elif defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
		QDir portableDir(applicationDirPath());
		portableDir.cdUp();
		QString portableRoot = portableDir.absolutePath();
		QString portablePath = FS::PathCombine(portableRoot, "portable.txt");
		if (QFileInfo::exists(portablePath)) {
			dataPath = portableRoot;
			adjustedBy +=
				"Portable mode (portable.txt found), using portable root " +
				dataPath;
		} else {
			QString xdgDataHome =
				QProcessEnvironment::systemEnvironment().value("XDG_DATA_HOME");
			if (xdgDataHome.isEmpty()) {
				xdgDataHome = QDir::homePath() + "/.local/share";
			}
			dataPath = FS::PathCombine(xdgDataHome, BuildConfig.MESHMC_NAME);
			adjustedBy +=
				"Non-portable mode, using XDG data location " + dataPath;
		}
#elif defined(Q_OS_WIN32)
		QString portablePath =
			FS::PathCombine(applicationDirPath(), "portable.txt");
		if (QFileInfo::exists(portablePath)) {
			dataPath = applicationDirPath();
			adjustedBy +=
				"Portable mode (portable.txt found), using binary path " +
				dataPath;
		} else {
			QString appDataPath = QStandardPaths::writableLocation(
				QStandardPaths::AppDataLocation);
			dataPath = appDataPath;
			adjustedBy +=
				"Non-portable mode, using AppData location " + dataPath;
		}
#else
		dataPath = applicationDirPath();
		adjustedBy += "Fallback to binary path " + dataPath;
#endif
	}

	if (!FS::ensureFolderPathExists(dataPath)) {
		showFatalErrorMessage(
			"MeshMC data folder could not be created.",
			QString("MeshMC data folder could not be created.\n"
					"\n"
#if defined(Q_OS_MAC)
					MACOS_HINT
#endif
					"Make sure you have the right permissions to MeshMC data "
					"folder and any folder needed to access it.\n"
					"(%1)\n"
					"\n"
					"MeshMC cannot continue until you fix this problem.")
				.arg(dataPath));
		return false;
	}

	if (!QDir::setCurrent(dataPath)) {
		showFatalErrorMessage(
			"MeshMC data folder could not be opened.",
			QString("MeshMC data folder could not be opened.\n"
					"\n"
#if defined(Q_OS_MAC)
					MACOS_HINT
#endif
					"Make sure you have the right permissions to MeshMC data "
					"folder.\n"
					"(%1)\n"
					"\n"
					"MeshMC cannot continue until you fix this problem.")
				.arg(dataPath));
		return false;
	}

#if defined(Q_OS_MAC)
	const auto legacyDataPath = QDir(applicationDirPath()).absolutePath();
	if (dataPath != legacyDataPath) {
		const auto legacyEntries = legacyMacDataEntries(legacyDataPath);
		if (!legacyEntries.isEmpty()) {
			qInfo() << "Migrating legacy macOS data from app bundle to"
					<< dataPath << legacyEntries;
			migrateLegacyMacData(legacyDataPath, dataPath);
		}
	}
#endif

	return true;
}

bool Application::initPeerInstance()
{
	/*
	 * Establish the mechanism for communication with an already running
	 * MeshMC that uses the same data path. If there is one, tell it what
	 * the user actually wanted to do and exit. We want to initialize this
	 * before logging to avoid messing with the log of a potential already
	 * running copy.
	 */
	auto appID = ApplicationId::fromPathAndVersion(
		QDir::currentPath(), BuildConfig.printableVersionString());

	// FIXME: you can run the same binaries with multiple data dirs
	// and they won't clash. This could cause issues for updates.
	m_peerInstance = new LocalPeer(this, appID);
	connect(m_peerInstance, &LocalPeer::messageReceived, this,
			&Application::messageReceived);
	if (m_peerInstance->isClient()) {
		int timeout = 2000;

		if (m_instanceIdToLaunch.isEmpty()) {
			ApplicationMessage activate;
			activate.command = "activate";
			m_peerInstance->sendMessage(activate.serialize(), timeout);

			if (!m_zipToImport.isEmpty()) {
				ApplicationMessage import;
				import.command = "import";
				import.args.insert("path", m_zipToImport.toString());
				m_peerInstance->sendMessage(import.serialize(), timeout);
			}
		} else {
			ApplicationMessage launch;
			launch.command = "launch";
			launch.args["id"] = m_instanceIdToLaunch;

			if (!m_serverToJoin.isEmpty()) {
				launch.args["server"] = m_serverToJoin;
			}
			if (!m_profileToUse.isEmpty()) {
				launch.args["profile"] = m_profileToUse;
			}
			m_peerInstance->sendMessage(launch.serialize(), timeout);
		}
		m_status = Application::Succeeded;
		return false;
	}

	return true;
}

bool Application::initLogging(const QString& dataPath)
{
	static const QString logBase = BuildConfig.MESHMC_NAME + "-%0.log";
	auto moveFile = [](const QString& oldName, const QString& newName) {
		static_cast<void>(QFile::remove(newName));
		static_cast<void>(QFile::copy(oldName, newName));
		static_cast<void>(QFile::remove(oldName));
	};

	moveFile(logBase.arg(3), logBase.arg(4));
	moveFile(logBase.arg(2), logBase.arg(3));
	moveFile(logBase.arg(1), logBase.arg(2));
	moveFile(logBase.arg(0), logBase.arg(1));

	logFile = std::make_unique<QFile>(logBase.arg(0));
	if (!logFile->open(QIODevice::WriteOnly | QIODevice::Text |
					   QIODevice::Truncate)) {
		showFatalErrorMessage(
			"MeshMC data folder is not writable!",
			QString("MeshMC couldn't create a log file - the data folder is "
					"not writable.\n"
					"\n"
#if defined(Q_OS_MAC)
					MACOS_HINT
#endif
					"Make sure you have write permissions to the data folder.\n"
					"(%1)\n"
					"\n"
					"MeshMC cannot continue until you fix this problem.")
				.arg(dataPath));
		return false;
	}
	qInstallMessageHandler(appDebugOutput);
	qDebug() << "<> Log initialized.";
	return true;
}

void Application::setupPaths(const QString& binPath, const QString& origcwdPath,
							 const QString& adjustedBy)
{
	// Root path is used for updates.
#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
	QDir foo(FS::PathCombine(binPath, ".."));
	m_rootPath = foo.absolutePath();
#elif defined(Q_OS_WIN32)
	m_rootPath = binPath;
#elif defined(Q_OS_MAC)
	QDir foo(FS::PathCombine(binPath, "../.."));
	m_rootPath = foo.absolutePath();
	// on macOS, touch the root to force Finder to reload the .app metadata
	// (and fix any icon change issues)
	FS::updateTimestamp(m_rootPath);
#endif

	qInfo() << BuildConfig.MESHMC_DISPLAYNAME << ", (c) 2026 "
			<< BuildConfig.MESHMC_COPYRIGHT;
	qInfo() << "Version                    : "
			<< BuildConfig.printableVersionString();
	qInfo() << "Git commit                 : " << BuildConfig.GIT_COMMIT;
	qInfo() << "Git refspec                : " << BuildConfig.GIT_REFSPEC;
	qInfo() << "Compiled for               : " << BuildConfig.systemID();
	qInfo() << "Compiled by                : " << BuildConfig.compilerID();
	qInfo() << "Build Artifact             : " << BuildConfig.BUILD_ARTIFACT;
	if (adjustedBy.size()) {
		qInfo() << "Work dir before adjustment : " << origcwdPath;
		qInfo() << "Work dir after adjustment  : " << QDir::currentPath();
		qInfo() << "Adjusted by                : " << adjustedBy;
	} else {
		qInfo() << "Work dir                   : " << QDir::currentPath();
	}
	qInfo() << "Binary path                : " << binPath;
	qInfo() << "Application root path      : " << m_rootPath;
	if (!m_instanceIdToLaunch.isEmpty()) {
		qInfo() << "ID of instance to launch   : " << m_instanceIdToLaunch;
	}
	if (!m_serverToJoin.isEmpty()) {
		qInfo() << "Address of server to join  :" << m_serverToJoin;
	}
	qInfo() << "<> Paths set.";

	if (m_liveCheck) {
		auto appID = ApplicationId::fromPathAndVersion(
			QDir::currentPath(), BuildConfig.printableVersionString());
		QFile check(liveCheckFile);
		if (check.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			auto payload = appID.toString().toUtf8();
			if (check.write(payload) != payload.size()) {
				qWarning() << "Could not write into" << liveCheckFile << "!";
				check.remove();
			} else {
				check.close();
			}
		} else {
			qWarning() << "Could not open" << liveCheckFile << "for writing!";
		}
	}
}

void Application::initSettings()
{
	m_settings.reset(
		new INISettingsObject(BuildConfig.MESHMC_CONFIGFILE, this));
	// Updates
	m_settings->registerSetting("UpdateChannel", BuildConfig.VERSION_CHANNEL);
	m_settings->registerSetting("AutoUpdate", true);

	// Theming
	m_settings->registerSetting("IconTheme", QString("pe_colored"));
	m_settings->registerSetting("ApplicationTheme", QString("system"));

	// Notifications
	m_settings->registerSetting("ShownNotifications", QString());

	// Remembered state
	m_settings->registerSetting("LastUsedGroupForNewInstance", QString());

	QString defaultMonospace;
	int defaultSize = 11;
#ifdef Q_OS_WIN32
	defaultMonospace = "Courier";
	defaultSize = 10;
#elif defined(Q_OS_MAC)
	defaultMonospace = "Menlo";
#else
	defaultMonospace = "Monospace";
#endif

	// resolve the font so the default actually matches
	QFont consoleFont;
	consoleFont.setFamily(defaultMonospace);
	consoleFont.setStyleHint(QFont::Monospace);
	consoleFont.setFixedPitch(true);
	QFontInfo consoleFontInfo(consoleFont);
	QString resolvedDefaultMonospace = consoleFontInfo.family();
	QFont resolvedFont(resolvedDefaultMonospace);
	qDebug() << "Detected default console font:" << resolvedDefaultMonospace
			 << ", substitutions:" << resolvedFont.substitutions().join(',');

	m_settings->registerSetting("ConsoleFont", resolvedDefaultMonospace);
	m_settings->registerSetting("ConsoleFontSize", defaultSize);
	m_settings->registerSetting("ConsoleMaxLines", 100000);
	m_settings->registerSetting("ConsoleOverflowStop", true);

	// Folders
	m_settings->registerSetting("InstanceDir", "instances");
	m_settings->registerSetting({"CentralModsDir", "ModsDir"}, "mods");
	m_settings->registerSetting("IconsDir", "icons");

	// Editors
	m_settings->registerSetting("JsonEditor", QString());

	// Language
	m_settings->registerSetting("Language", QString());

	// Console
	m_settings->registerSetting("ShowConsole", false);
	m_settings->registerSetting("AutoCloseConsole", false);
	m_settings->registerSetting("ShowConsoleOnError", true);
	m_settings->registerSetting("LogPrePostOutput", true);

	// Window Size
	m_settings->registerSetting({"LaunchMaximized", "MCWindowMaximize"}, false);
	m_settings->registerSetting({"MinecraftWinWidth", "MCWindowWidth"}, 854);
	m_settings->registerSetting({"MinecraftWinHeight", "MCWindowHeight"}, 480);

	// Proxy Settings
	m_settings->registerSetting("ProxyType", "None");
	m_settings->registerSetting({"ProxyAddr", "ProxyHostName"}, "127.0.0.1");
	m_settings->registerSetting("ProxyPort", 8080);
	m_settings->registerSetting({"ProxyUser", "ProxyUsername"}, "");
	m_settings->registerSetting({"ProxyPass", "ProxyPassword"}, "");

	// Memory — compute reasonable defaults based on system RAM
	int defaultMinMem = 512;
	int defaultMaxMem = 1024;
	{
		uint64_t systemRamMiB = Sys::getSystemRam() / Sys::mebibyte;
		if (systemRamMiB >= 32768) { // 32+ GB
			defaultMinMem = 1024;
			defaultMaxMem = 8192;
		} else if (systemRamMiB >= 16384) { // 16-32 GB
			defaultMinMem = 1024;
			defaultMaxMem = 6144;
		} else if (systemRamMiB >= 8192) { // 8-16 GB
			defaultMinMem = 512;
			defaultMaxMem = 4096;
		} else if (systemRamMiB >= 4096) { // 4-8 GB
			defaultMinMem = 512;
			defaultMaxMem = 2048;
		}
		// <4 GB: keep 512/1024 defaults
	}
	m_settings->registerSetting({"MinMemAlloc", "MinMemoryAlloc"},
								defaultMinMem);
	m_settings->registerSetting({"MaxMemAlloc", "MaxMemoryAlloc"},
								defaultMaxMem);
	m_settings->registerSetting("PermGen", 128);

	// Java Settings
	m_settings->registerSetting("JavaPath", "");
	m_settings->registerSetting("JavaTimestamp", 0);
	m_settings->registerSetting("JavaArchitecture", "");
	m_settings->registerSetting("JavaVersion", "");
	m_settings->registerSetting("JavaVendor", "");
	m_settings->registerSetting("LastHostname", "");
	m_settings->registerSetting("JvmArgs", "");

	// Java auto-download
	m_settings->registerSetting("JavaAutoDownload", true);
	m_settings->registerSetting("JavaAutoDownloadVendor", "net.minecraft.java");

	// Native library workarounds
	m_settings->registerSetting("UseNativeOpenAL", false);
	m_settings->registerSetting("UseNativeGLFW", false);

	// Game time
	m_settings->registerSetting("ShowGameTime", true);
	m_settings->registerSetting("ShowGlobalGameTime", true);
	m_settings->registerSetting("RecordGameTime", true);

	// Minecraft launch method
	m_settings->registerSetting("MCLaunchMethod", "MeshMCPart");

	// Wrapper command for launch
	m_settings->registerSetting("WrapperCommand", "");

	// Custom Commands
	m_settings->registerSetting({"PreLaunchCommand", "PreLaunchCmd"}, "");
	m_settings->registerSetting({"PostExitCommand", "PostExitCmd"}, "");

	// The cat
	m_settings->registerSetting("TheCat", false);
	m_settings->registerSetting("BackgroundCat", QString("kitteh"));
	m_settings->registerSetting("CatOpacity", 100);

	m_settings->registerSetting("InstSortMode", "Name");
	m_settings->registerSetting("SelectedInstance", QString());

	// Window state and geometry
	m_settings->registerSetting("MainWindowState", "");
	m_settings->registerSetting("MainWindowGeometry", "");

	m_settings->registerSetting("ConsoleWindowState", "");
	m_settings->registerSetting("ConsoleWindowGeometry", "");

	m_settings->registerSetting("SettingsGeometry", "");

	m_settings->registerSetting("PagedGeometry", "");

	m_settings->registerSetting("NewInstanceGeometry", "");

	m_settings->registerSetting("UpdateDialogGeometry", "");

	// paste.ee API key
	m_settings->registerSetting("PasteEEAPIKey", "meshmc");

	if (!BuildConfig.ANALYTICS_ID.isEmpty()) {
		// Analytics
		m_settings->registerSetting("Analytics", true);
		m_settings->registerSetting("AnalyticsSeen", 0);
		m_settings->registerSetting("AnalyticsClientID", QString());
	}

	// Init page provider
	{
		m_globalSettingsProvider =
			std::make_shared<GenericPageProvider>(tr("Settings"));
		m_globalSettingsProvider->addPage<MeshMCPage>();
		m_globalSettingsProvider->addPage<AppearancePage>();
		m_globalSettingsProvider->addPage<MinecraftPage>();
		m_globalSettingsProvider->addPage<JavaPage>();
		m_globalSettingsProvider->addPage<LanguagePage>();
		m_globalSettingsProvider->addPage<CustomCommandsPage>();
		m_globalSettingsProvider->addPage<ProxyPage>();
		m_globalSettingsProvider->addPage<ExternalToolsPage>();
		m_globalSettingsProvider->addPage<AccountListPage>();
		m_globalSettingsProvider->addPage<PasteEEPage>();
	}
	qDebug() << "<> Settings loaded.";
}

void Application::initSubsystems()
{
	// initialize network access and proxy setup
	{
		m_network = new QNetworkAccessManager();
		QString proxyTypeStr = settings()->get("ProxyType").toString();
		QString addr = settings()->get("ProxyAddr").toString();
		int port = settings()->get("ProxyPort").value<qint16>();
		QString user = settings()->get("ProxyUser").toString();
		QString pass = settings()->get("ProxyPass").toString();
		updateProxySettings(proxyTypeStr, addr, port, user, pass);
		qDebug() << "<> Network done.";
	}

	// load translations
	{
		m_translations.reset(new TranslationsModel("translations"));
		auto bcp47Name = m_settings->get("Language").toString();
		m_translations->selectLanguage(bcp47Name);
		qDebug() << "Your language is" << bcp47Name;
		qDebug() << "<> Translations loaded.";
	}

	// initialize the updater
	if (BuildConfig.UPDATER_ENABLED && UpdateChecker::isUpdaterSupported()) {
		m_updateChecker.reset(new UpdateChecker(m_network));
		qDebug() << "<> Updater initialized (feed:"
				 << BuildConfig.UPDATER_FEED_URL
				 << "| github:" << BuildConfig.UPDATER_GITHUB_API_URL << ").";
	} else if (BuildConfig.UPDATER_ENABLED) {
		qDebug() << "<> Updater disabled on this platform/mode.";
	}

	// Instance icons
	{
		auto setting = APPLICATION->settings()->getSetting("IconsDir");
		QStringList instFolders = {":/icons/multimc/32x32/instances/",
								   ":/icons/multimc/50x50/instances/",
								   ":/icons/multimc/128x128/instances/",
								   ":/icons/multimc/scalable/instances/"};
		m_icons.reset(new IconList(instFolders, setting->get().toString()));
		connect(setting.get(), &Setting::SettingChanged,
				[&](const Setting&, QVariant value) {
					m_icons->directoryChanged(value.toString());
				});
		qDebug() << "<> Instance icons intialized.";
	}

	// Icon themes
	{
		// TODO: icon themes and instance icons do not mesh well together.
		// Rearrange and fix discrepancies! set icon theme search path!
		auto searchPaths = QIcon::themeSearchPaths();
		searchPaths.append("iconthemes");
		QIcon::setThemeSearchPaths(searchPaths);
		qDebug() << "<> Icon themes initialized.";
	}

	// Initialize widget themes
	{
		m_themeManager = std::make_unique<ThemeManager>();
		qDebug() << "<> Widget themes initialized.";
	}

	// initialize and load all instances
	{
		auto InstDirSetting = m_settings->getSetting("InstanceDir");
		// instance path: check for problems with '!' in instance path and
		// warn the user in the log and remember that we have to show him a
		// dialog when the gui starts (if it does so)
		QString instDir = InstDirSetting->get().toString();
		qDebug() << "Instance path              : " << instDir;
		if (FS::checkProblemticPathJava(QDir(instDir))) {
			qWarning() << "Your instance path contains \'!\' and this is "
						  "known to cause java problems!";
		}
		m_instances.reset(new InstanceList(m_settings, instDir, this));
		connect(InstDirSetting.get(), &Setting::SettingChanged,
				m_instances.get(), &InstanceList::on_InstFolderChanged);
		qDebug() << "Loading Instances...";
		m_instances->loadList();
		qDebug() << "<> Instances loaded.";
	}

	// and accounts
	{
		m_accounts.reset(new AccountList(this));
		qDebug() << "Loading accounts...";
		m_accounts->setListFilePath("accounts.json", true);
		m_accounts->loadList();
		m_accounts->fillQueue();
		qDebug() << "<> Accounts loaded.";
	}

	// init the http meta cache
	{
		m_metacache.reset(new HttpMetaCache("metacache"));
		m_metacache->addBase("asset_indexes",
							 QDir("assets/indexes").absolutePath());
		m_metacache->addBase("asset_objects",
							 QDir("assets/objects").absolutePath());
		m_metacache->addBase("versions", QDir("versions").absolutePath());
		m_metacache->addBase("libraries", QDir("libraries").absolutePath());
		m_metacache->addBase("minecraftforge",
							 QDir("mods/minecraftforge").absolutePath());
		m_metacache->addBase("fmllibs",
							 QDir("mods/minecraftforge/libs").absolutePath());
		m_metacache->addBase("liteloader",
							 QDir("mods/liteloader").absolutePath());
		m_metacache->addBase("general", QDir("cache").absolutePath());
		m_metacache->addBase("ATLauncherPacks",
							 QDir("cache/ATLauncherPacks").absolutePath());
		m_metacache->addBase("FTBPacks", QDir("cache/FTBPacks").absolutePath());
		m_metacache->addBase("ModpacksCHPacks",
							 QDir("cache/ModpacksCHPacks").absolutePath());
		m_metacache->addBase("TechnicPacks",
							 QDir("cache/TechnicPacks").absolutePath());
		m_metacache->addBase("FlamePacks",
							 QDir("cache/FlamePacks").absolutePath());
		m_metacache->addBase("FlameModIcons",
							 QDir("cache/FlameModIcons").absolutePath());
		m_metacache->addBase("ModrinthPacks",
							 QDir("cache/ModrinthPacks").absolutePath());
		m_metacache->addBase("ModrinthModIcons",
							 QDir("cache/ModrinthModIcons").absolutePath());
		m_metacache->addBase("root", QDir::currentPath());
		m_metacache->addBase("translations",
							 QDir("translations").absolutePath());
		m_metacache->addBase("icons", QDir("cache/icons").absolutePath());
		m_metacache->addBase("meta", QDir("meta").absolutePath());
		m_metacache->Load();
		qDebug() << "<> Cache initialized.";
	}

	// now we have network, download translation updates
	m_translations->downloadIndex();

	// FIXME: what to do with these?
	m_profilers.insert("jprofiler", std::make_shared<JProfilerFactory>());
	m_profilers.insert("jvisualvm", std::make_shared<JVisualVMFactory>());
	for (auto profiler : m_profilers.values()) {
		profiler->registerSettings(m_settings);
	}

	// Create the MCEdit thing... why is this here?
	{
		m_mcedit.reset(new MCEditTool(m_settings));
	}

	connect(this, &Application::aboutToQuit, [this]() {
		if (m_instances) {
			// save any remaining instance state
			m_instances->saveNow();
		}
		// Shut down plugins while the log file and Qt are still alive
		if (m_pluginManager) {
			m_pluginManager->shutdownAll();
		}
		if (logFile) {
			logFile->flush();
			logFile->close();
		}
	});

	{
		m_themeManager->applyCurrentlySelectedTheme(true);
		qDebug() << "<> Theme applied.";
	}
}

bool Application::createSetupWizard()
{
	bool javaRequired = [&]() {
		QString currentHostName = QHostInfo::localHostName();
		QString oldHostName = settings()->get("LastHostname").toString();
		if (currentHostName != oldHostName) {
			settings()->set("LastHostname", currentHostName);
			return true;
		}
		QString currentJavaPath = settings()->get("JavaPath").toString();
		QString actualPath = FS::ResolveExecutable(currentJavaPath);
		if (actualPath.isNull()) {
			return true;
		}
		return false;
	}();
	bool analyticsRequired = [&]() {
		if (BuildConfig.ANALYTICS_ID.isEmpty()) {
			return false;
		}
		if (!settings()->get("Analytics").toBool()) {
			return false;
		}
		if (settings()->get("AnalyticsSeen").toInt() < analytics()->version()) {
			return true;
		}
		return false;
	}();
	bool languageRequired = [&]() {
		if (settings()->get("Language").toString().isEmpty())
			return true;
		return false;
	}();
	bool wizardRequired = javaRequired || analyticsRequired || languageRequired;

	if (wizardRequired) {
		m_setupWizard = new SetupWizard(nullptr);
		if (languageRequired) {
			m_setupWizard->addPage(new LanguageWizardPage(m_setupWizard));
		}
		if (javaRequired) {
			m_setupWizard->addPage(new JavaWizardPage(m_setupWizard));
		}
		if (analyticsRequired) {
			m_setupWizard->addPage(new AnalyticsWizardPage(m_setupWizard));
		}
		connect(m_setupWizard, &QDialog::finished, this,
				&Application::setupWizardFinished);
		m_setupWizard->show();
		return true;
	}
	return false;
}

void Application::setupWizardFinished(int status)
{
	qDebug() << "Wizard result =" << status;
	performMainStartupAction();
}

void Application::performCLIAction()
{
	if (m_cliListInstances) {
		auto instList = instances();
		int count = instList->count();
		if (count == 0) {
			fprintf(stdout, "No instances found.\n");
		} else {
			fprintf(stdout, "%-24s %-30s %-12s %s\n", "ID", "Name",
					"MC Version", "Group");
			for (int i = 0; i < 80; i++)
				fputc('-', stdout);
			fputc('\n', stdout);
			for (int i = 0; i < count; i++) {
				auto inst = instList->at(i);
				if (!inst)
					continue;
				QString group = instList->getInstanceGroup(inst->id());
				QString mcVer = "?";
				auto* mcInst = dynamic_cast<MinecraftInstance*>(inst.get());
				if (mcInst) {
					auto profile = mcInst->getPackProfile();
					if (profile)
						mcVer = profile->getComponentVersion("net.minecraft");
				}
				fprintf(
					stdout, "%-24s %-30s %-12s %s\n", qPrintable(inst->id()),
					qPrintable(inst->name()), qPrintable(mcVer),
					qPrintable(group.isEmpty() ? QStringLiteral("-") : group));
			}
		}
		fprintf(stdout, "\nTotal: %d instance(s)\n", count);
		m_status = Application::Succeeded;
		return;
	}

	if (!m_cliInstanceInfoId.isEmpty()) {
		auto inst = instances()->getInstanceById(m_cliInstanceInfoId);
		if (!inst) {
			fprintf(stderr, "Error: Instance '%s' not found.\n",
					qPrintable(m_cliInstanceInfoId));
			m_status = Application::Failed;
			return;
		}
		fprintf(stdout, "Instance: %s\n", qPrintable(inst->name()));
		fprintf(stdout, "ID:       %s\n", qPrintable(inst->id()));
		fprintf(stdout, "Path:     %s\n", qPrintable(inst->instanceRoot()));
		fprintf(stdout, "Type:     %s\n", qPrintable(inst->typeName()));
		fprintf(stdout, "Group:    %s\n",
				qPrintable(instances()->getInstanceGroup(inst->id())));

		auto* mcInst = dynamic_cast<MinecraftInstance*>(inst.get());
		if (mcInst) {
			auto profile = mcInst->getPackProfile();
			if (profile) {
				fprintf(
					stdout, "MC Ver:   %s\n",
					qPrintable(profile->getComponentVersion("net.minecraft")));
				fprintf(stdout, "\nComponents:\n");
				for (int i = 0; i < profile->rowCount(); i++) {
					auto comp = profile->getComponent(i);
					if (comp) {
						fprintf(stdout, "  %s %s\n",
								qPrintable(comp->getName()),
								qPrintable(comp->getVersion()));
					}
				}
			}
		}

		auto totalTime = inst->totalTimePlayed();
		if (totalTime > 0) {
			int hours = totalTime / 3600;
			int mins = (totalTime % 3600) / 60;
			fprintf(stdout, "\nPlay time: %dh %dm\n", hours, mins);
		}

		auto lastLaunch = inst->lastLaunch();
		if (lastLaunch > 0) {
			fprintf(stdout, "Last launched: %s\n",
					qPrintable(QDateTime::fromSecsSinceEpoch(lastLaunch)
								   .toString(Qt::ISODate)));
		}

		m_status = Application::Succeeded;
		return;
	}

	if (!m_cliExportId.isEmpty()) {
		auto inst = instances()->getInstanceById(m_cliExportId);
		if (!inst) {
			fprintf(stderr, "Error: Instance '%s' not found.\n",
					qPrintable(m_cliExportId));
			m_status = Application::Failed;
			return;
		}

		fprintf(stdout, "Exporting '%s' to %s ...\n", qPrintable(inst->name()),
				qPrintable(m_cliOutputPath));
		fflush(stdout);

		QString srcPath = inst->instanceRoot();
		auto noFilter = [](const QString&) { return false; };
		if (!MMCZip::compressDir(m_cliOutputPath, srcPath, noFilter)) {
			fprintf(stderr, "Error: Failed to export instance.\n");
			m_status = Application::Failed;
			return;
		}

		fprintf(stdout, "Export complete.\n");
		m_status = Application::Succeeded;
		return;
	}

	m_status = Application::Failed;
}

void Application::performMainStartupAction()
{
	m_status = Application::Initialized;
	if (!m_instanceIdToLaunch.isEmpty()) {
		auto inst = instances()->getInstanceById(m_instanceIdToLaunch);
		if (inst) {
			MinecraftServerTargetPtr serverToJoin = nullptr;
			MinecraftAccountPtr accountToUse = nullptr;

			qDebug() << "<> Instance" << m_instanceIdToLaunch << "launching";
			if (!m_serverToJoin.isEmpty()) {
				// FIXME: validate the server string
				serverToJoin.reset(new MinecraftServerTarget(
					MinecraftServerTarget::parse(m_serverToJoin)));
				qDebug() << "   Launching with server" << m_serverToJoin;
			}

			if (!m_profileToUse.isEmpty()) {
				accountToUse =
					accounts()->getAccountByProfileName(m_profileToUse);
				if (!accountToUse) {
					return;
				}
				qDebug() << "   Launching with account" << m_profileToUse;
			}

			launch(inst, true, nullptr, serverToJoin, accountToUse);
			return;
		}
	}
	if (!m_mainWindow) {
		// normal main window
		showMainWindow(false);
		qDebug() << "<> Main window shown.";
	}
	if (!m_zipToImport.isEmpty()) {
		qDebug() << "<> Importing instance from zip:" << m_zipToImport;
		m_mainWindow->droppedURLs({m_zipToImport});
	}
}

void Application::showFatalErrorMessage(const QString& title,
										const QString& content)
{
	m_status = Application::Failed;
	auto dialog = CustomMessageBox::selectable(nullptr, title, content,
											   QMessageBox::Critical);
	dialog->exec();
}

Application::~Application()
{
	// Shut down plugin system before tearing down the rest.
	// shutdownAll() was already called from aboutToQuit; this
	// is a no-op guard for any other exit path.
	if (m_pluginManager) {
		m_pluginManager->shutdownAll();
	}

	// Shut down logger by setting the logger function to nothing
	qInstallMessageHandler(nullptr);

#if defined Q_OS_WIN32
	// Detach from Windows console
	if (consoleAttached) {
		fclose(stdout);
		fclose(stdin);
		fclose(stderr);
		FreeConsole();
	}
#endif
}

void Application::messageReceived(const QByteArray& message)
{
	if (status() != Initialized) {
		qDebug() << "Received message" << message
				 << "while still initializing. It will be ignored.";
		return;
	}

	ApplicationMessage received;
	received.parse(message);

	auto& command = received.command;

	if (command == "activate") {
		showMainWindow();
	} else if (command == "import") {
		QString path = received.args["path"];
		if (path.isEmpty()) {
			qWarning() << "Received" << command
					   << "message without a zip path/URL.";
			return;
		}
		m_mainWindow->droppedURLs({QUrl(path)});
	} else if (command == "launch") {
		QString id = received.args["id"];
		QString server = received.args["server"];
		QString profile = received.args["profile"];

		InstancePtr instance;
		if (!id.isEmpty()) {
			instance = instances()->getInstanceById(id);
			if (!instance) {
				qWarning() << "Launch command requires an valid instance ID. "
						   << id << "resolves to nothing.";
				return;
			}
		} else {
			qWarning() << "Launch command called without an instance ID...";
			return;
		}

		MinecraftServerTargetPtr serverObject = nullptr;
		if (!server.isEmpty()) {
			serverObject = std::make_shared<MinecraftServerTarget>(
				MinecraftServerTarget::parse(server));
		}

		MinecraftAccountPtr accountObject;
		if (!profile.isEmpty()) {
			accountObject = accounts()->getAccountByProfileName(profile);
			if (!accountObject) {
				qWarning() << "Launch command requires the specified"
						   << "profile to be valid." << profile
						   << "does not resolve to any account.";
				return;
			}
		}

		launch(instance, true, nullptr, serverObject, accountObject);
	} else {
		qWarning() << "Received invalid message" << message;
	}
}

void Application::analyticsSettingChanged(const Setting&, QVariant value)
{
	if (!m_analytics)
		return;
	bool enabled = value.toBool();
	if (enabled) {
		qDebug() << "Analytics enabled by user.";
	} else {
		qDebug() << "Analytics disabled by user.";
	}
	m_analytics->enable(enabled);
}

std::shared_ptr<TranslationsModel> Application::translations()
{
	return m_translations;
}

std::shared_ptr<JavaInstallList> Application::javalist()
{
	if (!m_javalist) {
		m_javalist.reset(new JavaInstallList());
	}
	return m_javalist;
}

std::vector<ITheme*> Application::getValidApplicationThemes()
{
	return m_themeManager->allThemes();
}

void Application::setApplicationTheme(const QString& name, bool initial)
{
	m_themeManager->setApplicationTheme(name, initial);
}

void Application::setIconTheme(const QString& name)
{
	m_themeManager->setIconTheme(name);
}

ThemeManager* Application::themeManager() const
{
	return m_themeManager.get();
}

QIcon Application::getThemedIcon(const QString& name)
{
	if (name == "logo") {
		return QIcon(":/org.projecttick.MeshMC.svg");
	}
	return XdgIcon::fromTheme(name);
}

bool Application::openJsonEditor(const QString& filename)
{
	const QString file = QDir::current().absoluteFilePath(filename);
	if (m_settings->get("JsonEditor").toString().isEmpty()) {
		return DesktopServices::openUrl(QUrl::fromLocalFile(file));
	} else {
		return DesktopServices::run(m_settings->get("JsonEditor").toString(),
									{file});
	}
}

bool Application::launch(InstancePtr instance, bool online,
						 BaseProfilerFactory* profiler,
						 MinecraftServerTargetPtr serverToJoin,
						 MinecraftAccountPtr accountToUse)
{
	if (m_updateRunning) {
		qDebug() << "Cannot launch instances while an update is running. "
					"Please try again when updates are completed.";
	} else if (instance->canLaunch()) {
		auto& extras = m_instanceExtras[instance->id()];
		auto& window = extras.window;
		if (window) {
			if (!window->saveAll()) {
				return false;
			}
		}
		auto& controller = extras.controller;
		controller.reset(new LaunchController());
		controller->setInstance(instance);
		controller->setOnline(online);
		controller->setProfiler(profiler);
		controller->setServerToJoin(serverToJoin);
		controller->setAccountToUse(accountToUse);
		if (window) {
			controller->setParentWidget(window);
		} else if (m_mainWindow) {
			controller->setParentWidget(m_mainWindow);
		}
		connect(controller.get(), &LaunchController::succeeded, this,
				&Application::controllerSucceeded);
		connect(controller.get(), &LaunchController::failed, this,
				&Application::controllerFailed);
		addRunningInstance();
		controller->start();
		return true;
	} else if (instance->isRunning()) {
		showInstanceWindow(instance, "console");
		return true;
	} else if (instance->canEdit()) {
		showInstanceWindow(instance);
		return true;
	}
	return false;
}

bool Application::kill(InstancePtr instance)
{
	if (!instance->isRunning()) {
		qWarning() << "Attempted to kill instance" << instance->id()
				   << ", which isn't running.";
		return false;
	}
	auto& extras = m_instanceExtras[instance->id()];
	// NOTE: copy of the shared pointer keeps it alive
	auto controller = extras.controller;
	if (controller) {
		return controller->abort();
	}
	return true;
}

void Application::addRunningInstance()
{
	m_runningInstances++;
	if (m_runningInstances == 1) {
		emit updateAllowedChanged(false);
	}
}

void Application::subRunningInstance()
{
	if (m_runningInstances == 0) {
		qCritical() << "Something went really wrong and we now have less "
					   "than 0 running instances... WTF";
		return;
	}
	m_runningInstances--;
	if (m_runningInstances == 0) {
		emit updateAllowedChanged(true);
	}
}

bool Application::shouldExitNow() const
{
	return m_runningInstances == 0 && m_openWindows == 0;
}

bool Application::updatesAreAllowed()
{
	return m_runningInstances == 0;
}

void Application::updateIsRunning(bool running)
{
	m_updateRunning = running;
}

void Application::controllerSucceeded()
{
	auto controller = qobject_cast<LaunchController*>(QObject::sender());
	if (!controller)
		return;
	auto id = controller->id();
	auto& extras = m_instanceExtras[id];

	// on success, do...
	if (controller->instance()->settings()->get("AutoCloseConsole").toBool()) {
		if (extras.window) {
			extras.window->close();
		}
	}
	extras.controller.reset();
	subRunningInstance();

	// quit when there are no more windows.
	if (shouldExitNow()) {
		m_status = Status::Succeeded;
		exit(0);
	}
}

void Application::controllerFailed(const QString& error)
{
	Q_UNUSED(error);
	auto controller = qobject_cast<LaunchController*>(QObject::sender());
	if (!controller)
		return;
	auto id = controller->id();
	auto& extras = m_instanceExtras[id];

	// on failure, do... nothing
	extras.controller.reset();
	subRunningInstance();

	// quit when there are no more windows.
	if (shouldExitNow()) {
		m_status = Status::Failed;
		exit(1);
	}
}

void Application::ShowGlobalSettings(class QWidget* parent, QString open_page)
{
	if (!m_globalSettingsProvider) {
		return;
	}
	emit globalSettingsAboutToOpen();
	{
		SettingsObject::Lock lock(APPLICATION->settings());
		PageDialog dlg(m_globalSettingsProvider.get(), open_page, parent);
		dlg.exec();
	}
	emit globalSettingsClosed();
}

void Application::registerGlobalSettingsPage(std::function<BasePage*()> creator)
{
	if (m_globalSettingsProvider) {
		m_globalSettingsProvider->addPageCreator(creator);
	}
}

MainWindow* Application::showMainWindow(bool minimized)
{
	if (m_mainWindow) {
		m_mainWindow->setWindowState(m_mainWindow->windowState() &
									 ~Qt::WindowMinimized);
		m_mainWindow->raise();
		m_mainWindow->activateWindow();
	} else {
		m_mainWindow = new MainWindow();
		m_mainWindow->restoreState(QByteArray::fromBase64(
			APPLICATION->settings()->get("MainWindowState").toByteArray()));
		m_mainWindow->restoreGeometry(QByteArray::fromBase64(
			APPLICATION->settings()->get("MainWindowGeometry").toByteArray()));
		if (minimized) {
			m_mainWindow->showMinimized();
		} else {
			m_mainWindow->show();
		}

		m_mainWindow->checkInstancePathForProblems();
		connect(this, &Application::updateAllowedChanged, m_mainWindow,
				&MainWindow::updatesAllowedChanged);
		connect(m_mainWindow, &MainWindow::isClosing, this,
				&Application::on_windowClose);
		m_openWindows++;
	}
	// FIXME: move this somewhere else...
	if (m_analytics) {
		auto windowSize = m_mainWindow->size();
		auto sizeString =
			QString("%1x%2").arg(windowSize.width()).arg(windowSize.height());
		qDebug() << "Viewport size" << sizeString;
		m_analytics->setViewportSize(sizeString);
		/*
		 * cm1 = java min heap [MB]
		 * cm2 = java max heap [MB]
		 * cm3 = system RAM [MB]
		 *
		 * cd1 = java version
		 * cd2 = java architecture
		 * cd3 = system architecture
		 * cd4 = CPU architecture
		 */
		QVariantMap customValues;
		int min = m_settings->get("MinMemAlloc").toInt();
		int max = m_settings->get("MaxMemAlloc").toInt();
		if (min < max) {
			customValues["cm1"] = min;
			customValues["cm2"] = max;
		} else {
			customValues["cm1"] = max;
			customValues["cm2"] = min;
		}

		constexpr uint64_t Mega = 1024ull * 1024ull;
		int ramSize = int(Sys::getSystemRam() / Mega);
		qDebug() << "RAM size is" << ramSize << "MB";
		customValues["cm3"] = ramSize;

		customValues["cd1"] = m_settings->get("JavaVersion");
		customValues["cd2"] = m_settings->get("JavaArchitecture");
		customValues["cd3"] = Sys::isSystem64bit() ? "64" : "32";
		customValues["cd4"] = Sys::isCPU64bit() ? "64" : "32";
		auto kernelInfo = Sys::getKernelInfo();
		customValues["cd5"] = kernelInfo.kernelName;
		customValues["cd6"] = kernelInfo.kernelVersion;
		auto distInfo = Sys::getDistributionInfo();
		if (!distInfo.distributionName.isEmpty()) {
			customValues["cd7"] = distInfo.distributionName;
		}
		if (!distInfo.distributionVersion.isEmpty()) {
			customValues["cd8"] = distInfo.distributionVersion;
		}
		m_analytics->sendScreenView("Main Window", customValues);
	}
	return m_mainWindow;
}

InstanceWindow* Application::showInstanceWindow(InstancePtr instance,
												QString page)
{
	if (!instance)
		return nullptr;
	auto id = instance->id();
	auto& extras = m_instanceExtras[id];
	auto& window = extras.window;

	if (window) {
		window->raise();
		window->activateWindow();
	} else {
		window = new InstanceWindow(instance);
		// On Wayland, set the transient parent so the compositor
		// (Mutter/GNOME) knows this window belongs to the main
		// window and can manage stacking/focus correctly.
		// This must happen before show() so the compositor sees
		// the relationship when the surface is first mapped.
		if (m_mainWindow) {
			// Ensure both windows have native handles
			window->winId();
			if (window->windowHandle() && m_mainWindow->windowHandle()) {
				window->windowHandle()->setTransientParent(
					m_mainWindow->windowHandle());
			}
		}
		window->show();
		m_openWindows++;
		connect(window, &InstanceWindow::isClosing, this,
				&Application::on_windowClose);
	}
	if (!page.isEmpty()) {
		window->selectPage(page);
	}
	if (extras.controller) {
		extras.controller->setParentWidget(window);
	}
	return window;
}

void Application::on_windowClose()
{
	m_openWindows--;
	auto instWindow = qobject_cast<InstanceWindow*>(QObject::sender());
	if (instWindow) {
		auto& extras = m_instanceExtras[instWindow->instanceId()];
		extras.window = nullptr;
		if (extras.controller) {
			extras.controller->setParentWidget(m_mainWindow);
		}
	}
	auto mainWindow = qobject_cast<MainWindow*>(QObject::sender());
	if (mainWindow) {
		m_mainWindow = nullptr;
	}
	// quit when there are no more windows.
	if (shouldExitNow()) {
		exit(0);
	}
}

QString Application::msaClientId() const
{
	return BuildConfig.MSAClientID;
}

void Application::updateProxySettings(QString proxyTypeStr, QString addr,
									  int port, QString user, QString password)
{
	// Set the application proxy settings.
	if (proxyTypeStr == "SOCKS5") {
		QNetworkProxy::setApplicationProxy(QNetworkProxy(
			QNetworkProxy::Socks5Proxy, addr, port, user, password));
	} else if (proxyTypeStr == "HTTP") {
		QNetworkProxy::setApplicationProxy(QNetworkProxy(
			QNetworkProxy::HttpProxy, addr, port, user, password));
	} else if (proxyTypeStr == "None") {
		// If we have no proxy set, set no proxy and return.
		QNetworkProxy::setApplicationProxy(
			QNetworkProxy(QNetworkProxy::NoProxy));
	} else {
		// If we have "Default" selected, set Qt to use the system proxy
		// settings.
		QNetworkProxyFactory::setUseSystemConfiguration(true);
	}

	qDebug() << "Detecting proxy settings...";
	QNetworkProxy proxy = QNetworkProxy::applicationProxy();
	m_network->setProxy(proxy);

	QString proxyDesc;
	if (proxy.type() == QNetworkProxy::NoProxy) {
		qDebug() << "Using no proxy is an option!";
		return;
	}
	switch (proxy.type()) {
		case QNetworkProxy::DefaultProxy:
			proxyDesc = "Default proxy: ";
			break;
		case QNetworkProxy::Socks5Proxy:
			proxyDesc = "Socks5 proxy: ";
			break;
		case QNetworkProxy::HttpProxy:
			proxyDesc = "HTTP proxy: ";
			break;
		case QNetworkProxy::HttpCachingProxy:
			proxyDesc = "HTTP caching: ";
			break;
		case QNetworkProxy::FtpCachingProxy:
			proxyDesc = "FTP caching: ";
			break;
		default:
			proxyDesc = "DERP proxy: ";
			break;
	}
	proxyDesc += QString("%1:%2").arg(proxy.hostName()).arg(proxy.port());
	qDebug() << proxyDesc;
}

shared_qobject_ptr<HttpMetaCache> Application::metacache()
{
	return m_metacache;
}

shared_qobject_ptr<QNetworkAccessManager> Application::network()
{
	return m_network;
}

shared_qobject_ptr<Meta::Index> Application::metadataIndex()
{
	if (!m_metadataIndex) {
		m_metadataIndex.reset(new Meta::Index());
	}
	return m_metadataIndex;
}

QString Application::getJarsPath()
{
	if (m_jarsPath.isEmpty()) {
#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
		auto appDir = QCoreApplication::applicationDirPath();
		auto installedPath =
			FS::PathCombine(appDir, "..", "share", BuildConfig.MESHMC_NAME);
		if (QDir(installedPath).exists()) {
			return installedPath;
		}
		return FS::PathCombine(appDir, "jars");
#else
		return FS::PathCombine(QCoreApplication::applicationDirPath(), "jars");
#endif
	}
	return m_jarsPath;
}
