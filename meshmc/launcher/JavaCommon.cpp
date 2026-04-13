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

#include "JavaCommon.h"
#include "ui/dialogs/CustomMessageBox.h"
#include <MMCStrings.h>
#include <QRegularExpression>

bool JavaCommon::checkJVMArgs(QString jvmargs, QWidget* parent)
{
	if (jvmargs.contains("-XX:PermSize=") ||
		jvmargs.contains(QRegularExpression("-Xm[sx]")) ||
		jvmargs.contains("-XX-MaxHeapSize") ||
		jvmargs.contains("-XX:InitialHeapSize")) {
		auto warnStr =
			QObject::tr("You tried to manually set a JVM memory option (using "
						"\"-XX:PermSize\", \"-XX-MaxHeapSize\", "
						"\"-XX:InitialHeapSize\",  \"-Xmx\" or \"-Xms\").\n"
						"There are dedicated boxes for these in the settings "
						"(Java tab, in the Memory group at the top).\n"
						"This message will be displayed until you remove them "
						"from the JVM arguments.");
		CustomMessageBox::selectable(parent,
									 QObject::tr("JVM arguments warning"),
									 warnStr, QMessageBox::Warning)
			->exec();
		return false;
	}
	return true;
}

void JavaCommon::javaWasOk(QWidget* parent, JavaCheckResult result)
{
	QString text;
	text +=
		QObject::tr(
			"Java test succeeded!<br />Platform reported: %1<br />Java version "
			"reported: %2<br />Java vendor "
			"reported: %3<br />")
			.arg(result.realPlatform, result.javaVersion.toString(),
				 result.javaVendor);
	if (result.errorLog.size()) {
		auto htmlError = result.errorLog;
		htmlError.replace('\n', "<br />");
		text +=
			QObject::tr("<br />Warnings:<br /><font color=\"orange\">%1</font>")
				.arg(htmlError);
	}
	CustomMessageBox::selectable(parent, QObject::tr("Java test success"), text,
								 QMessageBox::Information)
		->show();
}

void JavaCommon::javaArgsWereBad(QWidget* parent, JavaCheckResult result)
{
	auto htmlError = result.errorLog;
	QString text;
	htmlError.replace('\n', "<br />");
	text += QObject::tr("The specified java binary didn't work with the "
						"arguments you provided:<br />");
	text += QString("<font color=\"red\">%1</font>").arg(htmlError);
	CustomMessageBox::selectable(parent, QObject::tr("Java test failure"), text,
								 QMessageBox::Warning)
		->show();
}

void JavaCommon::javaBinaryWasBad(QWidget* parent, JavaCheckResult result)
{
	QString text;
	text += QObject::tr("The specified java binary didn't work.<br />You "
						"should use the auto-detect feature, "
						"or set the path to the java executable.<br />");
	CustomMessageBox::selectable(parent, QObject::tr("Java test failure"), text,
								 QMessageBox::Warning)
		->show();
}

void JavaCommon::TestCheck::run()
{
	if (!JavaCommon::checkJVMArgs(m_args, m_parent)) {
		emit finished();
		return;
	}
	checker.reset(new JavaChecker());
	connect(checker.get(), &JavaChecker::checkFinished, this,
			&TestCheck::checkFinished);
	checker->m_path = m_path;
	checker->performCheck();
}

void JavaCommon::TestCheck::checkFinished(JavaCheckResult result)
{
	if (result.validity != JavaCheckResult::Validity::Valid) {
		javaBinaryWasBad(m_parent, result);
		emit finished();
		return;
	}
	checker.reset(new JavaChecker());
	connect(checker.get(), &JavaChecker::checkFinished, this,
			&TestCheck::checkFinishedWithArgs);
	checker->m_path = m_path;
	checker->m_args = m_args;
	checker->m_minMem = m_minMem;
	checker->m_maxMem = m_maxMem;
	if (result.javaVersion.requiresPermGen()) {
		checker->m_permGen = m_permGen;
	}
	checker->performCheck();
}

void JavaCommon::TestCheck::checkFinishedWithArgs(JavaCheckResult result)
{
	if (result.validity == JavaCheckResult::Validity::Valid) {
		javaWasOk(m_parent, result);
		emit finished();
		return;
	}
	javaArgsWereBad(m_parent, result);
	emit finished();
}
