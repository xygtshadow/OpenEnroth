#pragma once

#include <string_view>

#include "Application/Paths/GameVersion.h"

#include "Utility/Memory/Blob.h"

#include "Library/Lod/LodReader.h"

/**
 * This class provides access to everything in `/data` folder.
 */
class ResourceManager {
 public:
    ResourceManager();
    ~ResourceManager();

    void open(GameVersion version);

    Blob eventsData(std::string_view filename);

    /**
     * Like `eventsData`, but returns an empty `Blob` instead of throwing when the entry is absent
     * from the events LOD. Used for optional tables - e.g. placemon/hostile/history exist in MM7's
     * events.lod but not in MM6's icons.lod; their `Initialize` consumers no-op on an empty blob.
     */
    Blob eventsDataIfPresent(std::string_view filename);

 private:
    LodReader _eventsLodReader;
};
