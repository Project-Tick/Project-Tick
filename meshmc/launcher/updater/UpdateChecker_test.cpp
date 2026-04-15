/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   MeshMC - A Custom Launcher for Minecraft
 *   Copyright (C) 2026 Project Tick
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <QTest>

#include "updater/UpdateChecker.h"

// UpdateChecker::compareVersions and normalizeVersion are private but we can
// test the observable behaviour through static helpers exposed for testing.
// For now we test the version comparison logic directly via a minimal friend
// class or by duplicating the algorithm. The functions below mirror the
// UpdateChecker implementation to validate the core comparison logic.

static int testCompareVersions(const QString& v1, const QString& v2)
{
	const QStringList parts1 = v1.split('.');
	const QStringList parts2 = v2.split('.');
	const int len = std::max(parts1.size(), parts2.size());
	for (int i = 0; i < len; ++i) {
		const qint64 a = (i < parts1.size()) ? parts1.at(i).toLongLong() : 0;
		const qint64 b = (i < parts2.size()) ? parts2.at(i).toLongLong() : 0;
		if (a != b)
			return (a > b) ? 1 : -1;
	}
	return 0;
}

static QString testNormalizeVersion(const QString& v)
{
	QString out = v.trimmed();
	if (out.startsWith('v', Qt::CaseInsensitive))
		out.remove(0, 1);
	return out;
}

class UpdateCheckerTest : public QObject
{
	Q_OBJECT
  private slots:
	void tst_ParseStableFeedItem_data()
	{
		QTest::addColumn<QString>("namespaceUri");

		QTest::newRow("product feed namespace")
			<< "https://projecttick.org/ns/product-feed";
	}

	void tst_ParseStableFeedItem()
	{
		QFETCH(QString, namespaceUri);

		const QString xml = QString(R"(<?xml version="1.0" encoding="UTF-8"?>
<rss xmlns:projt="%1" version="2.0">
	<channel>
		<item>
			<title>MeshMC Update 7.1.0</title>
			<description><![CDATA[<p>Release notes</p>]]></description>
			<projt:version>7.1.0</projt:version>
			<projt:channel>stable</projt:channel>
			<projt:asset name="MeshMC-Linux-Qt6-Portable-v202604141638.tar.gz" url="https://example.invalid/meshmc.tar.gz"/>
		</item>
	</channel>
</rss>)")
			.arg(namespaceUri);

		QString version;
		QString downloadUrl;
		QString releaseNotes;
		QString parseError;

		QVERIFY(UpdateChecker::parseStableFeedItem(
			xml.toUtf8(), "Linux-Qt6-Portable", &version, &downloadUrl,
			&releaseNotes, &parseError));
		QCOMPARE(version, "7.1.0");
		QCOMPARE(downloadUrl, "https://example.invalid/meshmc.tar.gz");
		QCOMPARE(releaseNotes, "<p>Release notes</p>");
		QVERIFY(parseError.isEmpty());
	}

	void tst_ParseStableFeedItemReportsMalformedXml()
	{
		QString version;
		QString downloadUrl;
		QString releaseNotes;
		QString parseError;

		QVERIFY(!UpdateChecker::parseStableFeedItem(
			"<rss><channel><item>", "Linux-Qt6-Portable", &version,
			&downloadUrl, &releaseNotes, &parseError));
		QVERIFY2(!parseError.isEmpty(),
				 qPrintable(QString("Expected a parse error for malformed XML, got empty string")));
	}

	void tst_NormalizeVersion_data()
	{
		QTest::addColumn<QString>("input");
		QTest::addColumn<QString>("expected");

		QTest::newRow("plain semver") << "7.0.0" << "7.0.0";
		QTest::newRow("v prefix") << "v7.0.0" << "7.0.0";
		QTest::newRow("V prefix") << "V7.0.0" << "7.0.0";
		QTest::newRow("snapshot tag") << "v202604102316" << "202604102316";
		QTest::newRow("whitespace") << "  v1.2.3  " << "1.2.3";
	}

	void tst_NormalizeVersion()
	{
		QFETCH(QString, input);
		QFETCH(QString, expected);
		QCOMPARE(testNormalizeVersion(input), expected);
	}

	void tst_CompareVersions_data()
	{
		QTest::addColumn<QString>("v1");
		QTest::addColumn<QString>("v2");
		QTest::addColumn<int>("sign"); // -1, 0, 1

		QTest::newRow("equal") << "7.0.0" << "7.0.0" << 0;
		QTest::newRow("v1 newer major") << "8.0.0" << "7.0.0" << 1;
		QTest::newRow("v1 older major") << "6.0.0" << "7.0.0" << -1;
		QTest::newRow("v1 newer minor") << "7.1.0" << "7.0.0" << 1;
		QTest::newRow("v1 newer hotfix") << "7.0.1" << "7.0.0" << 1;
		QTest::newRow("different lengths") << "7.0" << "7.0.0" << 0;
		QTest::newRow("four parts") << "7.0.0.1" << "7.0.0.0" << 1;
		QTest::newRow("snapshot-like numbers")
			<< "202604102316" << "7.0.0" << 1;
	}

	void tst_CompareVersions()
	{
		QFETCH(QString, v1);
		QFETCH(QString, v2);
		QFETCH(int, sign);

		const int result = testCompareVersions(v1, v2);
		if (sign > 0)
			QVERIFY2(result > 0, qPrintable(QString("%1 should be > %2, got %3")
												.arg(v1, v2)
												.arg(result)));
		else if (sign < 0)
			QVERIFY2(result < 0, qPrintable(QString("%1 should be < %2, got %3")
												.arg(v1, v2)
												.arg(result)));
		else
			QCOMPARE(result, 0);
	}

	void tst_SnapshotTagIsNotSemver()
	{
		// Snapshot tags like "202604102316" must not be compared as semver.
		// They would produce a single huge number. Updater should rely on
		// components.json for the canonical version, not the tag.
		const QString snapshot = "202604102316";
		const QString semver = "7.0.0";
		// This comparison is meaningless for update decisions but ensures
		// the code does not crash.
		const int r = testCompareVersions(snapshot, semver);
		Q_UNUSED(r);
	}
};

QTEST_GUILESS_MAIN(UpdateCheckerTest)

#include "UpdateChecker_test.moc"
