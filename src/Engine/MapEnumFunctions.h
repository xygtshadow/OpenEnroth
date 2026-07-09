#pragma once

#include "MapEnums.h"

/**
 * Is map an outdoor map?
 *
 * Classified by the map's file extension in mapstats.txt. Map id ranges can't be used here
 * because they differ between games - e.g. map #7 is Celeste (indoor) in MM7, but an outdoor
 * map in MM6.
 */
bool isMapOutdoor(MapId mapid);

/**
 * Is map an indoor map?
 */
bool isMapIndoor(MapId mapid);

/**
 * Is map an outdoor underwater map (requires wetsuit etc.)?
 *
 * Classified by the map's file name in mapstats.txt (MM7's Shoals). Map id ranges can't be used
 * here because they differ between games - map #15 is the Shoals in MM7 but New Sorpigal in MM6.
 */
bool isMapUnderwater(MapId mapid);

/**
 * Is hirelings interactions are forbidden on this map?
 *
 * Classified by the map's file name in mapstats.txt (MM7's Shoals and The Lincoln), for the same
 * reason as `isMapUnderwater`.
 */
bool isHirelingsBlockedOnMap(MapId mapid);

/**
 * 1-based file index of the map's level file inside MM6's games.lod, or -1 for an invalid map.
 *
 * MM6 stores maps this way in Lloyd's Beacon slots, the transport schedules and 2dEvents exit
 * maps. Computed as the case-insensitive rank of the map's file name among all mapstats entries
 * (the MM6 games.lod directory holds the map files first, sorted case-insensitively).
 */
int mm6GamesLodFileIndex(MapId mapId);

/**
 * Inverse of `mm6GamesLodFileIndex`; returns MAP_INVALID when no map has this index.
 */
MapId mm6MapIdFromGamesLodFileIndex(int index);

inline Segment<MapId> allMaps() {
    return {MAP_FIRST, MAP_LAST};
}
