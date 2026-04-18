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

#include <launch/LaunchStep.h>
#ifndef MeshMC_DISABLE_JAVA_DOWNLOADER
#include "java/download/JavaRuntime.h"
#include "java/download/JavaDownloadTask.h"
#endif
#include "net/NetJob.h"

class VerifyJavaInstall : public LaunchStep
{
	Q_OBJECT

  public:
	explicit VerifyJavaInstall(LaunchTask* parent) : LaunchStep(parent) {};
	~VerifyJavaInstall() override = default;

	void executeTask() override;
	bool canAbort() const override
	{
		return false;
	}

  private:
	int determineRequiredJavaMajor() const;
	QString findInstalledJava(int requiredMajor) const;
	QString javaInstallDir() const;
#ifndef MeshMC_DISABLE_JAVA_DOWNLOADER
	void autoDownloadJava(int requiredMajor);
	void fetchVersionList(int requiredMajor);
	void fetchRuntimes(const QString& versionId, int requiredMajor);
	void startDownload(const JavaDownload::RuntimeEntry& runtime,
					   int requiredMajor);
	void setJavaPathAndSucceed(const QString& javaPath);

	QString m_preferredVendor;
	NetJob::Ptr m_fetchJob;
	QByteArray m_fetchData;
	std::unique_ptr<JavaDownloadTask> m_downloadTask;
#endif
};
