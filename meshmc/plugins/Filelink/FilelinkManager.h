/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * FilelinkManager — core logic for cross-instance file linking.
 *
 * Creates hard-links between instance sub-directories (mods,
 * resourcepacks, shaderpacks) so that identical files share a
 * single on-disk copy.  A JSON manifest tracks every link so
 * they can be enumerated, verified, and removed cleanly.
 *
 * Platform support:
 *   Linux  — link(2)
 *   Windows — CreateHardLinkW()
 *   macOS  — NOT SUPPORTED (compile-time guard)
 */

#pragma once

#include "plugin/sdk/mmco_sdk.h"

#ifdef Q_OS_MAC
#error "Filelink plugin does not support macOS"
#endif

struct FilelinkEntry {
	QString sourceInstance; // instance ID that owns the original file
	QString targetInstance; // instance ID that received the link
	QString subDir;			// e.g. "mods", "resourcepacks"
	QString fileName;		// leaf file name
	QString sourcePath;		// absolute path of the source file
	QString targetPath;		// absolute path of the created link
	QDateTime linkedAt;
};

class FilelinkManager
{
  public:
	explicit FilelinkManager(const QString& pluginDataDir);

	/* Create hard-links for all files from sourceDir into targetDir.
	 * subDir is a label such as "mods" used in the manifest.
	 * Returns the number of files linked, or -1 on error. */
	int linkDirectory(const QString& sourceInstanceId, const QString& sourceDir,
					  const QString& targetInstanceId, const QString& targetDir,
					  const QString& subDir);

	/* Create a single hard-link and record it. */
	bool linkFile(const QString& sourceInstanceId, const QString& sourcePath,
				  const QString& targetInstanceId, const QString& targetPath,
				  const QString& subDir);

	/* Remove a previously created link (deletes the target file). */
	bool unlinkFile(const QString& targetInstanceId, const QString& targetPath);

	/* Remove all links targeting a given instance. */
	int unlinkAllForInstance(const QString& targetInstanceId);

	/* Verify that every recorded link still exists on disk. */
	QStringList verifyLinks(const QString& instanceId) const;

	/* Enumerate all links. */
	QList<FilelinkEntry> allLinks() const;

	/* Links targeting a specific instance. */
	QList<FilelinkEntry> linksForInstance(const QString& instanceId) const;

	/* Persist manifest to disk. */
	bool save() const;

  private:
	void load();
	static bool platformLink(const QString& source, const QString& target);
	static bool platformUnlink(const QString& path);

	QString m_manifestPath;
	QList<FilelinkEntry> m_entries;
};
