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
 */
inline bool isMapUnderwater(MapId mapid) {
    return mapid == MAP_SHOALS;
}

/**
 * Is hirelings interactions are forbidden on this map?
 */
inline bool isHirelingsBlockedOnMap(MapId mapid) {
    return (mapid == MAP_SHOALS) || (mapid == MAP_LINCOLN);
}

inline Segment<MapId> allMaps() {
    return {MAP_FIRST, MAP_LAST};
}
