#include "UIHouses.h"

#include <cstdlib>
#include <memory>
#include <vector>
#include <utility>
#include <string>

#include "Engine/AssetsManager.h"
#include "Engine/Engine.h"
#include "Engine/Data/AwardEnums.h"
#include "Engine/Data/HouseEnumFunctions.h"
#include "Engine/Graphics/Camera.h"
#include "Engine/Objects/Decoration.h"
#include "Engine/Graphics/Indoor.h"
#include "Engine/Graphics/Image.h"
#include "Engine/Graphics/Renderer/Renderer.h"
#include "Engine/Localization.h"
#include "Engine/MapInfo.h"
#include "Engine/Party.h"
#include "Engine/PriceCalculator.h"
#include "Engine/Graphics/Viewport.h"
#include "Engine/Tables/HouseTable.h"
#include "Engine/Tables/TransitionTable.h"

#include "GUI/GUIButton.h"
#include "GUI/GUIFont.h"
#include "GUI/GUIWindow.h"
#include "GUI/GUIMessageQueue.h"
#include "GUI/UI/UIDialogue.h"
#include "GUI/UI/UIGame.h"
#include "GUI/UI/UIStatusBar.h"
#include "GUI/UI/NPCTopics.h"
#include "GUI/UI/Houses/MagicGuild.h"
#include "GUI/UI/Houses/Bank.h"
#include "GUI/UI/Houses/Jail.h"
#include "GUI/UI/Houses/Tavern.h"
#include "GUI/UI/Houses/Temple.h"
#include "GUI/UI/Houses/Training.h"
#include "GUI/UI/Houses/Transport.h"
#include "GUI/UI/Houses/MercenaryGuild.h"
#include "GUI/UI/Houses/TownHall.h"
#include "GUI/UI/Houses/Shops.h"

#include "Io/Mouse.h"
#include "Io/KeyboardInputHandler.h"

#include "Media/Audio/AudioPlayer.h"
#include "Media/MediaPlayer.h"

#include "Utility/Math/TrigLut.h"

using Io::TextInputType;

GraphicsImage *_591428_endcap = nullptr;

std::vector<HouseNpcDesc> houseNpcs;
int currentHouseNpc;

std::array<const HouseAnimDescr, 196> pAnimatedRooms = { {  // 0x4E5F70
    { "", 0x4, 0x1F4, HOUSE_TYPE_INVALID, 0, 0 },
    { "Human Armor01", 0x20, 0x2C0, HOUSE_TYPE_ARMOR_SHOP, 58, 0 },
    { "Necromancer Armor01", 0x20, 0x2D7, HOUSE_TYPE_ARMOR_SHOP, 70, 0 },
    { "Dwarven Armor01", 0x20, 0x2EE, HOUSE_TYPE_ARMOR_SHOP, 5, 0 },
    { "Wizard Armor", 0x20, 0x3BD, HOUSE_TYPE_ARMOR_SHOP, 19, 0 },
    { "Warlock Armor", 0x20, 0x2D6, HOUSE_TYPE_ARMOR_SHOP, 35, 0 },
    { "Elf Armor", 0x20, 0x2BC, HOUSE_TYPE_ARMOR_SHOP, 79, 0 },
    { "Human Alchemisht01", 0xE, 0x2BE, HOUSE_TYPE_ALCHEMY_SHOP, 95, 0 },
    { "Necromancer Alchemist01", 0xE, 0x2D6, HOUSE_TYPE_ALCHEMY_SHOP, 69, 0 },
    { "Dwarven Achemist01", 0xE, 0x387, HOUSE_TYPE_ALCHEMY_SHOP, 4, 0 },
    { "Wizard Alchemist", 0xE, 0x232, HOUSE_TYPE_ALCHEMY_SHOP, 25, 0 },
    { "Warlock Alchemist", 0xE, 0x2BE, HOUSE_TYPE_ALCHEMY_SHOP, 42, 0 },
    { "Elf Alchemist", 0xE, 0x38A, HOUSE_TYPE_ALCHEMY_SHOP, 84, 0 },
    { "Human Bank01", 0x6, 0x384, HOUSE_TYPE_BANK, 52, 0 },
    { "Necromancer Bank01", 0x6, 0x2D8, HOUSE_TYPE_BANK, 71, 0 },
    { "Dwarven Bank", 0x6, 0x2F3, HOUSE_TYPE_BANK, 6, 0 },
    { "Wizard Bank", 0x6, 0x3BA, HOUSE_TYPE_BANK, 20, 0 },
    { "Warlock Bank", 0x6, 0x39F, HOUSE_TYPE_BANK, 36, 0 },
    { "Elf Bank", 0x6, 0x2BC, HOUSE_TYPE_BANK, 71, 0 },
    { "Boat01", 0xF, 0x4C, HOUSE_TYPE_BOAT, 53, 3 },
    { "Boat01d", 0xF, 0x4C, HOUSE_TYPE_BOAT, 53, 3 }, // this movie doesn't exist
    { "Human Magic Shop01", 0xA, 0x2C8, HOUSE_TYPE_MAGIC_SHOP, 54, 0 },
    { "Necromancer Magic Shop01", 0xE, 0x2DC, HOUSE_TYPE_MAGIC_SHOP, 66, 0 },
    { "Dwarven Magic Shop01", 0x2A, 0x2EF, HOUSE_TYPE_MAGIC_SHOP, 91, 0 },
    { "Wizard Magic Shop", 0x1E, 0x2DF, HOUSE_TYPE_MAGIC_SHOP, 15, 0 },
    { "Warlock Magic Shop", 0x7, 0x3B9, HOUSE_TYPE_MAGIC_SHOP, 31, 0 },
    { "Elf Magic Shop", 0x24, 0x2CC, HOUSE_TYPE_MAGIC_SHOP, 82, 0 },
    { "Human Stables01", 0x21, 0x31, HOUSE_TYPE_STABLE, 48, 3 },
    { "Necromancer Stables", 0x21, 0x2DD, HOUSE_TYPE_STABLE, 67, 3 },
    { "", 0x21, 0x2F0, HOUSE_TYPE_STABLE, 91, 3 },
    { "Wizard Stables", 0x21, 0x3BA, HOUSE_TYPE_STABLE, 16, 3 },
    { "Warlock Stables", 0x21, 0x181, HOUSE_TYPE_STABLE, 77, 3 },  // movie exist but unused in MM7 as Nighon doesn't have stables
    { "Elf Stables", 0x21, 0x195, HOUSE_TYPE_STABLE, 77, 3 },
    { "Human Tavern01", 0xD, 0x2C2, HOUSE_TYPE_TAVERN, 49, 0 },
    { "Necromancer Tavern 01", 0xD, 0x3B0, HOUSE_TYPE_TAVERN, 57, 0 },
    { "Dwarven Tavern01", 0xD, 0x2FE, HOUSE_TYPE_TAVERN, 94, 0 },
    { "Wizard Tavern", 0xD, 0x3BB, HOUSE_TYPE_TAVERN, 17, 0 },
    { "Warlock Tavern", 0xD, 0x3A8, HOUSE_TYPE_TAVERN, 33, 0 },
    { "Elf Tavern", 0xD, 0x2CD, HOUSE_TYPE_TAVERN, 78, 0 },
    { "Human Temple01", 0x24, 0x2DB, HOUSE_TYPE_TEMPLE, 50, 3 },
    { "Necromancer Temple", 0x24, 0x2DF, HOUSE_TYPE_TEMPLE, 60, 3 },
    { "Dwarven Temple01", 0x24, 0x2F1, HOUSE_TYPE_TEMPLE, 86, 3 },
    { "Wizard Temple", 0x24, 0x2E0, HOUSE_TYPE_TEMPLE, 10, 3 },
    { "Warlock Temple", 0x24, 0x3A4, HOUSE_TYPE_TEMPLE, 27, 3 },
    { "Elf Temple", 0x24, 0x2CE, HOUSE_TYPE_TEMPLE, 72, 3 },
    { "Human Town Hall", 0x10, 0x39C, HOUSE_TYPE_TOWN_HALL, 14, 0 },
    { "Necromancer Town Hall01", 0x10, 0x3A4, HOUSE_TYPE_TOWN_HALL, 61, 0 },
    { "Dwarven Town Hall", 0x10, 0x2DB, HOUSE_TYPE_TOWN_HALL, 88, 0 }, // this movie doesn't exist, stone city doesn't have town hall
    { "Wizard Town Hall", 0x10, 0x3BD, HOUSE_TYPE_TOWN_HALL, 11, 0 },
    { "Warlock Town Hall", 0x10, 0x2DB, HOUSE_TYPE_TOWN_HALL, 28, 0 },
    { "Elf Town Hall", 0x10, 0x27A, HOUSE_TYPE_TOWN_HALL, 73, 0 },
    { "Human Training Ground01", 0x18, 0x2C7, HOUSE_TYPE_TRAINING_GROUND, 44, 0 },
    { "Necromancer Training Ground", 0x18, 0x3AD, HOUSE_TYPE_TRAINING_GROUND, 62, 0 },
    { "Dwarven Training Ground", 0x18, 0x2F2, HOUSE_TYPE_TRAINING_GROUND, 89, 0 },
    { "Wizard Training Ground", 0x18, 0x3A3, HOUSE_TYPE_TRAINING_GROUND, 12, 0 },
    { "Warlock Training Ground", 0x18, 0x3A6, HOUSE_TYPE_TRAINING_GROUND, 29, 0 },
    { "Elf Training Ground", 0x18, 0x19F, HOUSE_TYPE_TRAINING_GROUND, 74, 0 },
    { "Human Weapon Smith01", 0x16, 0x2C1, HOUSE_TYPE_WEAPON_SHOP, 45, 4 },
    { "Necromancer Weapon Smith01", 0x16, 0x2D9, HOUSE_TYPE_WEAPON_SHOP, 63, 4 },
    { "Dwarven Weapon Smith01", 0x16, 0x2EE, HOUSE_TYPE_WEAPON_SHOP, 82, 4 },
    { "Wizard Weapon Smith", 0x16, 0x2D5, HOUSE_TYPE_WEAPON_SHOP, 13, 4 },
    { "Warlock Weapon Smith", 0x16, 0x2D7, HOUSE_TYPE_WEAPON_SHOP, 23, 4 },
    { "Elf Weapon Smith", 0x16, 0x2CA, HOUSE_TYPE_WEAPON_SHOP, 75, 4 },
    { "Air Guild", 0x1D, 0xA4, HOUSE_TYPE_AIR_GUILD, 1, 3 },
    { "Body Guild", 0x19, 0x3BF, HOUSE_TYPE_BODY_GUILD, 2, 0 },
    { "Dark Guild", 0x19, 0x2D1, HOUSE_TYPE_DARK_GUILD, 3, 0 },
    { "Earth Guild", 0x19, 0x2CB, HOUSE_TYPE_EARTH_GUILD, 83, 0 },
    { "Fire Guild", 0x1C, 0x2BF, HOUSE_TYPE_FIRE_GUILD, 56, 0 },
    { "Light Guild", 0x1C, 0x2D5, HOUSE_TYPE_LIGHT_GUILD, 46, 0 },
    { "Mind Guild", 0x1C, 0xE5, HOUSE_TYPE_MIND_GUILD, 40, 0 },
    { "Spirit Guild", 0x1C, 0x2D2, HOUSE_TYPE_SPIRIT_GUILD, 41, 0 },
    { "Water Guild", 0x1B, 0x2D3, HOUSE_TYPE_WATER_GUILD, 24, 0 },
    { "Lord and Judge Out01", 1, 0, HOUSE_TYPE_HOUSE, 39, 0 },
    { "Human Poor House 1", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Human Poor House 2", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Human Poor House 3", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Human Medium House 1", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Human Medium House 2", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Human Medium House 3", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 }, // this movie doesn't exist
    { "Human Rich House 1", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Human Rich House 2", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Human Rich House 3", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 }, // this movie doesn't exist
    { "Elven Poor House 1", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Elven Poor House 2", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Elven Poor House 3", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 }, // this movie doesn't exist
    { "Elven Medium House 1", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Elven Medium House 2", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Elven Medium House 3", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 }, // this movie doesn't exist
    { "Elven Rich House 1", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Elven Rich House 2", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Elven Rich House 3", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Dwarven Poor House 1", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Dwarven Poor House 2", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Dwarven Poor House 3", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 }, // this movie doesn't exist
    { "Dwarven Medium House 1", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Dwarven Medium House 2", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Dwarven Medium House 3", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 }, // this movie doesn't exist
    { "Dwarven Rich House 1", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Dwarven Rich House 2", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Dwarven Rich House 3", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 }, // this movie doesn't exist
    { "Wizard Poor House 1", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Wizard Poor House 2", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Wizard Poor House 3", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 }, // this movie doesn't exist
    { "Wizard Medium House 1", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Wizard Medium House 2", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Wizard Medium House 3", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 }, // this movie doesn't exist
    { "Wizard Rich House 1", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Wizard Rich House 2", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Wizard Rich House 3", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 }, // this movie doesn't exist
    { "Necromancer Poor House 1", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Necromancer Poor House 2", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Necromancer Poor House 3", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 }, // this movie doesn't exist
    { "Necromancer Medium House 1", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Necromancer Medium House 2", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Necromancer Medium House 3", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 }, // this movie doesn't exist
    { "Necromancer Rich House 1", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Necromancer Rich House 2", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Necromancer Rich House 3", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 }, // this movie doesn't exist
    { "Warlock Poor House 1", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Warlock Poor House 2", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Warlock Poor House 3", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 }, // this movie doesn't exist
    { "Warlock Medium House 1", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Warlock Medium House 2", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Warlock Medium House 3", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 }, // this movie doesn't exist
    { "Warlock Rich House 1", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Warlock Rich House 2", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Warlock Rich House 3", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Out01 Temple of the Moon", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "Out01 Dragon Cave", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "Out02 Castle Harmondy", 0x24, 0, HOUSE_TYPE_CASTLE, 0, 0 },
    { "Out02 White Cliff Cave", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "Out03 Erathian Sewer", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "Out03 Fort Riverstride", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "Out03 Castle Gryphonheart", 0x24, 0, HOUSE_TYPE_CASTLE, 0, 0 },
    { "Out04 Elf Castle", 0x24, 0, HOUSE_TYPE_CASTLE, 0, 0 },
    { "Out04 Tularean Caves", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "Out04 Clanker's Laboratory", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "Out05 Hall of the Pit", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "Out05 Watchtower 6", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "Out06 School of Sorcery", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "Out06 Red Dwarf Mines", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "Out07 Castle Lambent", 0x24, 0, HOUSE_TYPE_CASTLE, 0, 0 },
    { "Out07 Walls of Mist", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "Out07 Temple of the Light", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "Out08 Evil Entrance", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "Out08 Breeding Zone", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "Out08 Temple of the Dark", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "Out09 Grand Temple of the Moon", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "Out09 Grand Temple of the Sun", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "Out10 Thunderfist Mountain", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "Out10 The Maze", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "Out10 Connecting Tunnel Cave #1", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "Out11 Stone City", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "Out12 Colony Zod", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "Out12 Connecting Tunnel Cave #1", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "Out13 Mercenary Guild", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "Out13 Tidewater Caverns", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "Out13 Wine Cellar", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "Out14 Titan's Stronghold", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "Out14 Temple of Baa", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "Out14 Hall under the Hill", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "Out15 The Linclon", 0x24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "Jail", 0x24, 0, HOUSE_TYPE_JAIL, 0, 0 },
    { "Harmondale Throne Room", 0x24, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Gryphonheart Throne Room", 0x24, 0, HOUSE_TYPE_THRONE_ROOM, 0, 0 },
    { "Elf Castle Throne Room", 0x24, 0, HOUSE_TYPE_THRONE_ROOM, 0, 0 },
    { "Wizard Castle Throne Room", 0x24, 0, HOUSE_TYPE_THRONE_ROOM, 0, 0 },
    { "Necromancer Castle Throne Rooms", 0x24, 0, HOUSE_TYPE_THRONE_ROOM, 0, 0 },
    { "Master Thief", 0x24, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Dwarven King", 0x24, 0, HOUSE_TYPE_THRONE_ROOM, 0, 0 },
    { "Arms Master", 0x24, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Warlock", 0x24, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Lord Markam", 0x24, 0, HOUSE_TYPE_THRONE_ROOM, 0, 0 },
    { "Arbiter Neutral Town", 0x24, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Arbiter Good Town", 0x24, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Arbiter Evil Town", 0x24, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Necromancer Throne Room Empty", 0x24, 0, HOUSE_TYPE_THRONE_ROOM, 0, 0 },
    { "", 0x24, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "", 0x24, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "", 0x24, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "", 0x24, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Boat01", 0xF, 0, HOUSE_TYPE_HOUSE, 53, 3 },
    { "", 0x24, 0, HOUSE_TYPE_BOAT, 0, 0 },
    { "", 0x24, 0, HOUSE_TYPE_BOAT, 0, 0 },
    { "", 0x24, 0, HOUSE_TYPE_BOAT, 0, 0 },
    { "", 0x24, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "Arbiter Room Neutral", 0x24, 0, HOUSE_TYPE_HOUSE, 0, 0 }, // this movie doesn't exist
    { "Out02 Castle Harmondy Abandoned", 0x24, 0, HOUSE_TYPE_CASTLE, 0, 0 },
    { "Human Temple02", 0x24, 0x3AB, HOUSE_TYPE_TEMPLE, 27, 0 },
    { "Player Castle Good", 0x24, 0, HOUSE_TYPE_CASTLE, 0, 0 },
    { "Player Castle Bad", 0x24, 0, HOUSE_TYPE_CASTLE, 0, 0 }
} };

// MM6.EXE 0x4BE888 ("HouseMovies" in MMExtension terms): the MM6 analog of `pAnimatedRooms`,
// indexed by the same 2dEvents "Picture" column. Per record: house FLC animation name (played in
// the viewport; the FLCs live extension-less in anims1/anims2.vid - FLIC playback itself is still
// pending), the evpan dialogue-panel index (stored in `uDialoguePanelId`), the proprietor portrait id
// ("npc%03u"), the building type (numbering matches the HouseType enum; only the throne-room jail
// check reads it) and the room sound id (sound = type + 100 * (id + 300), same formula as MM7).
std::array<const HouseAnimDescr, 119> pAnimatedRoomsMm6 = { {
    { "", 4, 500, HOUSE_TYPE_INVALID, 0, 0 },  // 0
    { "blcksrch", 22, 505, HOUSE_TYPE_WEAPON_SHOP, 34, 4 },
    { "Blcksmid", 13, 506, HOUSE_TYPE_WEAPON_SHOP, 33, 0 },
    { "blcksPor", 23, 507, HOUSE_TYPE_WEAPON_SHOP, 32, 0 },
    { "Apthcrch", 10, 501, HOUSE_TYPE_MAGIC_SHOP, 46, 0 },
    { "Apthcmid", 14, 502, HOUSE_TYPE_MAGIC_SHOP, 45, 0 },  // 5
    { "Apthcwch", 42, 503, HOUSE_TYPE_MAGIC_SHOP, 44, 0 },
    { "magrch", 30, 516, HOUSE_TYPE_MAGIC_SHOP, 43, 0 },
    { "magmid", 7, 517, HOUSE_TYPE_MAGIC_SHOP, 42, 0 },
    { "magicpor", 36, 518, HOUSE_TYPE_MAGIC_SHOP, 41, 3 },
    { "genstrch", 14, 513, HOUSE_TYPE_ALCHEMY_SHOP, 40, 0 },  // 10
    { "genstmid", 12, 514, HOUSE_TYPE_ALCHEMY_SHOP, 39, 0 },
    { "genstpor", 11, 515, HOUSE_TYPE_ALCHEMY_SHOP, 38, 0 },
    { "Cityrich", 16, 0, HOUSE_TYPE_TOWN_HALL_MM6, 14, 0 },
    { "Citymid", 41, 0, HOUSE_TYPE_TOWN_HALL_MM6, 13, 0 },
    { "CityPoor", 14, 0, HOUSE_TYPE_TOWN_HALL_MM6, 12, 5 },  // 15
    { "CitySpec", 30, 0, HOUSE_TYPE_TOWN_HALL_MM6, 0, 0 },
    { "Citytrtr", 16, 0, HOUSE_TYPE_TOWN_HALL_MM6, 0, 0 },
    { "throne06", 25, 0, HOUSE_TYPE_THRONE_ROOM, 61, 0 },
    { "throne03", 9, 0, HOUSE_TYPE_THRONE_ROOM, 60, 0 },
    { "throne02", 34, 0, HOUSE_TYPE_THRONE_ROOM, 59, 0 },  // 20
    { "throne01", 19, 0, HOUSE_TYPE_THRONE_ROOM, 63, 0 },
    { "throne05", 18, 0, HOUSE_TYPE_THRONE_ROOM, 62, 0 },
    { "throne04", 38, 0, HOUSE_TYPE_THRONE_ROOM, 64, 0 },
    { "tavpoor1", 13, 175, HOUSE_TYPE_TAVERN, 21, 0 },
    { "tavrich", 15, 530, HOUSE_TYPE_TAVERN, 20, 4 },  // 25
    { "tavpoor2", 21, 20, HOUSE_TYPE_TAVERN, 22, 0 },
    { "TavMid", 36, 297, HOUSE_TYPE_TAVERN, 23, 0 },
    { "tavpirat", 13, 358, HOUSE_TYPE_TAVERN, 24, 4 },
    { "tavgob", 20, 552, HOUSE_TYPE_TAVERN, 25, 0 },
    { "temppoor", 36, 550, HOUSE_TYPE_TEMPLE, 16, 3 },  // 30
    { "tempmid", 31, 549, HOUSE_TYPE_TEMPLE, 18, 0 },
    { "temprich", 18, 548, HOUSE_TYPE_TEMPLE, 17, 0 },
    { "tempevil", 30, 551, HOUSE_TYPE_TEMPLE, 19, 3 },
    { "tempruin", 32, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "t7", 24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },  // 35
    { "t6", 49, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "t1", 24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "t4", 49, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "t5", 20, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "t8", 49, 0, HOUSE_TYPE_DUNGEON, 0, 0 },  // 40
    { "oracrich", 17, 0, HOUSE_TYPE_SEER, 2, 0 },
    { "oracpoor", 33, 0, HOUSE_TYPE_SEER, 1, 0 },
    { "circus1", 55, 0, HOUSE_TYPE_CIRCUS, 0, 0 },
    { "Bank", 6, 504, HOUSE_TYPE_BANK, 15, 0 },
    { "stables", 33, 385, HOUSE_TYPE_STABLE, 11, 3 },  // 45
    { "ship", 15, 72, HOUSE_TYPE_BOAT, 10, 3 },
    { "jail", 49, 0, HOUSE_TYPE_JAIL, 0, 0 },
    { "thfrich", 37, 533, HOUSE_TYPE_TOWN_HALL, 28, 0 },
    { "thfpoor", 35, 534, HOUSE_TYPE_TOWN_HALL, 27, 0 },
    { "thfpirat", 36, 535, HOUSE_TYPE_TOWN_HALL, 26, 4 },  // 50
    { "mercrich", 39, 519, HOUSE_TYPE_MERCENARY_GUILD, 31, 0 },
    { "mercmid", 39, 520, HOUSE_TYPE_MERCENARY_GUILD, 30, 0 },
    { "mercpoor", 39, 521, HOUSE_TYPE_MERCENARY_GUILD, 29, 4 },
    { "elemFire", 28, 510, HOUSE_TYPE_FIRE_GUILD, 47, 0 },
    { "elemerth", 27, 509, HOUSE_TYPE_EARTH_GUILD, 50, 0 },  // 55
    { "elemair", 29, 508, HOUSE_TYPE_AIR_GUILD, 48, 3 },
    { "elemwatr", 26, 511, HOUSE_TYPE_WATER_GUILD, 49, 0 },
    { "elemall", 43, 512, HOUSE_TYPE_ELEMENTAL_GUILD, 56, 3 },
    { "mirpthl", 24, 332, HOUSE_TYPE_LIGHT_GUILD, 54, 3 },
    { "mirpthd", 24, 91, HOUSE_TYPE_DARK_GUILD, 55, 3 },  // 60
    { "mirpthdl", 24, 0, HOUSE_TYPE_MIRRORED_PATH_GUILD, 58, 3 },
    { "selfspir", 25, 260, HOUSE_TYPE_SPIRIT_GUILD, 51, 0 },
    { "selfmind", 38, 61, HOUSE_TYPE_MIND_GUILD, 52, 0 },
    { "selfbody", 25, 549, HOUSE_TYPE_BODY_GUILD, 53, 0 },
    { "selfall", 18, 256, HOUSE_TYPE_SELF_GUILD, 57, 0 },  // 65
    { "roompor1", 8, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "roompor2", 3, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "roompor3", 13, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "roompor4", 2, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "roommid1", 36, 0, HOUSE_TYPE_HOUSE, 0, 0 },  // 70
    { "roommid2", 36, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "roommid3", 1, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "roommid4", 15, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "roomrch1", 9, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "roomrch2", 41, 0, HOUSE_TYPE_HOUSE, 0, 0 },  // 75
    { "roomrch3", 30, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "roomrch4", 24, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "ArmRich", 22, 545, HOUSE_TYPE_ARMOR_SHOP, 37, 0 },
    { "Armmid", 36, 546, HOUSE_TYPE_ARMOR_SHOP, 36, 4 },
    { "Armpoor", 13, 547, HOUSE_TYPE_ARMOR_SHOP, 35, 4 },  // 80
    { "train1", 40, 532, HOUSE_TYPE_TRAINING_GROUND, 3, 4 },
    { "train2", 44, 532, HOUSE_TYPE_TRAINING_GROUND, 4, 4 },
    { "train3", 45, 532, HOUSE_TYPE_TRAINING_GROUND, 5, 4 },
    { "train4", 24, 532, HOUSE_TYPE_TRAINING_GROUND, 6, 4 },
    { "train5", 22, 532, HOUSE_TYPE_TRAINING_GROUND, 7, 4 },  // 85
    { "train6", 8, 532, HOUSE_TYPE_TRAINING_GROUND, 8, 4 },
    { "Pyramid", 53, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "hive", 54, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "d14", 52, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "d06", 24, 0, HOUSE_TYPE_DUNGEON, 0, 0 },  // 90
    { "d16", 49, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "d05", 13, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "d15", 46, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "d13", 25, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "d17", 25, 0, HOUSE_TYPE_DUNGEON, 0, 0 },  // 95
    { "d03", 30, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "d09", 51, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "d12", 25, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "t2", 25, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "t3", 47, 0, HOUSE_TYPE_DUNGEON, 0, 0 },  // 100
    { "d10", 49, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "d11", 13, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "d02", 20, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "d04", 49, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "d18", 49, 0, HOUSE_TYPE_DUNGEON, 0, 0 },  // 105
    { "d19", 20, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "d07", 51, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "d20", 49, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "d08", 19, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "CstlGood", 14, 0, HOUSE_TYPE_CASTLE, 9, 3 },  // 110
    { "d01", 49, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "cd1", 25, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "cd2", 49, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "cd3", 36, 0, HOUSE_TYPE_DUNGEON, 0, 0 },
    { "circus2", 55, 0, HOUSE_TYPE_CIRCUS, 0, 0 },  // 115
    { "statue", 55, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "archloop", 55, 0, HOUSE_TYPE_HOUSE, 0, 0 },
    { "noarchie", 55, 0, HOUSE_TYPE_HOUSE, 0, 0 },
} };

const HouseAnimDescr &houseAnimDescr(int animId) {
    if (engine->gameVersion() == GAME_VERSION_MM6) {
        if (animId < 0 || animId >= static_cast<int>(pAnimatedRoomsMm6.size()))
            animId = 0;
        return pAnimatedRoomsMm6[animId];
    }
    if (animId < 0 || animId >= static_cast<int>(pAnimatedRooms.size()))
        animId = 0;
    return pAnimatedRooms[animId];
}

// MM6.EXE 0x4BEFF8: transition-picture names, indexed by the 2dEvents exit-pic column and the
// exit-pic argument of transition events. Ids 6-8 coincide with MM7's list; 1-5 differ.
static constexpr std::array<const char *, 9> pHouse_ExitPicturesMm6 = {{
    "", "castle", "dungeon", "idoor", "isecdoor", "istairdn", "istairup", "itrap", "outside"
}};

const char *houseExitPictureName(unsigned picId) {
    if (engine->gameVersion() == GAME_VERSION_MM6)
        return pHouse_ExitPicturesMm6[picId < pHouse_ExitPicturesMm6.size() ? picId : 0];
    return pHouse_ExitPictures[picId < pHouse_ExitPictures.size() ? picId : 0];
}

// The house exit/cancel button: MM7 paints a wide box under the right panel; MM6's buttesc sits
// centered on the dialogue panel's bottom row.
static Pointi houseExitButtonPos() {
    return engine->gameVersion() == GAME_VERSION_MM6 ? MM6_DIALOGUE_ESC_CENTERED_POS : Pointi(471, 445);
}

static Sizei houseExitButtonSize() {
    return engine->gameVersion() == GAME_VERSION_MM6 ? MM6_DIALOGUE_BUTTON_SIZE : Sizei(169, 35);
}

const IndexedArray<int, HOUSE_TYPE_WEAPON_SHOP, HOUSE_TYPE_DARK_GUILD> itemAmountInShop = {{
    {HOUSE_TYPE_WEAPON_SHOP,   6},
    {HOUSE_TYPE_ARMOR_SHOP,    8},
    {HOUSE_TYPE_MAGIC_SHOP,   12},
    {HOUSE_TYPE_ALCHEMY_SHOP, 12},
    {HOUSE_TYPE_FIRE_GUILD,   12},
    {HOUSE_TYPE_AIR_GUILD,    12},
    {HOUSE_TYPE_WATER_GUILD,  12},
    {HOUSE_TYPE_EARTH_GUILD,  12},
    {HOUSE_TYPE_SPIRIT_GUILD, 12},
    {HOUSE_TYPE_MIND_GUILD,   12},
    {HOUSE_TYPE_BODY_GUILD,   12},
    {HOUSE_TYPE_LIGHT_GUILD,  12},
    {HOUSE_TYPE_DARK_GUILD,   12}
}};

static constexpr IndexedArray<const char *, HOUSE_TYPE_WEAPON_SHOP, HOUSE_TYPE_MIRRORED_PATH_GUILD> shopBackgroundNames = {{
    {HOUSE_TYPE_WEAPON_SHOP,           "WEPNTABL"},
    {HOUSE_TYPE_ARMOR_SHOP,            "ARMORY"},
    {HOUSE_TYPE_MAGIC_SHOP,            "MAGSHELF"},
    {HOUSE_TYPE_ALCHEMY_SHOP,          "MAGSHELF"},
    {HOUSE_TYPE_FIRE_GUILD,            "MAGSHELF"},
    {HOUSE_TYPE_AIR_GUILD,             "MAGSHELF"},
    {HOUSE_TYPE_WATER_GUILD,           "MAGSHELF"},
    {HOUSE_TYPE_EARTH_GUILD,           "MAGSHELF"},
    {HOUSE_TYPE_SPIRIT_GUILD,          "MAGSHELF"},
    {HOUSE_TYPE_MIND_GUILD,            "MAGSHELF"},
    {HOUSE_TYPE_BODY_GUILD,            "MAGSHELF"},
    {HOUSE_TYPE_LIGHT_GUILD,           "MAGSHELF"},
    {HOUSE_TYPE_DARK_GUILD,            "MAGSHELF"},
    {HOUSE_TYPE_ELEMENTAL_GUILD,       "MAGSHELF"},
    {HOUSE_TYPE_SELF_GUILD,            "MAGSHELF"},
    {HOUSE_TYPE_MIRRORED_PATH_GUILD,   "MAGSHELF"}
}};

bool enterHouse(HouseId uHouseID) {
    engine->_statusBar->clearAll();
    engine->_messageQueue->clear();
    keyboardInputHandler->EndTextInput();

    if (uHouseID == HOUSE_THRONEROOM_WIN_GOOD || uHouseID == HOUSE_THRONEROOM_WIN_EVIL) {
        // In MM7 both ids are endings won (good/evil side); in MM6 600 is Win and 601 is Lose
        // (the Hive reactor blast without the Ritual of the Void consumes the world).
        bool isLoss = engine->gameVersion() == GAME_VERSION_MM6 && uHouseID == HOUSE_THRONEROOM_WIN_EVIL;
        engine->_messageQueue->addMessageCurrentFrame(UIMSG_ShowGameOverWindow, isLoss, 0);
        return false;
    }

    current_npc_text.clear();

    int openHours = houseTable[uHouseID].uOpenTime;
    int closeHours = houseTable[uHouseID].uCloseTime;
    Time currentTime = pParty->GetPlayingTime();
    Time currentTimeDays = Time::fromDays(currentTime.toDays());
    bool isOpened = false;
    Time openTime = currentTimeDays + Duration::fromHours(openHours);
    Time closeTime = currentTimeDays + Duration::fromHours(closeHours);

    if (closeHours > openHours) {
        // Store opens during the day.
        isOpened = (currentTime >= openTime) && (currentTime <= closeTime);
    } else {
        // Store opens at night.
        isOpened = (currentTime <= closeTime) || (currentTime >= openTime);
    }

    if (!isOpened) {
        CivilTime openCivilTime = openTime.toCivilTime();
        CivilTime closeCivilTime = closeTime.toCivilTime();

        engine->_statusBar->setEvent(LSTR_THIS_PLACE_IS_OPEN_FROM_DS_TO_DS,
                                     openCivilTime.hourAmPm,
                                     localization->amPm(openCivilTime.isPm),
                                     closeCivilTime.hourAmPm,
                                     localization->amPm(closeCivilTime.isPm));
        if (pParty->hasActiveCharacter()) {
            pParty->activeCharacter().playReaction(SPEECH_STORE_CLOSED);
        }

        return false;
    }

    if (isShop(uHouseID)) {
        if (!(pParty->PartyTimes.shopBanTimes[uHouseID]) || (pParty->PartyTimes.shopBanTimes[uHouseID] <= pParty->GetPlayingTime())) {
            pParty->PartyTimes.shopBanTimes[uHouseID] = Time();
        } else {
            engine->_statusBar->setEvent(LSTR_YOUVE_BEEN_BANNED_FROM_THIS_SHOP);
            return false;
        }
    }

    uCurrentHouse_Animation = houseTable[uHouseID].uAnimationID;
    if (houseAnimDescr(uCurrentHouse_Animation).uBuildingType == HOUSE_TYPE_THRONE_ROOM && pParty->uFine) {  // going to jail
        uHouseID = HOUSE_JAIL;
        uCurrentHouse_Animation = houseTable[uHouseID].uAnimationID;
        restAndHeal(Duration::fromYears(1));
        ++pParty->uNumPrisonTerms;
        pParty->uFine = 0;
        for (Character &player : pParty->pCharacters) {
            player.timeToRecovery = 0_ticks;
            player.uNumDivineInterventionCastsThisDay = 0;
            player.SetVariable(VAR_Award, std::to_underlying(AWARD_PRISON_TERMS));
        }
    }

    currentHouseNpc = -1;
    if (engine->gameVersion() == GAME_VERSION_MM6) {
        // MM6.EXE 0x43c66a: each house type carries its own marble dialogue panel, drawn over the
        // right HUD column (see MM6_DIALOGUE_PANEL_POS).
        game_ui_dialogue_background = assets->getImage_Solid(fmt::format("evpan{:03}", houseAnimDescr(uCurrentHouse_Animation).uDialoguePanelId));
    } else {
        game_ui_dialogue_background = assets->getImage_Solid(dialogueBackgroundResourceByAlignment[pParty->alignment]);
    }

    prepareHouse(uHouseID);

    if (houseNpcs.size() == 1) {
        currentHouseNpc = 0;
    }
    pMediaPlayer->OpenHouseMovie(houseAnimDescr(uCurrentHouse_Animation).video_name, 1u);
    if (isMagicGuild(uHouseID)) {
        // TODO(pskelton): check this behaviour
        if (!pParty->hasActiveCharacter()) { // avoid nzi
            pParty->setActiveToFirstCanAct();
        }

        if (!pParty->activeCharacter()._achievedAwardsBits[membershipAwardForGuild(uHouseID)]) {
            playHouseSound(uHouseID, HOUSE_SOUND_MAGIC_GUILD_MEMBERS_ONLY);
            return true;
        }
    } else if ((houseTable[uHouseID].uType == HOUSE_TYPE_STABLE || houseTable[uHouseID].uType == HOUSE_TYPE_BOAT) && !isTravelAvailable(uHouseID)) {
        // Type-based check: MM6 transport house ids (48-68) collide with unrelated MM7 id ranges.
        return true;
    }
    playHouseSound(uHouseID, HOUSE_SOUND_GENERAL_GREETING);
    return true;
}

void prepareHouse(HouseId house) {
    houseNpcs.clear();

    // Default proprietor of non-simple houses. MM6 town halls have a named proprietor in 2dEvents
    // (Janice/Earnest/Jake) but no portrait in the animated-rooms table - the house dialogue is
    // still anchored on them, so push a portrait-less entry (draw sites skip null icons).
    int proprietorId = houseAnimDescr(houseTable[house].uAnimationID).house_npc_id;
    bool mm6NamedProprietor = engine->gameVersion() == GAME_VERSION_MM6 && !houseTable[house].pProprieterName.empty();
    if (proprietorId || mm6NamedProprietor) {
        HouseNpcDesc desc;
        desc.type = HOUSE_PROPRIETOR;
        desc.label = localization->format(LSTR_CONVERSE_WITH_S, houseTable[house].pProprieterName);
        if (proprietorId)
            desc.icon = assets->getImage_ColorKey(fmt::format("npc{:03}", proprietorId));

        houseNpcs.push_back(desc);
    }

    // NPCs of this house
    for (int i = 1; i < pNPCStats->uNumNewNPCs; ++i) {
        if (pNPCStats->pNPCData[i].house == house) {
            if (!(pNPCStats->pNPCData[i].flags & NPC_HIRED)) {
                HouseNpcDesc desc;
                desc.type = HOUSE_NPC;
                desc.label = localization->format(LSTR_CONVERSE_WITH_S, pNPCStats->pNPCData[i].name);
                desc.icon = assets->getImage_ColorKey(fmt::format("npc{:03}", pNPCStats->pNPCData[i].portraitId));
                desc.npc = &pNPCStats->pNPCData[i];

                houseNpcs.push_back(desc);
                if (!(pNPCStats->pNPCData[i].flags & NPC_GREETED_SECOND)) {
                    if (pNPCStats->pNPCData[i].flags & NPC_GREETED_FIRST) {
                        pNPCStats->pNPCData[i].flags &= ~NPC_GREETED_FIRST;
                        pNPCStats->pNPCData[i].flags |= NPC_GREETED_SECOND;
                    } else {
                        pNPCStats->pNPCData[i].flags |= NPC_GREETED_FIRST;
                    }
                }
            }
        }
    }

    // Dungeon entry (not present in MM7)
    if (houseTable[house].uExitPicID) {
        if (houseTable[house]._quest_bit == QBIT_INVALID || !pParty->_questBits[houseTable[house]._quest_bit]) {
            MapId id = houseTable[house].uExitMapID;

            // MM6 castle entrances chain to a throne room via a "2D <event>" exit that parses to no
            // map (deferred, see docs/pending/mm6-game-ui-skin.md) - don't offer a broken transition.
            if (id != MAP_INVALID) {
                HouseNpcDesc desc;
                desc.type = HOUSE_TRANSITION;
                desc.label = localization->format(LSTR_ENTER_S, pMapStats->pInfos[id].name);
                if (engine->gameVersion() == GAME_VERSION_MM6) {
                    // MM6's exit-pic column is an index into its own picture table; MM7's data
                    // instead makes the target map id double as the picture index below.
                    desc.icon = assets->getImage_ColorKey(houseExitPictureName(houseTable[house].uExitPicID));
                } else {
                    desc.icon = assets->getImage_ColorKey(pHouse_ExitPictures[static_cast<int>(id)]);
                }
                desc.targetMapID = id;

                houseNpcs.push_back(desc);
            }
        }
    }
}

/**
 * TODO(Nik-RE-dev): untested until houses NPC can join the party
 *
 * @offset 0x4B40E6
 */
void NPCHireableDialogPrepare() {
    int v0 = 0;
    NPCData *v1 = houseNpcs[currentHouseNpc].npc;

    pDialogueWindow = std::make_unique<GUIWindow>(WINDOW_Dialogue, Pointi(0, 0), Sizei(render->GetRenderDimensions().w, 350));
    pBtn_ExitCancel = pDialogueWindow->CreateButton(houseExitButtonPos(), houseExitButtonSize(), BUTTON_TYPE_NORMAL, 0,
        UIMSG_Escape, 0, INPUT_ACTION_INVALID, localization->str(LSTR_CANCEL), {ui_exit_cancel_button_background}
    );
    pDialogueWindow->CreateButton({0, 0}, {0, 0}, BUTTON_TYPE_NORMAL, 0, UIMSG_HouseScreenClick, 0);
    if (!pNPCStats->pProfessions[v1->profession].pBenefits.empty()) {
        pDialogueWindow->CreateButton({480, 160}, {140, 30}, BUTTON_TYPE_NORMAL, 0,
            UIMSG_SelectHouseNPCDialogueOption, std::to_underlying(DIALOGUE_PROFESSION_DETAILS), INPUT_ACTION_INVALID, localization->str(LSTR_MORE_INFORMATION)
        );
        v0 = 1;
    }
    pDialogueWindow->CreateButton({480, 30 * v0 + 160}, {140, 30}, BUTTON_TYPE_NORMAL, 0,
        UIMSG_SelectHouseNPCDialogueOption, std::to_underlying(DIALOGUE_HIRE_FIRE), INPUT_ACTION_INVALID, localization->str(LSTR_HIRE));
    pDialogueWindow->setKeyboardControlGroup(v0 + 1, false, 0, 2);
    window_SpeakInHouse->setCurrentDialogue(DIALOGUE_OTHER);
}

void selectHouseNPCDialogueOption(DialogueId topic) {
    NPCData *pCurrentNPCInfo = houseNpcs[currentHouseNpc].npc;

    if (topic >= DIALOGUE_SCRIPTED_LINE_1 && topic <= DIALOGUE_SCRIPTED_LINE_6) {
        DialogueId newTopic = handleScriptedNPCTopicSelection(topic, pCurrentNPCInfo);

        if (newTopic != DIALOGUE_MAIN) {
            window_SpeakInHouse->setCurrentDialogue(DIALOGUE_OTHER);
            window_SpeakInHouse->reinitDialogueWindow();
            window_SpeakInHouse->initializeNPCDialogueButtons(listNPCDialogueOptions(newTopic));
        }
        BackToHouseMenu();
        return;
    }

    if (topic == DIALOGUE_13_hiring_related) {
        current_npc_text = BuildDialogueString(pNPCStats->pProfessions[pCurrentNPCInfo->profession].pJoinText,
                                               pParty->activeCharacterIndex() - 1, pCurrentNPCInfo);
        NPCHireableDialogPrepare();
        dialogue_show_profession_details = false;
        BackToHouseMenu();
        return;
    }

    selectSpecialNPCTopicSelection(topic, pCurrentNPCInfo);

    if (topic != DIALOGUE_HIRE_FIRE) {
        if (topic == DIALOGUE_PROFESSION_DETAILS) {
            if (dialogue_show_profession_details) {
                current_npc_text = BuildDialogueString(pNPCStats->pProfessions[pCurrentNPCInfo->profession].pBenefits,
                                                       pParty->activeCharacterIndex() - 1, pCurrentNPCInfo);
            } else {
                current_npc_text = BuildDialogueString(pNPCStats->pProfessions[pCurrentNPCInfo->profession].pJoinText,
                                                       pParty->activeCharacterIndex() - 1, pCurrentNPCInfo);
            }
        }
        BackToHouseMenu();
        return;
    }

    if (!pCurrentNPCInfo->Hired()) {
        current_npc_text = BuildDialogueString(pNPCStats->pProfessions[pCurrentNPCInfo->profession].pJoinText,
                                               pParty->activeCharacterIndex() - 1, pCurrentNPCInfo);
        BackToHouseMenu();
        return;
    }

    prepareHouse(window_SpeakInHouse->houseId());
    BackToHouseMenu();
}

void updateHouseNPCTopics(int npc) {
    int num_menu_buttons = 0;

    currentHouseNpc = npc;
    if (houseNpcs[npc].type == HOUSE_TRANSITION) {
        // TODO(Nik-RE-dev): can use GUIWindow_Transition
        pDialogueWindow = std::make_unique<GUIWindow>(WINDOW_Dialogue, Pointi(0, 0), render->GetRenderDimensions());
        bool isMm6 = engine->gameVersion() == GAME_VERSION_MM6;
        Pointi escPos = isMm6 ? MM6_DIALOGUE_ESC_BUTTON_POS : Pointi(566, 445);
        Pointi yesPos = isMm6 ? MM6_DIALOGUE_YES_BUTTON_POS : Pointi(486, 445);
        Sizei buttonSize = isMm6 ? MM6_DIALOGUE_BUTTON_SIZE : Sizei(75, 33);
        Pointi portraitPos = isMm6 ? MM6_DIALOGUE_PORTRAIT_POS : Pointi(pNPCPortraits_x[0][0], pNPCPortraits_y[0][0]);
        pBtn_ExitCancel = pDialogueWindow->CreateButton(escPos, buttonSize, BUTTON_TYPE_NORMAL, 0, UIMSG_Escape, 0, INPUT_ACTION_TRANSITION_NO, localization->str(LSTR_CANCEL), {ui_buttdesc2});
        pBtn_YES = pDialogueWindow->CreateButton(yesPos, buttonSize, BUTTON_TYPE_NORMAL, 0, UIMSG_HouseTransitionConfirmation, 1, INPUT_ACTION_TRANSITION_YES, houseNpcs[npc].label, {ui_buttyes2});
        pDialogueWindow->CreateButton(portraitPos, {63, 73}, BUTTON_TYPE_NORMAL, 0, UIMSG_HouseTransitionConfirmation, 1,
                                      INPUT_ACTION_INTERACT, houseNpcs[npc].label);
        pDialogueWindow->CreateButton({8, 8}, {460, 344}, BUTTON_TYPE_NORMAL, 0, UIMSG_HouseTransitionConfirmation, 1, INPUT_ACTION_TRANSITION_YES, houseNpcs[npc].label);
    } else {
        if (window_SpeakInHouse->getCurrentDialogue() != DIALOGUE_OTHER) {
            for (int i = 0; i < houseNpcs.size(); ++i) {
                houseNpcs[i].button->Release();
                houseNpcs[i].button = nullptr;
            }
        }
        window_SpeakInHouse->setCurrentDialogue(DIALOGUE_MAIN);
        window_SpeakInHouse->reinitDialogueWindow();
        if (houseNpcs[npc].type == HOUSE_PROPRIETOR) {
            window_SpeakInHouse->initializeProprietorDialogue();
        } else {
            window_SpeakInHouse->initializeNPCDialogue(npc);
        }
    }
}

void selectProprietorDialogueOption(DialogueId option) {
    if (!pDialogueWindow || !pDialogueWindow->pNumPresenceButton) {
        return;
    }

    pParty->placeHeldItemInInventoryOrDrop();

    window_SpeakInHouse->houseDialogueOptionSelected(option);
    window_SpeakInHouse->reinitDialogueWindow();
    window_SpeakInHouse->initializeProprietorDialogue();
}

bool houseDialogPressEscape() {
    engine->_messageQueue->clear();
    keyboardInputHandler->EndTextInput();
    activeLevelDecoration = nullptr;
    current_npc_text.clear();
    pParty->placeHeldItemInInventoryOrDrop();

    if (currentHouseNpc == -1) {
        return false;
    }

    if (window_SpeakInHouse->getCurrentDialogue() == DIALOGUE_OTHER) {
        updateHouseNPCTopics(currentHouseNpc);
        BackToHouseMenu();
        return true;
    }

    if (window_SpeakInHouse->getCurrentDialogue() == DIALOGUE_NULL ||
        window_SpeakInHouse->getCurrentDialogue() == DIALOGUE_MAIN) {
        currentHouseNpc = -1;
        if (shop_ui_background) {
            shop_ui_background->release();
            shop_ui_background = nullptr;
        }
        window_SpeakInHouse->updateDialogueOnEscape();
        // we are in the middle of the update windows loop, so we need to delay closing the dialogue window until the end of the frame
        engine->_messageQueue->addMessageCurrentFrame(UIMSG_CloseDialogueWindow, 0, 0);

        if (houseNpcs.size() == 1) {
            return false;
        }

        pBtn_ExitCancel = window_SpeakInHouse->vButtons.front();
        for (int i = 0; i < houseNpcs.size(); ++i) {
            Pointi pos = {pNPCPortraits_x[houseNpcs.size() - 1][i], pNPCPortraits_y[houseNpcs.size() - 1][i]};
            houseNpcs[i].button = window_SpeakInHouse->CreateButton(pos, {63, 73}, BUTTON_TYPE_NORMAL, 0, UIMSG_ClickHouseNPCPortrait, i,
                                                                    INPUT_ACTION_INVALID, houseNpcs[i].label);
        }

        BackToHouseMenu();
        return true;
    }

    window_SpeakInHouse->updateDialogueOnEscape();
    window_SpeakInHouse->reinitDialogueWindow();
    window_SpeakInHouse->initializeProprietorDialogue();

    return true;
}

void createHouseUI(HouseId houseId) {
    switch (houseTable[houseId].uType) {
      case HOUSE_TYPE_FIRE_GUILD:
      case HOUSE_TYPE_AIR_GUILD:
      case HOUSE_TYPE_WATER_GUILD:
      case HOUSE_TYPE_EARTH_GUILD:
      case HOUSE_TYPE_SPIRIT_GUILD:
      case HOUSE_TYPE_MIND_GUILD:
      case HOUSE_TYPE_BODY_GUILD:
      case HOUSE_TYPE_LIGHT_GUILD:
      case HOUSE_TYPE_DARK_GUILD:
      case HOUSE_TYPE_ELEMENTAL_GUILD:
      case HOUSE_TYPE_SELF_GUILD:
      case HOUSE_TYPE_MIRRORED_PATH_GUILD:
        window_SpeakInHouse = std::make_unique<GUIWindow_MagicGuild>(houseId);
        break;
      case HOUSE_TYPE_BANK:
        window_SpeakInHouse = std::make_unique<GUIWindow_Bank>(houseId);
        break;
      case HOUSE_TYPE_TEMPLE:
        window_SpeakInHouse = std::make_unique<GUIWindow_Temple>(houseId);
        break;
      case HOUSE_TYPE_TAVERN:
        window_SpeakInHouse = std::make_unique<GUIWindow_Tavern>(houseId);
        break;
      case HOUSE_TYPE_TRAINING_GROUND:
        window_SpeakInHouse = std::make_unique<GUIWindow_Training>(houseId);
        break;
      case HOUSE_TYPE_STABLE:
      case HOUSE_TYPE_BOAT:
        window_SpeakInHouse = std::make_unique<GUIWindow_Transport>(houseId);
        break;
      case HOUSE_TYPE_TOWN_HALL:
        window_SpeakInHouse = std::make_unique<GUIWindow_TownHall>(houseId);
        break;
      case HOUSE_TYPE_JAIL:
        window_SpeakInHouse = std::make_unique<GUIWindow_Jail>(houseId);
        break;
      case HOUSE_TYPE_MERCENARY_GUILD:
        window_SpeakInHouse = std::make_unique<GUIWindow_MercenaryGuild>(houseId);
        break;
      case HOUSE_TYPE_WEAPON_SHOP:
        window_SpeakInHouse = std::make_unique<GUIWindow_WeaponShop>(houseId);
        break;
      case HOUSE_TYPE_ARMOR_SHOP:
        window_SpeakInHouse = std::make_unique<GUIWindow_ArmorShop>(houseId);
        break;
      case HOUSE_TYPE_MAGIC_SHOP:
        window_SpeakInHouse = std::make_unique<GUIWindow_MagicShop>(houseId);
        break;
      case HOUSE_TYPE_ALCHEMY_SHOP:
        window_SpeakInHouse = std::make_unique<GUIWindow_AlchemyShop>(houseId);
        break;
      default:
        window_SpeakInHouse = std::make_unique<GUIWindow_House>(houseId);
        break;
    }

    if (houseNpcs.size() == 1) {
        updateHouseNPCTopics(0);
    }
}

// TODO(Nik-RE-dev): looks like this function is not needed anymore
void BackToHouseMenu() {
    auto pMouse = EngineIocContainer::ResolveMouse();
    // TODO(Nik-RE-dev): Looks like it's artifact of MM6
#if 0
    if (window_SpeakInHouse && window_SpeakInHouse->houseId() == 165 &&
        !pMovie_Track) {
        GameOverNoSound = true;
        houseDialogPressEscape();
        window_SpeakInHouse->Release();
        pParty->uFlags &= 0xFFFFFFFD;
        if (enterHouse(HOUSE_BODY_GUILD_MASTER_ERATHIA)) {
            pAudioPlayer->playUISound(SOUND_Invalid);
            createHouseUI(HOUSE_BODY_GUILD_MASTER_ERATHIA);
        }
        GameOverNoSound = false;
    }
#endif
}

void playHouseSound(HouseId houseID, HouseSoundType type) {
    if (houseID != HOUSE_INVALID && houseAnimDescr(houseTable[houseID].uAnimationID).uRoomSoundId) {
        // TODO(captainurist): encapsulate
        int roomSoundId = houseAnimDescr(houseTable[houseID].uAnimationID).uRoomSoundId;
        SoundId soundId = SoundId(std::to_underlying(type) + 100 * (roomSoundId + 300));
        pAudioPlayer->playHouseSound(soundId, true);
    }
}

void GUIWindow_House::houseNPCDialogue() {
    if (houseNpcs[currentHouseNpc].type == HOUSE_TRANSITION) {
        Recti house_window = this->frameRect;
        MapId id = houseNpcs[currentHouseNpc].targetMapID;
        house_window.x = 493;
        house_window.w = 126;
        DrawTitleText(assets->pFontCreate.get(), 0, 2, colorTable.White, pMapStats->pInfos[id].name, 3, house_window);
        house_window.x = SIDE_TEXT_BOX_POS_X;
        house_window.w = SIDE_TEXT_BOX_WIDTH;
        if (pTransitionStrings[std::to_underlying(id)].empty()) { // TODO(captainurist): this is a weird access into pTransitionStrings, investigate & add docs
            auto str = localization->format(LSTR_ENTER_S, pMapStats->pInfos[id].name);
            DrawTitleText(assets->pFontCreate.get(), 0, (212 - assets->pFontCreate->CalcTextHeight(str, house_window.w, 0)) / 2 + 101, colorTable.White, str, 3, house_window);
            return;
        }

        int vertMargin = (212 - assets->pFontCreate->CalcTextHeight(pTransitionStrings[std::to_underlying(id)], house_window.w, 0)) / 2 + 101;
        DrawTitleText(assets->pFontCreate.get(), 0, vertMargin, colorTable.White, pTransitionStrings[std::to_underlying(id)], 3, house_window);
        return;
    }

    NPCData *pNPC = houseNpcs[currentHouseNpc].npc;
    drawNpcHouseNameAndTitle(pNPC);
    drawNpcHouseGreetingMessage(pNPC);
    drawNpcHouseDialogueOptions(pNPC);
    drawNpcHouseDialogueResponse();
}

void GUIWindow_House::drawNpcHouseNameAndTitle(NPCData *npcData) {
    Recti window = this->frameRect;
    window.w -= 10;
    DrawTitleText(assets->pFontCreate.get(), SIDE_TEXT_BOX_POS_X, SIDE_TEXT_BOX_POS_Y, colorTable.EasternBlue, NameAndTitle(npcData), 3, window);
}

void GUIWindow_House::drawNpcHouseGreetingMessage(NPCData *npcData) {
    if (houseNpcs[0].type != HOUSE_PROPRIETOR) {
        if (current_npc_text.length() == 0 && _currentDialogue == DIALOGUE_MAIN) {
            if (npcData->greetingIndex) {
                std::string greetString;
                if (npcData->flags & NPC_GREETED_SECOND) {
                    greetString = pNPCStats->pNPCGreetings[npcData->greetingIndex].pGreeting2;
                } else {
                    greetString = pNPCStats->pNPCGreetings[npcData->greetingIndex].pGreeting1;
                }
                DrawDialoguePanel(greetString);
            }
        }
    }
}

void GUIWindow_House::drawNpcHouseDialogueOptions(NPCData* npcData) const {
    std::vector<std::string> optionsText;

    int buttonLimit = pDialogueWindow->pStartingPosActiveItem + pDialogueWindow->pNumPresenceButton;
    for (int i = pDialogueWindow->pStartingPosActiveItem; i < buttonLimit; ++i) {
        GUIButton *pButton = pDialogueWindow->GetControl(i);
        if (pButton) {
            DialogueId topic = (DialogueId)pButton->msg_param;
            std::string str = npcDialogueOptionString(topic, npcData);
            if (str.empty() && topic >= DIALOGUE_SCRIPTED_LINE_1 && topic <= DIALOGUE_SCRIPTED_LINE_6) {
                pButton->msg_param = 0;
            }
            optionsText.push_back(str);
        }
    }

    if (optionsText.size()) {
        drawOptions(optionsText, colorTable.Sunflower);
    }
}

void GUIWindow_House::drawNpcHouseDialogueResponse() {
    DrawDialoguePanel(current_npc_text);
}

void GUIWindow_House::reinitDialogueWindow() {
    if (pDialogueWindow) {
        // reset dialogue window to default state, so it can be reused for different NPCs dialogues without creating new one
        pDialogueWindow->frameRect = { 0, 0, render->GetPresentDimensions().w, 345 };
        pDialogueWindow->sHint = "";
        pDialogueWindow->receives_keyboard_input = false;
        pDialogueWindow->DeleteButtons();
    } else {
        pDialogueWindow = std::make_unique<GUIWindow>(WINDOW_Dialogue, Pointi(0, 0), Sizei(render->GetPresentDimensions().w, 345));
    }

    pBtn_ExitCancel = pDialogueWindow->CreateButton(houseExitButtonPos(), houseExitButtonSize(), BUTTON_TYPE_NORMAL, 0, UIMSG_Escape, 0, INPUT_ACTION_INVALID,
        localization->str(LSTR_END_CONVERSATION), {ui_exit_cancel_button_background});
    pDialogueWindow->CreateButton({8, 8}, {450, 320}, BUTTON_TYPE_NORMAL, 0, UIMSG_HouseScreenClick, 0, INPUT_ACTION_INVALID, "");
}

bool GUIWindow_House::checkIfPlayerCanInteract() {
    if (!pParty->hasActiveCharacter()) {  // to avoid access zeroeleement
        return false;
    }

    // Do nothing if no current conversation exist
    if (!pDialogueWindow) {
        return true;
    }

    if (pParty->activeCharacter().CanAct()) {
        pDialogueWindow->pNumPresenceButton = _savedButtonsNum;
        return true;
    } else {
        pDialogueWindow->pNumPresenceButton = 0;
        Recti window = window_SpeakInHouse->frameRect;
        window.x = SIDE_TEXT_BOX_POS_X;
        window.w = SIDE_TEXT_BOX_WIDTH;

        std::string str = localization->format(LSTR_S_IS_IN_NO_CONDITION_TO_S, pParty->activeCharacter().name, localization->str(LSTR_DO_ANYTHING));
        DrawTitleText(assets->pFontArrus.get(), 0, (212 - assets->pFontArrus->CalcTextHeight(str, window.w, 0)) / 2 + 101, ui_house_player_cant_interact_color, str, 3, window);
        return false;
    }
}

// TODO(Nik-RE-dev): maybe need to unify selectColor for all dialogue
void GUIWindow_House::drawOptions(std::vector<std::string> &optionsText, Color selectColor, int topOptionShift, bool denseSpacing) const {
    Recti window = this->frameRect;
    window.x = SIDE_TEXT_BOX_POS_X;
    window.w = SIDE_TEXT_BOX_WIDTH;

    assert(optionsText.size() == pDialogueWindow->pNumPresenceButton);

    int allTextHeight = 0;
    int activeOptions = 0;
    for (int i = 0; i < optionsText.size(); ++i) {
        if (!optionsText[i].empty()) {
            allTextHeight += assets->pFontArrus->CalcTextHeight(optionsText[i], window.w, 0);
            activeOptions++;
        }
    }

    int spacing = 0;
    int offset = topOptionShift;
    if (!denseSpacing) {
        spacing = (SIDE_TEXT_BOX_BODY_TEXT_HEIGHT - topOptionShift - allTextHeight) / (activeOptions ? activeOptions : 1);
        if (spacing > SIDE_TEXT_BOX_MAX_SPACING) {
            spacing = SIDE_TEXT_BOX_MAX_SPACING;
        }
        offset += (SIDE_TEXT_BOX_BODY_TEXT_HEIGHT - topOptionShift - spacing * activeOptions - allTextHeight) / 2 - spacing / 2 + SIDE_TEXT_BOX_BODY_TEXT_OFFSET;
    }

    for (int i = 0; i < pDialogueWindow->pNumPresenceButton; ++i) {
        int buttonIndex = i + pDialogueWindow->pStartingPosActiveItem;
        GUIButton *button = pDialogueWindow->GetControl(buttonIndex);

        if (!optionsText[i].empty()) {
            Color textColor = (pDialogueWindow->pCurrentPosActiveItem == buttonIndex) ? selectColor : colorTable.White;
            int textHeight = assets->pFontArrus->CalcTextHeight(optionsText[i], window.w, 0);
            button->rect.y = spacing + offset;
            button->rect.h = textHeight + 6;
            button->sLabel = optionsText[i];
            if (denseSpacing) {
                offset += assets->pFontArrus->GetHeight() - 3 + textHeight;
            } else {
                offset = button->rect.y + textHeight - 1 + 6;
            }
            DrawTitleText(assets->pFontArrus.get(), 0, button->rect.y, textColor, optionsText[i], 3, window);
        } else if (button) {
            button->rect.y = 0;
            button->rect.h = 0;
            button->sLabel.clear();
        }
    }
}

void GUIWindow_House::houseDialogManager() {
    assert(window_SpeakInHouse != nullptr);

    Recti pWindow = this->frameRect;
    pWindow.w -= 18;
    bool isMm6 = engine->gameVersion() == GAME_VERSION_MM6;
    if (isMm6) {
        // MM6.EXE 0x497ebf: the HUD frames are already drawn (Engine::DrawGUI runs first); the
        // dialogue just blits its marble panel over the right column. No panel-frame redraw.
        render->DrawQuad2D(game_ui_dialogue_background, MM6_DIALOGUE_PANEL_POS);
    } else {
        render->DrawQuad2D(game_ui_dialogue_background, {477, 0});
        render->DrawQuad2D(game_ui_right_panel_frame, {468, 0});
    }

    if (currentHouseNpc == -1 || houseNpcs[currentHouseNpc].type != HOUSE_TRANSITION) {
        // Draw house title
        if (!houseTable[houseId()].name.empty()) {
            if (current_screen_type != SCREEN_SHOP_INVENTORY) {
                int yPos = 2 * assets->pFontCreate->GetHeight() - 6 - assets->pFontCreate->CalcTextHeight(houseTable[houseId()].name, 130, 0);
                if (yPos < 0) {
                    yPos = 0;
                }
                DrawTitleText(assets->pFontCreate.get(), 0x1EAu, yPos / 2 + 4, colorTable.White, houseTable[houseId()].name, 3, pWindow);
            }
        }
    }

    pWindow.w += 8;
    if (currentHouseNpc == -1) {
        // Either house have no residents or current screen is for selecting resident to begin dialogue
        render->DrawQuad2D(ui_exit_cancel_button_background, isMm6 ? MM6_DIALOGUE_ESC_CENTERED_POS : Pointi(471, 445));

        if (buildingType() == HOUSE_TYPE_JAIL) {
            houseSpecificDialogue();
            return;
        }
        DrawDialoguePanel(current_npc_text);

        for (int i = 0; i < houseNpcs.size(); ++i) {
            int portraitX = pNPCPortraits_x[houseNpcs.size() - 1][i];
            int portraitY = pNPCPortraits_y[houseNpcs.size() - 1][i];
            if (!isMm6) // MM6 has no evtnpc portrait frame - portraits sit directly on the panel.
                render->DrawQuad2D(game_ui_evtnpc, {portraitX - 4, portraitY - 4});
            if (houseNpcs[i].icon) // MM6 town-hall proprietors have no portrait.
                render->DrawQuad2D(houseNpcs[i].icon, {portraitX, portraitY});
            if (houseNpcs.size() < 4) {
                std::string pTitleText = "";
                int yPos = 0;
                switch (houseNpcs[i].type) {
                  case HOUSE_TRANSITION:
                    pTitleText = pMapStats->pInfos[houseNpcs[i].targetMapID].name;
                    yPos = 94 * i + SIDE_TEXT_BOX_POS_Y;
                    break;
                  case HOUSE_PROPRIETOR:
                    pTitleText = houseTable[houseId()].pProprieterTitle;
                    yPos = SIDE_TEXT_BOX_POS_Y;
                    break;
                  case HOUSE_NPC:
                    pTitleText = houseNpcs[i].npc->name;
                    yPos = pNPCPortraits_y[houseNpcs.size() - 1][i] + houseNpcs[i].icon->height() + 2;
                    break;
                }
                DrawTitleText(assets->pFontCreate.get(), SIDE_TEXT_BOX_POS_X, yPos, colorTable.EasternBlue, pTitleText, 3, pWindow);
            }
        }
        return;
    }

    if (isMm6) {
        // MM6.EXE 0x497f46: the selected occupant's portrait, frameless, at the panel's picture spot.
        if (houseNpcs[currentHouseNpc].icon)
            render->DrawQuad2D(houseNpcs[currentHouseNpc].icon, MM6_DIALOGUE_PORTRAIT_POS);
    } else {
        render->DrawQuad2D(game_ui_evtnpc, {pNPCPortraits_x[0][0] - 4, pNPCPortraits_y[0][0] - 4});
        render->DrawQuad2D(houseNpcs[currentHouseNpc].icon, {pNPCPortraits_x[0][0], pNPCPortraits_y[0][0]});
    }
    if (current_screen_type == SCREEN_SHOP_INVENTORY) {
        CharacterUI_InventoryTab_Draw(&pParty->activeCharacter(), true);
        render->DrawQuad2D(ui_exit_cancel_button_background, isMm6 ? MM6_DIALOGUE_ESC_CENTERED_POS : Pointi(471, 445));
        return;
    }
    if (currentHouseNpc || houseNpcs[0].type != HOUSE_PROPRIETOR) {
        // Dialogue with NPC in house
        houseNPCDialogue();
    } else {
        std::string nameAndTitle = NameAndTitle(houseTable[houseId()].pProprieterName, houseTable[houseId()].pProprieterTitle);
        DrawTitleText(assets->pFontCreate.get(), SIDE_TEXT_BOX_POS_X, SIDE_TEXT_BOX_POS_Y, colorTable.EasternBlue, nameAndTitle, 3, pWindow);
        houseSpecificDialogue();
    }
    if (currentHouseNpc != -1 && houseNpcs[currentHouseNpc].type == HOUSE_TRANSITION) {
        if (isMm6) {
            // MM6.EXE 0x4983a1: the yes/cancel pair on the panel's bottom row.
            render->DrawQuad2D(ui_exit_cancel_button_background, MM6_DIALOGUE_ESC_BUTTON_POS);
            render->DrawQuad2D(game_ui_mm6_buttyes, MM6_DIALOGUE_YES_BUTTON_POS);
        } else {
            render->DrawQuad2D(dialogue_ui_x_x_u, {556, 451});
            render->DrawQuad2D(dialogue_ui_x_ok_u, {476, 451});
        }
    } else {
        render->DrawQuad2D(ui_exit_cancel_button_background, isMm6 ? MM6_DIALOGUE_ESC_CENTERED_POS : Pointi(471, 445));
    }
}

void GUIWindow_House::initializeProprietorDialogue() {
    if (!pDialogueWindow) {
        return;
    }

    std::vector<DialogueId> optionList = listDialogueOptions();

    if (optionList.size()) {
        for (int i = 0; i < optionList.size(); i++) {
            pDialogueWindow->CreateButton({480, 146 + 30 * i}, {140, 30}, BUTTON_TYPE_NORMAL, 0, UIMSG_SelectProprietorDialogueOption, std::to_underlying(optionList[i]), INPUT_ACTION_INVALID, "");
        }
        pDialogueWindow->setKeyboardControlGroup(optionList.size(), false, 0, 2);
    }
    _savedButtonsNum = pDialogueWindow->pNumPresenceButton;
}

void GUIWindow_House::initializeNPCDialogue(int npc) {
    if (!pDialogueWindow) {
        return;
    }

    initializeNPCDialogueButtons(prepareScriptedNPCDialogueTopics(houseNpcs[npc].npc));
}

void GUIWindow_House::initializeNPCDialogueButtons(std::vector<DialogueId> optionList) {
    if (optionList.size()) {
        for (int i = 0; i < optionList.size(); i++) {
            pDialogueWindow->CreateButton({480, 160 + 30 * i}, {140, 30}, BUTTON_TYPE_NORMAL, 0, UIMSG_SelectHouseNPCDialogueOption, std::to_underlying(optionList[i]), INPUT_ACTION_INVALID, "");
        }
        pDialogueWindow->setKeyboardControlGroup(optionList.size(), false, 0, 2);
    }
    _savedButtonsNum = pDialogueWindow->pNumPresenceButton;
}

void GUIWindow_House::learnSkillsDialogue(Color selectColor) {
    if (!checkIfPlayerCanInteract()) {
        return;
    }

    bool haveLearnableSkills = false;
    std::vector<std::string> optionsText;
    int cost = PriceCalculator::skillLearningCostForPlayer(&pParty->activeCharacter(), houseTable[houseId()]);
    int buttonsLimit = pDialogueWindow->pStartingPosActiveItem + pDialogueWindow->pNumPresenceButton;
    for (int i = pDialogueWindow->pStartingPosActiveItem; i < buttonsLimit; i++) {
        Skill skill = GetLearningDialogueSkill((DialogueId)pDialogueWindow->GetControl(i)->msg_param);
        if (skillMaxMasteryPerClass[pParty->activeCharacter().classType][skill] != MASTERY_NONE &&
            !pParty->activeCharacter().pActiveSkills[skill]) {
            optionsText.push_back(localization->skillName(skill));
            haveLearnableSkills = true;
        } else {
            optionsText.push_back("");
        }
    }

    Recti dialogue = this->frameRect;
    dialogue.x = SIDE_TEXT_BOX_POS_X;
    dialogue.w = SIDE_TEXT_BOX_WIDTH;

    if (!haveLearnableSkills) {
        Character &player = pParty->activeCharacter();
        std::string str = localization->format(LSTR_SEEK_KNOWLEDGE_ELSEWHERE_S_THE_S, player.name, localization->className(player.classType));
        str = str + "\n \n" + localization->str(LSTR_I_CAN_OFFER_YOU_NOTHING_FURTHER);

        int text_height = assets->pFontArrus->CalcTextHeight(str, dialogue.w, 0);
        DrawTitleText(assets->pFontArrus.get(), 0, (SIDE_TEXT_BOX_BODY_TEXT_HEIGHT - text_height) / 2 + SIDE_TEXT_BOX_BODY_TEXT_OFFSET, colorTable.PaleCanary, str, 3, dialogue);
    } else {
        std::string skill_price_label = localization->format(LSTR_SKILL_COST_LU, cost);
        DrawTitleText(assets->pFontArrus.get(), 0, 146, colorTable.White, skill_price_label, 3, dialogue);
    }

    drawOptions(optionsText, selectColor, 18);
}

void GUIWindow_House::learnSelectedSkill(Skill skill) {
    int pPrice = PriceCalculator::skillLearningCostForPlayer(&pParty->activeCharacter(), houseTable[houseId()]);
    if (skillMaxMasteryPerClass[pParty->activeCharacter().classType][skill] != MASTERY_NONE) {
        if (!pParty->activeCharacter().pActiveSkills[skill]) {
            if (pParty->GetGold() < pPrice) {
                engine->_statusBar->setEvent(LSTR_YOU_DONT_HAVE_ENOUGH_GOLD);
                if (buildingType() == HOUSE_TYPE_TRAINING_GROUND) {
                    playHouseSound(houseId(), HOUSE_SOUND_TRAINING_NOT_ENOUGH_GOLD);
                } else if (buildingType() == HOUSE_TYPE_TAVERN) {
                    playHouseSound(houseId(), HOUSE_SOUND_TAVERN_NOT_ENOUGH_GOLD);
                } else {
                    playHouseSound(houseId(), HOUSE_SOUND_GENERAL_NOT_ENOUGH_GOLD);
                }
            } else {
                pParty->TakeGold(pPrice);
                _transactionPerformed = true;
                pParty->activeCharacter().pActiveSkills[skill] = CombinedSkillValue::novice();
                pParty->activeCharacter().playReaction(SPEECH_SKILL_LEARNED);
            }
        }
    }
}

GUIWindow_House::GUIWindow_House(HouseId houseId) : GUIWindow(WINDOW_HouseInterior, {0, 0}, render->GetRenderDimensions()), _houseId(houseId) {
    pEventTimer->setPaused(true);  // pause timer so not attacked

    current_screen_type = SCREEN_HOUSE;
    pBtn_ExitCancel = CreateButton(houseExitButtonPos(), houseExitButtonSize(), BUTTON_TYPE_NORMAL, 0, UIMSG_Escape, 0, INPUT_ACTION_INVALID,
                                   localization->str(LSTR_EXIT_BUILDING), {ui_exit_cancel_button_background});

    if (buildingType() <= HOUSE_TYPE_MIRRORED_PATH_GUILD) {
        shop_ui_background = assets->getImage_ColorKey(shopBackgroundNames[buildingType()]);
    }

    for (int i = 0; i < houseNpcs.size(); ++i) {
        Pointi pos = {pNPCPortraits_x[houseNpcs.size() - 1][i], pNPCPortraits_y[houseNpcs.size() - 1][i]};
        houseNpcs[i].button = CreateButton(pos, {63, 73}, BUTTON_TYPE_NORMAL, 0, UIMSG_ClickHouseNPCPortrait, i,
                                                      INPUT_ACTION_INVALID, houseNpcs[i].label);
    }

    CreateCharacterButtons();
}

void GUIWindow_House::Update() {
    if (!window_SpeakInHouse) {
        return;
    }
    houseDialogManager();
    if (!isShop(houseId())) {
        return;
    }
    if (pParty->PartyTimes.shopBanTimes[houseId()] <= pParty->GetPlayingTime()) {
        pParty->PartyTimes.shopBanTimes[houseId()] = Time();
        return;
    }
    engine->_messageQueue->addMessageCurrentFrame(UIMSG_Escape, 0, 0);  // banned from shop so leaving
}

GUIWindow_House::~GUIWindow_House() {
    for (HouseNpcDesc &desc : houseNpcs) {
        if (desc.icon) {
            desc.icon->release();
        }
    }
    houseNpcs.clear();

    if (game_ui_dialogue_background) {
        game_ui_dialogue_background->release();
        game_ui_dialogue_background = nullptr;
    }

    if (engine->config->settings.FlipOnExit.value()) {
        pParty->_viewYaw = (TrigLUT.uIntegerDoublePi - 1) & (TrigLUT.uIntegerPi + pParty->_viewYaw);
        pCamera3D->_viewYaw = pParty->_viewYaw;
    }
}

void GUIWindow_House::houseDialogueOptionSelected(DialogueId option) {
    _currentDialogue = option;
}

void GUIWindow_House::houseSpecificDialogue() {
    // Nothing
}

std::vector<DialogueId> GUIWindow_House::listDialogueOptions() {
    return {};
}

void GUIWindow_House::updateDialogueOnEscape() {
    if (_currentDialogue == DIALOGUE_MAIN) {
        _currentDialogue = DIALOGUE_NULL;
        return;
    }
    _currentDialogue = DIALOGUE_MAIN;
}

void GUIWindow_House::houseScreenClick() {
    // Nothing to do by default
}

void GUIWindow_House::playHouseGoodbyeSpeech() {
    // No speech by default
}
