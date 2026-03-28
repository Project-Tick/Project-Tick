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

#include "PackageManifest.h"
#include <Json.h>
#include <QDir>
#include <QDirIterator>
#include <QCryptographicHash>
#include <QDebug>

#ifndef Q_OS_WIN32
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

namespace mojang_files {

const Hash hash_of_empty_string = "da39a3ee5e6b4b0d3255bfef95601890afd80709";

int Path::compare(const Path& rhs) const
{
    auto left_cursor = begin();
    auto left_end = end();
    auto right_cursor = rhs.begin();
    auto right_end = rhs.end();

    while (left_cursor != left_end && right_cursor != right_end)
    {
        if(*left_cursor < *right_cursor)
        {
            return -1;
        }
        else if(*left_cursor > *right_cursor)
        {
            return 1;
        }
        left_cursor++;
        right_cursor++;
    }

    if(left_cursor == left_end)
    {
        if(right_cursor == right_end)
        {
            return 0;
        }
        return -1;
    }
    return 1;
}

void Package::addFile(const Path& path, const File& file) {
    addFolder(path.parent_path());
    files[path] = file;
}

void Package::addFolder(Path folder) {
    if(!folder.has_parent_path()) {
        return;
    }
    do {
        folders.insert(folder);
        folder = folder.parent_path();
    } while(folder.has_parent_path());
}

void Package::addLink(const Path& path, const Path& target) {
    addFolder(path.parent_path());
    symlinks[path] = target;
}

void Package::addSource(const FileSource& source) {
    sources[source.hash] = source;
}


namespace {

FileSource parseFileDownloads(
    const QJsonObject &fileObject, File &file)
{
    FileSource bestSource;
    auto downloads = Json::requireObject(fileObject, "downloads");
    for (auto iter2 = downloads.begin();
         iter2 != downloads.end(); iter2++) {
        FileSource source;

        auto downloadObject = Json::requireObject(iter2.value());
        source.hash = Json::requireString(downloadObject, "sha1");
        source.size = Json::requireInteger(downloadObject, "size");
        source.url = Json::requireString(downloadObject, "url");

        auto compression = iter2.key();
        if (compression == "raw") {
            file.hash = source.hash;
            file.size = source.size;
            source.compression = Compression::Raw;
        } else if (compression == "lzma") {
            source.compression = Compression::Lzma;
        } else {
            continue;
        }
        bestSource.upgrade(source);
    }
    return bestSource;
}

void fromJson(QJsonDocument & doc, Package & out) {
    std::set<Path> seen_paths;
    if (!doc.isObject())
    {
        throw JSONValidationError("file manifest is not an object");
    }
    QJsonObject root = doc.object();

    auto filesObj = Json::ensureObject(root, "files");
    auto iter = filesObj.begin();
    while (iter != filesObj.end())
    {
        Path objectPath = Path(iter.key());
        auto value = iter.value();
        iter++;
        if(seen_paths.count(objectPath)) {
            throw JSONValidationError("duplicate path inside manifest, the manifest is invalid");
        }
        if (!value.isObject())
        {
            throw JSONValidationError("file entry inside manifest is not an an object");
        }
        seen_paths.insert(objectPath);

        auto fileObject = value.toObject();
        auto type = Json::requireString(fileObject, "type");
        if(type == "directory") {
            out.addFolder(objectPath);
            continue;
        }
        else if(type == "file") {
            File file;
            file.executable = Json::ensureBoolean(fileObject, QString("executable"), false);
            auto bestSource = parseFileDownloads(fileObject, file);
            if(bestSource.isBad()) {
                throw JSONValidationError("No valid compression method for file " + iter.key());
            }
            out.addFile(objectPath, file);
            out.addSource(bestSource);
        }
        else if(type == "link") {
            auto target = Json::requireString(fileObject, "target");
            out.symlinks[objectPath] = target;
            out.addLink(objectPath, target);
        }
        else {
            throw JSONValidationError("Invalid item type in manifest: " + type);
        }
    }
    // make sure the containing folder exists
    out.folders.insert(Path());
}
}

Package Package::fromManifestContents(const QByteArray& contents)
{
    Package out;
    try
    {
        auto doc = Json::requireDocument(contents, "Manifest");
        fromJson(doc, out);
        return out;
    }
    catch (const Exception &e)
    {
        qDebug() << QString("Unable to parse manifest: %1").arg(e.cause());
        out.valid = false;
        return out;
    }
}

Package Package::fromManifestFile(const QString & filename) {
    Package out;
    try
    {
        auto doc = Json::requireDocument(filename, filename);
        fromJson(doc, out);
        return out;
    }
    catch (const Exception &e)
    {
        qDebug() << QString("Unable to parse manifest file %1: %2").arg(filename, e.cause());
        out.valid = false;
        return out;
    }
}

#ifndef Q_OS_WIN32

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

namespace {
// FIXME: Qt obscures symlink targets by making them absolute.
// This is the workaround - we do it ourselves
bool actually_read_symlink_target(const QString & filepath, Path & out)
{
    struct ::stat st;
    // FIXME: here, we assume the native filesystem encoding. May the Gods have mercy upon our Souls.
    QByteArray nativePath = filepath.toUtf8();
    const char * filepath_cstr = nativePath.data();

    if (lstat(filepath_cstr, &st) != 0)
    {
        return false;
    }

    auto size = st.st_size ? st.st_size + 1 : PATH_MAX;
    QByteArray temp(size, '\0');
    // because we don't reliably know how long the link target is,
    // we loop and expand. POSIX is naff
    do
    {
        auto link_length = ::readlink(
            filepath_cstr, temp.data(), temp.size());
        if(link_length == -1)
        {
            return false;
        }
        if(link_length < temp.size())
        {
            // buffer was long enough
            out = Path(QString::fromUtf8(
                temp.constData(), link_length));
            return true;
        }
        temp.resize(temp.size() * 2);
    } while (true);
}
}
#endif

// FIXME: Qt filesystem abstraction is bad, but ... let's hope it doesn't break too much?
// FIXME: The error handling is just DEFICIENT
Package Package::fromInspectedFolder(const QString& folderPath)
{
    QDir root(folderPath);

    Package out;
    QDirIterator iterator(
        folderPath,
        QDir::NoDotAndDotDot | QDir::AllEntries
            | QDir::System | QDir::Hidden,
        QDirIterator::Subdirectories);
    while(iterator.hasNext()) {
        iterator.next();

        auto fileInfo = iterator.fileInfo();
        auto relPath = root.relativeFilePath(fileInfo.filePath());
        // FIXME: this is probably completely busted on Windows anyway, so just disable it.
        // Qt makes shit up and doesn't understand the platform details
        // TODO: Actually use a filesystem library that isn't terrible and has decen license.
        //       I only know one, and I wrote it. Sadly, currently proprietary. PAIN.
#ifndef Q_OS_WIN32
        if(fileInfo.isSymLink()) {
            Path targetPath;
            if(!actually_read_symlink_target(fileInfo.filePath(), targetPath)) {
                qCritical() << "Folder inspection: Unknown filesystem object:" << fileInfo.absoluteFilePath();
                out.valid = false;
            }
            out.addLink(relPath, targetPath);
        }
        else
#endif
        if(fileInfo.isDir()) {
            out.addFolder(relPath);
        }
        else if(fileInfo.isFile()) {
            File f;
            f.executable = fileInfo.isExecutable();
            f.size = fileInfo.size();
            // FIXME: async / optimize the hashing
            QFile input(fileInfo.absoluteFilePath());
            if(!input.open(QIODevice::ReadOnly)) {
                qCritical() << "Folder inspection: Failed to open file:" << fileInfo.absoluteFilePath();
                out.valid = false;
                break;
            }
            f.hash = QCryptographicHash::hash(input.readAll(), QCryptographicHash::Sha1).toHex().constData();
            out.addFile(relPath, f);
        }
        else {
            // Something else... oh my
            qCritical() << "Folder inspection: Unknown filesystem object:" << fileInfo.absoluteFilePath();
            out.valid = false;
            break;
        }
    }
    out.folders.insert(Path("."));
    out.valid = true;
    return out;
}

namespace {
struct ShallowFirstSort
{
    bool operator()(const Path &lhs, const Path &rhs) const
    {
        auto lhs_depth = lhs.length();
        auto rhs_depth = rhs.length();
        if(lhs_depth < rhs_depth)
        {
            return true;
        }
        else if(lhs_depth == rhs_depth)
        {
            if(lhs < rhs)
            {
                return true;
            }
        }
        return false;
    }
};

struct DeepFirstSort
{
    bool operator()(const Path &lhs, const Path &rhs) const
    {
        auto lhs_depth = lhs.length();
        auto rhs_depth = rhs.length();
        if(lhs_depth > rhs_depth)
        {
            return true;
        }
        else if(lhs_depth == rhs_depth)
        {
            if(lhs < rhs)
            {
                return true;
            }
        }
        return false;
    }
};
}

static void resolveFiles(
    const Package &from, const Package &to,
    UpdateOperations &out)
{
    for (auto iter = from.files.begin();
         iter != from.files.end(); iter++) {
        const auto &current_hash = iter->second.hash;
        const auto &current_executable = iter->second.executable;
        const auto &path = iter->first;

        auto iter2 = to.files.find(path);
        if (iter2 == to.files.end()) {
            out.deletes.push_back(path);
            continue;
        }
        auto new_hash = iter2->second.hash;
        auto new_executable = iter2->second.executable;
        if (current_hash != new_hash) {
            out.deletes.push_back(path);
            auto sourceIt = to.sources.find(iter2->second.hash);
            if (sourceIt == to.sources.end()) {
                continue;
            }
            out.downloads.emplace(
                std::pair<Path, FileDownload>{
                    path,
                    FileDownload(sourceIt->second,
                                 iter2->second.executable)
                }
            );
        } else if (current_executable != new_executable) {
            out.executable_fixes[path] = new_executable;
        }
    }
    for (auto iter = to.files.begin();
         iter != to.files.end(); iter++) {
        auto path = iter->first;
        if (!from.files.count(path)) {
            auto sourceIt = to.sources.find(iter->second.hash);
            if (sourceIt == to.sources.end()) {
                continue;
            }
            out.downloads.emplace(
                std::pair<Path, FileDownload>{
                    path,
                    FileDownload(sourceIt->second,
                                 iter->second.executable)
                }
            );
        }
    }
}

static void resolveFolders(
    const Package &from, const Package &to,
    UpdateOperations &out)
{
    std::set<Path, DeepFirstSort> remove_folders;
    std::set<Path, ShallowFirstSort> make_folders;
    for (auto from_path : from.folders) {
        if (to.folders.find(from_path) == to.folders.end()) {
            remove_folders.insert(from_path);
        }
    }
    for (auto &rmdir : remove_folders) {
        out.rmdirs.push_back(rmdir);
    }
    for (auto to_path : to.folders) {
        if (from.folders.find(to_path) == from.folders.end()) {
            make_folders.insert(to_path);
        }
    }
    for (auto &mkdir : make_folders) {
        out.mkdirs.push_back(mkdir);
    }
}

static void resolveSymlinks(
    const Package &from, const Package &to,
    UpdateOperations &out)
{
    for (auto iter = from.symlinks.begin();
         iter != from.symlinks.end(); iter++) {
        const auto &current_target = iter->second;
        const auto &path = iter->first;

        auto iter2 = to.symlinks.find(path);
        if (iter2 == to.symlinks.end()) {
            out.deletes.push_back(path);
            continue;
        }
        if (current_target != iter2->second) {
            out.deletes.push_back(path);
            out.mklinks[path] = iter2->second;
        }
    }
    for (auto iter = to.symlinks.begin();
         iter != to.symlinks.end(); iter++) {
        if (!from.symlinks.count(iter->first)) {
            out.mklinks[iter->first] = iter->second;
        }
    }
}

UpdateOperations UpdateOperations::resolve(
    const Package& from, const Package& to)
{
    UpdateOperations out;

    if (!from.valid || !to.valid) {
        out.valid = false;
        return out;
    }

    resolveFiles(from, to, out);
    resolveFolders(from, to, out);
    resolveSymlinks(from, to, out);

    out.valid = true;
    return out;
}

}
