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
#include "FileSystem.h"

#include <QDir>
#include <QProcess>
#include <QProcessEnvironment>

#include <csignal>
#include <cstdlib>
#ifndef Q_OS_WIN
#include <unistd.h>
#endif

// #define BREAK_INFINITE_LOOP
// #define BREAK_EXCEPTION
// #define BREAK_RETURN

#ifdef BREAK_INFINITE_LOOP
#include <thread>
#include <chrono>
#endif

static void launchCrashReporter()
{
	// Locate the crash reporter binary next to the running executable.
	QString crashReporterName = "meshmc-crashreporter";
#ifdef Q_OS_WIN
	crashReporterName += ".exe";
#endif
	QString crashReporterPath =
		FS::PathCombine(QApplication::applicationDirPath(), crashReporterName);

	if (!QFile::exists(crashReporterPath)) {
		return;
	}

	QStringList args;
	args << "--logdir" << QDir::currentPath();
	args << "--name" << BuildConfig.MESHMC_NAME;

	QString apiKey = "public";
	if (APPLICATION && APPLICATION->settings()) {
		QString key = APPLICATION->settings()->get("PasteEEAPIKey").toString();
		if (key != "meshmc" && !key.isEmpty()) {
			apiKey = key;
		} else {
			apiKey = BuildConfig.PASTE_EE_KEY;
		}
	}
	args << "--apikey" << apiKey;

	// Flush the log file before launching the crash reporter
	if (APPLICATION && APPLICATION->logFile) {
		APPLICATION->logFile->flush();
	}

	QProcess::startDetached(crashReporterPath, args);
}

static void crashSignalHandler(int sig)
{
	// Re-set default handler to avoid infinite loops
	signal(sig, SIG_DFL);

	launchCrashReporter();

	// Re-raise the signal so the default handler produces a core dump etc.
	raise(sig);
}

int main(int argc, char* argv[])
{
#ifdef BREAK_INFINITE_LOOP
	while (true) {
		std::this_thread::sleep_for(std::chrono::milliseconds(250));
	}
#endif
#ifdef BREAK_EXCEPTION
	throw 42;
#endif
#ifdef BREAK_RETURN
	return 42;
#endif

#if (QT_VERSION >= QT_VERSION_CHECK(6, 8, 0))
	QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

	// initialize Qt
#ifdef Q_OS_LINUX
	{
		QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

		// Prefer Wayland backend when running under a Wayland session
		if (!env.contains("QT_QPA_PLATFORM") &&
			env.value("XDG_SESSION_TYPE") == "wayland") {
			qputenv("QT_QPA_PLATFORM", "wayland");
		}

		// Use xdgdesktopportal for file dialogs and theming on all
		// Linux DEs.  The old "gtk3" theme forced Qt to use GTK3's
		// native file-dialog integration, which deadlocks/freezes
		// on GNOME/Mutter (and other GTK-based compositors) with
		// Qt 6.  xdgdesktopportal delegates to the DE's own portal
		// implementation and works everywhere.
		if (!env.contains("QT_QPA_PLATFORMTHEME")) {
			qputenv("QT_QPA_PLATFORMTHEME", "xdgdesktopportal");
		}
	}
#endif

	Application app(argc, argv);

	// Install crash signal handlers to launch meshmc-crashreporter
	signal(SIGSEGV, crashSignalHandler);
	signal(SIGABRT, crashSignalHandler);
#ifndef Q_OS_WIN
	signal(SIGBUS, crashSignalHandler);
#endif

	switch (app.status()) {
		case Application::StartingUp:
		case Application::Initialized: {
			Q_INIT_RESOURCE(multimc);
			Q_INIT_RESOURCE(backgrounds);
			Q_INIT_RESOURCE(documents);
			Q_INIT_RESOURCE(meshmc);

			Q_INIT_RESOURCE(pe_dark);
			Q_INIT_RESOURCE(pe_light);
			Q_INIT_RESOURCE(pe_blue);
			Q_INIT_RESOURCE(pe_colored);
			Q_INIT_RESOURCE(breeze_dark);
			Q_INIT_RESOURCE(breeze_light);
			Q_INIT_RESOURCE(OSX);
			Q_INIT_RESOURCE(iOS);
			Q_INIT_RESOURCE(flat);
			Q_INIT_RESOURCE(flat_white);

			int ret = app.exec();

			// Use _exit() to terminate immediately after the Qt event
			// loop ends.  All meaningful cleanup (instance save, plugin
			// shutdown, log flush/close) has already happened in the
			// Application::aboutToQuit handler while Qt was still alive.
			//
			// A normal return would run C++ static destructors via
			// exit()/__cxa_finalize.  Plugin .mmco modules statically
			// link MeshMC_logic, which embeds a duplicate global
			// `const Config BuildConfig` (non-trivial dtor with ~20
			// QString members).  Those duplicate destructors corrupt
			// the heap when they run after Qt is torn down, causing
			// glibc's "corrupted double-linked list" abort.
			//
			// _exit() bypasses all atexit handlers and static dtors,
			// avoiding the double-destruction entirely.  The OS
			// reclaims all process memory.
			_exit(ret);
		}
		case Application::Failed:
			return 1;
		case Application::Succeeded:
			return 0;
	}
}
