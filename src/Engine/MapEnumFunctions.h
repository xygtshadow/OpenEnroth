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

inline Segment<MapId> allMaps() {
    return {MAP_FIRST, MAP_LAST};
}
