#pragma once

#include "Utility/IndexedArray.h"

#include "Application/Paths/GameVersion.h"

#include "Engine/Data/HouseData.h"

class Blob;

void initializeHouses(const Blob &houses, GameVersion version);

extern IndexedArray<HouseData, HOUSE_FIRST, HOUSE_LAST> houseTable;
