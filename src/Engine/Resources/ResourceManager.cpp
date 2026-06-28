#include "ResourceManager.h"

#include "Library/LodFormats/LodFormats.h"
#include "Library/FileSystem/Interface/FileSystem.h"

#include "EngineFileSystem.h"

ResourceManager::ResourceManager() = default;
ResourceManager::~ResourceManager() = default;

void ResourceManager::open(GameVersion version) {
    // MM7/MM8 keep the global event & table data (dsft.bin, dmonlist.bin, global.txt, the .evt scripts, ...)
    // in events.lod. MM6 has no events.lod - it ships that same data inside icons.lod, which also doubles as
    // the icon image archive. (MM6's new.lod, by contrast, is a new-game savegame template - party.bin etc. -
    // not event/table data.)
    std::string_view eventsLodPath = version == GAME_VERSION_MM6 ? "data/icons.lod" : "data/events.lod";

    _eventsLodReader.open(dfs->read(eventsLodPath));
    // TODO(captainurist):
    //  on exception:
    //      Error(localization->str(LSTR_MIGHT_AND_MAGIC_VII_IS_HAVING_TROUBLE), localization->str(LSTR_REINSTALL_NECESSARY));
    // but we can't use localization object here cause it's not yet initialized.
}

Blob ResourceManager::eventsData(std::string_view filename) {
    return lod::decodeMaybeCompressed(_eventsLodReader.read(filename));
}
