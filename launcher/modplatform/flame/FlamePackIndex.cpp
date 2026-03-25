#include "FlamePackIndex.h"

#include "Json.h"

void Flame::loadIndexedPack(Flame::IndexedPack & pack, QJsonObject & obj)
{
    pack.addonId = Json::requireInteger(obj, "id");
    pack.name = Json::requireString(obj, "name");
    pack.description = Json::ensureString(obj, "summary", "");

    // API v1: links.websiteUrl, API v2: websiteUrl at root
    if(obj.contains("links") && obj.value("links").isObject()) {
        pack.websiteUrl = Json::ensureString(obj.value("links").toObject(), "websiteUrl", "");
    } else {
        pack.websiteUrl = Json::ensureString(obj, "websiteUrl", "");
    }

    // API v1: logo is a single object, API v2: attachments is an array
    bool thumbnailFound = false;
    if(obj.contains("logo") && obj.value("logo").isObject()) {
        auto logoObj = obj.value("logo").toObject();
        pack.logoName = Json::ensureString(logoObj, "title", pack.name);
        pack.logoUrl = Json::ensureString(logoObj, "thumbnailUrl", "");
        thumbnailFound = !pack.logoUrl.isEmpty();
    }
    if(!thumbnailFound && obj.contains("attachments")) {
        auto attachments = Json::requireArray(obj, "attachments");
        for(auto attachmentRaw: attachments) {
            auto attachmentObj = Json::requireObject(attachmentRaw);
            bool isDefault = attachmentObj.value("isDefault").toBool(false);
            if(isDefault) {
                thumbnailFound = true;
                pack.logoName = Json::requireString(attachmentObj, "title");
                pack.logoUrl = Json::requireString(attachmentObj, "thumbnailUrl");
                break;
            }
        }
    }
    if(!thumbnailFound) {
        pack.logoName = pack.name;
        pack.logoUrl = "";
    }

    auto authors = Json::requireArray(obj, "authors");
    for(auto authorIter: authors) {
        auto author = Json::requireObject(authorIter);
        Flame::ModpackAuthor packAuthor;
        packAuthor.name = Json::requireString(author, "name");
        packAuthor.url = Json::ensureString(author, "url", "");
        pack.authors.append(packAuthor);
    }

    // API v1: mainFileId, API v2: defaultFileId
    int defaultFileId = 0;
    if(obj.contains("mainFileId")) {
        defaultFileId = Json::requireInteger(obj, "mainFileId");
    } else {
        defaultFileId = Json::requireInteger(obj, "defaultFileId");
    }

    bool found = false;
    // check if there are some files before adding the pack
    auto files = Json::ensureArray(obj, "latestFiles");
    for(auto fileIter: files) {
        auto file = Json::requireObject(fileIter);
        int id = Json::requireInteger(file, "id");

        // NOTE: for now, ignore everything that's not the default...
        if(id != defaultFileId) {
            continue;
        }

        // API v1: gameVersions, API v2: gameVersion
        QJsonArray versionArray;
        if(file.contains("gameVersions")) {
            versionArray = Json::requireArray(file, "gameVersions");
        } else {
            versionArray = Json::requireArray(file, "gameVersion");
        }
        if(versionArray.size() < 1) {
            continue;
        }

        found = true;
        break;
    }
    if(!found) {
        throw JSONValidationError(QString("Pack with no good file, skipping: %1").arg(pack.name));
    }
}

void Flame::loadIndexedPackVersions(Flame::IndexedPack & pack, QJsonArray & arr)
{
    QVector<Flame::IndexedVersion> unsortedVersions;
    for(auto versionIter: arr) {
        auto version = Json::requireObject(versionIter);
        Flame::IndexedVersion file; 

        file.addonId = pack.addonId;
        file.fileId = Json::requireInteger(version, "id");
        auto versionArray = Json::requireArray(version, "gameVersion");
        if(versionArray.size() < 1) {
            continue;
        }

        // pick the latest version supported
        file.mcVersion = versionArray[0].toString();
        file.version = Json::requireString(version, "displayName");
        file.downloadUrl = Json::requireString(version, "downloadUrl");
        unsortedVersions.append(file);
    }

    auto orderSortPredicate = [](const IndexedVersion & a, const IndexedVersion & b) -> bool
    {
        return a.fileId > b.fileId;
    };
    std::sort(unsortedVersions.begin(), unsortedVersions.end(), orderSortPredicate);
    pack.versions = unsortedVersions;
    pack.versionsLoaded = true;
}
