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

#include <functional>
#include <memory>
#include <QObject>

namespace details
{
	struct DeleteQObjectLater {
		void operator()(QObject* obj) const
		{
			obj->deleteLater();
		}
	};
} // namespace details
/**
 * A unique pointer class with unique pointer semantics intended for derivates
 * of QObject Calls deleteLater() instead of destroying the contained object
 * immediately
 */
template <typename T>
using unique_qobject_ptr = std::unique_ptr<T, details::DeleteQObjectLater>;

/**
 * A shared pointer class with shared pointer semantics intended for derivates
 * of QObject Calls deleteLater() instead of destroying the contained object
 * immediately
 */
template <typename T> class shared_qobject_ptr
{
  public:
	shared_qobject_ptr() {}
	shared_qobject_ptr(T* wrap)
	{
		reset(wrap);
	}
	shared_qobject_ptr(const shared_qobject_ptr<T>& other)
	{
		m_ptr = other.m_ptr;
	}
	template <typename Derived>
	shared_qobject_ptr(const shared_qobject_ptr<Derived>& other)
		: m_ptr(other.unwrap())
	{
	}

  public:
	void reset(T* wrap)
	{
		if (wrap) {
			using namespace std::placeholders;
			m_ptr.reset(wrap, std::bind(&QObject::deleteLater, _1));
		} else {
			m_ptr.reset();
		}
	}
	void reset(const shared_qobject_ptr<T>& other)
	{
		m_ptr = other.m_ptr;
	}
	void reset()
	{
		m_ptr.reset();
	}
	T* get() const
	{
		return m_ptr.get();
	}
	T* operator->() const
	{
		return m_ptr.get();
	}
	T& operator*() const
	{
		return *m_ptr.get();
	}
	operator bool() const
	{
		return m_ptr.get() != nullptr;
	}
	const std::shared_ptr<T> unwrap() const
	{
		return m_ptr;
	}
	bool operator==(const shared_qobject_ptr<T>& other) const
	{
		return m_ptr == other.m_ptr;
	}
	bool operator!=(const shared_qobject_ptr<T>& other) const
	{
		return m_ptr != other.m_ptr;
	}

  private:
	std::shared_ptr<T> m_ptr;
};
