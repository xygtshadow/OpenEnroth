#include "ResourceManager.h"

#include "Library/LodFormats/LodFormats.h"
#include "Library/FileSystem/Interface/FileSystem.h"

#include "EngineFileSystem.h"

ResourceManager::ResourceManager() = default;
ResourceManager::~ResourceManager() = default;

void ResourceManager::open(GameVersion version) {
    // MM7/MM8 keep the global event & table data (dsft.bin, dmonlist.bin, ...) in events.lod.
    // MM6 has no events.lod - it ships the equivalent data in new.lod instead.
    std::string_view eventsLodPath = version == GAME_VERSION_MM6 ? "data/new.lod" : "data/events.lod";

    // MM6's new.lod contains duplicate entries (e.g. 'header.bin'); allow them (the first one wins).
    LodOpenFlags openFlags;
    if (version == GAME_VERSION_MM6)
        openFlags |= LOD_ALLOW_DUPLICATES;

    _eventsLodReader.open(dfs->read(eventsLodPath), openFlags);
    // TODO(captainurist):
    //  on exception:
    //      Error(localization->str(LSTR_MIGHT_AND_MAGIC_VII_IS_HAVING_TROUBLE), localization->str(LSTR_REINSTALL_NECESSARY));
    // but we can't use localization object here cause it's not yet initialized.
}

Blob ResourceManager::eventsData(std::string_view filename) {
    return lod::decodeMaybeCompressed(_eventsLodReader.read(filename));
}
