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

#include "SequentialTask.h"

SequentialTask::SequentialTask(QObject* parent)
	: Task(parent), m_currentIndex(-1)
{
}

void SequentialTask::addTask(Task::Ptr task)
{
	m_queue.append(task);
}

void SequentialTask::executeTask()
{
	m_currentIndex = -1;
	startNext();
}

void SequentialTask::startNext()
{
	if (m_currentIndex != -1) {
		Task::Ptr previous = m_queue[m_currentIndex];
		disconnect(previous.get(), 0, this, 0);
	}
	m_currentIndex++;
	if (m_queue.isEmpty() || m_currentIndex >= m_queue.size()) {
		emitSucceeded();
		return;
	}
	Task::Ptr next = m_queue[m_currentIndex];
	connect(next.get(), &Task::failed, this,
			&SequentialTask::subTaskFailed);
	connect(next.get(), &Task::status, this,
			&SequentialTask::subTaskStatus);
	connect(next.get(), &Task::progress, this,
			&SequentialTask::subTaskProgress);
	connect(next.get(), &Task::succeeded, this, &SequentialTask::startNext);
	next->start();
}

void SequentialTask::subTaskFailed(const QString& msg)
{
	emitFailed(msg);
}
void SequentialTask::subTaskStatus(const QString& msg)
{
	setStatus(msg);
}
void SequentialTask::subTaskProgress(qint64 current, qint64 total)
{
	if (total == 0) {
		setProgress(0, 100);
		return;
	}
	setProgress(current, total);
}
