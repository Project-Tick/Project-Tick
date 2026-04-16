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

#include "LoggedProcess.h"
#include "MessageLevel.h"
#include <QDebug>

#ifdef Q_OS_UNIX
#include <signal.h>
#include <unistd.h>
#endif

LoggedProcess::LoggedProcess(QObject* parent) : QProcess(parent)
{
	// QProcess has a strange interface... let's map a lot of those into a few.
	connect(this, &QProcess::readyReadStandardOutput, this,
			&LoggedProcess::on_stdOut);
	connect(this, &QProcess::readyReadStandardError, this,
			&LoggedProcess::on_stdErr);
	connect(this, &QProcess::finished, this, &LoggedProcess::on_exit);
	connect(this, &QProcess::errorOccurred, this, &LoggedProcess::on_error);
	connect(this, &QProcess::stateChanged, this,
			&LoggedProcess::on_stateChange);

#ifdef Q_OS_UNIX
	// Create a new process group so we can kill the entire tree
	setChildProcessModifier([]() { setsid(); });
#endif
}

LoggedProcess::~LoggedProcess()
{
	if (m_is_detachable) {
		setProcessState(QProcess::NotRunning);
	}
}

QStringList reprocess(const QByteArray& data, QString& leftover)
{
	QString str = leftover + QString::fromLocal8Bit(data);

	str.remove('\r');
	QStringList lines = str.split("\n");
	leftover = lines.takeLast();
	return lines;
}

void LoggedProcess::on_stdErr()
{
	auto lines = reprocess(readAllStandardError(), m_err_leftover);
	emit log(lines, MessageLevel::StdErr);
}

void LoggedProcess::on_stdOut()
{
	auto lines = reprocess(readAllStandardOutput(), m_out_leftover);
	emit log(lines, MessageLevel::StdOut);
}

void LoggedProcess::on_exit(int exit_code, QProcess::ExitStatus status)
{
	// save the exit code
	m_exit_code = exit_code;

	// Flush console window
	if (!m_err_leftover.isEmpty()) {
		emit log({m_err_leftover}, MessageLevel::StdErr);
		m_err_leftover.clear();
	}
	if (!m_out_leftover.isEmpty()) {
		emit log({m_err_leftover}, MessageLevel::StdOut);
		m_out_leftover.clear();
	}

	// based on state, send signals
	if (!m_is_aborting) {
		if (status == QProcess::NormalExit) {
			//: Message displayed on instance exit
			emit log({tr("Process exited with code %1.").arg(exit_code)},
					 MessageLevel::MeshMC);
			changeState(LoggedProcess::Finished);
		} else {
			//: Message displayed on instance crashed
			if (exit_code == -1)
				emit log({tr("Process crashed.")}, MessageLevel::MeshMC);
			else
				emit log(
					{tr("Process crashed with exitcode %1.").arg(exit_code)},
					MessageLevel::MeshMC);
			changeState(LoggedProcess::Crashed);
		}
	} else {
		//: Message displayed after the instance exits due to kill request
		emit log({tr("Process was killed by user.")}, MessageLevel::Error);
		changeState(LoggedProcess::Aborted);
	}
}

void LoggedProcess::on_error(QProcess::ProcessError error)
{
	switch (error) {
		case QProcess::FailedToStart: {
			emit log({tr("The process failed to start.")}, MessageLevel::Fatal);
			changeState(LoggedProcess::FailedToStart);
			break;
		}
		// we'll just ignore those... never needed them
		case QProcess::Crashed:
		case QProcess::ReadError:
		case QProcess::Timedout:
		case QProcess::UnknownError:
		case QProcess::WriteError:
			break;
	}
}

void LoggedProcess::kill()
{
	m_is_aborting = true;
#ifdef Q_OS_UNIX
	// Kill the entire process group to ensure all child processes
	// (e.g. Java launched through a wrapper) are terminated
	auto pid = processId();
	if (pid > 0) {
		::kill(-pid, SIGKILL);
	}
#else
	QProcess::kill();
#endif
}

int LoggedProcess::exitCode() const
{
	return m_exit_code;
}

void LoggedProcess::changeState(LoggedProcess::State state)
{
	if (state == m_state)
		return;
	m_state = state;
	emit stateChanged(m_state);
}

LoggedProcess::State LoggedProcess::state() const
{
	return m_state;
}

void LoggedProcess::on_stateChange(QProcess::ProcessState state)
{
	switch (state) {
		case QProcess::NotRunning:
			break; // let's not - there are too many that handle this already.
		case QProcess::Starting: {
			if (m_state != LoggedProcess::NotRunning) {
				qWarning() << "Wrong state change for process from state"
						   << m_state << "to" << (int)LoggedProcess::Starting;
			}
			changeState(LoggedProcess::Starting);
			return;
		}
		case QProcess::Running: {
			if (m_state != LoggedProcess::Starting) {
				qWarning() << "Wrong state change for process from state"
						   << m_state << "to" << (int)LoggedProcess::Running;
			}
			changeState(LoggedProcess::Running);
			return;
		}
	}
}

#if defined Q_OS_WIN32
#include <windows.h>
#endif

qint64 LoggedProcess::processId() const
{
	return QProcess::processId();
}

void LoggedProcess::setDetachable(bool detachable)
{
	m_is_detachable = detachable;
}
