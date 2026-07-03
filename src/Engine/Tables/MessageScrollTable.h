#pragma once

#include <map>
#include <string>

#include "Engine/Objects/ItemEnums.h"

class Blob;

/**
 * @offset 0x4764C2
 */
void initializeMessageScrolls(const Blob &scrolls);

/**
 * Message scroll texts keyed by the scroll's item id, as read from scroll.txt. MM6 message scrolls
 * are item ids 500-581, MM7's are 700-781 (82 entries each), hence a map rather than an
 * `IndexedArray` over an id range.
 */
extern std::map<ItemId, std::string> pMessageScrolls;
