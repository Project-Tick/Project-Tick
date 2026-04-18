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

#include <QStringList>
#include <QString>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QStringList>

#include <settings/Setting.h>

#include <QDebug>
#include "java/JavaUtils.h"
#include "java/JavaInstallList.h"
#include "FileSystem.h"

#define IBUS "@im=ibus"

JavaUtils::JavaUtils() {}

QString JavaUtils::managedJavaRoot()
{
	return FS::PathCombine(QDir::currentPath(), "java");
}

namespace
{
	QString javaBinaryName()
	{
#if defined(Q_OS_WIN)
		return "javaw.exe";
#else
		return "java";
#endif
	}

	void appendUniquePath(QList<QString>& paths, const QString& path)
	{
		if (!path.isEmpty() && !paths.contains(path)) {
			paths.append(path);
		}
	}

	void appendExecutablePath(QList<QString>& paths, const QString& path)
	{
		QFileInfo info(path);
		if (info.exists() && info.isFile()) {
			appendUniquePath(paths, info.absoluteFilePath());
		}
	}

	void appendJavaHome(QList<QString>& paths, const QString& homePath)
	{
		if (homePath.isEmpty()) {
			return;
		}

		QFileInfo info(homePath);
		if (info.exists() && info.isFile()) {
			appendExecutablePath(paths, info.absoluteFilePath());
			return;
		}

		const auto binaryName = javaBinaryName();
		const auto contentsHome = FS::PathCombine(homePath, "Contents", "Home");
		appendExecutablePath(paths,
							 FS::PathCombine(homePath, "bin", binaryName));
		appendExecutablePath(
			paths, FS::PathCombine(homePath, "jre", "bin", binaryName));
		appendExecutablePath(paths,
							 FS::PathCombine(contentsHome, "bin", binaryName));
		appendExecutablePath(
			paths, FS::PathCombine(FS::PathCombine(contentsHome, "jre"), "bin",
								   binaryName));
		appendExecutablePath(
			paths, FS::PathCombine(FS::PathCombine(homePath, "Contents"),
								   "Commands", binaryName));
		appendExecutablePath(
			paths,
			FS::PathCombine(
				FS::PathCombine(homePath, "libexec/openjdk.jdk/Contents/Home"),
				"bin", binaryName));
	}

	[[maybe_unused]] void scanJavaHomes(QList<QString>& paths,
										const QString& rootPath)
	{
		QDir root(rootPath);
		if (!root.exists()) {
			return;
		}

		const auto entries = root.entryInfoList(
			QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
		for (const auto& entry : entries) {
			appendJavaHome(paths, entry.absoluteFilePath());
		}
	}

	[[maybe_unused]] void appendManagedJavaCandidates(QList<QString>& paths,
													  const QString& rootPath)
	{
		QDir managedDir(rootPath);
		if (!managedDir.exists()) {
			return;
		}

		QDirIterator it(rootPath, QStringList() << javaBinaryName(),
						QDir::Files, QDirIterator::Subdirectories);
		while (it.hasNext()) {
			it.next();
			const auto candidate = QDir::fromNativeSeparators(it.filePath());
			if (candidate.contains("/bin/")) {
				appendUniquePath(paths, candidate);
			}
		}
	}
} // namespace

#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
static QString processLD_LIBRARY_PATH(const QString& LD_LIBRARY_PATH)
{
	QDir mmcBin(QCoreApplication::applicationDirPath());
	auto items = LD_LIBRARY_PATH.split(':');
	QStringList final;
	for (auto& item : items) {
		QDir test(item);
		if (test == mmcBin) {
			qDebug() << "Env:LD_LIBRARY_PATH ignoring path" << item;
			continue;
		}
		final.append(item);
	}
	return final.join(':');
}
#endif

QProcessEnvironment CleanEnviroment()
{
	// prepare the process environment
	QProcessEnvironment rawenv = QProcessEnvironment::systemEnvironment();
	QProcessEnvironment env;

	QStringList ignored = {"JAVA_ARGS",	   "CLASSPATH",		   "CONFIGPATH",
						   "JAVA_HOME",	   "JRE_HOME",		   "_JAVA_OPTIONS",
						   "JAVA_OPTIONS", "JAVA_TOOL_OPTIONS"};
	for (auto key : rawenv.keys()) {
		auto value = rawenv.value(key);
		// filter out dangerous java crap
		if (ignored.contains(key)) {
			qDebug() << "Env: ignoring" << key << value;
			continue;
		}
		// filter MeshMC-related things
		if (key.startsWith("QT_")) {
			qDebug() << "Env: ignoring" << key << value;
			continue;
		}
#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
		// Do not pass LD_* variables to java. They were intended for MeshMC
		if (key.startsWith("LD_")) {
			qDebug() << "Env: ignoring" << key << value;
			continue;
		}
		// Strip IBus
		// IBus is a Linux IME framework. For some reason, it breaks MC?
		if (key == "XMODIFIERS" && value.contains(IBUS)) {
			QString save = value;
			value.replace(IBUS, "");
			qDebug() << "Env: stripped" << IBUS << "from" << save << ":"
					 << value;
		}
		if (key == "GAME_PRELOAD") {
			env.insert("LD_PRELOAD", value);
			continue;
		}
		if (key == "GAME_LIBRARY_PATH") {
			env.insert("LD_LIBRARY_PATH", processLD_LIBRARY_PATH(value));
			continue;
		}
#endif
		// qDebug() << "Env: " << key << value;
		env.insert(key, value);
	}
#ifdef Q_OS_LINUX
	// HACK: Workaround for QTBUG42500
	if (!env.contains("LD_LIBRARY_PATH")) {
		env.insert("LD_LIBRARY_PATH", "");
	}
#endif

	return env;
}

JavaInstallPtr JavaUtils::MakeJavaPtr(QString path, QString id, QString arch)
{
	JavaInstallPtr javaVersion(new JavaInstall());

	javaVersion->id = id;
	javaVersion->arch = arch;
	javaVersion->path = path;

	return javaVersion;
}

JavaInstallPtr JavaUtils::GetDefaultJava()
{
	JavaInstallPtr javaVersion(new JavaInstall());

	javaVersion->id = "java";
	javaVersion->arch = "unknown";
#if defined(Q_OS_WIN32)
	javaVersion->path = "javaw";
#else
	javaVersion->path = "java";
#endif

	return javaVersion;
}

#if defined(Q_OS_WIN32)
QList<JavaInstallPtr> JavaUtils::FindJavaFromRegistryKey(DWORD keyType,
														 QString keyName,
														 QString keyJavaDir,
														 QString subkeySuffix)
{
	QList<JavaInstallPtr> javas;

	QString archType = "unknown";
	if (keyType == KEY_WOW64_64KEY)
		archType = "64";
	else if (keyType == KEY_WOW64_32KEY)
		archType = "32";

	HKEY jreKey;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, keyName.toStdString().c_str(), 0,
					  KEY_READ | keyType | KEY_ENUMERATE_SUB_KEYS,
					  &jreKey) == ERROR_SUCCESS) {
		// Read the current type version from the registry.
		// This will be used to find any key that contains the JavaHome value.
		char* value = new char[0];
		DWORD valueSz = 0;
		if (RegQueryValueExA(jreKey, "CurrentVersion", nullptr, nullptr,
							 (BYTE*)value, &valueSz) == ERROR_MORE_DATA) {
			value = new char[valueSz];
			RegQueryValueExA(jreKey, "CurrentVersion", nullptr, nullptr,
							 (BYTE*)value, &valueSz);
		}

		char subKeyName[255];
		DWORD subKeyNameSize, numSubKeys, retCode;

		// Get the number of subkeys
		RegQueryInfoKeyA(jreKey, nullptr, nullptr, nullptr, &numSubKeys,
						 nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
						 nullptr);

		// Iterate until RegEnumKeyEx fails
		for (DWORD i = 0; i < numSubKeys; i++) {
			subKeyNameSize = 255;
			retCode = RegEnumKeyExA(jreKey, i, subKeyName, &subKeyNameSize,
									nullptr, nullptr, nullptr, nullptr);
			if (retCode != ERROR_SUCCESS)
				continue;

			// Now open the registry key for the version that we just got.
			QString newKeyName = keyName + "\\" + subKeyName + subkeySuffix;

			HKEY newKey;
			if (RegOpenKeyExA(
					HKEY_LOCAL_MACHINE, newKeyName.toStdString().c_str(), 0,
					KEY_READ | KEY_WOW64_64KEY, &newKey) != ERROR_SUCCESS)
				continue;

			// Read the JavaHome value to find where Java is installed.
			value = new char[0];
			valueSz = 0;
			if (RegQueryValueExA(newKey, keyJavaDir.toStdString().c_str(),
								 nullptr, nullptr, (BYTE*)value,
								 &valueSz) == ERROR_MORE_DATA) {
				value = new char[valueSz];
				RegQueryValueExA(newKey, keyJavaDir.toStdString().c_str(),
								 nullptr, nullptr, (BYTE*)value, &valueSz);

				// Now, we construct the version object and add it to the list.
				JavaInstallPtr javaVersion(new JavaInstall());

				javaVersion->id = subKeyName;
				javaVersion->arch = archType;
				javaVersion->path = QDir(FS::PathCombine(value, "bin"))
										.absoluteFilePath("javaw.exe");
				javas.append(javaVersion);
			}

			RegCloseKey(newKey);
		}

		RegCloseKey(jreKey);
	}

	return javas;
}

QList<QString> JavaUtils::FindJavaPaths()
{
	QList<JavaInstallPtr> java_candidates;

	// Oracle
	QList<JavaInstallPtr> JRE64s = this->FindJavaFromRegistryKey(
		KEY_WOW64_64KEY, "SOFTWARE\\JavaSoft\\Java Runtime Environment",
		"JavaHome");
	QList<JavaInstallPtr> JDK64s = this->FindJavaFromRegistryKey(
		KEY_WOW64_64KEY, "SOFTWARE\\JavaSoft\\Java Development Kit",
		"JavaHome");
	QList<JavaInstallPtr> JRE32s = this->FindJavaFromRegistryKey(
		KEY_WOW64_32KEY, "SOFTWARE\\JavaSoft\\Java Runtime Environment",
		"JavaHome");
	QList<JavaInstallPtr> JDK32s = this->FindJavaFromRegistryKey(
		KEY_WOW64_32KEY, "SOFTWARE\\JavaSoft\\Java Development Kit",
		"JavaHome");

	// Oracle for Java 9 and newer
	QList<JavaInstallPtr> NEWJRE64s = this->FindJavaFromRegistryKey(
		KEY_WOW64_64KEY, "SOFTWARE\\JavaSoft\\JRE", "JavaHome");
	QList<JavaInstallPtr> NEWJDK64s = this->FindJavaFromRegistryKey(
		KEY_WOW64_64KEY, "SOFTWARE\\JavaSoft\\JDK", "JavaHome");
	QList<JavaInstallPtr> NEWJRE32s = this->FindJavaFromRegistryKey(
		KEY_WOW64_32KEY, "SOFTWARE\\JavaSoft\\JRE", "JavaHome");
	QList<JavaInstallPtr> NEWJDK32s = this->FindJavaFromRegistryKey(
		KEY_WOW64_32KEY, "SOFTWARE\\JavaSoft\\JDK", "JavaHome");

	// AdoptOpenJDK
	QList<JavaInstallPtr> ADOPTOPENJRE32s = this->FindJavaFromRegistryKey(
		KEY_WOW64_32KEY, "SOFTWARE\\AdoptOpenJDK\\JRE", "Path",
		"\\hotspot\\MSI");
	QList<JavaInstallPtr> ADOPTOPENJRE64s = this->FindJavaFromRegistryKey(
		KEY_WOW64_64KEY, "SOFTWARE\\AdoptOpenJDK\\JRE", "Path",
		"\\hotspot\\MSI");
	QList<JavaInstallPtr> ADOPTOPENJDK32s = this->FindJavaFromRegistryKey(
		KEY_WOW64_32KEY, "SOFTWARE\\AdoptOpenJDK\\JDK", "Path",
		"\\hotspot\\MSI");
	QList<JavaInstallPtr> ADOPTOPENJDK64s = this->FindJavaFromRegistryKey(
		KEY_WOW64_64KEY, "SOFTWARE\\AdoptOpenJDK\\JDK", "Path",
		"\\hotspot\\MSI");

	// Eclipse Foundation
	QList<JavaInstallPtr> FOUNDATIONJDK32s = this->FindJavaFromRegistryKey(
		KEY_WOW64_32KEY, "SOFTWARE\\Eclipse Foundation\\JDK", "Path",
		"\\hotspot\\MSI");
	QList<JavaInstallPtr> FOUNDATIONJDK64s = this->FindJavaFromRegistryKey(
		KEY_WOW64_64KEY, "SOFTWARE\\Eclipse Foundation\\JDK", "Path",
		"\\hotspot\\MSI");

	// Eclipse Adoptium
	QList<JavaInstallPtr> ADOPTIUMJRE32s = this->FindJavaFromRegistryKey(
		KEY_WOW64_32KEY, "SOFTWARE\\Eclipse Adoptium\\JRE", "Path",
		"\\hotspot\\MSI");
	QList<JavaInstallPtr> ADOPTIUMJRE64s = this->FindJavaFromRegistryKey(
		KEY_WOW64_64KEY, "SOFTWARE\\Eclipse Adoptium\\JRE", "Path",
		"\\hotspot\\MSI");
	QList<JavaInstallPtr> ADOPTIUMJDK32s = this->FindJavaFromRegistryKey(
		KEY_WOW64_32KEY, "SOFTWARE\\Eclipse Adoptium\\JDK", "Path",
		"\\hotspot\\MSI");
	QList<JavaInstallPtr> ADOPTIUMJDK64s = this->FindJavaFromRegistryKey(
		KEY_WOW64_64KEY, "SOFTWARE\\Eclipse Adoptium\\JDK", "Path",
		"\\hotspot\\MSI");

	// Microsoft
	QList<JavaInstallPtr> MICROSOFTJDK64s = this->FindJavaFromRegistryKey(
		KEY_WOW64_64KEY, "SOFTWARE\\Microsoft\\JDK", "Path", "\\hotspot\\MSI");

	// Azul Zulu
	QList<JavaInstallPtr> ZULU64s = this->FindJavaFromRegistryKey(
		KEY_WOW64_64KEY, "SOFTWARE\\Azul Systems\\Zulu", "InstallationPath");
	QList<JavaInstallPtr> ZULU32s = this->FindJavaFromRegistryKey(
		KEY_WOW64_32KEY, "SOFTWARE\\Azul Systems\\Zulu", "InstallationPath");

	// BellSoft Liberica
	QList<JavaInstallPtr> LIBERICA64s = this->FindJavaFromRegistryKey(
		KEY_WOW64_64KEY, "SOFTWARE\\BellSoft\\Liberica", "InstallationPath");
	QList<JavaInstallPtr> LIBERICA32s = this->FindJavaFromRegistryKey(
		KEY_WOW64_32KEY, "SOFTWARE\\BellSoft\\Liberica", "InstallationPath");

	// List x64 before x86
	java_candidates.append(JRE64s);
	java_candidates.append(NEWJRE64s);
	java_candidates.append(ADOPTOPENJRE64s);
	java_candidates.append(ADOPTIUMJRE64s);
	java_candidates.append(
		MakeJavaPtr("C:/Program Files/Java/jre8/bin/javaw.exe"));
	java_candidates.append(
		MakeJavaPtr("C:/Program Files/Java/jre7/bin/javaw.exe"));
	java_candidates.append(
		MakeJavaPtr("C:/Program Files/Java/jre6/bin/javaw.exe"));
	java_candidates.append(JDK64s);
	java_candidates.append(NEWJDK64s);
	java_candidates.append(ADOPTOPENJDK64s);
	java_candidates.append(FOUNDATIONJDK64s);
	java_candidates.append(ADOPTIUMJDK64s);
	java_candidates.append(MICROSOFTJDK64s);
	java_candidates.append(ZULU64s);
	java_candidates.append(LIBERICA64s);

	java_candidates.append(JRE32s);
	java_candidates.append(NEWJRE32s);
	java_candidates.append(ADOPTOPENJRE32s);
	java_candidates.append(ADOPTIUMJRE32s);
	java_candidates.append(
		MakeJavaPtr("C:/Program Files (x86)/Java/jre8/bin/javaw.exe"));
	java_candidates.append(
		MakeJavaPtr("C:/Program Files (x86)/Java/jre7/bin/javaw.exe"));
	java_candidates.append(
		MakeJavaPtr("C:/Program Files (x86)/Java/jre6/bin/javaw.exe"));
	java_candidates.append(JDK32s);
	java_candidates.append(NEWJDK32s);
	java_candidates.append(ADOPTOPENJDK32s);
	java_candidates.append(FOUNDATIONJDK32s);
	java_candidates.append(ADOPTIUMJDK32s);
	java_candidates.append(ZULU32s);
	java_candidates.append(LIBERICA32s);

	// Scan MeshMC's managed java directory
	// ({workdir}/java/{vendor}/{version}/bin/javaw.exe)
	QString managedJavaDir = JavaUtils::managedJavaRoot();
	QDir managedDir(managedJavaDir);
	if (managedDir.exists()) {
		QDirIterator vendorIt(managedJavaDir,
							  QDir::Dirs | QDir::NoDotAndDotDot);
		while (vendorIt.hasNext()) {
			vendorIt.next();
			QDirIterator versionIt(vendorIt.filePath(),
								   QDir::Dirs | QDir::NoDotAndDotDot);
			while (versionIt.hasNext()) {
				versionIt.next();
				QDirIterator binIt(versionIt.filePath(),
								   QStringList() << "javaw.exe", QDir::Files,
								   QDirIterator::Subdirectories);
				while (binIt.hasNext()) {
					binIt.next();
					if (binIt.filePath().contains("/bin/")) {
						java_candidates.append(MakeJavaPtr(binIt.filePath()));
					}
				}
			}
		}
	}

	java_candidates.append(MakeJavaPtr(this->GetDefaultJava()->path));

	QList<QString> candidates;
	for (JavaInstallPtr java_candidate : java_candidates) {
		if (!candidates.contains(java_candidate->path)) {
			candidates.append(java_candidate->path);
		}
	}

	return candidates;
}

#elif defined(Q_OS_MAC)
QList<QString> JavaUtils::FindJavaPaths()
{
	QList<QString> javas;
	appendUniquePath(javas, this->GetDefaultJava()->path);
	appendJavaHome(javas,
				   QProcessEnvironment::systemEnvironment().value("JAVA_HOME"));
	appendJavaHome(javas,
				   QProcessEnvironment::systemEnvironment().value("JDK_HOME"));
	appendJavaHome(javas,
				   QProcessEnvironment::systemEnvironment().value("JRE_HOME"));
	appendExecutablePath(
		javas, "/Applications/Xcode.app/Contents/Applications/Application "
			   "Loader.app/Contents/MacOS/itms/java/bin/java");
	appendJavaHome(
		javas,
		"/Library/Internet Plug-Ins/JavaAppletPlugin.plugin/Contents/Home");
	appendExecutablePath(javas, "/System/Library/Frameworks/JavaVM.framework/"
								"Versions/Current/Commands/java");
	scanJavaHomes(javas, "/Library/Java/JavaVirtualMachines");
	scanJavaHomes(javas, "/System/Library/Java/JavaVirtualMachines");
	scanJavaHomes(javas, "/opt/homebrew/opt");
	scanJavaHomes(javas, "/usr/local/opt");
	scanJavaHomes(javas, "/opt/jdk");
	scanJavaHomes(javas, "/opt/jdks");
	scanJavaHomes(javas, "/opt/java");
	scanJavaHomes(javas, "/usr/local/java");
	appendManagedJavaCandidates(javas, JavaUtils::managedJavaRoot());
	return javas;
}

#elif defined(Q_OS_LINUX)
QList<QString> JavaUtils::FindJavaPaths()
{
	qDebug() << "Linux Java detection incomplete - defaulting to \"java\"";

	QList<QString> javas;
	javas.append(this->GetDefaultJava()->path);
	auto scanJavaDir = [&](const QString& dirPath) {
		QDir dir(dirPath);
		if (!dir.exists())
			return;
		auto entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot |
										 QDir::NoSymLinks);
		for (auto& entry : entries) {

			QString prefix;
			if (entry.isAbsolute()) {
				prefix = entry.absoluteFilePath();
			} else {
				prefix = entry.filePath();
			}

			javas.append(FS::PathCombine(prefix, "jre/bin/java"));
			javas.append(FS::PathCombine(prefix, "bin/java"));
		}
	};
	// oracle RPMs
	scanJavaDir("/usr/java");
	// general locations used by distro packaging
	scanJavaDir("/usr/lib/jvm");
	scanJavaDir("/usr/lib64/jvm");
	scanJavaDir("/usr/lib32/jvm");
	// javas stored in MeshMC's folder (recursive scan for managed
	// java/{vendor}/{name}/... structure)
	{
		QDir javaBaseDir(JavaUtils::managedJavaRoot());
		if (javaBaseDir.exists()) {
			QDirIterator it(JavaUtils::managedJavaRoot(),
							QStringList() << "java", QDir::Files,
							QDirIterator::Subdirectories);
			while (it.hasNext()) {
				it.next();
				if (it.filePath().contains("/bin/")) {
					javas.append(it.filePath());
				}
			}
		}
	}
	// manually installed JDKs in /opt
	scanJavaDir("/opt/jdk");
	scanJavaDir("/opt/jdks");
	// Flatpak bundled JRE
	scanJavaDir("/app/jre");
	return javas;
}
#else
QList<QString> JavaUtils::FindJavaPaths()
{
	qDebug() << "Unknown operating system build - defaulting to \"java\"";

	QList<QString> javas;
	javas.append(this->GetDefaultJava()->path);

	return javas;
}
#endif
