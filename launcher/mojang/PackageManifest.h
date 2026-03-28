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

#pragma once

#include <QString>
#include <map>
#include <set>
#include <QStringList>
#include "tasks/Task.h"

namespace mojang_files {

using Hash = QString;
extern const Hash empty_hash;

// simple-ish path implementation. assumes always relative and does not allow '..' entries
class Path
{
public:
    using parts_type = QStringList;

    Path() = default;
    Path(QString string) {  // NOLINT(IMPLICIT_CONSTRUCTOR)
        auto parts_in = string.split('/');
        for(auto & part: parts_in) {
            if(part.isEmpty() || part == ".") {
                continue;
            }
            if(part == "..") {
                if(parts.size()) {
                    parts.pop_back();
                }
                continue;
            }
            parts.push_back(part);
        }
    }

    bool has_parent_path() const
    {
        return parts.size() > 0;
    }

    Path parent_path() const
    {
        if (parts.empty())
            return Path();
        return Path(parts.begin(), std::prev(parts.end()));
    }

    bool empty() const
    {
        return parts.empty();
    }

    int length() const
    {
        return parts.length();
    }

    bool operator==(const Path & rhs) const {
        return parts == rhs.parts;
    }

    bool operator!=(const Path & rhs) const {
        return parts != rhs.parts;
    }

    inline bool operator<(const Path& rhs) const
    {
        return compare(rhs) < 0;
    }

    parts_type::const_iterator begin() const
    {
        return parts.begin();
    }

    parts_type::const_iterator end() const
    {
        return parts.end();
    }

    QString toString() const {
        return parts.join("/");
    }

private:
    Path(const parts_type::const_iterator & start, const parts_type::const_iterator & end) {
        auto cursor = start;
        while(cursor != end) {
            parts.push_back(*cursor);
            cursor++;
        }
    }
    int compare(const Path& p) const;

    parts_type parts;
};


enum class Compression {
    Raw,
    Lzma,
    Unknown
};


struct FileSource
{
    Compression compression = Compression::Unknown;
    Hash hash;
    QString url;
    std::size_t size = 0;
    void upgrade(const FileSource & other) {
        if(compression == Compression::Unknown || other.size < size) {
            *this = other;
        }
    }
    bool isBad() const {
        return compression == Compression::Unknown;
    }
};

struct File
{
    Hash hash;
    bool executable;
    std::uint64_t size = 0;
};

struct Package {
    static Package fromInspectedFolder(const QString &folderPath);
    static Package fromManifestFile(const QString &path);
    static Package fromManifestContents(const QByteArray& contents);

    explicit operator bool() const
    {
        return valid;
    }
    void addFolder(Path folder);
    void addFile(const Path & path, const File & file);
    void addLink(const Path & path, const Path & target);
    void addSource(const FileSource & source);

    std::map<Hash, FileSource> sources;
    bool valid = true;
    std::set<Path> folders;
    std::map<Path, File> files;
    std::map<Path, Path> symlinks;
};

struct FileDownload : FileSource
{
    FileDownload(const FileSource& source, bool executable) {
        static_cast<FileSource &> (*this) = source;
        this->executable = executable;
    }
    bool executable = false;
};

struct UpdateOperations {
    static UpdateOperations resolve(const Package & from, const Package & to);
    bool valid = false;
    std::vector<Path> deletes;
    std::vector<Path> rmdirs;
    std::vector<Path> mkdirs;
    std::map<Path, FileDownload> downloads;
    std::map<Path, Path> mklinks;
    std::map<Path, bool> executable_fixes;
};

}
