#include "Transport.h"

#include <string>
#include <array>
#include <vector>

#include "GUI/UI/UIStatusBar.h"
#include "GUI/GUIFont.h"
#include "GUI/GUIButton.h"
#include "GUI/GUIMessageQueue.h"

#include "Engine/Data/HouseEnumFunctions.h"
#include "Engine/AssetsManager.h"
#include "Engine/Localization.h"
#include "Engine/SaveLoad.h"
#include "Engine/PriceCalculator.h"
#include "Engine/Graphics/Camera.h"
#include "Engine/Objects/NPC.h"
#include "Engine/MapInfo.h"
#include "Engine/Party.h"
#include "Engine/Engine.h"

#include "Media/Audio/AudioPlayer.h"

#include "Utility/IndexedArray.h"

struct TransportInfo {
    MapId uMapInfoID;
    std::array<unsigned char, 7> pSchedule;
    int uTravelTime; // In days.
    Vec3f arrivalPos;
    int arrival_view_yaw;
    QuestBit uQuestBit;  // quest bit required to set for this travel option to be enabled; otherwise 0
};

// 004F09B0
static constexpr std::array<TransportInfo, 35> transportSchedule = {{
//    location name        schedule            days  pos                     yaw   qbit
    { MAP_ERATHIA,         {1, 0, 1, 0, 1, 0, 0}, 2, {-18048,  4636,  833},  1536, QBIT_INVALID },  // for stable
    { MAP_TULAREAN_FOREST, {0, 1, 0, 1, 0, 1, 0}, 2, {-2527,  -6773,  1153}, 896,  QBIT_INVALID },
    { MAP_TATALIA,         {1, 0, 1, 0, 1, 0, 0}, 2, { 4730,  -10580, 320},  1024, QBIT_INVALID },
    { MAP_HARMONDALE,      {0, 1, 0, 1, 0, 1, 0}, 2, {-5692,   11137, 1},    1024, QBIT_INVALID },
    { MAP_DEYJA,           {1, 0, 0, 1, 0, 0, 0}, 3, { 7227,  -16007, 2625}, 640,  QBIT_INVALID },
    { MAP_BRACADA_DESERT,  {0, 0, 1, 0, 0, 1, 0}, 3, { 8923,   17191, 1},    512,  QBIT_INVALID },
    { MAP_AVLEE,           {1, 0, 1, 0, 1, 0, 0}, 3, { 17059,  12331, 512},  1152, QBIT_INVALID },
    { MAP_DEYJA,           {0, 1, 0, 0, 1, 0, 1}, 2, { 7227,  -16007, 2625}, 640,  QBIT_INVALID },
    { MAP_HARMONDALE,      {0, 1, 0, 1, 0, 1, 0}, 2, {-5692,   11137, 1},    1024, QBIT_INVALID },
    { MAP_ERATHIA,         {1, 0, 1, 0, 1, 0, 0}, 3, {-18048,  4636,  833},  1536, QBIT_INVALID },
    { MAP_TULAREAN_FOREST, {0, 1, 0, 1, 0, 1, 0}, 2, {-2527,  -6773,  1153}, 896,  QBIT_INVALID },
    { MAP_ERATHIA,         {1, 0, 1, 0, 1, 0, 1}, 3, {-18048,  4636,  833},  1536, QBIT_INVALID },
    { MAP_HARMONDALE,      {0, 1, 0, 0, 0, 1, 0}, 5, {-5692,   11137, 1},    1024, QBIT_INVALID },
    { MAP_ERATHIA,         {0, 1, 0, 1, 0, 1, 0}, 2, {-18048,  4636,  833},  1536, QBIT_INVALID },
    { MAP_TULAREAN_FOREST, {0, 1, 0, 1, 0, 1, 0}, 3, {-2527,  -6773,  1153}, 896,  QBIT_INVALID },
    { MAP_DEYJA,           {0, 0, 1, 0, 0, 0, 1}, 5, { 7227,  -16007, 2625}, 640,  QBIT_INVALID },
    { MAP_TATALIA,         {0, 1, 0, 1, 0, 1, 0}, 2, {-2183,  -6941,  97},   0,    QBIT_INVALID },
    { MAP_AVLEE,           {1, 0, 0, 0, 1, 0, 0}, 4, { 7913,   9476,  193},  0,    QBIT_INVALID },
    { MAP_EVENMORN_ISLAND, {0, 0, 0, 0, 0, 0, 1}, 7, { 15616,  6390,  193},  1536, QBIT_EVENMORN_MAP_FOUND },
    { MAP_BRACADA_DESERT,  {0, 0, 1, 0, 0, 0, 0}, 6, { 19171, -19722, 193},  1024, QBIT_INVALID },
    { MAP_AVLEE,           {0, 1, 0, 1, 0, 1, 0}, 3, { 7913,   9476,  193},  0,    QBIT_INVALID },
    { MAP_BRACADA_DESERT,  {1, 0, 1, 0, 0, 0, 0}, 6, { 19171, -19722, 193},  1024, QBIT_INVALID },
    { MAP_TATALIA,         {1, 0, 1, 0, 1, 0, 0}, 4, {-2183,  -6941,  97},   0,    QBIT_INVALID },
    { MAP_TULAREAN_FOREST, {0, 0, 0, 0, 0, 1, 0}, 6, {-709,   -14087, 193},  1024, QBIT_INVALID },  // for boat
    { MAP_ERATHIA,         {0, 0, 0, 0, 0, 0, 1}, 6, {-10471,  13497, 193},  1536, QBIT_INVALID },
    { MAP_EVENMORN_ISLAND, {0, 1, 0, 1, 0, 0, 0}, 1, { 15616,  6390,  193},  1536, QBIT_EVENMORN_MAP_FOUND },
    { MAP_BRACADA_DESERT,  {0, 1, 0, 1, 0, 0, 0}, 1, { 19171, -19722, 193},  1024, QBIT_INVALID },
    { MAP_ERATHIA,         {0, 1, 0, 1, 0, 1, 0}, 2, {-10471,  13497, 193},  1536, QBIT_INVALID },
    { MAP_BRACADA_DESERT,  {1, 0, 1, 0, 0, 0, 0}, 4, { 19171, -19722, 193},  1024, QBIT_INVALID },
    { MAP_EVENMORN_ISLAND, {0, 0, 0, 0, 0, 0, 1}, 5, { 15616,  6390,  193},  1536, QBIT_EVENMORN_MAP_FOUND },
    { MAP_AVLEE,           {0, 0, 0, 0, 1, 0, 0}, 5, { 7913,   9476,  193},  0,    QBIT_INVALID },
    { MAP_ERATHIA,         {0, 1, 0, 0, 0, 1, 0}, 4, {-10471,  13497, 193},  1536, QBIT_INVALID },
    { MAP_TULAREAN_FOREST, {1, 0, 1, 0, 1, 0, 0}, 3, {-709,   -14087, 193},  1024, QBIT_INVALID },
    { MAP_TATALIA,         {0, 0, 0, 1, 0, 0, 0}, 5, {-2183,  -6941,  97},   0,    QBIT_INVALID },
    { MAP_ARENA,           {0, 0, 0, 0, 0, 0, 1}, 4, { 3844,   2906,  193},  512,  QBIT_INVALID }
}};

static constexpr IndexedArray<std::array<int, 4>, HOUSE_FIRST_TRANSPORT, HOUSE_LAST_TRANSPORT> transportRoutes = {
    {HOUSE_STABLE_HARMONDALE,       { 0, 1, 1, 34 }},
    {HOUSE_STABLE_ERATHIA,          { 2, 3, 4, 5 }},
    {HOUSE_STABLE_TULAREAN_FOREST,  { 6, 7, 8, 8 }},
    {HOUSE_STABLE_DEYJA,            { 9, 10, 10, 10 }},
    {HOUSE_STABLE_BRACADA_DESERT,   { 11, 11, 12, 12 }},
    {HOUSE_STABLE_TATALIA,          { 13, 13, 13, 13 }},
    {HOUSE_STABLE_AVLEE,            { 14, 14, 15, 15 }},
    {HOUSE_61,                      { 255, 255, 255, 255 }},
    {HOUSE_62,                      { 255, 255, 255, 255 }},
    {HOUSE_BOAT_EMERALD_ISLAND,     { 255, 255, 255, 255 }},
    {HOUSE_BOAT_ERATHIA,            { 16, 17, 18, 19 }},
    {HOUSE_BOAT_TULAREAN_FOREST,    { 18, 20, 21, 21 }},
    {HOUSE_BOAT_BRACADA_DESERT,     { 22, 23, 24, 25 }},
    {HOUSE_BOAT_EVENMORN_ISLAND,    { 22, 22, 23, 23 }},
    {HOUSE_68,                      { 255, 255, 255, 255 }},
    {HOUSE_BOAT_TATALIA,            { 27, 28, 29, 30 }},
    {HOUSE_BOAT_AVLEE,              { 31, 32, 33, 33 }},
    {HOUSE_71,                      { 24, 24, 24, 24 }},
    {HOUSE_72,                      { 255, 255, 255, 255 }},
    {HOUSE_73,                      { 255, 255, 255, 255 }}
};

// MM6 destination map ids (mapstats.txt rows).
static constexpr MapId MAP_MM6_HERMITS_ISLE = static_cast<MapId>(3);
static constexpr MapId MAP_MM6_KRIEGSPIRE = static_cast<MapId>(4);
static constexpr MapId MAP_MM6_BLACKSHIRE = static_cast<MapId>(5);
static constexpr MapId MAP_MM6_FROZEN_HIGHLANDS = static_cast<MapId>(7);
static constexpr MapId MAP_MM6_FREE_HAVEN = static_cast<MapId>(8);
static constexpr MapId MAP_MM6_MIRE_OF_THE_DAMNED = static_cast<MapId>(9);
static constexpr MapId MAP_MM6_SILVER_COVE = static_cast<MapId>(10);
static constexpr MapId MAP_MM6_BOOTLEG_BAY = static_cast<MapId>(11);
static constexpr MapId MAP_MM6_CASTLE_IRONFIST = static_cast<MapId>(12);
static constexpr MapId MAP_MM6_EEL_INFESTED_WATERS = static_cast<MapId>(13);
static constexpr MapId MAP_MM6_MISTY_ISLANDS = static_cast<MapId>(14);
static constexpr MapId MAP_MM6_NEW_SORPIGAL = static_cast<MapId>(15);
static constexpr MapId MAP_MM6_ARENA = static_cast<MapId>(52);

// MM6 quest bits gating some routes: 178/179 are set on first (on-foot) visit to
// Free Haven / Silver Cove, 168 enables the secret Silver Helm routes.
static constexpr QuestBit QBIT_MM6_VISITED_FREE_HAVEN = static_cast<QuestBit>(178);
static constexpr QuestBit QBIT_MM6_VISITED_SILVER_COVE = static_cast<QuestBit>(179);
static constexpr QuestBit QBIT_MM6_SILVER_HELM_ROUTES = static_cast<QuestBit>(168);

// MM6.EXE 0x4C3F20: TravelInfo[36]. Destination map ids translated from the stored
// 1-based games.lod file indices.
static constexpr std::array<TransportInfo, 36> transportScheduleMm6 = {{
//    location name                  schedule            days  pos                     yaw   qbit
    { MAP_MM6_CASTLE_IRONFIST,      {1, 0, 1, 0, 1, 0, 0},  2, { 14317,   2696,   96}, 1024, QBIT_INVALID },  // 0: New Sorpigal stables
    { MAP_MM6_NEW_SORPIGAL,         {1, 0, 1, 0, 1, 0, 0},  2, {-10464,  -9386,  161},    0, QBIT_INVALID },  // 1: Ironfist stables
    { MAP_MM6_FREE_HAVEN,           {0, 1, 0, 0, 0, 1, 0},  4, {  5010,  13225,  161}, 1536, QBIT_MM6_VISITED_FREE_HAVEN },
    { MAP_MM6_ARENA,                {0, 0, 0, 0, 0, 0, 1},  1, {  3833,   2913,  193},  512, QBIT_INVALID },
    { MAP_MM6_BLACKSHIRE,           {1, 0, 0, 1, 0, 0, 0},  3, {-14595,  14450,   97}, 1024, QBIT_INVALID },  // 4: Free Haven stables (west)
    { MAP_MM6_KRIEGSPIRE,           {0, 1, 0, 0, 1, 0, 0},  3, {  5627, -18515,  256},  512, QBIT_INVALID },
    { MAP_MM6_FROZEN_HIGHLANDS,     {0, 0, 1, 0, 0, 1, 0},  3, { -4444,  14865,   97},  512, QBIT_INVALID },
    { MAP_MM6_SILVER_COVE,          {1, 0, 0, 1, 0, 0, 0},  4, {  7293,  -6558,  225},  512, QBIT_INVALID },  // 7: Free Haven stables (east)
    { MAP_MM6_CASTLE_IRONFIST,      {0, 1, 0, 0, 1, 0, 0},  4, { 14317,   2696,   96}, 1024, QBIT_INVALID },
    { MAP_MM6_MIRE_OF_THE_DAMNED,   {0, 0, 1, 0, 0, 1, 0},  5, {-18439,   8569,  256},    0, QBIT_INVALID },
    { MAP_MM6_FREE_HAVEN,           {1, 0, 0, 0, 1, 0, 0},  5, {  5010,  13225,  161}, 1536, QBIT_INVALID },  // 10: Darkmoor stables
    { MAP_MM6_FREE_HAVEN,           {1, 0, 0, 0, 1, 0, 0},  4, {  5010,  13225,  161}, 1536, QBIT_INVALID },  // 11: Silver Cove stables
    { MAP_MM6_FREE_HAVEN,           {1, 0, 0, 1, 0, 0, 0},  3, {  5010,  13225,  161}, 1536, QBIT_INVALID },  // 12: White Cap stables
    { MAP_MM6_FREE_HAVEN,           {0, 0, 1, 0, 0, 1, 0},  3, {  5010,  13225,  161}, 1536, QBIT_INVALID },  // 13: Kriegspire stables
    { MAP_MM6_FREE_HAVEN,           {0, 1, 0, 0, 1, 0, 0},  3, {  5010,  13225,  161}, 1536, QBIT_INVALID },  // 14: Blackshire stables
    { MAP_MM6_MISTY_ISLANDS,        {0, 1, 0, 1, 0, 1, 0},  3, { -4225, -14604,  177},    0, QBIT_INVALID },  // 15: New Sorpigal dock
    { MAP_MM6_NEW_SORPIGAL,         {0, 1, 0, 1, 0, 1, 0},  2, {  1536, -10212,  161}, 1024, QBIT_INVALID },  // 16: Ironfist dock
    { MAP_MM6_MISTY_ISLANDS,        {1, 0, 1, 0, 1, 0, 0},  2, { -4225, -14604,  177},    0, QBIT_INVALID },
    { MAP_MM6_BOOTLEG_BAY,          {0, 1, 0, 0, 1, 0, 0},  3, {  6923,  11118,  161},  512, QBIT_INVALID },
    { MAP_MM6_NEW_SORPIGAL,         {0, 0, 0, 0, 0, 0, 1},  2, { 19889, -17320,    1},  768, QBIT_MM6_SILVER_HELM_ROUTES },  // 19: Ironfist secret dock
    { MAP_MM6_NEW_SORPIGAL,         {0, 0, 0, 0, 0, 0, 1}, 14, { 15953,  13520,    1}, 1544, QBIT_MM6_SILVER_HELM_ROUTES },
    { MAP_MM6_HERMITS_ISLE,         {1, 1, 1, 1, 1, 1, 1}, 21, {-18486, -20342,    1},  256, QBIT_MM6_SILVER_HELM_ROUTES },
    { MAP_MM6_CASTLE_IRONFIST,      {1, 0, 1, 0, 1, 0, 0},  2, { 21141,   8191,  192}, 1024, QBIT_INVALID },  // 22: Misty Islands dock
    { MAP_MM6_SILVER_COVE,          {1, 0, 0, 1, 0, 0, 0},  3, {  4515, -11489,  177},    0, QBIT_MM6_VISITED_SILVER_COVE },
    { MAP_MM6_BOOTLEG_BAY,          {0, 1, 0, 1, 0, 1, 0},  2, {  6923,  11118,  161},  512, QBIT_INVALID },
    { MAP_MM6_MISTY_ISLANDS,        {1, 0, 0, 1, 0, 1, 0},  3, { -4225, -14604,  177},    0, QBIT_INVALID },  // 25: Silver Cove dock
    { MAP_MM6_FREE_HAVEN,           {0, 1, 0, 0, 1, 0, 0},  3, { 14745,  12793,  177}, 1024, QBIT_INVALID },  // 26: ...to the Free Haven lake dock
    { MAP_MM6_EEL_INFESTED_WATERS,  {0, 0, 1, 0, 0, 0, 0},  1, {  6399,   8992,  177},    0, QBIT_INVALID },
    { MAP_MM6_MISTY_ISLANDS,        {1, 0, 0, 1, 0, 0, 0},  4, { -4225, -14604,  177},    0, QBIT_INVALID },  // 28: Free Haven dock
    { MAP_MM6_SILVER_COVE,          {0, 1, 0, 1, 0, 0, 0},  3, {  4515, -11489,  177},    0, QBIT_INVALID },
    { MAP_MM6_CASTLE_IRONFIST,      {0, 0, 1, 0, 0, 0, 0},  5, { 21141,   8191,  192}, 1024, QBIT_INVALID },
    { MAP_MM6_EEL_INFESTED_WATERS,  {0, 0, 0, 1, 0, 0, 0},  2, { -1726,  -4024,  177}, 1536, QBIT_INVALID },  // 31: Silver Cove north isle dock
    { MAP_MM6_EEL_INFESTED_WATERS,  {0, 0, 0, 0, 0, 1, 0},  1, {  6399,   8992,  177},    0, QBIT_INVALID },  // 32: Eel Infested south isle dock
    { MAP_MM6_SILVER_COVE,          {1, 0, 0, 0, 0, 0, 0},  2, {  4515, -11489,  177},    0, QBIT_INVALID },  // 33: Eel Infested north isle dock
    { MAP_MM6_BOOTLEG_BAY,          {1, 0, 1, 0, 0, 0, 0},  1, {   875,   3945,  161}, 1024, QBIT_INVALID },  // 34: Bootleg Bay east isle dock
    { MAP_MM6_CASTLE_IRONFIST,      {0, 1, 0, 1, 0, 0, 0},  4, { 21141,   8191,  192}, 1024, QBIT_INVALID }   // 35: Bootleg Bay west isle dock
}};

// MM6.EXE 0x4C43A0: three schedule slots for each of the 2dEvents transport houses 48..67
// (stables 48-56, docks 57-67). House 68, the Free Haven lake dock, is arrival-only.
static constexpr IndexedArray<std::array<int, 4>, static_cast<HouseId>(48), static_cast<HouseId>(68)> transportRoutesMm6 = {
    {static_cast<HouseId>(48),      { 0, 0, 0, 255 }},     // New Sorpigal stables
    {static_cast<HouseId>(49),      { 1, 2, 3, 255 }},     // Castle Ironfist stables
    {static_cast<HouseId>(50),      { 4, 5, 6, 255 }},     // Free Haven stables (west)
    {static_cast<HouseId>(51),      { 7, 8, 9, 255 }},     // Free Haven stables (east)
    {static_cast<HouseId>(52),      { 10, 10, 10, 255 }},  // Darkmoor stables
    {static_cast<HouseId>(53),      { 11, 11, 11, 255 }},  // Silver Cove stables
    {static_cast<HouseId>(54),      { 12, 12, 12, 255 }},  // White Cap stables
    {static_cast<HouseId>(55),      { 13, 13, 13, 255 }},  // Kriegspire stables
    {static_cast<HouseId>(56),      { 14, 14, 14, 255 }},  // Blackshire stables
    {static_cast<HouseId>(57),      { 15, 15, 15, 255 }},  // New Sorpigal dock
    {static_cast<HouseId>(58),      { 16, 17, 18, 255 }},  // Castle Ironfist dock
    {static_cast<HouseId>(59),      { 19, 20, 21, 255 }},  // Castle Ironfist secret dock
    {static_cast<HouseId>(60),      { 22, 23, 24, 255 }},  // Misty Islands dock
    {static_cast<HouseId>(61),      { 25, 26, 27, 255 }},  // Silver Cove dock
    {static_cast<HouseId>(62),      { 28, 29, 30, 255 }},  // Free Haven dock
    {static_cast<HouseId>(63),      { 31, 31, 31, 255 }},  // Silver Cove north isle dock
    {static_cast<HouseId>(64),      { 32, 32, 32, 255 }},  // Eel Infested Waters south isle dock
    {static_cast<HouseId>(65),      { 33, 33, 33, 255 }},  // Eel Infested Waters north isle dock
    {static_cast<HouseId>(66),      { 34, 34, 34, 255 }},  // Bootleg Bay east isle dock
    {static_cast<HouseId>(67),      { 35, 35, 35, 255 }},  // Bootleg Bay west isle dock
    {static_cast<HouseId>(68),      { 255, 255, 255, 255 }}  // Free Haven lake dock (arrival only)
};

static const TransportInfo &transportScheduleEntry(int scheduleId) {
    if (engine->gameVersion() == GAME_VERSION_MM6) {
        assert(scheduleId >= 0 && scheduleId < transportScheduleMm6.size());
        return transportScheduleMm6[scheduleId];
    }
    assert(scheduleId >= 0 && scheduleId < transportSchedule.size());
    return transportSchedule[scheduleId];
}

static const std::array<int, 4> &transportRoutesForHouse(HouseId houseId) {
    if (engine->gameVersion() == GAME_VERSION_MM6)
        return transportRoutesMm6[houseId];
    return transportRoutes[houseId];
}

void GUIWindow_Transport::mainDialogue() {
    Recti travel_window = this->frameRect;
    travel_window.x = SIDE_TEXT_BOX_POS_X;
    travel_window.w = SIDE_TEXT_BOX_WIDTH;

    assert(pParty->hasActiveCharacter()); // code in this function couldn't handle pParty->activeCharacterIndex() = 0 and crash

    if (!checkIfPlayerCanInteract()) {
        return;
    }

    std::vector<std::string> optionsText;
    int price = PriceCalculator::transportCostForPlayer(&pParty->activeCharacter(), houseTable[houseId()]);
    std::string travelCost = localization->format(LSTR_TRAVEL_COST_D_GOLD, price);
    int startingOffset = assets->pFontArrus->CalcTextHeight(travelCost, travel_window.w, 0) + (assets->pFontArrus->GetHeight() - 3) + 146;
    int lastsched = 255;
    bool hasActiveRoute = false;

    for (int schedule_id : transportRoutesForHouse(houseId())) {
        bool routeActive = false;

        if (schedule_id != 255 && (lastsched != schedule_id)) {
            routeActive = transportScheduleEntry(schedule_id).pSchedule[pParty->uCurrentDayOfMonth % 7];
        }

        lastsched = schedule_id;

        if (routeActive && (transportScheduleEntry(schedule_id).uQuestBit == QBIT_INVALID || pParty->_questBits[transportScheduleEntry(schedule_id).uQuestBit])) {
            int travel_time = getTravelTimeTransportDays(schedule_id);
            optionsText.push_back(localization->format(LSTR_D_DAYS_TO_S, travel_time, pMapStats->pInfos[transportScheduleEntry(schedule_id).uMapInfoID].name));
            hasActiveRoute = true;
        } else {
            optionsText.push_back("");
        }
    }

    if (hasActiveRoute) {
        DrawTitleText(assets->pFontArrus.get(), 0, 146, colorTable.White, travelCost, 3, travel_window);
        drawOptions(optionsText, colorTable.PaleCanary, startingOffset, true);
    } else {
        int textHeight = assets->pFontArrus->CalcTextHeight(localization->str(LSTR_SORRY_COME_BACK_ANOTHER_DAY), travel_window.w, 0);
        int vertMargin = (SIDE_TEXT_BOX_BODY_TEXT_HEIGHT - textHeight) / 2 + SIDE_TEXT_BOX_BODY_TEXT_OFFSET;
        DrawTitleText(assets->pFontArrus.get(), 0, vertMargin, colorTable.White, localization->str(LSTR_SORRY_COME_BACK_ANOTHER_DAY), 3, travel_window);
    }
}

void GUIWindow_Transport::transportDialogue() {
    int pPrice = PriceCalculator::transportCostForPlayer(&pParty->activeCharacter(), houseTable[houseId()]);

    if (pParty->GetGold() < pPrice) {
        engine->_statusBar->setEvent(LSTR_YOU_DONT_HAVE_ENOUGH_GOLD);
        playHouseSound(houseId(), HOUSE_SOUND_TRANSPORT_NOT_ENOUGH_GOLD);
        engine->_messageQueue->addMessageCurrentFrame(UIMSG_Escape, 1, 0);
        return;
    }

    int choice_id = std::to_underlying(_currentDialogue) - std::to_underlying(DIALOGUE_TRANSPORT_SCHEDULE_1);
    const TransportInfo *pTravel = &transportScheduleEntry(transportRoutesForHouse(houseId())[choice_id]);

    if (pTravel->pSchedule[pParty->uCurrentDayOfMonth % 7]) {
        if (engine->_currentLoadedMapId != pTravel->uMapInfoID) {
            autoSave();
            engine->_transitionMapId = pTravel->uMapInfoID;

            dword_6BE364_game_settings_1 |= GAME_SETTINGS_SKIP_WORLD_UPDATE;
            uGameState = GAME_STATE_CHANGE_LOCATION;
            engine->_teleportPoint.setTeleportTarget(pTravel->arrivalPos, pTravel->arrival_view_yaw, 0, 0);
        } else {
            // travelling to map we are already in
            pCamera3D->_viewYaw = 0;
            pParty->pos = pTravel->arrivalPos;
            pParty->uFallStartZ = pParty->pos.z;
            pParty->_viewPitch = 0;
            pParty->_viewYaw = pTravel->arrival_view_yaw;
        }

        pParty->TakeGold(pPrice);
        playHouseSound(houseId(), HOUSE_SOUND_TRANSPORT_TRAVEL);

        SpeechId pSpeech;
        if (houseTable[houseId()].uType == HOUSE_TYPE_BOAT) {
            pSpeech = SPEECH_TRAVEL_BOAT;
        } else {
            pSpeech = SPEECH_TRAVEL_HORSE;
        }

        restAndHeal(Duration::fromDays(getTravelTimeTransportDays(transportRoutesForHouse(houseId())[choice_id])));
        pParty->activeCharacter().playReaction(pSpeech);
        pAudioPlayer->soundDrain();
        while (houseDialogPressEscape()) {}
    } else {
        pAudioPlayer->playUISound(SOUND_error);
    }
    engine->_messageQueue->addMessageCurrentFrame(UIMSG_Escape, 0, 0);
}

void GUIWindow_Transport::houseSpecificDialogue() {
    assert(pParty->hasActiveCharacter()); // code in this function couldn't handle pParty->activeCharacterIndex() = 0 and crash

    switch (_currentDialogue) {
      case DIALOGUE_MAIN:
        mainDialogue();
        break;
      case DIALOGUE_TRANSPORT_SCHEDULE_1:
      case DIALOGUE_TRANSPORT_SCHEDULE_2:
      case DIALOGUE_TRANSPORT_SCHEDULE_3:
      case DIALOGUE_TRANSPORT_SCHEDULE_4:
        transportDialogue();
        break;
      default:
        break;
    }
}

void GUIWindow_Transport::houseDialogueOptionSelected(DialogueId option) {
    _currentDialogue = option;
}

std::vector<DialogueId> GUIWindow_Transport::listDialogueOptions() {
    switch (_currentDialogue) {
      case DIALOGUE_MAIN:
        return {DIALOGUE_TRANSPORT_SCHEDULE_1, DIALOGUE_TRANSPORT_SCHEDULE_2, DIALOGUE_TRANSPORT_SCHEDULE_3, DIALOGUE_TRANSPORT_SCHEDULE_4};
      default:
        return {};
    }
}

int GUIWindow_Transport::getTravelTimeTransportDays(int schedule_id) {
    int travel_time = transportScheduleEntry(schedule_id).uTravelTime;
    if (houseTable[houseId()].uType == HOUSE_TYPE_BOAT) {
        if (CheckHiredNPCSpeciality(Sailor))
            travel_time -= 2;
        if (CheckHiredNPCSpeciality(Navigator))
            travel_time -= 3;
        if (CheckHiredNPCSpeciality(Pirate))
            travel_time -= 2;
    } else {
        if (CheckHiredNPCSpeciality(Horseman))
            travel_time -= 2;
    }
    if (CheckHiredNPCSpeciality(Explorer))
        travel_time -= 1;
    if (travel_time < 1)
        travel_time = 1;
    return travel_time;
}

bool isTravelAvailable(HouseId houseId) {
    for (int schedule : transportRoutesForHouse(houseId)) {
        if (schedule == 255)
            continue;
        if (transportScheduleEntry(schedule).pSchedule[pParty->uCurrentDayOfMonth % 7]) {
            if (transportScheduleEntry(schedule).uQuestBit == QBIT_INVALID || pParty->_questBits[transportScheduleEntry(schedule).uQuestBit]) {
                return true;
            }
        }
    }
    return false;
}
