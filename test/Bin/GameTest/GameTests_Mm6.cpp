#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Testing/Game/GameTest.h"

#include "Engine/Data/AwardEnums.h"
#include "Engine/Engine.h"
#include "Engine/Localization.h"
#include "Engine/Evt/EvtProgram.h"
#include "Engine/Evt/Processor.h"
#include "Engine/MapEnumFunctions.h"
#include "Engine/MapInfo.h"
#include "Engine/Party.h"
#include "Engine/PriceCalculator.h"
#include "Engine/Resources/EngineFileSystem.h"
#include "Engine/Resources/LodTextureCache.h"
#include "Engine/Resources/ResourceManager.h"
#include "Engine/SaveLoad.h"
#include "Engine/mm7_data.h"

#include "Utility/Math/TrigLut.h"
#include "Utility/ScopeGuard.h"
#include "Utility/Segment.h"
#include "Utility/String/Split.h"
#include "Engine/Graphics/BSPModel.h"
#include "Engine/Graphics/Image.h"
#include "Engine/Graphics/Indoor.h"
#include "Engine/Graphics/Outdoor.h"
#include "Engine/Graphics/LocationFunctions.h"
#include "Engine/Graphics/Overlays.h"
#include "Engine/Graphics/PaletteManager.h"
#include "Engine/Graphics/Renderer/Renderer.h"
#include "Engine/Graphics/Sprites.h"
#include "Engine/Graphics/TurnBasedOverlay.h"
#include "Engine/Graphics/Viewport.h"
#include "Engine/Graphics/Weather.h"
#include "Engine/TurnEngine/TurnEngineEnums.h"
#include "Engine/Objects/Actor.h"
#include "Engine/Objects/CharacterEnumFunctions.h"
#include "Engine/Objects/Chest.h"
#include "Engine/Objects/ItemEnumFunctions.h"
#include "Engine/Objects/CombinedSkillValue.h"
#include "Engine/Objects/Decoration.h"
#include "Engine/Objects/MonsterEnumFunctions.h"
#include "Engine/Objects/Monsters.h"
#include "Engine/Objects/NPC.h"
#include "Engine/Objects/ObjectList.h"
#include "Engine/Objects/SpriteEnumFunctions.h"
#include "Engine/Objects/SpriteObject.h"
#include "Engine/Spells/CastSpellInfo.h"
#include "Engine/Spells/SpellEnums.h"
#include "Engine/Spells/SpellEnumFunctions.h"
#include "Engine/Spells/Spells.h"
#include "Engine/Tables/AutonoteTable.h"
#include "Engine/Tables/AwardTable.h"
#include "Engine/Tables/HouseTable.h"
#include "Engine/Tables/ItemTable.h"
#include "Engine/Tables/MessageScrollTable.h"
#include "Engine/Tables/NPCTable.h"
#include "Engine/Tables/TileTable.h"
#include "Engine/Tables/TransitionTable.h"
#include "Engine/Time/Timer.h"

#include "Engine/AssetsManager.h"

#include "GUI/GUIButton.h"
#include "GUI/GUIDialogues.h"
#include "GUI/GUIFont.h"
#include "GUI/GUIMessageQueue.h"
#include "GUI/GUIWindow.h"
#include "GUI/UI/Books/AutonotesBook.h"
#include "GUI/UI/UIBooks.h"
#include "GUI/UI/UICharacter.h"
#include "GUI/UI/UIDialogue.h"
#include "GUI/UI/UIGame.h"
#include "GUI/UI/UIHouses.h"
#include "GUI/UI/UIMainMenu.h"
#include "GUI/UI/UIMessageScroll.h"
#include "GUI/UI/UIMm6Segue.h"
#include "GUI/UI/UIPartyCreation.h"
#include "GUI/UI/UISpell.h"
#include "GUI/UI/UISpellbook.h"
#include "GUI/UI/UIStatusBar.h"
#include "GUI/UI/UITransition.h"
#include "GUI/UI/Houses/Shops.h"
#include "GUI/UI/Houses/TownHall.h"
#include "GUI/UI/Houses/Transport.h"

#include "Io/KeyboardActionMapping.h"
#include "Io/Mouse.h"

#include "Library/Color/ColorTable.h"
#include "Library/Image/Pcx.h"
#include "Library/Lod/LodReader.h"
#include "Library/LodFormats/LodImage.h"

#include "Media/MediaPlayer.h"
#include "Media/Audio/AudioPlayer.h"

extern std::unordered_set<InputAction> key_map_conflicted;  // 506E6C

// MM6 bring-up tests. These require MM6 game data and only run when the test binary is
// invoked with '--game-version mm6'; under the default MM7 test suite they are skipped.

GAME_TEST(Mm6, NewGame) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // MM6 new game starts outdoors in New Sorpigal.
    EXPECT_EQ(uCurrentlyLoadedLevelType, LEVEL_OUTDOOR);
    EXPECT_EQ(pMapStats->pInfos[engine->_currentLoadedMapId].fileName, "oute3.odm");

    // First visit to an outdoor map uses MM6's first-visit sky, "sky01" (MM6.EXE @0x46dfe8). MM7's
    // "plansky3" doesn't exist in MM6's bitmaps.lod and would tile the sky with the "pending"
    // placeholder - a grid of red no-signs.
    EXPECT_EQ(pOutdoor->loc_time.skyTextureName, "sky01");

    // Entities placed in oute3.ddm should have been loaded: 38 peasants (MM6 monster ids 121-135,
    // random encounter spawns can add more on top), 42 sprite objects and 20 chests.
    int placedPeasants = 0;
    for (const Actor &actor : pActors) {
        int monsterId = std::to_underlying(actor.monsterId);
        placedPeasants += monsterId >= 121 && monsterId <= 135;
    }
    EXPECT_GE(placedPeasants, 38);
    EXPECT_TRUE(std::ranges::any_of(pActors, [](const Actor &actor) {
        // First actor record of oute3.ddm - a level 3 peasant (PeasantF1C).
        return std::to_underlying(actor.monsterId) == 123 && actor.initialPosition == Vec3f(-10296, -7528, 160);
    }));
    EXPECT_GE(pSpriteObjects.size(), 42u);
    EXPECT_TRUE(std::ranges::any_of(pSpriteObjects, [](const SpriteObject &object) {
        return object.containing_item.itemId == static_cast<ItemId>(160); // Most common oute3.ddm sprite object.
    }));
    EXPECT_EQ(vChests.size(), 20u);

    game.tick(10); // And the game loop should be able to run for a bit without crashing.
}

GAME_TEST(Mm6, NewGameDefaults) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // The authoritative MM6 new-game state lives in new.lod's party.bin, a savegame template:
    // 200 gold, 7 food, quest bits 81 (The Letter delivery quest active) and 181 set, and the
    // default party Roderick/Alexis/Serena/Zoltan with fixed stats, skills, spells and gear.
    EXPECT_EQ(pParty->GetGold(), 200);
    EXPECT_EQ(pParty->GetFood(), 7);
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(81)]);
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(181)]);
    EXPECT_FALSE(pParty->_questBits[QBIT_EMERALD_ISLAND_RED_POTION_ACTIVE]); // No MM7 leakage.

    // The party.bin start pose: New Sorpigal, facing north.
    EXPECT_EQ(pMapStats->pInfos[engine->_currentLoadedMapId].fileName, "oute3.odm");
    EXPECT_EQ(pParty->pos.x, -9728);
    EXPECT_EQ(pParty->pos.y, -11319);
    EXPECT_NEAR(pParty->pos.z, 160, 1);
    EXPECT_EQ(pParty->_viewYaw, 512);

    // The autosave is named after the game version, so MM6 and MM7 saves don't mix.
    EXPECT_TRUE(ufs->exists("saves/autosave.mm6"));
    EXPECT_FALSE(ufs->exists("saves/autosave.mm7"));

    struct DefaultCharacter {
        const char *name;
        Class classType;
        Sex sex;
        int face;
        std::array<int, 7> stats; // Might, Intellect, Personality, Endurance, Accuracy, Speed, Luck.
        std::array<Skill, 4> skills;
        std::vector<int> spells;
        int experience;
        int age;
        int hp;
        int sp;
        std::vector<int> backpack;
        int mainHand;
    };
    // Every caster knows the first spell of their school and carries the book of the second.
    // Gear comes from the skill-derived creation grant (MM6.EXE 0x452820): the ring and herb
    // are random rolls (asserted structurally below), the rest is deterministic per skills.
    std::array<DefaultCharacter, 4> expected = {{
        {"Roderick", CLASS_PALADIN, SEX_MALE, 0, {17, 5, 15, 15, 15, 13, 6},
         {SKILL_SWORD, SKILL_SHIELD, SKILL_CHAIN, SKILL_SPIRIT}, {45}, 343, 21, 31, 7,
         {345, 505}, 1}, // Bless book, The Letter; Longsword.
        {"Alexis", CLASS_ARCHER, SEX_FEMALE, 11, {14, 15, 5, 15, 17, 13, 6},
         {SKILL_AXE, SKILL_BOW, SKILL_AIR, SKILL_PERCEPTION}, {12}, 291, 21, 31, 7,
         {312, 163}, 23}, // Static Charge book, bottle (+ a random herb); Hand Axe.
        {"Serena", CLASS_CLERIC, SEX_FEMALE, 9, {11, 7, 17, 15, 13, 11, 12},
         {SKILL_MACE, SKILL_MIND, SKILL_BODY, SKILL_MEDITATION}, {56, 67}, 266, 22, 24, 22,
         {356, 367, 163}, 50}, // 2 books, bottle (+ a random herb); Mace.
        {"Zoltan", CLASS_SORCERER, SEX_MALE, 7, {11, 17, 7, 15, 13, 13, 9},
         {SKILL_DAGGER, SKILL_FIRE, SKILL_WATER, SKILL_MEDITATION}, {1, 23}, 336, 25, 24, 22,
         {301, 323, 163}, 15}, // 2 books, bottle (+ a random herb); Dagger.
    }};

    for (int i = 0; i < 4; i++) {
        const DefaultCharacter &want = expected[i];
        const Character &have = pParty->pCharacters[i];
        EXPECT_EQ(have.name, want.name);
        EXPECT_EQ(have.classType, want.classType) << want.name;
        EXPECT_EQ(have.uSex, want.sex) << want.name;
        EXPECT_EQ(have.uCurrentFace, want.face) << want.name;
        EXPECT_EQ(have.uLevel, 1) << want.name;
        EXPECT_EQ(have.experience, want.experience) << want.name;
        EXPECT_EQ(have.GetBaseAge(), want.age) << want.name;

        for (int s = 0; s < 7; s++)
            EXPECT_EQ(have._stats[static_cast<Attribute>(s)], want.stats[s]) << want.name << " stat " << s;

        int activeSkills = 0;
        for (Skill skill : allSkills())
            activeSkills += static_cast<bool>(have.pActiveSkills[skill]);
        EXPECT_EQ(activeSkills, 4) << want.name;
        for (Skill skill : want.skills)
            EXPECT_EQ(have.getSkillValue(skill), CombinedSkillValue::novice()) << want.name;

        int knownSpells = 0;
        for (SpellId spell : have.bHaveSpell.indices())
            knownSpells += have.bHaveSpell[spell];
        EXPECT_EQ(knownSpells, static_cast<int>(want.spells.size())) << want.name;
        for (int spell : want.spells)
            EXPECT_TRUE(have.bHaveSpell[static_cast<SpellId>(spell)]) << want.name << " spell " << spell;

        InventoryConstEntry mainHand = have.inventory.entry(ITEM_SLOT_MAIN_HAND);
        ASSERT_TRUE(mainHand) << want.name;
        EXPECT_EQ(mainHand->itemId, static_cast<ItemId>(want.mainHand)) << want.name;
        for (int itemId : want.backpack)
            EXPECT_TRUE(have.inventory.find(static_cast<ItemId>(itemId))) << want.name << " item " << itemId;
        // Every character rolls a random tier-2 ring; the misc-skill characters also carry a
        // random herb next to their potion bottle.
        bool hasRing = false;
        bool hasHerb = false;
        for (InventoryConstEntry entry : have.inventory.entries()) {
            hasRing = hasRing || pItemTable->items[entry->itemId].type == ITEM_TYPE_RING;
            hasHerb = hasHerb || (std::to_underlying(entry->itemId) >= 160 && std::to_underlying(entry->itemId) <= 162);
        }
        EXPECT_TRUE(hasRing) << want.name;
        EXPECT_EQ(hasHerb, have.pActiveSkills[SKILL_MEDITATION] || have.pActiveSkills[SKILL_PERCEPTION]) << want.name;
        for (InventoryConstEntry entry : have.inventory.entries())
            EXPECT_TRUE(entry->IsIdentified()) << want.name;

        // MM6 has no races, and max HP/SP come from the MM6 class tables - the template's values.
        EXPECT_EQ(have.GetRace(), RACE_HUMAN) << want.name;
        EXPECT_EQ(have.GetMaxHealth(), want.hp) << want.name;
        EXPECT_EQ(have.GetMaxMana(), want.sp) << want.name;
        EXPECT_EQ(have.health, have.GetMaxHealth()) << want.name;
        EXPECT_EQ(have.mana, have.GetMaxMana()) << want.name;
    }

    // MM6 starts in year 1165 (MM7 in 1168).
    EXPECT_EQ(pParty->GetPlayingTime().toCivilTime().year, 1165);

    // HP/SP growth: a paladin gains 3 hp & 1 sp per level, and each promotion tier
    // adds another +1 hp & +1 sp per level.
    Character &roderick = pParty->pCharacters[0];
    roderick.uLevel = 2;
    EXPECT_EQ(roderick.GetMaxHealth(), 34); // 25 + 3 * (2 + endBonus 1).
    EXPECT_EQ(roderick.GetMaxMana(), 8);    // 5 + 1 * (2 + perBonus 1).
    roderick.classType = CLASS_CRUSADER;
    EXPECT_EQ(roderick.GetMaxHealth(), 37);
    EXPECT_EQ(roderick.GetMaxMana(), 11);
    roderick.classType = CLASS_HERO;
    EXPECT_EQ(roderick.GetMaxHealth(), 40);
    EXPECT_EQ(roderick.GetMaxMana(), 14);
    roderick.classType = CLASS_PALADIN;
    roderick.uLevel = 1;

    // Roderick's shield hand and armor, and Alexis' bow slot - the creation grant equips
    // what the skills provide.
    EXPECT_EQ(pParty->pCharacters[0].inventory.entry(ITEM_SLOT_OFF_HAND)->itemId, static_cast<ItemId>(84));
    EXPECT_EQ(pParty->pCharacters[0].inventory.entry(ITEM_SLOT_ARMOUR)->itemId, static_cast<ItemId>(71));
    EXPECT_EQ(pParty->pCharacters[1].inventory.entry(ITEM_SLOT_BOW)->itemId, static_cast<ItemId>(47));
}

GAME_TEST(Mm6, WalkAndInteract) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // Walking forward moves the party.
    Vec3f posBefore = pParty->pos;
    game.pressKey(PlatformKey::KEY_UP);
    game.tick(20);
    game.releaseKey(PlatformKey::KEY_UP);
    game.tick(1);
    EXPECT_NE(pParty->pos, posBefore);

    // Turning changes view yaw but not position.
    int yawBefore = pParty->_viewYaw;
    posBefore = pParty->pos;
    game.pressKey(PlatformKey::KEY_LEFT);
    game.tick(5);
    game.releaseKey(PlatformKey::KEY_LEFT);
    game.tick(1);
    EXPECT_NE(pParty->_viewYaw, yawBefore);
    EXPECT_EQ(pParty->pos, posBefore);

    // Pressing the interact key with nothing targeted shouldn't crash.
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(5);

    // And the world should keep simulating - actor AI, animations, ambient sounds - without crashing.
    game.tick(200);
}

GAME_TEST(Mm6, ModernControls) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    // mm6-extra defaults to the modern control scheme: WASD movement, Space jump / fly up,
    // X fly down, Q attack, E quick cast, F interact, L quest book. Native-res rendering
    // (render_filter = 0) is also the default.
    EXPECT_EQ(engine->config->keybindings.Forward.defaultValue(), PlatformKey::KEY_W);
    EXPECT_EQ(engine->config->keybindings.Backward.defaultValue(), PlatformKey::KEY_S);
    EXPECT_EQ(engine->config->keybindings.StepLeft.defaultValue(), PlatformKey::KEY_A);
    EXPECT_EQ(engine->config->keybindings.StepRight.defaultValue(), PlatformKey::KEY_D);
    EXPECT_EQ(engine->config->keybindings.Jump.defaultValue(), PlatformKey::KEY_SPACE);
    EXPECT_EQ(engine->config->keybindings.FlyUp.defaultValue(), PlatformKey::KEY_SPACE);
    EXPECT_EQ(engine->config->keybindings.FlyDown.defaultValue(), PlatformKey::KEY_X);
    EXPECT_EQ(engine->config->keybindings.Attack.defaultValue(), PlatformKey::KEY_Q);
    EXPECT_EQ(engine->config->keybindings.CastReady.defaultValue(), PlatformKey::KEY_E);
    EXPECT_EQ(engine->config->keybindings.EventTrigger.defaultValue(), PlatformKey::KEY_F);
    EXPECT_EQ(engine->config->keybindings.Quest.defaultValue(), PlatformKey::KEY_L);
    EXPECT_EQ(engine->config->keybindings.ToggleMouseLook.defaultValue(), PlatformKey::KEY_F10); // Keyboard fallback; middle mouse is the primary toggle.
    EXPECT_EQ(engine->config->graphics.RenderFilter.defaultValue(), 0);

    // The game-test harness pins the classic bindings before every test (recorded traces and the
    // older scripted tests replay raw keypresses that assume them) - apply the modern defaults to
    // actually exercise the shipped scheme. The next test's prepareForNextTest() re-pins classic.
    keyboardActionMapping->applyKeybindings(keyboardActionMapping->defaultKeybindings(KEYBINDINGS_ALL));

    game.startNewGame();

    // W walks forward.
    Vec3f posBefore = pParty->pos;
    game.pressKey(PlatformKey::KEY_W);
    game.tick(20);
    game.releaseKey(PlatformKey::KEY_W);
    game.tick(1);
    EXPECT_NE(pParty->pos, posBefore);

    // A strafes left: position changes, yaw doesn't.
    int yawBefore = pParty->_viewYaw;
    posBefore = pParty->pos;
    game.pressKey(PlatformKey::KEY_A);
    game.tick(20);
    game.releaseKey(PlatformKey::KEY_A);
    game.tick(1);
    EXPECT_NE(pParty->pos, posBefore);
    EXPECT_EQ(pParty->_viewYaw, yawBefore);

    // Space jumps: the party leaves the ground.
    float zBefore = pParty->pos.z;
    game.pressKey(PlatformKey::KEY_SPACE);
    game.tick(3);
    game.releaseKey(PlatformKey::KEY_SPACE);
    EXPECT_GT(pParty->pos.z, zBefore);
    game.tick(30); // Land again.

    // Q attacks: the active character swings and goes into recovery. The engine then advances
    // the active character to the next ready one, so keep a reference to the attacker.
    Character &qAttacker = pParty->activeCharacter();
    EXPECT_EQ(qAttacker.timeToRecovery, 0_ticks);
    game.pressAndReleaseKey(PlatformKey::KEY_Q);
    game.tick(2);
    EXPECT_GT(qAttacker.timeToRecovery, 0_ticks);

    // E quick-casts: with no quick spell readied it falls back to an attack, which exercises
    // the INPUT_ACTION_QUICK_CAST binding end to end. The attacker is now the next ready
    // character (Alexis, who shoots her bow - recovery is applied when the arrow sprite spawns).
    Character &eAttacker = pParty->activeCharacter();
    EXPECT_NE(&eAttacker, &qAttacker);
    EXPECT_EQ(eAttacker.timeToRecovery, 0_ticks);
    game.pressAndReleaseKey(PlatformKey::KEY_E);
    game.tick(2);
    EXPECT_GT(eAttacker.timeToRecovery, 0_ticks);

    // F interacts: the wandering above has left the party in front of a house door, so the
    // interact fires the doorway event and enters the house - the binding works end to end.
    game.pressAndReleaseKey(PlatformKey::KEY_F);
    game.tick(5);
    EXPECT_EQ(current_screen_type, SCREEN_HOUSE);

    // Escape leaves the house.
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);

    // Middle mouse toggles mouselook. While enabled, the pick/cursor position is pinned to the
    // viewport center (crosshair targeting) regardless of where the mouse moves.
    EXPECT_EQ(mouse->_mouseLook, Io::Mouse::MouseLookState::Disabled);
    game.pressAndReleaseButton(BUTTON_MIDDLE, 320, 240);
    game.tick(1);
    EXPECT_EQ(mouse->_mouseLook, Io::Mouse::MouseLookState::Enabled);
    game.moveMouse(50, 50);
    game.tick(1);
    EXPECT_EQ(mouse->position(), pViewport.center());
    game.pressAndReleaseButton(BUTTON_MIDDLE, 320, 240);
    game.tick(1);
    EXPECT_EQ(mouse->_mouseLook, Io::Mouse::MouseLookState::Disabled);
}

GAME_TEST(Mm6, StrafeKeybindingConflictChecked) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    // The modern scheme puts strafe on A/D, but the strafe actions sit past the controls menu's
    // two 14-slot pages and used to sit outside the configurable segment too - so the menu's
    // conflict scan never saw them. Rebinding Attack onto A (the classic MM7 attack key, the
    // single most likely rebind a returning player performs) was accepted silently, and holding
    // A then attacked AND strafed every frame. Strafe is conflict-checked and DEFAULT-reset now;
    // the menu art has exactly 2x14 engraved slots and two page tabs, so the two strafe rows
    // themselves are still not rendered (rebinding strafe means editing openenroth.ini).

    // The harness pins classic bindings before every test - apply the shipped modern defaults
    // (Attack=Q, StepLeft=A) to reproduce the fresh-install scenario. The next test's
    // prepareForNextTest() re-pins classic; the scope guard just keeps this test tidy on
    // early EXPECT failures.
    keyboardActionMapping->applyKeybindings(keyboardActionMapping->defaultKeybindings(KEYBINDINGS_ALL));
    MM_AT_SCOPE_EXIT({
        keyboardActionMapping->applyKeybindings(keyboardActionMapping->defaultKeybindings(KEYBINDINGS_ALL));
    });

    game.startNewGame();

    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_MENU);
    engine->_messageQueue->addMessageCurrentFrame(UIMSG_OpenKeyMappingOptions, 0, 0);
    game.tick(3);
    ASSERT_EQ(current_screen_type, SCREEN_KEYBOARD_OPTIONS);

    // Rebind Attack onto A through the real rebind flow: select the Attack row, press A.
    engine->_messageQueue->addMessageCurrentFrame(UIMSG_ChangeKeyButton, std::to_underlying(INPUT_ACTION_ATTACK), 0);
    game.tick(2);
    game.pressAndReleaseKey(PlatformKey::KEY_A);
    game.tick(2);

    // The clash with strafe-left must be flagged...
    EXPECT_TRUE(key_map_conflicted.contains(INPUT_ACTION_ATTACK));
    EXPECT_TRUE(key_map_conflicted.contains(INPUT_ACTION_STRAFE_LEFT));

    // ...and the RETURN button must refuse to close the menu while the conflict stands, exactly
    // like it does for a conflict between two visible actions.
    game.pressAndReleaseButton(BUTTON_LEFT, 348, 322);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_KEYBOARD_OPTIONS);

    if (current_screen_type == SCREEN_KEYBOARD_OPTIONS) {
        // DEFAULT resolves the conflict (it covers strafe now too), after which RETURN works.
        game.pressGuiButton("KeyBinding_Default");
        game.tick(2);
        EXPECT_TRUE(key_map_conflicted.empty());
        game.pressAndReleaseButton(BUTTON_LEFT, 348, 322);
        game.tick(2);
    }
    EXPECT_EQ(current_screen_type, SCREEN_MENU);
    EXPECT_EQ(engine->config->keybindings.Attack.value(), PlatformKey::KEY_Q);
    EXPECT_EQ(engine->config->keybindings.StepLeft.value(), PlatformKey::KEY_A);

    game.goToGame();
}

GAME_TEST(Mm6, KillAndLootPeasant) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // Find the known placed level 3 peasant (first actor record of oute3.ddm).
    auto peasant = std::ranges::find_if(pActors, [](const Actor &actor) {
        return std::to_underlying(actor.monsterId) == 123 && actor.initialPosition == Vec3f(-10296, -7528, 160);
    });
    ASSERT_NE(peasant, pActors.end());
    int peasantId = peasant->id;

    // The ddm-embedded stat block is what the engine uses (matching the original), and for this actor it
    // slightly diverges from monsters.txt row 123 (PeasantF1C): 3D6 gold instead of the txt's 4D6.
    EXPECT_EQ(peasant->monsterInfo.exp, 39);
    EXPECT_EQ(peasant->monsterInfo.goldDiceRolls, 3);
    EXPECT_EQ(peasant->monsterInfo.goldDiceSides, 6);

    auto teleportNextTo = [&](Vec3f targetPos, float distance) {
        Vec3f pos = targetPos + Vec3f(-distance, 0, 0);
        int yawDegrees = TrigLUT.atan2(targetPos.x - pos.x, targetPos.y - pos.y) * 90 / 512;
        game.teleportTo(engine->_currentLoadedMapId, pos, yawDegrees);
        game.tick(1);
    };

    uint64_t expBefore = 0;
    for (const Character &character : pParty->pCharacters)
        expBefore += character.experience;
    int goldBefore = pParty->GetGold();

    // Melee the peasant until it dies, chasing it if it flees.
    for (int i = 0; i < 100 && pActors[peasantId].aiState != Dead; i++) {
        teleportNextTo(pActors[peasantId].pos, 160);
        game.pressAndReleaseKey(PlatformKey::KEY_A);
        game.tick(2);
    }
    EXPECT_EQ(pActors[peasantId].aiState, Dead);

    // The kill awards the monster's exp, split between the four party members.
    uint64_t expAfter = 0;
    for (const Character &character : pParty->pCharacters)
        expAfter += character.experience;
    EXPECT_GE(expAfter - expBefore, 36u); // 39 / 4 = 9 per character, at least.

    // Looting the corpse with the interact key rolls the peasant's 3D6 gold dice. Loot from a bit further
    // out: a corpse right at the party's feet projects below the game viewport and isn't pickable, just
    // like in the original. The peasant flees mid-fight and can die with scenery or bystanders in the
    // way, so try several approach sides and distances until the pick lands on the corpse.
    int goldFound = 0;
    Vec3f corpsePos = pActors[peasantId].pos;
    for (Vec3f offset : {Vec3f(-350, 0, 0), Vec3f(350, 0, 0), Vec3f(0, -350, 0), Vec3f(0, 350, 0),
                         Vec3f(-250, 0, 0), Vec3f(250, 0, 0), Vec3f(0, -250, 0), Vec3f(0, 250, 0)}) {
        Vec3f pos = corpsePos + offset;
        int yawDegrees = TrigLUT.atan2(corpsePos.x - pos.x, corpsePos.y - pos.y) * 90 / 512;
        game.teleportTo(engine->_currentLoadedMapId, pos, yawDegrees);
        game.tick(1);
        game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
        game.tick(2);
        if (current_screen_type != SCREEN_GAME) { // Picked a live bystander instead - dismiss its dialogue and retry.
            game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
            game.tick(2);
            continue;
        }
        goldFound = pParty->GetGold() - goldBefore;
        if (goldFound)
            break;
    }
    EXPECT_GE(goldFound, 3);
    EXPECT_LE(goldFound, 18);
}

GAME_TEST(Mm6, EnterGoblinwatch) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // Goblinwatch is New Sorpigal's dungeon, an indoor map.
    MapId goblinwatch = pMapStats->GetMapInfo("d01.blv");
    ASSERT_NE(goblinwatch, MAP_INVALID);
    game.teleportTo(goblinwatch, Vec3f(-1850, 4304, -512), 0); // First spawn point of d01.blv.

    // The blv geometry should have been loaded: d01.blv carries 2287 vertices, 2291 faces, 59 sectors
    // and 200 door records.
    EXPECT_EQ(uCurrentlyLoadedLevelType, LEVEL_INDOOR);
    EXPECT_EQ(pIndoor->vertices.size(), 2287u);
    EXPECT_EQ(pIndoor->faces.size(), 2291u);
    EXPECT_EQ(pIndoor->sectors.size(), 59u);
    EXPECT_EQ(pIndoor->doors.size(), 200u);

    // And the party should be inside the dungeon, in a valid sector.
    EXPECT_NE(pIndoor->GetSector(pParty->pos.x, pParty->pos.y, pParty->pos.z), 0);

    // First entry respawns the location: monsters are generated from the blv's 66 spawn points
    // (rats / goblins / bloodsuckers per mapstats.txt), and d01.dlv's 20 chest records are loaded.
    EXPECT_GE(pActors.size(), 66u);
    EXPECT_TRUE(std::ranges::all_of(pActors, [](const Actor &actor) {
        int monsterId = std::to_underlying(actor.monsterId);
        return (monsterId >= 13 && monsterId <= 15)      // Bloodsucker A-C.
            || (monsterId >= 76 && monsterId <= 78)      // Goblin A-C.
            || (monsterId >= 145 && monsterId <= 147);   // Rat A-C.
    }));
    EXPECT_EQ(vChests.size(), 20u);

    // The game loop should keep running: actor AI, animations, doors.
    game.tick(20);
}

GAME_TEST(Mm6, SelectionRingReloadedOnMapChange) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // The first HUD draw lazily resolves the "aframe1" frameset (the animated gold ring around the
    // active character's portrait) and loads its sprites.
    ASSERT_TRUE(pParty->hasActiveCharacter());
    int framesetId = pSpriteFrameTable->FastFindSprite("aframe1");
    ASSERT_GT(framesetId, 0);
    ASSERT_TRUE(pSpriteFrameTable->pSpriteSFrames[framesetId].flags & SPRITE_FRAME_LOADED);

    // A map change releases all unreserved sprites and clears every frameset's LOADED flag, leaving
    // sprites[] dangling into the freed cache nodes. The next HUD draw must re-initialize the
    // frameset so the ring points at live sprites again.
    MapId goblinwatch = pMapStats->GetMapInfo("d01.blv");
    ASSERT_NE(goblinwatch, MAP_INVALID);
    game.teleportTo(goblinwatch, Vec3f(-1850, 4304, -512), 0); // First spawn point of d01.blv.
    game.tick(1);

    EXPECT_TRUE(pSpriteFrameTable->pSpriteSFrames[framesetId].flags & SPRITE_FRAME_LOADED);
    EXPECT_NE(pSpriteFrameTable->pSpriteSFrames[framesetId].sprites[0], nullptr);
}

GAME_TEST(Mm6, EnterTempleOfBaaThroughDoor) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // New Sorpigal's Abandoned Temple of Baa entrance is the door face wired to local event 102,
    // an ungated MoveToMap into d02.blv.
    const BLVFace *door = nullptr;
    for (const BSPModel &model : pOutdoor->pBModels) {
        for (const BLVFace &face : model.faces) {
            if (face.eventId == 102 && face.Clickable()) {
                door = &face;
                break;
            }
        }
    }
    ASSERT_NE(door, nullptr);

    // Stand right in front of the door, facing it.
    Vec3f doorCenter = door->boundingBox.center();
    Vec3f pos = doorCenter + door->facePlane.normal * 130;
    pos.z = door->boundingBox.z1; // Feet on the ground, not at the door's mid-height.
    int yawDegrees = TrigLUT.atan2(doorCenter.x - pos.x, doorCenter.y - pos.y) * 90 / 512;
    game.teleportTo(engine->_currentLoadedMapId, pos, yawDegrees);
    game.tick(1);

    // Interacting with the door fires event 102, which opens the enter-the-dungeon prompt.
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(1);

    // The prompt draws MM6's dialogue skin: the evpan004 marble panel with the transition picture
    // directly on it, and the yes/cancel buttons on the panel's bottom row (see Mm6.DialogueSkin).
    ASSERT_NE(game_ui_dialogue_background, nullptr);
    EXPECT_EQ(game_ui_dialogue_background->name(), "evpan004");
    EXPECT_EQ(game_ui_dialogue_background->size(), Sizei(152, 353));
    ASSERT_NE(transition_ui_icon, nullptr);
    EXPECT_EQ(transition_ui_icon->name(), "castle"); // Event 102 passes exit-pic id 1 = MM6's "castle".
    ASSERT_NE(pBtn_YES, nullptr);
    EXPECT_EQ(pBtn_YES->rect, Recti(486, 313, 62, 29));
    ASSERT_NE(pBtn_ExitCancel, nullptr);
    EXPECT_EQ(pBtn_ExitCancel->rect, Recti(566, 313, 62, 29));

    // Confirming it loads the Abandoned Temple.
    game.pressAndReleaseKey(PlatformKey::KEY_Y);
    game.tick(5);
    EXPECT_EQ(uCurrentlyLoadedLevelType, LEVEL_INDOOR);
    EXPECT_EQ(pMapStats->pInfos[engine->_currentLoadedMapId].fileName, "d02.blv");

    // The event's MoveToMap teleports the party to the temple's entrance and the map is live.
    EXPECT_EQ(pParty->pos, Vec3f(16406, -19669, 865));
    EXPECT_NE(pIndoor->GetSector(pParty->pos.x, pParty->pos.y, pParty->pos.z), 0);
    game.tick(20);
}

GAME_TEST(Mm6, MapActorGarbageNpcIds) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // Several shipped MM6 dungeon templates (d05, d12, d19, t4, zdwj02) store uninitialized
    // map-editor garbage in the actor npcId slot - d05.dlv's Snergle record reads 30757. Such a
    // value must not survive the load as an NPC link: GetDisplayName would otherwise index
    // pAdditionalNPC[30757 - 5000] on a 100-entry array.
    MapId snergleMines = pMapStats->GetMapInfo("d05.blv");
    ASSERT_NE(snergleMines, MAP_INVALID);
    game.teleportTo(snergleMines, Vec3f(0, 0, 0), 0);
    EXPECT_EQ(uCurrentlyLoadedLevelType, LEVEL_INDOOR);

    // MM6 map records never hold NPC handles - street citizens are only branded on first talk -
    // so every freshly loaded actor must have a zero npcId and a displayable name.
    ASSERT_FALSE(pActors.empty());
    for (const Actor &actor : pActors) {
        EXPECT_EQ(actor.npcId, 0);
        EXPECT_FALSE(actor.GetDisplayName().empty());
    }
}

GAME_TEST(Mm6, OpenChestInGoblinwatch) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    MapId goblinwatch = pMapStats->GetMapInfo("d01.blv");
    ASSERT_NE(goblinwatch, MAP_INVALID);
    game.teleportTo(goblinwatch, Vec3f(-1850, 4304, -512), 0); // First spawn point of d01.blv.

    // Find a clickable vertical (side-facing) face wired to an OpenChest event, so that the party
    // can stand in front of it. Prefer an untrapped chest so that interacting opens the chest UI
    // instead of setting off the trap.
    const BLVFace *chestFace = nullptr;
    int chestId = -1;
    for (const BLVFace &face : pIndoor->faces) {
        if (!face.eventId || !face.Clickable() || !engine->_localEventMap.hasEvent(face.eventId))
            continue;
        if (std::abs(face.facePlane.normal.z) >= 0.5f)
            continue; // A chest lid/floor plate - the party can't stand in front of it.
        for (const EvtInstruction &instruction : engine->_localEventMap.function(face.eventId)) {
            if (instruction.opcode == EVENT_OpenChest && !vChests[instruction.data.chest_id].Trapped()) {
                chestFace = &face;
                chestId = instruction.data.chest_id;
                break;
            }
        }
        if (chestFace)
            break;
    }
    ASSERT_NE(chestFace, nullptr);

    // Stand in front of the chest, facing it.
    Vec3f chestCenter = chestFace->boundingBox.center();
    Vec3f pos = chestCenter + chestFace->facePlane.normal * 130;
    pos.z = chestFace->boundingBox.z1;
    int yawDegrees = TrigLUT.atan2(chestCenter.x - pos.x, chestCenter.y - pos.y) * 90 / 512;
    game.teleportTo(goblinwatch, pos, yawDegrees);
    game.tick(1);

    // Interacting with the chest fires its OpenChest event and brings up the chest screen.
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(1);
    EXPECT_EQ(current_screen_type, SCREEN_CHEST);

    // The opened chest was set up: its stash was laid out on the chest grid.
    EXPECT_TRUE(vChests[chestId].Initialized());

    // With MM6 item data loaded, chest contents are real items: every item in every chest of the
    // dungeon resolves to a named items.txt entry, and the chests aren't all empty.
    int chestItemCount = 0;
    for (const Chest &chest : vChests) {
        for (auto entry : chest.inventory.entries()) {
            EXPECT_FALSE(pItemTable->items[(*entry).itemId].name.empty())
                << "item id " << std::to_underlying((*entry).itemId);
            chestItemCount++;
        }
    }
    EXPECT_GT(chestItemCount, 0);

    // Escape closes the chest and the game is live again.
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
    game.tick(20);
}

GAME_TEST(Mm6, SwitchChangesDoorStateInGoblinwatch) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    MapId goblinwatch = pMapStats->GetMapInfo("d01.blv");
    ASSERT_NE(goblinwatch, MAP_INVALID);
    game.teleportTo(goblinwatch, Vec3f(-1850, 4304, -512), 0); // First spawn point of d01.blv.

    // Find a clickable vertical face whose event script starts with a ChangeDoorState - a switch
    // or a door handle - and locate the door it drives.
    const BLVFace *switchFace = nullptr;
    int doorId = -1;
    for (const BLVFace &face : pIndoor->faces) {
        if (!face.eventId || !face.Clickable() || !engine->_localEventMap.hasEvent(face.eventId))
            continue;
        if (std::abs(face.facePlane.normal.z) >= 0.5f)
            continue;
        const std::vector<EvtInstruction> &script = engine->_localEventMap.function(face.eventId);
        if (script.size() >= 2 && script[1].opcode == EVENT_ChangeDoorState) {
            switchFace = &face;
            doorId = script[1].data.door_descr.door_id;
            break;
        }
    }
    ASSERT_NE(switchFace, nullptr);

    BLVDoor *door = nullptr;
    for (BLVDoor &candidate : pIndoor->doors) {
        if (candidate.doorId == static_cast<uint32_t>(doorId)) {
            door = &candidate;
            break;
        }
    }
    ASSERT_NE(door, nullptr);
    DoorState stateBefore = door->state;

    // Stand in front of the switch, facing it.
    Vec3f switchCenter = switchFace->boundingBox.center();
    Vec3f pos = switchCenter + switchFace->facePlane.normal * 130;
    pos.z = switchFace->boundingBox.z1;
    int yawDegrees = TrigLUT.atan2(switchCenter.x - pos.x, switchCenter.y - pos.y) * 90 / 512;
    game.teleportTo(goblinwatch, pos, yawDegrees);
    game.tick(1);

    // Interacting fires the event, and the door it drives starts moving.
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);
    EXPECT_NE(door->state, stateBefore);
    game.tick(20); // And the door animation keeps the game loop happy.
}

GAME_TEST(Mm6, AllMapEventsParse) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // The global event map is parsed during engine initialization; its first event is a Compare record.
    EXPECT_TRUE(engine->_globalEventMap.hasEvent(1));

    // Local events of every MM6 map parse with the MM6 opcode/operand layouts.
    int parsed = 0;
    for (MapId mapId : allMaps()) {
        if (!isMapIndoor(mapId) && !isMapOutdoor(mapId))
            continue; // Not an MM6 map slot.
        std::string fileName = pMapStats->pInfos[mapId].fileName;
        std::string baseName = fileName.substr(0, fileName.rfind('.'));
        EXPECT_NO_THROW(EvtProgram::load(engine->resources()->eventsData(baseName + ".evt"), engine->gameVersion())) << fileName;
        parsed++;
    }
    EXPECT_EQ(parsed, 67); // All of MM6's maps.
}

GAME_TEST(Mm6, ClassPromotion) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // MM6 stores a character's class as a byte, base*3 + tier, base order Knight/Cleric/Sorcerer/
    // Paladin/Archer/Druid (see mm6-character-model.md). Promotion events drive it via
    // If(Class == byte) / Set(Class, byte); the byte must map onto the engine's MM7-shaped Class enum,
    // else a promotion gate misfires (a Paladin, CLASS_PALADIN=12, fails If(Class == 9)) or lands on
    // a garbage class (Set(Class, 10) would be CLASS_MASTER, not Crusader).
    struct PromotionCase { int mm6Byte; Class expected; };
    static const std::array<PromotionCase, 18> cases = {{
        {0, CLASS_KNIGHT}, {1, CLASS_CAVALIER}, {2, CLASS_CHAMPION},
        {3, CLASS_CLERIC}, {4, CLASS_PRIEST}, {5, CLASS_PRIEST_OF_SUN},
        {6, CLASS_SORCERER}, {7, CLASS_WIZARD}, {8, CLASS_ARCHAMGE},
        {9, CLASS_PALADIN}, {10, CLASS_CRUSADER}, {11, CLASS_HERO},
        {12, CLASS_ARCHER}, {13, CLASS_WARRIOR_MAGE}, {14, CLASS_MASTER_ARCHER},
        {15, CLASS_DRUID}, {16, CLASS_GREAT_DRUID}, {17, CLASS_ARCH_DRUID},
    }};

    Character &character = pParty->pCharacters[0];
    for (const PromotionCase &c : cases) {
        // Set(Class, byte) - the promotion "grant" - lands on the right enum class...
        character.SetVariable(VAR_Class, c.mm6Byte);
        EXPECT_EQ(character.classType, c.expected) << "Set(Class, " << c.mm6Byte << ")";
        // ...and If(Class == byte) - the promotion "gate" - matches the class just set, and nothing else.
        EXPECT_TRUE(character.CompareVariable(VAR_Class, c.mm6Byte)) << "If(Class == " << c.mm6Byte << ")";
        if (c.mm6Byte != 9) // Paladin.
            EXPECT_FALSE(character.CompareVariable(VAR_Class, 9)) << "byte 9 must not match byte " << c.mm6Byte;
    }

    // Promotion grows HP/SP by a tier: Paladin(3/1) -> Crusader(4/2) -> Hero(5/3) per level.
    character.SetVariable(VAR_Class, 9); // Paladin.
    int paladinHp = character.GetMaxHealth(), paladinSp = character.GetMaxMana();
    character.SetVariable(VAR_Class, 10); // Crusader.
    int crusaderHp = character.GetMaxHealth(), crusaderSp = character.GetMaxMana();
    character.SetVariable(VAR_Class, 11); // Hero.
    EXPECT_GT(crusaderHp, paladinHp);
    EXPECT_GT(crusaderSp, paladinSp);
    EXPECT_GT(character.GetMaxHealth(), crusaderHp);
    EXPECT_GT(character.GetMaxMana(), crusaderSp);

    // End-to-end: run the real Cleric->Priest promotion (global event 36), the effect an NPC promotion
    // topic executes. Its class byte differs from its enum (Cleric=3 vs CLASS_CLERIC=24), so it only
    // promotes once the translation is in place. Start at step 4 to skip the QBits[106] quest gate and
    // its promotion-speech ShowMessage; the mechanic (per-member ForPartyMember -> If(Class==3) ->
    // Set(Class,4)) runs from there. Global events resolve against the global map only while a level
    // decoration is active, so point at a scratch one for the call. The default MM6 party's Serena is
    // the Cleric (member 2); the non-Clerics must be left untouched.
    Character &serena = pParty->pCharacters[2];
    ASSERT_EQ(serena.classType, CLASS_CLERIC);
    Class roderickClass = pParty->pCharacters[0].classType; // Roderick, a Paladin - not a Cleric.
    LevelDecoration scratchDecoration;
    LevelDecoration *savedDecoration = activeLevelDecoration;
    activeLevelDecoration = &scratchDecoration;
    eventProcessor(36, Pid(), false, 4);
    activeLevelDecoration = savedDecoration;
    EXPECT_EQ(serena.classType, CLASS_PRIEST);
    EXPECT_EQ(pParty->pCharacters[0].classType, roderickClass);
}

GAME_TEST(Mm6, RiddlePasswordPrompt) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // The Dragoons' Caverns (cd1.blv) sword-in-the-rock is MM6's password prompt: local event 69
    // asks "What's the password?" (answers JBARD / jbard). A correct answer jumps to the
    // MapVars[6] = 1 branch, a wrong answer falls through into a teleport trap.
    MapId caverns = pMapStats->GetMapInfo("cd1.blv");
    ASSERT_NE(caverns, MAP_INVALID);
    game.teleportTo(caverns, Vec3f(-3136, 2240, 224), 0); // A known-valid cd1 position (the event's own teleport target).

    // Event 69 hangs on a pressure-plate floor face: it fires when the party steps onto it.
    const BLVFace *plate = nullptr;
    for (const BLVFace &face : pIndoor->faces) {
        if (face.eventId == 69 && (face.attributes & FACE_PRESSURE_PLATE)) {
            plate = &face;
            break;
        }
    }
    ASSERT_NE(plate, nullptr);
    Vec3f platePos = plate->boundingBox.center();
    platePos.z = plate->boundingBox.z1;
    Vec3f awayPos = platePos + Vec3f(300, 0, 0);

    // Stepping onto the plate opens the password prompt.
    game.teleportTo(caverns, awayPos, 0);
    game.tick(1);
    game.teleportTo(caverns, platePos, 0);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_BRANCHLESS_NPC_DIALOG);

    // Escape cancels the prompt without taking either event branch: no trap teleport, no unlock.
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
    EXPECT_LE((pParty->pos - platePos).length(), 256.0f);
    EXPECT_EQ(engine->_persistentVariables.mapVars[6], 0);

    // Step off and back on: the prompt reopens (pressure plates are edge-triggered).
    game.teleportTo(caverns, awayPos, 0);
    game.tick(2);
    game.teleportTo(caverns, platePos, 0);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_BRANCHLESS_NPC_DIALOG);

    // Typing the password unlocks: the event jumps to its correct-answer branch.
    for (PlatformKey key : {PlatformKey::KEY_J, PlatformKey::KEY_B, PlatformKey::KEY_A, PlatformKey::KEY_R, PlatformKey::KEY_D}) {
        game.pressAndReleaseKey(key);
        game.tick(1);
    }
    game.pressAndReleaseKey(PlatformKey::KEY_RETURN);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
    EXPECT_LE((pParty->pos - platePos).length(), 256.0f); // Not teleported into the wrong-answer trap.
    EXPECT_EQ(engine->_persistentVariables.mapVars[6], 1);
    game.tick(5);
}

GAME_TEST(Mm6, QuestItemPickup) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // MM6 item data is loaded: the Fly spell scroll is items.txt row 220, and the rnditems.txt
    // random-generation chances are populated.
    EXPECT_EQ(pItemTable->items[ItemId(220)].name, "Fly");
    EXPECT_EQ(pItemTable->items[ItemId(220)].iconName, "scroll4");
    EXPECT_EQ(pItemTable->items[ItemId(220)].type, ITEM_TYPE_SPELL_SCROLL);
    EXPECT_EQ(pItemTable->items[ItemId(220)].baseValue, 300);
    EXPECT_GT(pItemTable->itemChanceSumByTreasureLevel[ITEM_TREASURE_LEVEL_1], 0);

    // New Sorpigal's free Fly scroll: local event 225 is an ungated Add(ItemInHands, 220) pickup
    // hanging on a clickable outdoor model face.
    const BLVFace *scrollFace = nullptr;
    for (const BSPModel &model : pOutdoor->pBModels) {
        for (const BLVFace &face : model.faces) {
            if (face.eventId == 225 && face.Clickable()) {
                scrollFace = &face;
                break;
            }
        }
    }
    ASSERT_NE(scrollFace, nullptr);

    // Stand in front of the scroll, facing it.
    Vec3f scrollCenter = scrollFace->boundingBox.center();
    Vec3f pos = scrollCenter + scrollFace->facePlane.normal * 130;
    pos.z = scrollFace->boundingBox.z1;
    int yawDegrees = TrigLUT.atan2(scrollCenter.x - pos.x, scrollCenter.y - pos.y) * 90 / 512;
    game.teleportTo(engine->_currentLoadedMapId, pos, yawDegrees);
    game.tick(1);

    // Interacting fires the event and the scroll lands in the party's hands (on the cursor).
    EXPECT_EQ(pParty->pPickedItem.itemId, ITEM_NULL);
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);
    EXPECT_EQ(pParty->pPickedItem.itemId, ItemId(220));
    game.tick(5);
}

// Collects the proprietor dialogue options currently on offer, in on-screen order.
static std::vector<DialogueId> listProprietorOptions() {
    std::vector<DialogueId> result;
    if (pDialogueWindow)
        for (const GUIButton *button : pDialogueWindow->vButtons)
            if (button->msg == UIMSG_SelectProprietorDialogueOption)
                result.push_back(static_cast<DialogueId>(button->msg_param));
    return result;
}

GAME_TEST(Mm6, EnterWeaponShop) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // The Knife Shoppe is New Sorpigal's weapon shop: 2dEvents.txt row 1, loaded into houseTable.
    EXPECT_EQ(houseTable[HouseId(1)].name, "The Knife Shoppe");
    EXPECT_EQ(houseTable[HouseId(1)].uType, HOUSE_TYPE_WEAPON_SHOP);

    // Its door face is wired to local event 17, an ungated SpeakInHouse(1).
    const BLVFace *door = nullptr;
    for (const BSPModel &model : pOutdoor->pBModels) {
        for (const BLVFace &face : model.faces) {
            if (face.eventId == 17 && face.Clickable()) {
                door = &face;
                break;
            }
        }
    }
    ASSERT_NE(door, nullptr);

    // Stand in front of the door, facing it.
    Vec3f doorCenter = door->boundingBox.center();
    Vec3f pos = doorCenter + door->facePlane.normal * 130;
    pos.z = door->boundingBox.z1;
    int yawDegrees = TrigLUT.atan2(doorCenter.x - pos.x, doorCenter.y - pos.y) * 90 / 512;
    game.teleportTo(engine->_currentLoadedMapId, pos, yawDegrees);
    game.tick(1);

    // Interacting with the door fires the SpeakInHouse event and brings up the house screen.
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_HOUSE);
    ASSERT_NE(window_SpeakInHouse, nullptr);
    EXPECT_EQ(window_SpeakInHouse->houseId(), HouseId(1));

    // MM6 shops offer the flat five-option menu of MM6.EXE's option factory - Buy Standard /
    // Sell / Identify / Repair / Buy Special - and NO skill teaching (no MM6 shop teaches).
    EXPECT_EQ(listProprietorOptions(),
              (std::vector<DialogueId>{DIALOGUE_SHOP_BUY_STANDARD, DIALOGUE_SHOP_SELL, DIALOGUE_SHOP_IDENTIFY,
                                       DIALOGUE_SHOP_REPAIR, DIALOGUE_SHOP_BUY_SPECIAL}));

    // The labels are MM6's own one-word global.txt rows (the option-label table @0x4461d8 reads
    // rows 33/200/113/179/210) - row 33 is "Buy" in MM6 but "Ranger Lord" in MM7's global.txt.
    std::vector<std::string> labels;
    for (const GUIButton *button : pDialogueWindow->vButtons)
        if (button->msg == UIMSG_SelectProprietorDialogueOption)
            labels.push_back(button->sLabel);
    EXPECT_EQ(labels, (std::vector<std::string>{"Buy", "Sell", "Identify", "Repair", "Special"}));

    // Escape leaves the shop and the game is live again.
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
    game.tick(5);
}

// MM6 ships every movie as a Smacker clip with an extension-less entry name inside anims1/anims2.vid -
// including the house room animations (the FLC-era names in pAnimatedRoomsMm6 notwithstanding, every
// shipped payload starts with the SMK2 magic, so the existing ffmpeg smacker path decodes them all).
// MM6.EXE plays 3dologo -> jvc -> mm6intro at startup (0x4A6AE0), losegame on a party death (0x4A6C70),
// credits as a movie from the main menu (0x4A6C90), and comped + end_dome/planetxp + end_seq1 (win only)
// around the endgame certificate (0x4A6CB0). LoadMovie resolves the extension-less names, so all of
// these open; house/transition screens then pick their movies up through the milestone-44 anim table.
GAME_TEST(Mm6, MoviesPlay) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // Every clip of MM6's fullscreen sequences resolves and opens through ffmpeg. mm6intro and
    // end_seq1 carry a SECOND simultaneous audio track, which opens into its own streaming track.
    for (std::string_view clip : {"3dologo", "jvc", "mm6intro", "losegame", "credits",
                                  "comped", "end_dome", "planetxp", "end_seq1"}) {
        std::unique_ptr<IMovie> movie = pMediaPlayer->loadFullScreenMovie(clip);
        ASSERT_NE(movie, nullptr) << "clip: " << clip;
        int expectedAudioTracks = (clip == "mm6intro" || clip == "end_seq1") ? 2 : 1;
        EXPECT_EQ(movie->audioTrackCount(), expectedAudioTracks) << "clip: " << clip;
    }

    // Entering a house loads its room animation ("Blcksmid" for The Knife Shoppe); leaving unloads it.
    ASSERT_TRUE(enterHouse(HouseId(1)));
    createHouseUI(HouseId(1));
    EXPECT_NE(pMovie_Track, nullptr);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
    EXPECT_EQ(pMovie_Track, nullptr);
    game.tick(5);
}

// MM6 dialogue screens draw MM6's own skin, reversed from MM6.EXE (street dialogue draw @0x43ab90,
// house dialogue draw @0x497ebf, transition draws @0x43a300/0x43a630, house asset init @0x43c66a,
// occupant loader @0x43c140): the regular HUD stays up and an "evpan###" marble panel (152x353)
// covers the right column at (481,0) - per-house from the animated-rooms table (MM6.EXE 0x4BE888,
// now `pAnimatedRoomsMm6`), evpan019 for street NPCs, evpan004 for transition prompts. Portraits
// (63x73) sit frameless at (525,34) on the panel; the buttyes/buttesc buttons (61x28) sit on the
// panel's bottom row. The same table fixes the house room sounds (id formula type + 100 * (id +
// 300) hits MM6's own 3xx_0y sound entries) and the proprietor portraits (npc505+ commoner block).
GAME_TEST(Mm6, DialogueSkin) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // The animated-rooms accessor dispatches to the MM6 table extracted from MM6.EXE 0x4BE888.
    EXPECT_EQ(pAnimatedRoomsMm6.size(), 119u);
    EXPECT_EQ(houseTable[HouseId(1)].uAnimationID, 2); // The Knife Shoppe.
    EXPECT_EQ(houseAnimDescr(2).video_name, "Blcksmid");
    EXPECT_EQ(houseAnimDescr(2).uDialoguePanelId, 13);
    EXPECT_EQ(houseAnimDescr(2).house_npc_id, 506);
    EXPECT_EQ(houseAnimDescr(2).uRoomSoundId, 33);
    EXPECT_EQ(houseAnimDescr(44).video_name, "Bank");
    EXPECT_EQ(houseAnimDescr(44).uBuildingType, HOUSE_TYPE_BANK);
    // Town halls (houses 89-91) use the City* rows: a named proprietor in 2dEvents but no portrait.
    EXPECT_EQ(houseAnimDescr(houseTable[HouseId(89)].uAnimationID).house_npc_id, 0);
    EXPECT_EQ(houseTable[HouseId(89)].pProprieterName, "Janice");

    // MM6's 2dEvents exit columns: the pic id indexes MM6's own picture table and the map is a
    // plain mapstats id - the City Council's exit leads to the Oracle of Enroth (map 49), gated
    // on quest bit 167 (all six council quests done; see Mm6.CouncilQuestsAndTraitor). Indexing
    // MM7's 11-entry picture list with the raw map value used to run out of bounds.
    EXPECT_EQ(houseTable[HouseId(165)].uExitPicID, 5);
    EXPECT_EQ(houseTable[HouseId(165)].uExitMapID, pMapStats->GetMapInfo("oracle.blv"));
    EXPECT_EQ(houseTable[HouseId(165)]._quest_bit, static_cast<QuestBit>(167));
    pParty->_questBits[static_cast<QuestBit>(167)] = true;
    prepareHouse(HouseId(165));
    pParty->_questBits[static_cast<QuestBit>(167)] = false;
    ASSERT_FALSE(houseNpcs.empty());
    EXPECT_EQ(houseNpcs.back().type, HOUSE_TRANSITION);
    ASSERT_NE(houseNpcs.back().icon, nullptr);
    EXPECT_EQ(houseNpcs.back().icon->name(), "istairdn");
    EXPECT_EQ(houseNpcs.back().icon->size(), Sizei(57, 67));
    for (HouseNpcDesc &desc : houseNpcs)
        if (desc.icon)
            desc.icon->release();
    houseNpcs.clear();

    // Enter The Knife Shoppe (see Mm6.EnterWeaponShop for the door mechanics).
    const BLVFace *door = nullptr;
    for (const BSPModel &model : pOutdoor->pBModels) {
        for (const BLVFace &face : model.faces) {
            if (face.eventId == 17 && face.Clickable()) {
                door = &face;
                break;
            }
        }
    }
    ASSERT_NE(door, nullptr);
    Vec3f doorCenter = door->boundingBox.center();
    Vec3f pos = doorCenter + door->facePlane.normal * 130;
    pos.z = door->boundingBox.z1;
    int yawDegrees = TrigLUT.atan2(doorCenter.x - pos.x, doorCenter.y - pos.y) * 90 / 512;
    game.teleportTo(engine->_currentLoadedMapId, pos, yawDegrees);
    game.tick(1);
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_HOUSE);

    // The house dialogue panel is the shop's own marble evpan, and the proprietor is the MM6
    // blacksmith portrait from the commoner block, not a garbage id from MM7's table.
    ASSERT_NE(game_ui_dialogue_background, nullptr);
    EXPECT_EQ(game_ui_dialogue_background->name(), "evpan013");
    EXPECT_EQ(game_ui_dialogue_background->size(), Sizei(152, 353));
    ASSERT_FALSE(houseNpcs.empty());
    EXPECT_EQ(houseNpcs[0].type, HOUSE_PROPRIETOR);
    ASSERT_NE(houseNpcs[0].icon, nullptr);
    EXPECT_EQ(houseNpcs[0].icon->name(), "npc506");
    EXPECT_EQ(houseNpcs[0].icon->size(), Sizei(63, 73));

    // The exit button sits on the panel's bottom row (CreateButton stores w+1/h+1).
    ASSERT_NE(pBtn_ExitCancel, nullptr);
    EXPECT_EQ(pBtn_ExitCancel->rect, Recti(526, 313, 62, 29));

    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
    game.tick(5);
}

GAME_TEST(Mm6, BuyFromWeaponShop) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // Enter The Knife Shoppe (see Mm6.EnterWeaponShop for the door mechanics).
    const BLVFace *door = nullptr;
    for (const BSPModel &model : pOutdoor->pBModels) {
        for (const BLVFace &face : model.faces) {
            if (face.eventId == 17 && face.Clickable()) {
                door = &face;
                break;
            }
        }
    }
    ASSERT_NE(door, nullptr);
    Vec3f doorCenter = door->boundingBox.center();
    Vec3f pos = doorCenter + door->facePlane.normal * 130;
    pos.z = door->boundingBox.z1;
    int yawDegrees = TrigLUT.atan2(doorCenter.x - pos.x, doorCenter.y - pos.y) * 90 / 512;
    game.teleportTo(engine->_currentLoadedMapId, pos, yawDegrees);
    game.tick(1);
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_HOUSE);

    pParty->SetGold(2000); // Enough for any treasure-level-1 weapon.

    // Pick "Buy Standard Goods" - the first right-hand dialogue option.
    game.pressAndReleaseButton(BUTTON_LEFT, 550, 160);
    game.tick(2);

    // Entering the buy screen generated the shop's stock from MM6 item data.
    const std::array<Item, 12> &stock = pParty->standartItemsInShops[HouseId(1)];
    int slot = -1;
    for (int i = 0; i < 6; i++) {
        if (stock[i].itemId != ITEM_NULL) {
            slot = i;
            break;
        }
    }
    ASSERT_NE(slot, -1);
    ItemId stockItem = stock[slot].itemId;
    EXPECT_FALSE(pItemTable->items[stockItem].name.empty());

    // Click the item to buy it: gold is paid and it moves into the active character's inventory.
    // Click coordinates mirror GUIWindow_Shop::houseScreenClick's hit test: the slot's icon is
    // horizontally centered at 60 + slot * 70, vertically at weaponYPos[slot] + 30 plus half the icon.
    int goldBefore = pParty->GetGold();
    int x = 60 + slot * 70;
    int y = weaponYPos[slot] + 30 + shop_ui_items_in_store[slot]->height() / 2;
    game.pressAndReleaseButton(BUTTON_LEFT, x, y);
    game.tick(2);
    EXPECT_LT(pParty->GetGold(), goldBefore);
    EXPECT_EQ(stock[slot].itemId, ITEM_NULL); // The shelf slot sold out.
    EXPECT_TRUE(pParty->activeCharacter().inventory.find(stockItem));

    // Leave the buy screen, then the shop.
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
    game.tick(5);
}

// Clicks a proprietor dialogue option (a shop menu entry, a transport schedule line, a town-hall
// service, ...) in an open house dialogue. The option buttons are re-laid-out to rendered-text metrics
// on draw, so locate them by message param.
static void clickProprietorOption(EngineController &game, DialogueId option) {
    ASSERT_NE(pDialogueWindow, nullptr);
    const GUIButton *optionButton = nullptr;
    for (const GUIButton *button : pDialogueWindow->vButtons)
        if (button->msg == UIMSG_SelectProprietorDialogueOption && button->msg_param == std::to_underlying(option))
            optionButton = button;
    ASSERT_NE(optionButton, nullptr);
    game.pressAndReleaseButton(BUTTON_LEFT, optionButton->rect.x + optionButton->rect.w / 2,
                               optionButton->rect.y + optionButton->rect.h / 2);
    game.tick(2);
}

// MM6 "General Store" is its alchemist-shop analog: MM6.EXE maps the 2dEvents Type prefix "gen" to
// house type 4 (the alchemist slot), and stocks it from the EXE's own general-store tables (standard
// @0x4C459C, special @0x4C47B8, generators @0x49FB40/0x49FD40): six shelf slots, each rolling one of
// six columns - a random cloak or boots at the store's treasure level, two empty-bottle columns (163)
// and the three herbs (162/161/160), everything identified; special goods share the table at treasure
// level 1. General stores also buy ANY item at half the usual sell price (2dEvents annotates them
// '"Sell Anything"' / 'Value /2'; MM7 still carries the SHOP_SCREEN_SELL_FOR_CHEAP enum from this).
GAME_TEST(Mm6, GeneralStoreBuyAndSellAnything) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // The 2dEvents parse must land General Stores on the type-4 alchemy-shop slot.
    EXPECT_EQ(houseTable[HouseId(42)].uType, HOUSE_TYPE_ALCHEMY_SHOP);

    // Enter Traveler's Supply - the New Sorpigal general store (oute3 event 9 = SpeakInHouse(42)).
    const BLVFace *door = nullptr;
    for (const BSPModel &model : pOutdoor->pBModels) {
        for (const BLVFace &face : model.faces) {
            if (face.eventId == 9 && face.Clickable()) {
                door = &face;
                break;
            }
        }
    }
    ASSERT_NE(door, nullptr);
    Vec3f doorCenter = door->boundingBox.center();
    Vec3f pos = doorCenter + door->facePlane.normal * 130;
    pos.z = door->boundingBox.z1;
    int yawDegrees = TrigLUT.atan2(doorCenter.x - pos.x, doorCenter.y - pos.y) * 90 / 512;
    game.teleportTo(engine->_currentLoadedMapId, pos, yawDegrees);
    game.tick(1);
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_HOUSE);
    ASSERT_NE(window_SpeakInHouse, nullptr);
    EXPECT_EQ(window_SpeakInHouse->houseId(), HouseId(42));

    pParty->SetGold(2000); // Enough for any treasure-level-1 cloak or boots.

    // MM6 general stores offer just Buy / Sell (MM6.EXE option factory type-4 case @0x4987c7);
    // their special stock exists but is unreachable even in the original engine.
    EXPECT_EQ(listProprietorOptions(), (std::vector<DialogueId>{DIALOGUE_SHOP_BUY_STANDARD, DIALOGUE_SHOP_SELL}));

    // Pick "Buy Standard Goods" - this generates both stocks from the MM6 general-store model.
    clickProprietorOption(game, DIALOGUE_SHOP_BUY_STANDARD);

    auto isGeneralStoreWare = [](const Item &item) {
        if (item.itemId == ItemId(163) || item.itemId == ItemId(162) ||
            item.itemId == ItemId(161) || item.itemId == ItemId(160))
            return true; // Empty Bottle / Widoweeps Berries / Phirna Root / Poppysnaps.
        ItemType type = pItemTable->items[item.itemId].type;
        return type == ITEM_TYPE_CLOAK || type == ITEM_TYPE_BOOTS;
    };

    const std::array<Item, 12> &stock = pParty->standartItemsInShops[HouseId(42)];
    const std::array<Item, 12> &specialStock = pParty->specialItemsInShops[HouseId(42)];
    for (int i = 0; i < 6; i++) {
        ASSERT_NE(stock[i].itemId, ITEM_NULL) << i;
        EXPECT_TRUE(isGeneralStoreWare(stock[i])) << static_cast<int>(stock[i].itemId);
        ASSERT_NE(specialStock[i].itemId, ITEM_NULL) << i;
        EXPECT_TRUE(isGeneralStoreWare(specialStock[i])) << static_cast<int>(specialStock[i].itemId);
    }
    for (int i = 6; i < 12; i++) {
        EXPECT_EQ(stock[i].itemId, ITEM_NULL) << i; // MM6 general stores have 6 shelf slots, not 12.
        EXPECT_EQ(specialStock[i].itemId, ITEM_NULL) << i;
    }

    // The MM6 wares screen stands the six items ON the GENSHELF table (MM6.EXE 0x4a1040): bottom
    // edge at y = 308, centered at x = 75 * slot + 40, first/last slots clamped to the table edges.
    for (int i = 0; i < 6; i++) {
        Pointi pos = mm6GeneralStoreItemPos(i);
        EXPECT_GE(pos.x, 18) << i;
        EXPECT_LE(pos.x + shop_ui_items_in_store[i]->width(), 457) << i; // Bottle/herb/cloak/boots icons are narrow.
        EXPECT_EQ(pos.y + shop_ui_items_in_store[i]->height(), 308) << i; // ...and stand on the table surface.
    }

    // Buy shelf slot 1 with a click at its on-table position.
    Item wanted = stock[1];
    int goldBefore = pParty->GetGold();
    int buyPrice = PriceCalculator::itemBuyingPriceForPlayer(&pParty->activeCharacter(), wanted.GetValue(),
                                                             houseTable[HouseId(42)].fPriceMultiplier);
    Pointi slot1 = mm6GeneralStoreItemPos(1);
    game.pressAndReleaseButton(BUTTON_LEFT, slot1.x + shop_ui_items_in_store[1]->width() / 2,
                               slot1.y + shop_ui_items_in_store[1]->height() / 2);
    game.tick(2);
    EXPECT_EQ(pParty->GetGold(), goldBefore - buyPrice);
    EXPECT_EQ(stock[1].itemId, ITEM_NULL); // The shelf slot sold out.
    EXPECT_TRUE(pParty->activeCharacter().inventory.find(wanted.itemId));

    // Sell ANYTHING: an MM7 alchemist would refuse a sword, the MM6 general store buys it -
    // at half the usual sell price.
    InventoryEntry sword = pParty->activeCharacter().inventory.tryAdd(Item(ItemId(5))); // Lionheart Sword.
    ASSERT_TRUE(sword);
    int sellPrice = std::max(1, PriceCalculator::itemSellingPriceForPlayer(&pParty->activeCharacter(), *sword,
                                                                           houseTable[HouseId(42)].fPriceMultiplier) / 2);
    Recti swordCells = sword.geometry(); // In 32px inventory-grid cells, grid origin is at (14, 17).
    Pointi swordCenter(14 + 32 * swordCells.x + 16 * swordCells.w, 17 + 32 * swordCells.y + 16 * swordCells.h);

    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE); // Leave the buy screen.
    game.tick(2);
    clickProprietorOption(game, DIALOGUE_SHOP_SELL); // Sell is a top-level option in MM6's flat menu.

    goldBefore = pParty->GetGold();
    game.pressAndReleaseButton(BUTTON_LEFT, swordCenter.x, swordCenter.y);
    game.tick(2);
    EXPECT_EQ(pParty->GetGold(), goldBefore + sellPrice);
    EXPECT_FALSE(pParty->activeCharacter().inventory.find(ItemId(5)));

    // Leave the sell screen (straight back to the main menu), then the shop.
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
    game.tick(5);
}

// MM6.EXE 0x4A4C30 is the shop eligibility check shared by the sell, identify and repair clicks:
// quest property is refused by raw id - 430-435 (Leather Pouches, Hourglass of Time, Sacred
// Chalice, Horn of Ros) and everything from 446 up (Third Eye, keys, maps, Snergle's Axe,
// Lord Kilburn's Shield, message scrolls) - while the gems 436-445 remain sellable. The type
// gates: weapon shops take weapons (items.txt equip stat Weapon/Weapon2/Weapon1or2/Missile),
// armor shops take Armor through Boots, magic shops take anything with skill column "Misc"
// (rings, wands, scrolls, books, potions, misc armor pieces, gems), general stores take all.
GAME_TEST(Mm6, ShopSellRepairIdentifyEligibility) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // MM6 2dEvents rows: 1-14 weapon shops, 15-28 armor shops, 29-41 magic shops, 42-47
    // general stores - the EXE eligibility check dispatches on these raw id ranges.
    HouseId weapon = HouseId(1), armor = HouseId(15), magic = HouseId(29), general = HouseId(42);
    ASSERT_EQ(houseTable[weapon].uType, HOUSE_TYPE_WEAPON_SHOP);
    ASSERT_EQ(houseTable[armor].uType, HOUSE_TYPE_ARMOR_SHOP);
    ASSERT_EQ(houseTable[magic].uType, HOUSE_TYPE_MAGIC_SHOP);
    ASSERT_EQ(houseTable[general].uType, HOUSE_TYPE_ALCHEMY_SHOP);

    auto sellable = [](int id, HouseId house) { return Item(ItemId(id)).canSellRepairIdentifyAt(house); };

    // Weapon shop: weapons only - including artifacts (Mordred 400) but NOT wands.
    EXPECT_TRUE(sellable(1, weapon));    // Longsword.
    EXPECT_TRUE(sellable(400, weapon));  // Mordred - MM6 artifacts are ordinary merchandise.
    EXPECT_FALSE(sellable(135, weapon)); // Wand of Flame - equip stat "WeaponW" is not a weapon-shop ware.
    EXPECT_FALSE(sellable(89, weapon));  // Helm.

    // Armor shop: equip stats Armor..Boots.
    EXPECT_TRUE(sellable(66, armor));   // Leather Armor.
    EXPECT_TRUE(sellable(89, armor));   // Helm.
    EXPECT_TRUE(sellable(79, armor));   // Kite Shield.
    EXPECT_FALSE(sellable(1, armor));   // Longsword.

    // Magic shop: everything with skill column "Misc" - including misc armor pieces like helms.
    EXPECT_TRUE(sellable(120, magic));  // Fine Ring.
    EXPECT_TRUE(sellable(135, magic));  // Wand of Flame.
    EXPECT_TRUE(sellable(200, magic));  // Torch Light scroll.
    EXPECT_TRUE(sellable(300, magic));  // Torch Light book.
    EXPECT_TRUE(sellable(164, magic));  // Cure Wounds potion.
    EXPECT_TRUE(sellable(89, magic));   // Helm - skill "Misc", so magic shops DO take it.
    EXPECT_TRUE(sellable(436, magic));  // Diamond.
    EXPECT_FALSE(sellable(1, magic));   // Longsword - skill "Sword".

    // The quest-id gate applies at EVERY shop, general stores included, and even to quest
    // weapons/armor at their own shop type.
    for (HouseId house : {weapon, armor, magic, general}) {
        EXPECT_FALSE(sellable(433, house)) << std::to_underlying(house); // Hourglass of Time.
        EXPECT_FALSE(sellable(449, house)) << std::to_underlying(house); // Candelabra.
        EXPECT_FALSE(sellable(505, house)) << std::to_underlying(house); // The Letter.
    }
    EXPECT_FALSE(sellable(498, weapon)); // Snergle's Axe - a weapon, but quest property.
    EXPECT_FALSE(sellable(499, armor));  // Lord Kilburn's Shield - armor, but quest property.
    EXPECT_TRUE(sellable(445, general)); // Sapphire - the gems 436-445 stay sellable.
    EXPECT_TRUE(sellable(429, general)); // Hera, the last relic - fine at a general store.
}

// MM6 training halls are 2dEvents rows 79-88 and they ONLY train - the MM6.EXE option factory
// (type-30 case @0x4984a6) creates the single Train option, and no learn-skill options exist
// anywhere in MM6 houses outside the guilds. The per-hall level caps come from the EXE's word
// table @0x4C3DE2 indexed by raw house id (0xFFFF = The Sparring Ground's "no max"); the MM7
// cap table is keyed [89, 98] and would abort on MM6's ids. The training price is the SAME
// formula as MM7's (level x multiplier x class tier, merchant-discounted, floored at a third):
// MM6.EXE @0x499e27 divides the class byte by 3 and uses remainder+1, which IS the class tier.
GAME_TEST(Mm6, TrainAtTrainingHall) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // New Sorpigal Training Grounds: 2dEvents row 79, cap "Max level = 15".
    EXPECT_EQ(houseTable[HouseId(79)].uType, HOUSE_TYPE_TRAINING_GROUND);
    ASSERT_TRUE(enterHouse(HouseId(79)));
    createHouseUI(HouseId(79));

    // Train is the only option on offer - no Learn Skills (MM6 has no Armsmaster at all).
    EXPECT_EQ(listProprietorOptions(), (std::vector<DialogueId>{DIALOGUE_TRAINING_HALL_TRAIN}));

    // Train Roderick to level 2.
    Character &hero = pParty->activeCharacter();
    hero.experience = 1000; // Exactly the level-2 requirement (1000 * level * (level + 1) / 2).
    pParty->SetGold(10000);
    int price = PriceCalculator::trainingCostForPlayer(&hero, houseTable[HouseId(79)]);
    EXPECT_GT(price, 0);
    int skillPointsBefore = hero.uSkillPoints;
    Time timeBefore = pParty->GetPlayingTime();
    clickProprietorOption(game, DIALOGUE_TRAINING_HALL_TRAIN);
    EXPECT_EQ(hero.uLevel, 2);
    EXPECT_EQ(hero.uSkillPoints, skillPointsBefore + 5);
    EXPECT_EQ(hero.health, hero.GetMaxHealth());
    EXPECT_EQ(pParty->GetGold(), 10000 - price);
    EXPECT_GE(pParty->GetPlayingTime() - timeBefore, Duration::fromDays(7)); // Training takes a week.

    // The hall's level cap refuses further training: at the cap, nothing changes.
    hero.uLevel = 15;
    hero.experience = 1000000;
    int goldBefore = pParty->GetGold();
    clickProprietorOption(game, DIALOGUE_TRAINING_HALL_TRAIN);
    EXPECT_EQ(hero.uLevel, 15);
    EXPECT_EQ(pParty->GetGold(), goldBefore);

    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
    game.tick(5);
}

// MM6 street townsfolk are generated citizens: the first talk to a peasant actor generates a random
// citizen lazily (towns place far more peasants than the citizen buffer holds) - sex from the peasant's
// monster row (PeasantF* rows 121-132 / PeasantM* rows 133-144; the ddm npcId carries no reliable sex),
// name by sex from npcnames.txt, profession rolled on npcprof.txt's "Random Chance" column, portrait
// from the npc501..npc554 commoner block. Talking opens the standard hireable-NPC dialogue: it greets
// with a regional news line (npcnews.txt - the old bare-news flow folded into the dialogue) and offers
// the profession-details and hire topics; hiring pays the profession's hire price and puts the citizen
// in the party.
GAME_TEST(Mm6, StreetCitizenDialogueAndHire) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // No eager generation at level load - peasants keep their raw ddm npcId (0/1/2, all below the
    // 5000+ generated-NPC handles).
    int peasants = 0;
    for (const Actor &actor : pActors) {
        if (isPeasant(actor.monsterInfo.id, GAME_VERSION_MM6)) {
            EXPECT_LT(actor.npcId, 5000);
            peasants++;
        }
    }
    EXPECT_GT(peasants, 0);
    EXPECT_EQ(pNPCStats->uNewlNPCBufPos, 0);

    // Teleport right next to a placed peasant, facing it, and talk to it.
    auto peasant = std::ranges::find_if(pActors, [](const Actor &actor) {
        return isPeasant(actor.monsterInfo.id, GAME_VERSION_MM6) && actor.CanAct();
    });
    ASSERT_NE(peasant, pActors.end());
    Vec3f peasantPos = peasant->pos;
    Vec3f pos = peasantPos + Vec3f(-160, 0, 0);
    int yawDegrees = TrigLUT.atan2(peasantPos.x - pos.x, peasantPos.y - pos.y) * 90 / 512;
    game.teleportTo(engine->_currentLoadedMapId, pos, yawDegrees);
    game.tick(1);
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);

    // The standard NPC dialogue opened on the generated citizen, drawing MM6's street-dialogue
    // skin: the fixed evpan019 marble panel, the portrait frameless on it, and the cancel button
    // on the panel's bottom row (see Mm6.DialogueSkin).
    ASSERT_EQ(current_screen_type, SCREEN_NPC_DIALOGUE);
    ASSERT_NE(game_ui_dialogue_background, nullptr);
    EXPECT_EQ(game_ui_dialogue_background->name(), "evpan019");
    EXPECT_EQ(game_ui_dialogue_background->size(), Sizei(152, 353));
    ASSERT_NE(pBtn_ExitCancel, nullptr);
    EXPECT_EQ(pBtn_ExitCancel->rect, Recti(526, 313, 62, 29));
    ASSERT_GE(speakingNpcId, 5000);
    NPCData *citizen = getNPCData(speakingNpcId);
    EXPECT_FALSE(citizen->name.empty());
    EXPECT_TRUE(std::ranges::contains(pNPCStats->pNPCNames[citizen->sex], citizen->name));
    // Portraits come from MM6's per-sex pools over the regular npcXXX space (EXE tables
    // @0x4C13F0/0x4C15D0) - not from the npc501..554 commoner block.
    EXPECT_TRUE(std::ranges::contains(NPCStats::mm6CitizenPortraitPool(citizen->sex), static_cast<int>(citizen->portraitId)));
    EXPECT_NE(citizen->profession, NoProfession);
    EXPECT_GT(pNPCStats->pProfessions[citizen->profession].uHirePrice, 0u);
    EXPECT_TRUE(citizen->canJoin);
    EXPECT_EQ(citizen->fame, 0);
    // The reputation requirement is rolled at generation: 59% none, then +200/-300/+400/-600.
    EXPECT_TRUE(citizen->rep == 0 || citizen->rep == 200 || citizen->rep == -300 ||
                citizen->rep == 400 || citizen->rep == -600);

    // A regional news entry was assigned to the citizen at this first dialogue, and it sticks.
    const std::vector<RegionalNewsEntry> &localNews = pNPCStats->pRegionalNews[engine->_currentLoadedMapId];
    EXPECT_EQ(localNews.size(), 30u); // New Sorpigal's share of npcnews.txt.
    auto saysIt = [&](const RegionalNewsEntry &entry) { return entry.text == citizen->mm6News.text; };
    EXPECT_TRUE(std::ranges::any_of(localNews, saysIt)); // New Sorpigal HAS news, so no kingdom-wide fallback.
    std::string firstNewsLine = citizen->mm6News.text;

    // Make sure the citizen talks regardless of its rolled reputation requirement, and reopen: the
    // talk menu is MM6's trio - the profession's weekday small talk, Join, and News.
    citizen->rep = 0;
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_NPC_DIALOGUE);
    auto *dialogue = static_cast<GUIWindow_Dialogue *>(pDialogueWindow.get());
    EXPECT_EQ(dialogue->mm6StreetPage(), MM6_STREET_PAGE_TALK);
    EXPECT_EQ(citizen->mm6News.text, firstNewsLine); // Same line on every visit.
    auto findOption = [&](DialogueId topic) -> const GUIButton * {
        for (const GUIButton *button : pDialogueWindow->vButtons)
            if (button->msg == UIMSG_SelectNPCDialogueOption && button->msg_param == std::to_underlying(topic))
                return button;
        return nullptr;
    };
    ASSERT_NE(findOption(DIALOGUE_STREET_MM6_PROF_TOPIC), nullptr);
    ASSERT_NE(findOption(DIALOGUE_STREET_MM6_NEWS), nullptr);
    EXPECT_EQ(findOption(DIALOGUE_PROFESSION_DETAILS), nullptr); // The MM7-style menu is gone.

    // The profession option is labeled with the weekday's proftext.txt topic; News with the news topic.
    EXPECT_EQ(findOption(DIALOGUE_STREET_MM6_PROF_TOPIC)->sLabel,
              pNPCStats->mm6ProfText[citizen->profession][pParty->uCurrentDayOfMonth % 7].topic);
    EXPECT_EQ(findOption(DIALOGUE_STREET_MM6_NEWS)->sLabel, citizen->mm6News.topic);

    // The News option replies with the assigned news line.
    const GUIButton *newsOption = findOption(DIALOGUE_STREET_MM6_NEWS);
    game.pressAndReleaseButton(BUTTON_LEFT, newsOption->rect.x + newsOption->rect.w / 2,
                               newsOption->rect.y + newsOption->rect.h / 2);
    game.tick(2);
    EXPECT_EQ(dialogue->getDisplayedDialogueType(), DIALOGUE_STREET_MM6_NEWS);

    // Hire the citizen: MM6's one-click Join (buttons are re-laid-out on draw - locate by msg_param).
    pParty->SetGold(5000); // Some professions cost up to 2000 to hire.
    const GUIButton *hireOption = findOption(DIALOGUE_HIRE_FIRE);
    ASSERT_NE(hireOption, nullptr);
    game.pressAndReleaseButton(BUTTON_LEFT, hireOption->rect.x + hireOption->rect.w / 2,
                               hireOption->rect.y + hireOption->rect.h / 2);
    game.tick(2);

    EXPECT_TRUE(citizen->Hired());
    // Burglars are the one profession hired for free (the hire handler skips the gold check for them).
    int expectedPrice = citizen->profession == Burglar ? 0 : pNPCStats->pProfessions[citizen->profession].uHirePrice;
    EXPECT_EQ(pParty->GetGold(), 5000 - expectedPrice);
    EXPECT_TRUE(pParty->pHirelings[0].name == citizen->name || pParty->pHirelings[1].name == citizen->name);
}

GAME_TEST(Mm6, QuestNpcDialogueInTavern) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // A Lonely Knight is New Sorpigal's tavern (2dEvents row 92). Andover Potbello (npcdata row 1)
    // lives there with two scripted dialogue topics wired to global.evt: event 1 "The Letter" and
    // event 296 "Quest" - the candelabra quest that MM6 opens with.
    EXPECT_EQ(houseTable[HouseId(92)].name, "A Lonely Knight");
    NPCData *andover = &pNPCStats->pNPCData[1];
    EXPECT_EQ(andover->name, "Andover Potbello");
    EXPECT_EQ(andover->house, HouseId(92));
    EXPECT_EQ(andover->dialogue_1_evt_id, 1u);
    EXPECT_EQ(andover->dialogue_2_evt_id, 296u);
    EXPECT_EQ(pNPCTopics[1].pTopic, "The Letter");
    EXPECT_EQ(pNPCTopics[296].pTopic, "Quest");

    // The tavern door face is wired to local event 11, an ungated SpeakInHouse(92).
    const BLVFace *door = nullptr;
    for (const BSPModel &model : pOutdoor->pBModels) {
        for (const BLVFace &face : model.faces) {
            if (face.eventId == 11 && face.Clickable()) {
                door = &face;
                break;
            }
        }
    }
    ASSERT_NE(door, nullptr);
    Vec3f doorCenter = door->boundingBox.center();
    Vec3f pos = doorCenter + door->facePlane.normal * 130;
    pos.z = door->boundingBox.z1;
    int yawDegrees = TrigLUT.atan2(doorCenter.x - pos.x, doorCenter.y - pos.y) * 90 / 512;
    game.teleportTo(engine->_currentLoadedMapId, pos, yawDegrees);
    game.tick(1);
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_HOUSE);
    ASSERT_NE(window_SpeakInHouse, nullptr);
    EXPECT_EQ(window_SpeakInHouse->houseId(), HouseId(92));

    // The tavern hosts the proprietor plus the house NPCs Andover and Maria; click Andover's portrait.
    ASSERT_GE(houseNpcs.size(), 3u);
    int andoverIndex = -1;
    for (int i = 0; i < houseNpcs.size(); i++)
        if (houseNpcs[i].type == HOUSE_NPC && houseNpcs[i].npc == andover)
            andoverIndex = i;
    ASSERT_NE(andoverIndex, -1);
    ASSERT_NE(houseNpcs[andoverIndex].button, nullptr);
    Recti portrait = houseNpcs[andoverIndex].button->rect;
    game.pressAndReleaseButton(BUTTON_LEFT, portrait.x + portrait.w / 2, portrait.y + portrait.h / 2);
    game.tick(2);

    // Both scripted topics became dialogue options. Their buttons are re-laid-out to the rendered
    // text metrics on draw, so locate the "Quest" option (DIALOGUE_SCRIPTED_LINE_2) by its message
    // param instead of assuming creation-time coordinates.
    ASSERT_NE(pDialogueWindow, nullptr);
    const GUIButton *questOption = nullptr;
    for (const GUIButton *button : pDialogueWindow->vButtons) {
        if (button->msg == UIMSG_SelectHouseNPCDialogueOption) {
            EXPECT_TRUE(button->msg_param == std::to_underlying(DIALOGUE_SCRIPTED_LINE_1) ||
                        button->msg_param == std::to_underlying(DIALOGUE_SCRIPTED_LINE_2));
            if (button->msg_param == std::to_underlying(DIALOGUE_SCRIPTED_LINE_2))
                questOption = button;
        }
    }
    ASSERT_NE(questOption, nullptr);

    // Clicking the "Quest" topic runs global event 296: quest bit 126 is granted, the topic rewires
    // itself to event 297 via SetNPCTopic, and the reply text (npctext.txt row 305) is displayed.
    EXPECT_FALSE(pParty->_questBits[static_cast<QuestBit>(126)]);
    game.pressAndReleaseButton(BUTTON_LEFT, questOption->rect.x + questOption->rect.w / 2,
                               questOption->rect.y + questOption->rect.h / 2);
    game.tick(2);
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(126)]);
    EXPECT_EQ(andover->dialogue_2_evt_id, 297u);
    EXPECT_TRUE(current_npc_text.contains("Temple of Baa"));

    // Escape backs out to the portrait selection, a second escape leaves the tavern.
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
    game.tick(5);
}

// A generated citizen's 5000+ actor brand is serialized with the map's actor delta, but the
// records it points into are not: pAdditionalNPC is process-local, and uNewlNPCBufPos resets on
// every level load. A brand that survived a reload would therefore read a default-constructed
// (empty) citizen after a restart, or alias whatever citizen takes slot 0 next in the same
// session - talk to peasant P, reload, talk to peasant Q, and P and Q become the same record.
// Street citizens are per-map-session, so level load clears the stale brands and the next talk
// regenerates a fresh citizen.
GAME_TEST(Mm6, StreetCitizenBrandClearedOnReload) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    auto talkToActor = [&](int actorIndex) {
        Vec3f target = pActors[actorIndex].pos;
        Vec3f from = target + Vec3f(-160, 0, 0);
        int yawDegrees = TrigLUT.atan2(target.x - from.x, target.y - from.y) * 90 / 512;
        game.teleportTo(engine->_currentLoadedMapId, from, yawDegrees);
        game.tick(1);
        game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
        game.tick(2);
    };

    // Talk to a peasant: it gets branded with the first citizen slot.
    auto peasant = std::ranges::find_if(pActors, [](const Actor &actor) {
        return isPeasant(actor.monsterInfo.id, GAME_VERSION_MM6) && actor.CanAct();
    });
    ASSERT_NE(peasant, pActors.end());
    int peasantIndex = std::distance(pActors.begin(), peasant);
    talkToActor(peasantIndex);
    ASSERT_EQ(current_screen_type, SCREEN_NPC_DIALOGUE);
    EXPECT_EQ(pActors[peasantIndex].npcId, 5000);
    EXPECT_EQ(pNPCStats->uNewlNPCBufPos, 1);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_GAME);

    // Reload. The citizen buffer restarts empty, so the stale brand must go with it.
    Blob save = game.saveGame();
    game.loadGame(save);
    game.tick(1);
    EXPECT_EQ(pNPCStats->uNewlNPCBufPos, 0);
    for (const Actor &actor : pActors)
        EXPECT_LT(actor.npcId, 5000);

    // Talking again regenerates a fresh citizen in slot 0 instead of reusing the stale handle.
    talkToActor(peasantIndex);
    ASSERT_EQ(current_screen_type, SCREEN_NPC_DIALOGUE);
    ASSERT_GE(speakingNpcId, 5000);
    EXPECT_EQ(pActors[peasantIndex].npcId, 5000);
    EXPECT_EQ(pNPCStats->uNewlNPCBufPos, 1);
    EXPECT_FALSE(getNPCData(speakingNpcId)->name.empty());
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
}

// Walks the party up to A Lonely Knight, New Sorpigal's tavern, and enters through its door
// (local event 11, an ungated SpeakInHouse(92)). The party must already be in New Sorpigal.
static void enterLonelyKnightTavern(EngineController &game) {
    const BLVFace *door = nullptr;
    for (const BSPModel &model : pOutdoor->pBModels) {
        for (const BLVFace &face : model.faces) {
            if (face.eventId == 11 && face.Clickable()) {
                door = &face;
                break;
            }
        }
    }
    ASSERT_NE(door, nullptr);
    Vec3f doorCenter = door->boundingBox.center();
    Vec3f pos = doorCenter + door->facePlane.normal * 130;
    pos.z = door->boundingBox.z1;
    int yawDegrees = TrigLUT.atan2(doorCenter.x - pos.x, doorCenter.y - pos.y) * 90 / 512;
    game.teleportTo(engine->_currentLoadedMapId, pos, yawDegrees);
    game.tick(1);
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_HOUSE);
}

// Opens the dialogue with a house NPC by clicking their portrait.
static void clickHouseNpcPortrait(EngineController &game, const NPCData *npc) {
    int npcIndex = -1;
    for (int i = 0; i < houseNpcs.size(); i++)
        if (houseNpcs[i].type == HOUSE_NPC && houseNpcs[i].npc == npc)
            npcIndex = i;
    ASSERT_NE(npcIndex, -1);
    ASSERT_NE(houseNpcs[npcIndex].button, nullptr);
    Recti portrait = houseNpcs[npcIndex].button->rect;
    game.pressAndReleaseButton(BUTTON_LEFT, portrait.x + portrait.w / 2, portrait.y + portrait.h / 2);
    game.tick(2);
}

// Finds the dialogue-option button for a scripted topic line. Option buttons are re-laid-out to
// rendered-text metrics on draw, so they are located by their message params, never by fixed
// coordinates. Returns nullptr when the NPC doesn't offer the topic.
static const GUIButton *findScriptedTopicButton(DialogueId topicLine) {
    if (!pDialogueWindow)
        return nullptr;
    for (const GUIButton *button : pDialogueWindow->vButtons)
        if (button->msg == UIMSG_SelectHouseNPCDialogueOption && button->msg_param == std::to_underlying(topicLine))
            return button;
    return nullptr;
}

// Clicks a scripted topic in an open NPC dialogue, running its global.evt script.
static void selectScriptedTopic(EngineController &game, DialogueId topicLine) {
    const GUIButton *option = findScriptedTopicButton(topicLine);
    ASSERT_NE(option, nullptr);
    game.pressAndReleaseButton(BUTTON_LEFT, option->rect.x + option->rect.w / 2,
                               option->rect.y + option->rect.h / 2);
    game.tick(2);
}

GAME_TEST(Mm6, HouseOccupantSelectionStrip) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // MM6's occupant-selection screen places the 63x73 portrait buttons from its own slot table
    // (MM6.EXE 0x4BE7E0; buttons created at the same spots - 0x419C67 on entry, 0x4A4BBE on escape
    // back to the selection), NOT from MM7's pNPCPortraits: counts 1-3 stack one centered column
    // at x=525 with a 95px pitch, count 6 opens the two columns already on the top row.
    static constexpr std::array<std::array<Pointi, 6>, 6> expectedSlots = {{
        {{{525, 34}}},
        {{{525, 34}, {525, 129}}},
        {{{525, 34}, {525, 129}, {525, 224}}},
        {{{525, 34}, {486, 129}, {564, 129}, {525, 224}}},
        {{{525, 34}, {486, 129}, {564, 129}, {486, 224}, {564, 224}}},
        {{{486, 34}, {564, 34}, {486, 129}, {564, 129}, {486, 224}, {564, 224}}},
    }};

    // A Lonely Knight hosts three occupants: the proprietor plus Andover and Maria.
    enterLonelyKnightTavern(game);
    int count = static_cast<int>(houseNpcs.size());
    ASSERT_EQ(count, 3);
    for (int i = 0; i < count; i++) {
        ASSERT_NE(houseNpcs[i].button, nullptr) << "occupant " << i;
        EXPECT_EQ(houseNpcs[i].button->rect.topLeft(), expectedSlots[count - 1][i]) << "occupant " << i;
    }

    // Escaping out of an occupant's dialogue re-creates the strip at the same EXE slots.
    NPCData *andover = &pNPCStats->pNPCData[1];
    clickHouseNpcPortrait(game, andover);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    ASSERT_EQ(houseNpcs.size(), 3u);
    for (int i = 0; i < count; i++) {
        ASSERT_NE(houseNpcs[i].button, nullptr) << "occupant " << i;
        EXPECT_EQ(houseNpcs[i].button->rect.topLeft(), expectedSlots[count - 1][i]) << "occupant " << i;
    }

    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
    game.tick(5);
}

GAME_TEST(Mm6, CompleteLetterQuestDelivery) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // MM6's opening quest: deliver The Letter (item 505) to Andover Potbello in A Lonely Knight.
    // His "The Letter" topic runs global event 1: with the letter in the party's possession it pays
    // 1000 gold, clears quest bit 81, sets quest bit 82 and retires the topic to event 2; without
    // it, it shows a refusal and changes nothing.
    NPCData *andover = &pNPCStats->pNPCData[1];
    EXPECT_EQ(andover->name, "Andover Potbello");
    EXPECT_EQ(andover->dialogue_1_evt_id, 1u);
    EXPECT_EQ(pNPCTopics[1].pTopic, "The Letter");
    EXPECT_EQ(pItemTable->items[ItemId(505)].name, "The Letter");

    // A new MM6 party starts with quest bit 81 set and The Letter in Roderick's backpack
    // (Mm6.NewGameDefaults); take the letter away to exercise the refusal branch first.
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(81)]);
    InventoryEntry letter = pParty->pCharacters[0].inventory.find(ItemId(505));
    ASSERT_TRUE(letter);
    pParty->pCharacters[0].inventory.take(letter);

    enterLonelyKnightTavern(game);

    // Without the letter the Compare(PlayerItemInHands, 505) branch falls through to the refusal
    // reply (npctext.txt row 3) and the quest state doesn't budge.
    int goldBefore = pParty->GetGold();
    clickHouseNpcPortrait(game, andover);
    selectScriptedTopic(game, DIALOGUE_SCRIPTED_LINE_1);
    EXPECT_TRUE(current_npc_text.contains("you don't have a letter"));
    EXPECT_EQ(pParty->GetGold(), goldBefore);
    EXPECT_EQ(andover->dialogue_1_evt_id, 1u);
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(81)]);
    EXPECT_FALSE(pParty->_questBits[static_cast<QuestBit>(82)]);

    // Hand over the letter: back out to the portraits, put it in a backpack, ask again.
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    ASSERT_TRUE(pParty->pCharacters[0].inventory.add(Item(ItemId(505))));
    clickHouseNpcPortrait(game, andover);
    selectScriptedTopic(game, DIALOGUE_SCRIPTED_LINE_1);
    EXPECT_TRUE(current_npc_text.contains("The Seal"));
    EXPECT_EQ(pParty->GetGold(), goldBefore + 1000);
    EXPECT_FALSE(pParty->_questBits[static_cast<QuestBit>(81)]);
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(82)]);
    EXPECT_EQ(andover->dialogue_1_evt_id, 2u); // Topic retired to event 2 via SetNPCTopic.
    EXPECT_TRUE(pParty->pCharacters[0].inventory.find(ItemId(505))); // The script doesn't take it.

    // Asking again hits the retired topic: a "you got your gold" brush-off, no second payout.
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    clickHouseNpcPortrait(game, andover);
    selectScriptedTopic(game, DIALOGUE_SCRIPTED_LINE_1);
    EXPECT_TRUE(current_npc_text.contains("You got your gold"));
    EXPECT_EQ(pParty->GetGold(), goldBefore + 1000);

    // Escape back to the portraits, then out of the tavern.
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
    game.tick(5);
}

GAME_TEST(Mm6, CompleteCandelabraFetchQuest) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    MapId newSorpigal = engine->_currentLoadedMapId;
    Vec3f newSorpigalPos = pParty->pos;

    // The full fetch-quest loop: Andover Potbello's "Quest" topic (global event 296) asks the party
    // to recover the Candelabra (item 449) left behind in the old Temple of Baa - the Abandoned
    // Temple, d02.blv. Returning it to him (event 297) pays out and retires the whole topic.
    NPCData *andover = &pNPCStats->pNPCData[1];
    EXPECT_EQ(andover->dialogue_2_evt_id, 296u);
    EXPECT_EQ(pNPCTopics[296].pTopic, "Quest");
    EXPECT_EQ(pItemTable->items[ItemId(449)].name, "Candelabra");

    // Take the quest: quest bit 126 is granted and the topic chains to event 297.
    enterLonelyKnightTavern(game);
    clickHouseNpcPortrait(game, andover);
    selectScriptedTopic(game, DIALOGUE_SCRIPTED_LINE_2);
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(126)]);
    EXPECT_EQ(andover->dialogue_2_evt_id, 297u);
    EXPECT_TRUE(current_npc_text.contains("candelabra"));

    // Asking again empty-handed hits event 297's Compare fall-through (npctext.txt row 306).
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    clickHouseNpcPortrait(game, andover);
    selectScriptedTopic(game, DIALOGUE_SCRIPTED_LINE_2);
    EXPECT_TRUE(current_npc_text.contains("Baa is patient"));
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(126)]);

    // Leave the tavern.
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_GAME);

    // Fetch: the candelabra sits in a chest in the Abandoned Temple.
    MapId temple = pMapStats->GetMapInfo("d02.blv");
    ASSERT_NE(temple, MAP_INVALID);
    game.teleportTo(temple, Vec3f(16406, -19669, 865), 0); // The temple's entrance.
    game.tick(1);
    int chestId = -1;
    for (int i = 0; i < vChests.size(); i++)
        if (vChests[i].inventory.find(ItemId(449)))
            chestId = i;
    ASSERT_NE(chestId, -1);

    // Find a side face of that chest so the party can stand in front of it (see
    // Mm6.OpenChestInGoblinwatch), and disarm it - the trap flow isn't what's under test here.
    const BLVFace *chestFace = nullptr;
    for (const BLVFace &face : pIndoor->faces) {
        if (!face.eventId || !face.Clickable() || !engine->_localEventMap.hasEvent(face.eventId))
            continue;
        if (std::abs(face.facePlane.normal.z) >= 0.5f)
            continue;
        for (const EvtInstruction &instruction : engine->_localEventMap.function(face.eventId)) {
            if (instruction.opcode == EVENT_OpenChest && instruction.data.chest_id == chestId) {
                chestFace = &face;
                break;
            }
        }
        if (chestFace)
            break;
    }
    ASSERT_NE(chestFace, nullptr);
    vChests[chestId].flags &= ~CHEST_TRAPPED;

    Vec3f chestCenter = chestFace->boundingBox.center();
    Vec3f standPos = chestCenter + chestFace->facePlane.normal * 130;
    standPos.z = chestFace->boundingBox.z1;
    int chestYawDegrees = TrigLUT.atan2(chestCenter.x - standPos.x, chestCenter.y - standPos.y) * 90 / 512;
    game.teleportTo(temple, standPos, chestYawDegrees);
    game.tick(1);
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(1);
    ASSERT_EQ(current_screen_type, SCREEN_CHEST);

    // Spacebar grabs one chest item at a time into the active character's backpack; keep grabbing
    // until the candelabra comes out.
    auto partyHasCandelabra = [] {
        return std::ranges::any_of(pParty->pCharacters, [](const Character &character) {
            return static_cast<bool>(character.inventory.find(ItemId(449)));
        });
    };
    for (int i = 0; i < 30 && !partyHasCandelabra(); i++) {
        game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
        game.tick(1);
    }
    EXPECT_TRUE(partyHasCandelabra());
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_GAME);

    // Return to Andover and hand it over: event 297's success branch thanks the party
    // (npctext.txt row 307), takes the candelabra, and pays out - award 39 and 2000 experience
    // to everyone, 1000 gold, a 200-point reputation boost (MM6 reputation improves downwards)
    // and quest bit 126 back off. SetNPCTopic(npc 1, index 1, 0) then removes the topic for good.
    game.teleportTo(newSorpigal, newSorpigalPos, 0);
    game.tick(1);
    enterLonelyKnightTavern(game);
    int goldBefore = pParty->GetGold();
    std::vector<uint64_t> xpBefore;
    for (const Character &character : pParty->pCharacters)
        xpBefore.push_back(character.experience);
    int reputationBefore = currentLocationInfo().reputation;
    clickHouseNpcPortrait(game, andover);
    selectScriptedTopic(game, DIALOGUE_SCRIPTED_LINE_2);
    EXPECT_TRUE(current_npc_text.contains("Baa be praised"));
    EXPECT_EQ(pParty->GetGold(), goldBefore + 1000);
    for (int i = 0; i < pParty->pCharacters.size(); i++) {
        EXPECT_EQ(pParty->pCharacters[i].experience, xpBefore[i] + 2000);
        EXPECT_TRUE(pParty->pCharacters[i]._achievedAwardsBits[static_cast<AwardId>(39)]);
    }
    EXPECT_EQ(currentLocationInfo().reputation, reputationBefore - 200);
    EXPECT_FALSE(partyHasCandelabra());
    EXPECT_FALSE(pParty->_questBits[static_cast<QuestBit>(126)]);
    EXPECT_EQ(andover->dialogue_2_evt_id, 0u);

    // With the topic gone, reopening the dialogue offers no second quest line.
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    clickHouseNpcPortrait(game, andover);
    EXPECT_EQ(findScriptedTopicButton(DIALOGUE_SCRIPTED_LINE_2), nullptr);
    EXPECT_NE(findScriptedTopicButton(DIALOGUE_SCRIPTED_LINE_1), nullptr); // The Letter is still on offer.

    // Escape back to the portraits, then out of the tavern.
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
    game.tick(5);
}

GAME_TEST(Mm6, UseSpellScroll) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // Item 200 is MM6's Torch Light scroll (items.txt binds it to spell 1 via Mod1 "S1").
    // Right-clicking a character portrait with the scroll held uses it on that character:
    // the scroll is consumed and the bound spell is cast, lighting the party torch.
    ASSERT_EQ(pItemTable->items[ItemId(200)].type, ITEM_TYPE_SPELL_SCROLL);
    EXPECT_FALSE(pParty->TorchlightActive());
    for (int i = 0; i < 100 && pParty->pCharacters[0].timeToRecovery != 0_ticks; i++)
        game.tick(1); // Using a scroll requires the target character to be recovered.
    ASSERT_EQ(pParty->pCharacters[0].timeToRecovery, 0_ticks);
    pParty->setHoldingItem(Item(ItemId(200)));
    game.pressAndReleaseButton(BUTTON_RIGHT, 50, 420); // Character 1's portrait.
    game.tick(5);
    EXPECT_EQ(pParty->pPickedItem.itemId, ITEM_NULL); // Scroll consumed.
    EXPECT_TRUE(pParty->TorchlightActive());
}

GAME_TEST(Mm6, LearnSpellFromBook) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // Item 300 is MM6's Torch Light spell book (Mod1 "S1"). Learning it requires the Fire skill;
    // the book is used on the character whose portrait is right-clicked while it's held.
    ASSERT_EQ(pItemTable->items[ItemId(300)].type, ITEM_TYPE_BOOK);
    Character &learner = pParty->pCharacters[0];
    learner.setSkillValue(SKILL_FIRE, CombinedSkillValue::novice());
    EXPECT_FALSE(learner.bHaveSpell[SPELL_FIRE_TORCH_LIGHT]);
    pParty->setHoldingItem(Item(ItemId(300)));
    game.pressAndReleaseButton(BUTTON_RIGHT, 50, 420);
    game.tick(5);
    EXPECT_TRUE(learner.bHaveSpell[SPELL_FIRE_TORCH_LIGHT]);
}

GAME_TEST(Mm6, ReadMessageScroll) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // Item 505 is "The Letter" from the Andover Potbello quest, an Mscroll whose text lives in
    // scroll.txt under the same item id. Right-clicking a portrait with it held opens the scroll
    // reading window with that text.
    ASSERT_EQ(pItemTable->items[ItemId(505)].type, ITEM_TYPE_MESSAGE_SCROLL);
    pParty->setHoldingItem(Item(ItemId(505)));
    // Like all right-click popups, the scroll shows only while the right button is held.
    game.pressButton(BUTTON_RIGHT, 50, 420);
    game.tick(2);
    ASSERT_NE(pGUIWindow_ScrollWindow, nullptr);
    EXPECT_EQ(pGUIWindow_ScrollWindow->scroll_type, ItemId(505));
    EXPECT_TRUE(pMessageScrolls[ItemId(505)].starts_with("My Dear Sulman"));

    game.releaseButton(BUTTON_RIGHT, 50, 420);
    game.tick(2);
    EXPECT_EQ(pGUIWindow_ScrollWindow, nullptr);
}

GAME_TEST(Mm6, ArtifactIdsAndTreasureRoll) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // MM6 artifacts & relics are items 400-429 - below the MM7-shaped [500, 528] window that
    // pIsArtifactFound used to span. A quest script granting one via SetVariable(ItemInHands)
    // must mark it as found without going out of bounds.
    ASSERT_EQ(pItemTable->items[ItemId(401)].rarity, RARITY_ARTIFACT); // Thor.
    pParty->pCharacters[0].SetVariable(VAR_PlayerItemInHands, 401);
    EXPECT_EQ(pParty->pPickedItem.itemId, ItemId(401));
    EXPECT_TRUE(pParty->pIsArtifactFound[ItemId(401)]);
    pParty->takeHoldingItem();

    // MM6 quest items sit inside MM7's artifact id range and must NOT be counted as artifacts.
    pParty->pCharacters[0].SetVariable(VAR_PlayerItemInHands, 505); // The Letter.
    EXPECT_FALSE(pParty->pIsArtifactFound[ItemId(505)]);
    pParty->takeHoldingItem();

    // The treasure-level-6 artifact roll must produce MM6 artifacts/relics - rolling MM7's id
    // range instead would spawn MM6 message scrolls and quest items as random loot.
    int artifactsRolled = 0;
    for (int i = 0; i < 400; i++) {
        Item item;
        pItemTable->generateItem(ITEM_TREASURE_LEVEL_6, RANDOM_ITEM_ANY, &item);
        ItemRarity rarity = pItemTable->items[item.itemId].rarity;
        if (rarity == RARITY_ARTIFACT || rarity == RARITY_RELIC) {
            EXPECT_GE(std::to_underlying(item.itemId), 400);
            EXPECT_LE(std::to_underlying(item.itemId), 429);
            artifactsRolled++;
        }
        EXPECT_NE(pItemTable->items[item.itemId].type, ITEM_TYPE_MESSAGE_SCROLL);
        EXPECT_NE(item.itemId, ItemId(505));
    }
    EXPECT_GT(artifactsRolled, 0); // ~5% of 400 rolls, capped by the artifact limit.
}

GAME_TEST(Mm6, DrinkPotions) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // MM6 potions are items 164-188, bound to content ids P1-P25 in items.txt. Unlike MM7 they
    // have no potion power: effects are fixed, temporary boosts go into the until-rest bonus
    // fields (statBonuses/sACModifier/sRes*Bonus/sLevelModifier), buff potions last 6 hours, and
    // black potions are permanent (essences once per stat per character).
    ASSERT_EQ(pItemTable->items[ItemId(164)].type, ITEM_TYPE_POTION);
    ASSERT_EQ(pItemTable->items[ItemId(164)].potionId, 1); // Cure Wounds.
    ASSERT_EQ(pItemTable->items[ItemId(188)].potionId, 25); // Rejuvenation.

    // Generated MM6 potions must not get MM7-style potion power.
    Item generated(ItemId(165));
    generated.postGenerate(ITEM_SOURCE_CHEST);
    EXPECT_EQ(generated.potionPower, 0);
    for (int i = 0; i < 20; i++) {
        Item rolled;
        pItemTable->generateItem(ITEM_TREASURE_LEVEL_3, RANDOM_ITEM_POTION, &rolled);
        ASSERT_TRUE(rolled.isPotion());
        EXPECT_EQ(rolled.potionPower, 0);
    }

    Character &knight = pParty->pCharacters[0]; // Character 1, portrait at (50,420).
    int casterIndex = -1; // Someone with spell points for the mana potions.
    for (int i = 0; i < pParty->pCharacters.size(); i++) {
        if (pParty->pCharacters[i].GetMaxMana() >= 10) {
            casterIndex = i;
            break;
        }
    }
    ASSERT_NE(casterIndex, -1);
    Character &caster = pParty->pCharacters[casterIndex];

    auto drink = [&](int itemId, int targetCharacter) {
        pParty->setHoldingItem(Item(ItemId(itemId)));
        pParty->pCharacters[0].useItem(targetCharacter, true);
        game.tick(1);
        // Drinking changes the held potion into an empty Potion Bottle (useitems.txt:
        // "Change Item to 163").
        EXPECT_EQ(pParty->pPickedItem.itemId, ItemId(163));
        pParty->takeHoldingItem();
    };

    // P1 Cure Wounds heals 10, via the real right-click-portrait path.
    for (int i = 0; i < 100 && knight.timeToRecovery != 0_ticks; i++)
        game.tick(1);
    ASSERT_GT(knight.health, 15);
    knight.health -= 15;
    int hpBefore = knight.health;
    pParty->setHoldingItem(Item(ItemId(164)));
    game.pressAndReleaseButton(BUTTON_RIGHT, 50, 420);
    game.tick(3);
    EXPECT_EQ(pParty->pPickedItem.itemId, ItemId(163)); // Left an empty bottle in hand.
    pParty->takeHoldingItem();
    EXPECT_EQ(knight.health, hpBefore + 10);

    // P2 Magic restores 10 spell points.
    caster.mana = 0;
    drink(165, casterIndex);
    EXPECT_EQ(caster.mana, std::min(10, caster.GetMaxMana()));

    // P3 Energy adds 10 to all seven stats until rest; P9 Extreme Energy stacks another 20.
    for (Attribute stat : knight._statBonuses.indices())
        ASSERT_EQ(knight._statBonuses[stat], 0);
    drink(166, 0);
    for (Attribute stat : knight._statBonuses.indices())
        EXPECT_EQ(knight._statBonuses[stat], 10);
    drink(172, 0);
    for (Attribute stat : knight._statBonuses.indices())
        EXPECT_EQ(knight._statBonuses[stat], 30);

    // P4 Protection adds 10 AC until rest; P7 Supreme Protection another 20.
    ASSERT_EQ(knight.sACModifier, 0);
    drink(167, 0);
    EXPECT_EQ(knight.sACModifier, 10);
    drink(170, 0);
    EXPECT_EQ(knight.sACModifier, 30);

    // P5 Resistance adds 10 to the five MM6 resistances until rest; P10 Super Resistance
    // another 20. MM6 Elec/Cold/Poison map to Air/Water/Earth, and the single MM6 "Magic"
    // resistance fans out to Mind/Spirit/Body, mirroring the monsters.txt column mapping.
    ASSERT_EQ(knight.sResFireBonus, 0);
    drink(168, 0);
    EXPECT_EQ(knight.sResFireBonus, 10);
    EXPECT_EQ(knight.sResAirBonus, 10);
    EXPECT_EQ(knight.sResWaterBonus, 10);
    EXPECT_EQ(knight.sResEarthBonus, 10);
    EXPECT_EQ(knight.sResMindBonus, 10);
    EXPECT_EQ(knight.sResSpiritBonus, 10);
    EXPECT_EQ(knight.sResBodyBonus, 10);
    EXPECT_EQ(knight.sResPhysicalBonus, 0); // Not a character resistance in MM6.
    EXPECT_EQ(knight.sResLightBonus, 0);
    EXPECT_EQ(knight.sResDarkBonus, 0);
    drink(173, 0);
    EXPECT_EQ(knight.sResFireBonus, 30);
    EXPECT_EQ(knight.sResBodyBonus, 30);

    // P6 Cure Poison clears all poison stages.
    knight.conditions.set(CONDITION_POISON_MEDIUM, pParty->GetPlayingTime());
    drink(169, 0);
    EXPECT_FALSE(knight.conditions.has(CONDITION_POISON_WEAK));
    EXPECT_FALSE(knight.conditions.has(CONDITION_POISON_MEDIUM));
    EXPECT_FALSE(knight.conditions.has(CONDITION_POISON_SEVERE));

    // P8 Restoration cures everything except dead, stone and eradicated.
    knight.conditions.set(CONDITION_WEAK, pParty->GetPlayingTime());
    knight.conditions.set(CONDITION_DISEASE_SEVERE, pParty->GetPlayingTime());
    knight.conditions.set(CONDITION_PARALYZED, pParty->GetPlayingTime());
    knight.conditions.set(CONDITION_DEAD, pParty->GetPlayingTime());
    drink(171, 0);
    EXPECT_FALSE(knight.conditions.has(CONDITION_WEAK));
    EXPECT_FALSE(knight.conditions.has(CONDITION_DISEASE_SEVERE));
    EXPECT_FALSE(knight.conditions.has(CONDITION_PARALYZED));
    EXPECT_TRUE(knight.conditions.has(CONDITION_DEAD));
    knight.conditions.reset(CONDITION_DEAD);

    // P11 Heroism, P12 Haste, P13 Stone Skin, P14 Bless: the spell effect for 6 hours.
    Time drinkStart = pParty->GetPlayingTime();
    drink(174, 0);
    drink(175, 0);
    drink(176, 0);
    drink(177, 0);
    Time drinkEnd = pParty->GetPlayingTime();
    for (CharacterBuff buff : {CHARACTER_BUFF_HEROISM, CHARACTER_BUFF_HASTE,
                               CHARACTER_BUFF_STONESKIN, CHARACTER_BUFF_BLESS}) {
        EXPECT_TRUE(knight.pCharacterBuffs[buff].Active());
        EXPECT_GE(knight.pCharacterBuffs[buff].expireTime, drinkStart + Duration::fromHours(6));
        EXPECT_LE(knight.pCharacterBuffs[buff].expireTime, drinkEnd + Duration::fromHours(6));
    }

    // P15 Divine Power adds 20 levels until rest and a year of magical age.
    ASSERT_EQ(knight.sLevelModifier, 0);
    int ageModifier = knight.sAgeModifier;
    drink(178, 0);
    EXPECT_EQ(knight.sLevelModifier, 20);
    EXPECT_EQ(knight.sAgeModifier, ageModifier + 1);

    // P16 Divine Cure restores 100 hit points and ages a year.
    knight.health = 1;
    drink(179, 0);
    EXPECT_EQ(knight.health, std::min(101, knight.GetMaxHealth()));
    EXPECT_EQ(knight.sAgeModifier, ageModifier + 2);

    // P17 Divine Magic restores 100 spell points and ages a year.
    caster.mana = 0;
    int casterAgeModifier = caster.sAgeModifier;
    drink(180, casterIndex);
    EXPECT_EQ(caster.mana, std::min(100, caster.GetMaxMana()));
    EXPECT_EQ(caster.sAgeModifier, casterAgeModifier + 1);

    // P18 Essence of Might: +15 might / -5 intellect, permanent, once per character.
    int mightBefore = knight._stats[ATTRIBUTE_MIGHT];
    int intellectBefore = knight._stats[ATTRIBUTE_INTELLIGENCE];
    drink(181, 0);
    EXPECT_EQ(knight._stats[ATTRIBUTE_MIGHT], mightBefore + 15);
    EXPECT_EQ(knight._stats[ATTRIBUTE_INTELLIGENCE], intellectBefore - 5);
    EXPECT_TRUE(knight._pureStatPotionUsed[ATTRIBUTE_MIGHT]);
    drink(181, 0); // A second one is drunk but has no further effect.
    EXPECT_EQ(knight._stats[ATTRIBUTE_MIGHT], mightBefore + 15);
    EXPECT_EQ(knight._stats[ATTRIBUTE_INTELLIGENCE], intellectBefore - 5);

    // P21 Essence of Endurance: +15 endurance / -1 everything else.
    IndexedArray<int, ATTRIBUTE_FIRST_STAT, ATTRIBUTE_LAST_STAT> statsBefore = knight._stats;
    drink(184, 0);
    for (Attribute stat : knight._stats.indices())
        EXPECT_EQ(knight._stats[stat], statsBefore[stat] + (stat == ATTRIBUTE_ENDURANCE ? 15 : -1));

    // P24 Essence of Luck: +15 luck / -5 accuracy.
    int luckBefore = knight._stats[ATTRIBUTE_LUCK];
    int accuracyBefore = knight._stats[ATTRIBUTE_ACCURACY];
    drink(187, 0);
    EXPECT_EQ(knight._stats[ATTRIBUTE_LUCK], luckBefore + 15);
    EXPECT_EQ(knight._stats[ATTRIBUTE_ACCURACY], accuracyBefore - 5);

    // P25 Rejuvenation wipes magical aging at the price of 1 point of every stat, permanently.
    ASSERT_GT(knight.sAgeModifier, 0);
    statsBefore = knight._stats;
    drink(188, 0);
    EXPECT_EQ(knight.sAgeModifier, 0);
    for (Attribute stat : knight._stats.indices())
        EXPECT_EQ(knight._stats[stat], statsBefore[stat] - 1);
}

GAME_TEST(Mm6, EatHerbs) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // MM6 herbs are edible reagents (useitems.txt): Poppysnaps (160) set weak poison, Phirna
    // Root (161) restores 2 spell points, Widoweeps Berries (162) heal 2 hit points, and the
    // herb is consumed. MM7's useItem carries a broken remnant of exactly this mechanic.
    ASSERT_EQ(pItemTable->items[ItemId(160)].type, ITEM_TYPE_REAGENT);
    ASSERT_EQ(pItemTable->items[ItemId(161)].type, ITEM_TYPE_REAGENT);
    ASSERT_EQ(pItemTable->items[ItemId(162)].type, ITEM_TYPE_REAGENT);

    Character &knight = pParty->pCharacters[0]; // Character 1, portrait at (50,420).
    for (int i = 0; i < 100 && knight.timeToRecovery != 0_ticks; i++)
        game.tick(1);

    // Widoweeps Berries heal 2, via the real right-click-portrait path.
    ASSERT_GT(knight.health, 5);
    knight.health -= 5;
    int hpBefore = knight.health;
    pParty->setHoldingItem(Item(ItemId(162)));
    game.pressAndReleaseButton(BUTTON_RIGHT, 50, 420);
    game.tick(3);
    EXPECT_EQ(pParty->pPickedItem.itemId, ITEM_NULL); // Eaten.
    EXPECT_EQ(knight.health, hpBefore + 2);

    // Phirna Root restores 2 spell points - eaten by a character who has any.
    int casterIndex = -1;
    for (int i = 0; i < pParty->pCharacters.size(); i++) {
        if (pParty->pCharacters[i].GetMaxMana() >= 2) {
            casterIndex = i;
            break;
        }
    }
    ASSERT_NE(casterIndex, -1);
    Character &caster = pParty->pCharacters[casterIndex];
    caster.mana = 0;
    pParty->setHoldingItem(Item(ItemId(161)));
    pParty->pCharacters[0].useItem(casterIndex, true);
    game.tick(1);
    EXPECT_EQ(pParty->pPickedItem.itemId, ITEM_NULL);
    EXPECT_EQ(caster.mana, 2);

    // Poppysnaps poison the eater.
    EXPECT_FALSE(knight.conditions.has(CONDITION_POISON_WEAK));
    pParty->setHoldingItem(Item(ItemId(160)));
    pParty->pCharacters[0].useItem(0, true);
    game.tick(1);
    EXPECT_EQ(pParty->pPickedItem.itemId, ITEM_NULL);
    EXPECT_TRUE(knight.conditions.has(CONDITION_POISON_WEAK));
}

GAME_TEST(Mm6, MixPotions) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // MM6 potion mixing is recipe-less: the full combination matrix ships in useitems.txt.
    // mm6PotionCombination returns ITEM_NULL when nothing happens, ItemId(1..4) for the E1..E4
    // explosion tiers (the same encoding MM7's potion.txt parse uses - MM7 inherited MM6's
    // explosion code verbatim), or the resulting item id.
    auto mix = [](int held, int target) {
        return std::to_underlying(mm6PotionCombination(ItemId(held), ItemId(target)));
    };

    // The matrix is symmetric.
    for (int a = 160; a <= 188; a++)
        for (int b = 160; b <= 188; b++)
            EXPECT_EQ(mix(a, b), mix(b, a)) << "a=" << a << " b=" << b;

    // Herbs fill an empty Potion Bottle (163) and combine with nothing else.
    EXPECT_EQ(mix(160, 163), 166); // Poppysnaps -> Energy.
    EXPECT_EQ(mix(161, 163), 165); // Phirna Root -> Magic.
    EXPECT_EQ(mix(162, 163), 164); // Widoweeps Berries -> Cure Wounds.
    EXPECT_EQ(mix(160, 161), 0);
    EXPECT_EQ(mix(160, 164), 0);
    EXPECT_EQ(mix(160, 178), 0);

    // The 22 potion recipes of the useitems.txt matrix.
    EXPECT_EQ(mix(164, 165), 169); // Cure Wounds + Magic = Cure Poison.
    EXPECT_EQ(mix(164, 166), 167); // Cure Wounds + Energy = Protection.
    EXPECT_EQ(mix(164, 167), 174); // Cure Wounds + Protection = Heroism.
    EXPECT_EQ(mix(164, 174), 181); // Cure Wounds + Heroism = Pure Might.
    EXPECT_EQ(mix(164, 175), 186); // Cure Wounds + Haste = Pure Speed.
    EXPECT_EQ(mix(165, 166), 168); // Magic + Energy = Resistance.
    EXPECT_EQ(mix(165, 167), 176); // Magic + Protection = Stone Skin.
    EXPECT_EQ(mix(165, 168), 173); // Magic + Resistance = Super Resistance.
    EXPECT_EQ(mix(165, 169), 177); // Magic + Cure Poison = Bless.
    EXPECT_EQ(mix(165, 171), 183); // Magic + Restoration = Pure Personality.
    EXPECT_EQ(mix(165, 176), 182); // Magic + Stone Skin = Pure Intellect.
    EXPECT_EQ(mix(166, 167), 172); // Energy + Protection = Extreme Energy.
    EXPECT_EQ(mix(166, 168), 175); // Energy + Resistance = Haste.
    EXPECT_EQ(mix(166, 170), 184); // Energy + Supreme Protection = Pure Endurance.
    EXPECT_EQ(mix(166, 177), 185); // Energy + Bless = Pure Accuracy.
    EXPECT_EQ(mix(167, 168), 170); // Protection + Resistance = Supreme Protection.
    EXPECT_EQ(mix(167, 171), 179); // Protection + Restoration = Divine Cure.
    EXPECT_EQ(mix(168, 169), 171); // Resistance + Cure Poison = Restoration.
    EXPECT_EQ(mix(168, 172), 188); // Resistance + Extreme Energy = Rejuvenation.
    EXPECT_EQ(mix(168, 173), 180); // Resistance + Super Resistance = Divine Magic.
    EXPECT_EQ(mix(169, 172), 178); // Cure Poison + Extreme Energy = Divine Power.
    EXPECT_EQ(mix(169, 173), 187); // Cure Poison + Super Resistance = Pure Luck.

    // Same potion, white+white, black+black and bottle+potion: nothing happens.
    EXPECT_EQ(mix(164, 164), 0);
    EXPECT_EQ(mix(170, 171), 0);
    EXPECT_EQ(mix(174, 175), 0);
    EXPECT_EQ(mix(178, 179), 0);
    EXPECT_EQ(mix(163, 163), 0);
    EXPECT_EQ(mix(163, 164), 0);
    EXPECT_EQ(mix(163, 188), 0);

    // Every other pair explodes, tier by category: colored+colored=E1, colored+white=E2,
    // colored+black=E3, white+black=E4.
    EXPECT_EQ(mix(164, 168), 1);
    EXPECT_EQ(mix(164, 169), 1);
    EXPECT_EQ(mix(166, 169), 1);
    EXPECT_EQ(mix(164, 170), 2);
    EXPECT_EQ(mix(169, 176), 2);
    EXPECT_EQ(mix(164, 178), 3);
    EXPECT_EQ(mix(169, 188), 3);
    EXPECT_EQ(mix(170, 178), 4);
    EXPECT_EQ(mix(177, 188), 4);

    // End-to-end through the inventory right-click path: Magic onto Cure Wounds makes Cure
    // Poison and returns a spare empty bottle to the inventory.
    game.pressAndReleaseKey(PlatformKey::KEY_I);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_CHARACTERS);
    Character &active = pParty->activeCharacter();
    InventoryEntry red = active.inventory.add(Item(ItemId(164)));
    ASSERT_TRUE(red);
    Pointi gridPos = red.geometry().topLeft();
    auto countBottles = [&] {
        int count = 0;
        for (InventoryEntry e : active.inventory.entries(ItemId(163)))
            count++;
        return count;
    };
    int bottlesBefore = countBottles();
    pParty->setHoldingItem(Item(ItemId(165)));
    // Right-click actions fire while the button is held (popup mode), so press, tick, release.
    game.pressButton(BUTTON_RIGHT, 14 + 32 * gridPos.x + 16, 17 + 32 * gridPos.y + 16);
    game.tick(2);
    game.releaseButton(BUTTON_RIGHT, 14 + 32 * gridPos.x + 16, 17 + 32 * gridPos.y + 16);
    game.tick(1);
    EXPECT_EQ(pParty->pPickedItem.itemId, ITEM_NULL);
    InventoryEntry mixed = active.inventory.entry(gridPos);
    ASSERT_TRUE(mixed);
    EXPECT_EQ(mixed->itemId, ItemId(169)); // Cure Poison.
    EXPECT_EQ(countBottles(), bottlesBefore + 1);

    // And the explosion path: Cure Wounds onto Resistance is E1 - 10-20 fire damage, both
    // potions destroyed, no bottle back.
    InventoryEntry green = active.inventory.add(Item(ItemId(168)));
    ASSERT_TRUE(green);
    Pointi greenPos = green.geometry().topLeft();
    int bottlesBeforeExplosion = countBottles();
    active.health = active.GetMaxHealth();
    int hpBefore = active.health;
    pParty->setHoldingItem(Item(ItemId(164)));
    game.pressButton(BUTTON_RIGHT, 14 + 32 * greenPos.x + 16, 17 + 32 * greenPos.y + 16);
    game.tick(2);
    game.releaseButton(BUTTON_RIGHT, 14 + 32 * greenPos.x + 16, 17 + 32 * greenPos.y + 16);
    game.tick(1);
    EXPECT_EQ(pParty->pPickedItem.itemId, ITEM_NULL);
    EXPECT_FALSE(active.inventory.entry(greenPos));
    EXPECT_LT(active.health, hpBefore);
    EXPECT_GE(active.health, hpBefore - 20);
    EXPECT_EQ(countBottles(), bottlesBeforeExplosion);
}

GAME_TEST(Mm6, ArtifactPowers) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // The MM6 default party starts with gear equipped (Mm6.NewGameDefaults); strip it so the
    // item bonus checks below see only the artifact under test.
    for (Character &character : pParty->pCharacters)
        for (ItemSlot slot : allItemSlots())
            if (InventoryEntry equipped = character.inventory.entry(slot))
                character.inventory.take(equipped);

    Character &knight = pParty->pCharacters[0];
    int casterIndex = -1; // Someone whose class has spell points, for the +N spell point powers.
    for (int i = 0; i < pParty->pCharacters.size(); i++) {
        if (pParty->pCharacters[i].GetMaxMana() >= 10) {
            casterIndex = i;
            break;
        }
    }
    ASSERT_NE(casterIndex, -1);
    Character &caster = pParty->pCharacters[casterIndex];

    // Equips an MM6 artifact into its natural slot (freeing the slot first), runs the checks,
    // and removes it again. The artifact bonus lookup iterates all equipped items, so freeing
    // the slot only matters for the equip() free-slot precondition, not for the bonuses.
    auto withEquipped = [&](Character &character, ItemSlot slot, int itemId, auto &&checks) {
        if (InventoryEntry existing = character.inventory.entry(slot))
            character.inventory.take(existing);
        InventoryEntry entry = character.inventory.equip(slot, Item(ItemId(itemId)));
        checks();
        character.inventory.take(entry);
    };

    // Excalibur: +30 Might.
    ASSERT_EQ(pItemTable->items[ItemId(403)].name, "Excalibur");
    ASSERT_EQ(pItemTable->items[ItemId(403)].rarity, RARITY_ARTIFACT);
    int mightBefore = knight.GetActualMight();
    withEquipped(knight, ITEM_SLOT_MAIN_HAND, 403, [&] {
        EXPECT_EQ(knight.GetActualMight(), mightBefore + 30);
    });
    EXPECT_EQ(knight.GetActualMight(), mightBefore); // Gone once unequipped.

    // Mordred is Vampiric only - no stat entries must have crept in.
    withEquipped(knight, ITEM_SLOT_MAIN_HAND, 400, [&] {
        EXPECT_EQ(knight.GetActualMight(), mightBefore);
    });

    // Arthur: 'of the Gods' (+10 to all seven stats) and +25 spell points.
    IndexedArray<int, ATTRIBUTE_FIRST_STAT, ATTRIBUTE_LAST_STAT> statsBefore;
    for (Attribute stat : statsBefore.indices())
        statsBefore[stat] = caster.GetActualStat(stat);
    int manaBefore = caster.GetMaxMana();
    withEquipped(caster, ITEM_SLOT_HELMET, 409, [&] {
        for (Attribute stat : statsBefore.indices())
            EXPECT_EQ(caster.GetActualStat(stat), statsBefore[stat] + 10);
        // +10 to the mana stats bumps their step-function parameter bonus too, so the max
        // spell points delta is at least the flat +25 the crown grants.
        EXPECT_GE(caster.GetMaxMana(), manaBefore + 25);
        EXPECT_EQ(caster.GetItemsBonus(ATTRIBUTE_MANA), 25);
    });

    // Galahad: 'of Protection' (+10 to the five MM6 resistances) and +25 hit points. MM6
    // Elec/Cold/Poison map onto Air/Water/Earth and "Magic" fans out to Mind/Spirit/Body,
    // like the potion and monsters.txt resistance mappings.
    const std::array<Attribute, 7> allResistances = {
        ATTRIBUTE_RESIST_FIRE, ATTRIBUTE_RESIST_AIR, ATTRIBUTE_RESIST_WATER,
        ATTRIBUTE_RESIST_EARTH, ATTRIBUTE_RESIST_MIND, ATTRIBUTE_RESIST_SPIRIT,
        ATTRIBUTE_RESIST_BODY};
    std::array<int, 7> resistancesBefore;
    for (size_t i = 0; i < allResistances.size(); i++)
        resistancesBefore[i] = knight.GetActualResistance(allResistances[i]);
    int healthBefore = knight.GetMaxHealth();
    withEquipped(knight, ITEM_SLOT_ARMOUR, 406, [&] {
        for (size_t i = 0; i < allResistances.size(); i++)
            EXPECT_EQ(knight.GetActualResistance(allResistances[i]), resistancesBefore[i] + 10);
        EXPECT_EQ(knight.GetMaxHealth(), healthBefore + 25);
    });

    // Odin: +50 to resistances at -40 Speed - relics carry downsides.
    ASSERT_EQ(pItemTable->items[ItemId(424)].rarity, RARITY_RELIC);
    int speedBefore = knight.GetActualSpeed();
    withEquipped(knight, ITEM_SLOT_HELMET, 424, [&] {
        for (size_t i = 0; i < allResistances.size(); i++)
            EXPECT_EQ(knight.GetActualResistance(allResistances[i]), resistancesBefore[i] + 50);
        EXPECT_EQ(knight.GetActualSpeed(), speedBefore - 40);
    });

    // Poseidon: +20 Might/Endurance/Accuracy, -10 AC and Speed. Armor class is checked via
    // the item bonus - actual AC also moves with the speed parameter bonus.
    int enduranceBefore = knight.GetActualEndurance();
    int accuracyBefore = knight.GetActualAccuracy();
    withEquipped(knight, ITEM_SLOT_MAIN_HAND, 417, [&] {
        EXPECT_EQ(knight.GetActualMight(), mightBefore + 20);
        EXPECT_EQ(knight.GetActualEndurance(), enduranceBefore + 20);
        EXPECT_EQ(knight.GetActualAccuracy(), accuracyBefore + 20);
        EXPECT_EQ(knight.GetItemsBonus(ATTRIBUTE_AC_BONUS), -10);
        EXPECT_EQ(knight.GetActualSpeed(), speedBefore - 10);
    });

    // Hera: +50 hit points, spell points and Luck for -50 Personality.
    int casterHealthBefore = caster.GetMaxHealth();
    int luckBefore = caster.GetActualLuck();
    int personalityBefore = caster.GetActualPersonality();
    withEquipped(caster, ITEM_SLOT_AMULET, 429, [&] {
        EXPECT_EQ(caster.GetMaxHealth(), casterHealthBefore + 50);
        EXPECT_EQ(caster.GetItemsBonus(ATTRIBUTE_MANA), 50);
        EXPECT_EQ(caster.GetActualLuck(), luckBefore + 50);
        EXPECT_EQ(caster.GetActualPersonality(), personalityBefore - 50);
    });

    // Guinevere: +30 spell points, 'of Light Magic' and 'of Dark Magic' - the school boosts
    // add half the character's skill level to the effective skill, like MM7's Ruler's Ring.
    caster.pActiveSkills[SKILL_LIGHT] = CombinedSkillValue(10, MASTERY_EXPERT);
    caster.pActiveSkills[SKILL_DARK] = CombinedSkillValue(7, MASTERY_EXPERT);
    withEquipped(caster, ITEM_SLOT_RING1, 412, [&] {
        EXPECT_EQ(caster.GetItemsBonus(ATTRIBUTE_MANA), 30);
        EXPECT_EQ(caster.GetItemsBonus(ATTRIBUTE_SKILL_LIGHT), 5);
        EXPECT_EQ(caster.GetItemsBonus(ATTRIBUTE_SKILL_DARK), 3);
        EXPECT_EQ(caster.GetItemsBonus(ATTRIBUTE_SKILL_FIRE), 0); // Only the two bound schools.
    });
    caster.pActiveSkills[SKILL_LIGHT] = CombinedSkillValue();
    caster.pActiveSkills[SKILL_DARK] = CombinedSkillValue();
}

GAME_TEST(Mm6, ArtifactBehavioralPowers) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    Character &knight = pParty->pCharacters[0];

    auto withEquipped = [&](Character &character, ItemSlot slot, int itemId, auto &&checks) {
        if (InventoryEntry existing = character.inventory.entry(slot))
            character.inventory.take(existing);
        InventoryEntry entry = character.inventory.equip(slot, Item(ItemId(itemId)));
        checks();
        character.inventory.take(entry);
    };

    // Mordred (400) is Vampiric: hits drain the target's life instead of adding elemental
    // damage. Hades (415) drips acid: +20 poison damage (MM6 Poison maps onto Earth, like
    // the monsters.txt resistance columns). Ares (416) burns: +30 fire damage. Artemis (420)
    // fires charged bolts: +20 electricity damage (MM6 Elec maps onto Air).
    {
        DamageType damageType = DAMAGE_PHYSICAL;
        bool drainsHp = false;

        Item mordred(ItemId(400));
        EXPECT_EQ(mordred._439DF3_get_additional_damage(&damageType, &drainsHp), 0);
        EXPECT_TRUE(drainsHp);
        EXPECT_EQ(damageType, DAMAGE_DARK);

        Item hades(ItemId(415));
        EXPECT_EQ(hades._439DF3_get_additional_damage(&damageType, &drainsHp), 20);
        EXPECT_FALSE(drainsHp);
        EXPECT_EQ(damageType, DAMAGE_EARTH);

        Item ares(ItemId(416));
        EXPECT_EQ(ares._439DF3_get_additional_damage(&damageType, &drainsHp), 30);
        EXPECT_FALSE(drainsHp);
        EXPECT_EQ(damageType, DAMAGE_FIRE);

        Item artemis(ItemId(420));
        EXPECT_EQ(artemis._439DF3_get_additional_damage(&damageType, &drainsHp), 20);
        EXPECT_FALSE(drainsHp);
        EXPECT_EQ(damageType, DAMAGE_AIR);
    }

    // The monster supertypes behind the slaying powers follow MM6's monsters.txt rows, not
    // MM7's id ranges: demons ARE the devils (DemonFly 25-27, Demon 28-30, zDemonqueen 172),
    // dragons are DragonCave 31-33 / DragonLand 37-39 / DragonCover 40-42 - while DragonFly
    // 34-36 is an insect - and Ghost/Lich/Skeleton are the undead.
    EXPECT_EQ(supertypeForMonsterId(MonsterId(25), GAME_VERSION_MM6), MONSTER_SUPERTYPE_KREEGAN);
    EXPECT_EQ(supertypeForMonsterId(MonsterId(28), GAME_VERSION_MM6), MONSTER_SUPERTYPE_KREEGAN);
    EXPECT_EQ(supertypeForMonsterId(MonsterId(172), GAME_VERSION_MM6), MONSTER_SUPERTYPE_KREEGAN);
    EXPECT_EQ(supertypeForMonsterId(MonsterId(31), GAME_VERSION_MM6), MONSTER_SUPERTYPE_DRAGON);
    EXPECT_EQ(supertypeForMonsterId(MonsterId(37), GAME_VERSION_MM6), MONSTER_SUPERTYPE_DRAGON);
    EXPECT_EQ(supertypeForMonsterId(MonsterId(42), GAME_VERSION_MM6), MONSTER_SUPERTYPE_DRAGON);
    EXPECT_EQ(supertypeForMonsterId(MonsterId(34), GAME_VERSION_MM6), MONSTER_SUPERTYPE_NONE);
    EXPECT_EQ(supertypeForMonsterId(MonsterId(73), GAME_VERSION_MM6), MONSTER_SUPERTYPE_UNDEAD);
    EXPECT_EQ(supertypeForMonsterId(MonsterId(94), GAME_VERSION_MM6), MONSTER_SUPERTYPE_UNDEAD);
    EXPECT_EQ(supertypeForMonsterId(MonsterId(154), GAME_VERSION_MM6), MONSTER_SUPERTYPE_UNDEAD);
    EXPECT_EQ(supertypeForMonsterId(MonsterId(166), GAME_VERSION_MM6), MONSTER_SUPERTYPE_TITAN);
    EXPECT_EQ(supertypeForMonsterId(MonsterId(58), GAME_VERSION_MM6), MONSTER_SUPERTYPE_WATER_ELEMENTAL);
    EXPECT_EQ(supertypeForMonsterId(MonsterId(76), GAME_VERSION_MM6), MONSTER_SUPERTYPE_NONE);

    // Conan (402) slays devils and dragons: double damage. Conan is 3d7+10, so a single
    // roll is 13-31 and a doubled one 26-62; with 16 rolls at least one lands above the
    // single-roll maximum unless the power is missing.
    {
        Item conan(ItemId(402));
        ASSERT_EQ(pItemTable->items[ItemId(402)].name, "Conan");
        const ItemData &conanData = pItemTable->items[ItemId(402)];
        int minSingle = conanData.damageMod + conanData.damageDice;
        int maxSingle = conanData.damageMod + conanData.damageDice * conanData.damageRoll;
        bool exceededSingleMax = false;
        for (int i = 0; i < 16; i++) {
            int vsDragon = knight.CalculateMeleeDmgToEnemyWithWeapon(&conan, MonsterId(37), false);
            EXPECT_GE(vsDragon, 2 * minSingle);
            EXPECT_LE(vsDragon, 2 * maxSingle);
            exceededSingleMax = exceededSingleMax || vsDragon > maxSingle;
            int vsDevil = knight.CalculateMeleeDmgToEnemyWithWeapon(&conan, MonsterId(28), false);
            EXPECT_GE(vsDevil, 2 * minSingle);
            int vsGoblin = knight.CalculateMeleeDmgToEnemyWithWeapon(&conan, MonsterId(76), false);
            EXPECT_GE(vsGoblin, minSingle);
            EXPECT_LE(vsGoblin, maxSingle);
        }
        EXPECT_TRUE(exceededSingleMax);
    }

    // Swiftness: Merlin (404) and Percival (405) attack 20 ticks faster than a plain staff
    // (61) and bow (42).
    {
        Duration plainStaffRecovery, merlinRecovery;
        withEquipped(knight, ITEM_SLOT_MAIN_HAND, 61, [&] {
            plainStaffRecovery = knight.GetAttackRecoveryTime(false);
        });
        withEquipped(knight, ITEM_SLOT_MAIN_HAND, 404, [&] {
            merlinRecovery = knight.GetAttackRecoveryTime(false);
        });
        EXPECT_EQ(plainStaffRecovery - merlinRecovery, 20_ticks);

        Duration plainBowRecovery, percivalRecovery;
        withEquipped(knight, ITEM_SLOT_BOW, 42, [&] {
            plainBowRecovery = knight.GetAttackRecoveryTime(true);
        });
        withEquipped(knight, ITEM_SLOT_BOW, 405, [&] {
            percivalRecovery = knight.GetAttackRecoveryTime(true);
        });
        EXPECT_EQ(plainBowRecovery - percivalRecovery, 20_ticks);
    }

    // Force: a blow from Thor (401) knocks enemies back, via the same special-item bonus
    // MM7's 'of Force' enchantment feeds into the knockback code.
    EXPECT_EQ(knight.GetSpecialItemBonus(ITEM_ENCHANTMENT_OF_FORCE), 0);
    withEquipped(knight, ITEM_SLOT_MAIN_HAND, 401, [&] {
        EXPECT_EQ(knight.GetSpecialItemBonus(ITEM_ENCHANTMENT_OF_FORCE), 5);
    });

    // Hit Recovery: Pellinore (407) speeds up recovery like MM7's 'of Recovery'.
    EXPECT_EQ(knight.GetSpecialItemBonus(ITEM_ENCHANTMENT_OF_RECOVERY), 0);
    withEquipped(knight, ITEM_SLOT_ARMOUR, 407, [&] {
        EXPECT_EQ(knight.GetSpecialItemBonus(ITEM_ENCHANTMENT_OF_RECOVERY), 50);
    });

    // Shielding: Valeria (408) and Aegis (423) halve incoming missile damage.
    EXPECT_FALSE(knight.wearsShieldingItem());
    withEquipped(knight, ITEM_SLOT_OFF_HAND, 408, [&] {
        EXPECT_TRUE(knight.wearsShieldingItem());
    });
    withEquipped(knight, ITEM_SLOT_OFF_HAND, 423, [&] {
        EXPECT_TRUE(knight.wearsShieldingItem());
    });

    // Carnage: Percival's arrows explode in a fireball on impact; Artemis (420), the other
    // bow, doesn't carry the power.
    EXPECT_TRUE(Item(ItemId(405)).grantsCarnage());
    EXPECT_FALSE(Item(ItemId(420)).grantsCarnage());

    // Thievery: Pendragon (410) and Hades (415) each DOUBLE the raw disarm skill level
    // (MM6.EXE 0x4853E0; the doublings stack), and the MM6 mastery multiplier is 2/3/4 -
    // even a novice gets x2.
    CombinedSkillValue disarmSkillBefore = knight.pActiveSkills[SKILL_TRAP_DISARM];
    knight.pActiveSkills[SKILL_TRAP_DISARM] = CombinedSkillValue(4, MASTERY_NOVICE);
    int plainDisarm = knight.GetDisarmTrap();
    EXPECT_EQ(plainDisarm, 8);
    withEquipped(knight, ITEM_SLOT_CLOAK, 410, [&] {
        EXPECT_EQ(knight.GetDisarmTrap(), 16);
    });
    withEquipped(knight, ITEM_SLOT_MAIN_HAND, 415, [&] {
        EXPECT_EQ(knight.GetDisarmTrap(), 16);
    });
    knight.pActiveSkills[SKILL_TRAP_DISARM] = disarmSkillBefore;

    // Immunity: Pendragon blocks all poison severities, Aegis blocks Flesh to Stone.
    knight.SetCondition(CONDITION_POISON_WEAK, 1);
    EXPECT_TRUE(knight.conditions.has(CONDITION_POISON_WEAK)); // Unprotected, the poison sticks.
    knight.conditions.reset(CONDITION_POISON_WEAK);
    withEquipped(knight, ITEM_SLOT_CLOAK, 410, [&] {
        for (Condition poison : {CONDITION_POISON_WEAK, CONDITION_POISON_MEDIUM, CONDITION_POISON_SEVERE}) {
            knight.SetCondition(poison, 1);
            EXPECT_FALSE(knight.conditions.has(poison));
        }
    });
    withEquipped(knight, ITEM_SLOT_OFF_HAND, 423, [&] {
        knight.SetCondition(CONDITION_PETRIFIED, 1);
        EXPECT_FALSE(knight.conditions.has(CONDITION_PETRIFIED));
    });

    // Hit Point Regeneration: Pellinore heals on the 5-minute regen tick; Hades draws its
    // power from its wielder - Negative Regeneration drains on the same tick.
    withEquipped(knight, ITEM_SLOT_ARMOUR, 407, [&] {
        knight.health = 1;
        pParty->last_regenerated = pParty->GetPlayingTime();
        pParty->playing_time += Duration::fromMinutes(11); // Two 5-minute regen ticks.
        RegeneratePartyHealthMana();
        EXPECT_EQ(knight.health, 3);
    });
    withEquipped(knight, ITEM_SLOT_MAIN_HAND, 415, [&] {
        int healthBefore = knight.health = knight.GetMaxHealth();
        pParty->last_regenerated = pParty->GetPlayingTime();
        pParty->playing_time += Duration::fromMinutes(11);
        RegeneratePartyHealthMana();
        EXPECT_EQ(knight.health, healthBefore - 2);
    });
    knight.health = knight.GetMaxHealth();
}

GAME_TEST(Mm6, MonsterModel) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // MM6 monsters.txt attack types: Elec/Cold/Pois map onto MM7's Air/Water/Earth like the
    // resistance columns do, Magic is the non-elemental DAMAGE_MAGIC, and Ener is a real energy
    // attack (the MM7 parser's Ener->Earth first-letter collision is an MM7-only preserved bug).
    EXPECT_EQ(pMonsterStats->infos[MonsterId(12)].attack1Type, DAMAGE_AIR);    // BeholderC, "Elec".
    EXPECT_EQ(pMonsterStats->infos[MonsterId(10)].attack1Type, DAMAGE_WATER);  // BeholderA, "Cold".
    EXPECT_EQ(pMonsterStats->infos[MonsterId(37)].attack1Type, DAMAGE_EARTH);  // DragonLandA, "Pois".
    EXPECT_EQ(pMonsterStats->infos[MonsterId(73)].attack1Type, DAMAGE_MAGIC);  // GhostA, "Magic".
    EXPECT_EQ(pMonsterStats->infos[MonsterId(36)].attack1Type, DAMAGE_ENERGY); // DragonFlyC, "Ener".

    // Ranged attackers keep their elemental bolt projectiles; Ghosts strike in melee (their
    // "Magic" is the attack TYPE - GhostA's missile column is empty). The full projectile bank,
    // including the MM6-only Magic/Rock missiles, is covered by Mm6.MonsterProjectiles.
    EXPECT_EQ(pMonsterStats->infos[MonsterId(12)].attack1MissileType, MONSTER_PROJECTILE_AIR_BOLT);
    EXPECT_EQ(pMonsterStats->infos[MonsterId(10)].attack1MissileType, MONSTER_PROJECTILE_WATER_BOLT);
    EXPECT_EQ(pMonsterStats->infos[MonsterId(37)].attack1MissileType, MONSTER_PROJECTILE_EARTH_BOLT);
    EXPECT_EQ(pMonsterStats->infos[MonsterId(36)].attack1MissileType, MONSTER_PROJECTILE_ENERGY_BOLT);
    EXPECT_EQ(pMonsterStats->infos[MonsterId(73)].attack1MissileType, MONSTER_PROJECTILE_NONE);

    // MM6 "Magic" damage is checked against Magic resistance, which this engine represents as
    // the Mind/Spirit/Body fan-out. A highly resistant target sees its damage halved at least
    // once in a handful of rolls; without the mapping the damage always lands in full (which is
    // MM7's correct Souldrinker behavior).
    Character &knight = pParty->pCharacters[0];
    int16_t mindResBefore = knight.sResMindBase;
    knight.sResMindBase = 500;
    bool characterResisted = false;
    for (int i = 0; i < 64 && !characterResisted; i++)
        characterResisted = knight.CalculateIncommingDamage(DAMAGE_MAGIC, 1000) < 1000;
    EXPECT_TRUE(characterResisted);
    knight.sResMindBase = mindResBefore;

    Actor resistantActor;
    resistantActor.monsterInfo.resMind = 100;
    bool actorResisted = false;
    for (int i = 0; i < 64 && !actorResisted; i++)
        actorResisted = resistantActor.CalcMagicalDamageToActor(DAMAGE_MAGIC, 1000) < 1000;
    EXPECT_TRUE(actorResisted);

    // MM6 peasants are the PeasantF*/PeasantM* rows 121-144. The MM7 id ranges would also
    // swallow everything from Oozes up to zReactor - killing a Titan must not read as a
    // peasant murder.
    EXPECT_TRUE(isPeasant(MonsterId(121), GAME_VERSION_MM6));  // PeasantF1A.
    EXPECT_TRUE(isPeasant(MonsterId(135), GAME_VERSION_MM6));  // PeasantM1C.
    EXPECT_TRUE(isPeasant(MonsterId(144), GAME_VERSION_MM6));  // PeasantM4C.
    EXPECT_FALSE(isPeasant(MonsterId(118), GAME_VERSION_MM6)); // Ogre.
    EXPECT_FALSE(isPeasant(MonsterId(147), GAME_VERSION_MM6)); // Giant Rat.
    EXPECT_FALSE(isPeasant(MonsterId(154), GAME_VERSION_MM6)); // Skeleton.
    EXPECT_FALSE(isPeasant(MonsterId(166), GAME_VERSION_MM6)); // Titan.
    EXPECT_FALSE(isPeasant(MonsterId(173), GAME_VERSION_MM6)); // zReactor.

    // Actor::IsPeasant goes through the hostility group and must agree.
    Actor titan;
    titan.monsterInfo.id = MonsterId(166);
    titan.hostilityGroup = monsterTypeForMonsterId(titan.monsterInfo.id);
    EXPECT_FALSE(titan.IsPeasant());

    Actor peasant;
    peasant.monsterInfo.id = MonsterId(123);
    peasant.hostilityGroup = monsterTypeForMonsterId(peasant.monsterInfo.id);
    EXPECT_TRUE(peasant.IsPeasant());
}

// Advances party time forward to the given 0-based month index (Might & Magic months are
// exactly 28 days), then runs a frame so the per-frame time update recomputes uCurrentMonth.
static void advanceToMonth(EngineController &game, int monthIndex) {
    int currentMonthIndex = pParty->GetPlayingTime().toCivilTime().month - 1;
    pParty->GetPlayingTime() += Duration::fromDays(28 * ((monthIndex - currentMonthIndex + 12) % 12));
    game.tick(1);
    EXPECT_EQ(pParty->uCurrentMonth, monthIndex);
}

GAME_TEST(Mm6, SnowFromMapEvents) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // MM6 snow is event-driven: outc1 (Frozen Highlands) has OnMapReload event 211 doing
    // SetSnow(0, 1), so it snows there permanently - even in summer. The MM7 every-third-
    // winter-day hack must not overwrite the event-set value in an MM6 session.
    advanceToMonth(game, 5); // June.
    MapId frozenHighlands = pMapStats->GetMapInfo("outc1.odm");
    ASSERT_NE(frozenHighlands, MAP_INVALID);
    game.teleportTo(frozenHighlands, Vec3f(0, 0, 0), 0);
    game.tick(5);
    EXPECT_TRUE(pWeather->bRenderSnow);

    // New Sorpigal has no SetSnow event - no snow, not even in deep winter.
    MapId newSorpigal = pMapStats->GetMapInfo("oute3.odm");
    ASSERT_NE(newSorpigal, MAP_INVALID);
    game.teleportTo(newSorpigal, Vec3f(-9728, -11319, 160), 0);
    game.tick(5);
    EXPECT_FALSE(pWeather->bRenderSnow);
    advanceToMonth(game, 0); // January.
    game.tick(5);
    EXPECT_FALSE(pWeather->bRenderSnow);
}

GAME_TEST(Mm6, TerrainUnchangedBySeasons) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    MapId newSorpigal = pMapStats->GetMapInfo("oute3.odm");
    MapId goblinwatch = pMapStats->GetMapInfo("d01.blv");
    ASSERT_NE(newSorpigal, MAP_INVALID);
    ASSERT_NE(goblinwatch, MAP_INVALID);

    // Reloads New Sorpigal (bouncing through Goblinwatch) so OutdoorLocation::Initialize
    // re-runs with the current month, then counts terrain tilesets.
    auto tilesetCountsAfterReload = [&](int monthIndex) {
        advanceToMonth(game, monthIndex);
        game.teleportTo(goblinwatch, Vec3f(-1850, 4304, -512), 0);
        game.teleportTo(newSorpigal, Vec3f(-9728, -11319, 160), 0);
        game.tick(1);
        std::map<Tileset, int> counts;
        for (int y = 0; y < 128; y++)
            for (int x = 0; x < 128; x++)
                counts[pTileTable->tile(pOutdoor->pTerrain.tileIdByGrid(Pointi(x, y))).tileset]++;
        return counts;
    };

    // MM6.EXE has no seasonal rendering at all: the Month global's only consumers are date
    // strings, the calendar UI, the circus schedule and bounty regen, and the game ships no
    // seasonal tile or sprite art (permanently snowy maps have snow tiles and snow-tree
    // decorations painted into the map data itself). OpenEnroth's seasons_change terrain swap
    // is a fan MM7 enhancement and must not run in an MM6 session even though it defaults to
    // on - New Sorpigal looks the same in January as in June.
    // map::operator[] inserts, which would break the whole-map equality checks below.
    auto countOf = [](const std::map<Tileset, int> &counts, Tileset tileset) {
        auto pos = counts.find(tileset);
        return pos == counts.end() ? 0 : pos->second;
    };

    std::map<Tileset, int> summer = tilesetCountsAfterReload(5); // June.
    EXPECT_GT(countOf(summer, TILESET_GRASS), 0);
    EXPECT_EQ(countOf(summer, TILESET_SNOW), 0);

    std::map<Tileset, int> autumn = tilesetCountsAfterReload(9); // October.
    EXPECT_EQ(autumn, summer);

    std::map<Tileset, int> winter = tilesetCountsAfterReload(0); // January.
    EXPECT_EQ(winter, summer);
}

GAME_TEST(Mm6, CobbleRoads) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame(); // Starts in New Sorpigal.
    game.tick(1);

    // Every MM6 outdoor map puts tileset 22 in the road slot of its tileset table, and in MM6's
    // dtile.bin that's drsr* - the cobblestone-on-dirt road tiles. New Sorpigal's oute3.odm has
    // 116 road-band cells in its tile map, and roads are exempt from transition recalculation,
    // so all 116 must come out as road tiles.
    int roadCells = 0;
    for (int y = 0; y < 127; y++)
        for (int x = 0; x < 127; x++)
            if (pOutdoor->pTerrain.tileDataByGrid(Pointi(x, y)).tileset == TILESET_COBBLE_ROAD)
                roadCells++;
    EXPECT_EQ(roadCells, 116);

    // And the road tile group resolves to the drsr* tiles, not to MM7's grass-cobble roads.
    int crossingId = pTileTable->tileId(TILESET_COBBLE_ROAD, TILE_VARIANT_ROAD_N_S_E_W);
    ASSERT_NE(crossingId, 0);
    EXPECT_EQ(pTileTable->tile(crossingId).textureName, "drsrcros");
}

GAME_TEST(Mm6, TerrainTilesets) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame(); // Starts in New Sorpigal.
    game.tick(1);

    // MM6's dtile.bin slots 3/8/9 hold real terrain art (volcanic / tropical sand / city stone),
    // unlike MM7 where they are dirt/sand filler, so they must parse into their own tilesets.
    EXPECT_EQ(pTileTable->tile(pTileTable->tileId(TILESET_COOLED_LAVA, TILE_VARIANT_BASE1)).textureName, "voltyl");
    EXPECT_EQ(pTileTable->tile(pTileTable->tileId(TILESET_TROPICAL, TILE_VARIANT_BASE1)).textureName, "troptyl");
    EXPECT_EQ(pTileTable->tile(pTileTable->tileId(TILESET_CITY, TILE_VARIANT_BASE1)).textureName, "cstyl");

    // And their transition tiles must land on the transition variants, not shadow other tilesets.
    EXPECT_EQ(pTileTable->tile(pTileTable->tileId(TILESET_COOLED_LAVA, TILE_VARIANT_TRANSITION_N)).textureName, "voldrtn");
    EXPECT_EQ(pTileTable->tile(pTileTable->tileId(TILESET_TROPICAL, TILE_VARIANT_TRANSITION_S_W)).textureName, "tropsw");

    // Water shore tiles must keep their wtrdr* names - the hwtrdr* rename is MM7-only hardware-renderer
    // art that doesn't exist in MM6's bitmaps.lod. With the rename they all loaded as the 64x64 error
    // texture, and the first one to claim a renderer texture unit pushed all real 128px terrain tiles
    // out of the unit the terrain shader samples, turning ALL outdoor terrain into water/error art.
    EXPECT_EQ(pTileTable->tile(pTileTable->tileId(TILESET_WATER, TILE_VARIANT_TRANSITION_N)).textureName, "wtrdrn");

    auto countTileset = [](Tileset tileset) {
        int result = 0;
        for (int y = 0; y < 127; y++)
            for (int x = 0; x < 127; x++)
                if (pOutdoor->pTerrain.tileDataByGrid(Pointi(x, y)).tileset == tileset)
                    result++;
        return result;
    };

    // New Sorpigal's oute3.odm has tileset 3 in terrain group 2 with 528 tile map cells in the
    // group's band - the volcanic patches around the mountains.
    EXPECT_EQ(countTileset(TILESET_COOLED_LAVA), 528);

    // Misty Islands' outd2.odm has tileset 8 in terrain group 2 with 441 cells - tropical beaches.
    MapId mistyIslands = pMapStats->GetMapInfo("outd2.odm");
    ASSERT_NE(mistyIslands, MAP_INVALID);
    game.teleportTo(mistyIslands, Vec3f(-18688, 19200, 96), 0);
    game.tick(1);
    EXPECT_EQ(countTileset(TILESET_TROPICAL), 441);
}

GAME_TEST(Mm6, SaveLoadRoundtrip) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // Party-state mutations that must survive the .mm6 save.
    pParty->SetGold(1234);
    pParty->_questBits[static_cast<QuestBit>(100)] = true;
    // Artifact-found flags roundtrip through the version-keyed save window - MM6's artifacts are
    // ids 400-429, with the 30th flag (429) overflowing MM7's 29-slot array into field_7d7.
    pParty->pIsArtifactFound[static_cast<ItemId>(400)] = true;
    pParty->pIsArtifactFound[static_cast<ItemId>(429)] = true;

    // Map-delta mutation on New Sorpigal: the first placed peasant of oute3.ddm dies.
    auto isFirstPeasant = [](const Actor &actor) {
        return std::to_underlying(actor.monsterId) == 123 && actor.initialPosition == Vec3f(-10296, -7528, 160);
    };
    auto peasant = std::ranges::find_if(pActors, isFirstPeasant);
    ASSERT_NE(peasant, pActors.end());
    peasant->aiState = Dead;
    peasant->hp = 0;

    // Leave through a REAL transition - the Abandoned Temple of Baa door (event 102) - because
    // that's the path that autosaves and thereby serializes oute3's delta into the save's
    // map-delta set. The teleportTo() test shortcut skips the autosave on purpose.
    const BLVFace *door = nullptr;
    for (const BSPModel &model : pOutdoor->pBModels) {
        for (const BLVFace &face : model.faces) {
            if (face.eventId == 102 && face.Clickable()) {
                door = &face;
                break;
            }
        }
    }
    ASSERT_NE(door, nullptr);
    Vec3f doorCenter = door->boundingBox.center();
    Vec3f doorPos = doorCenter + door->facePlane.normal * 130;
    doorPos.z = door->boundingBox.z1;
    int yawDegrees = TrigLUT.atan2(doorCenter.x - doorPos.x, doorCenter.y - doorPos.y) * 90 / 512;
    game.teleportTo(engine->_currentLoadedMapId, doorPos, yawDegrees); // Same-map teleport, no transition.
    game.tick(1);
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(1);
    game.pressAndReleaseKey(PlatformKey::KEY_Y);
    game.tick(5);
    ASSERT_EQ(pMapStats->pInfos[engine->_currentLoadedMapId].fileName, "d02.blv");

    // Indoor map-delta mutation: chest 0 has been opened.
    ASSERT_FALSE(vChests.empty());
    EXPECT_FALSE(vChests[0].flags & CHEST_OPENED);
    vChests[0].flags |= CHEST_OPENED;

    Vec3f savedPos = pParty->pos;

    // Save, then load the save back.
    Blob save = game.saveGame();
    game.loadGame(save);
    game.tick(1);

    // We're back in the Abandoned Temple with the party state intact.
    EXPECT_EQ(pMapStats->pInfos[engine->_currentLoadedMapId].fileName, "d02.blv");
    EXPECT_EQ(uCurrentlyLoadedLevelType, LEVEL_INDOOR);
    EXPECT_NEAR(pParty->pos.x, savedPos.x, 1);
    EXPECT_NEAR(pParty->pos.y, savedPos.y, 1);
    EXPECT_NEAR(pParty->pos.z, savedPos.z, 1);
    EXPECT_EQ(pParty->GetGold(), 1234);
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(100)]);
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(81)]); // New-game quest bits are still there.
    EXPECT_TRUE(pParty->hasItem(static_cast<ItemId>(505))); // Roderick still carries The Letter.
    EXPECT_TRUE(vChests[0].flags & CHEST_OPENED); // The current map's delta came from the save.
    EXPECT_TRUE(pParty->pIsArtifactFound[static_cast<ItemId>(400)]); // Mordred, first save slot.
    EXPECT_TRUE(pParty->pIsArtifactFound[static_cast<ItemId>(429)]); // Hera, the field_7d7 overflow slot.
    EXPECT_FALSE(pParty->pIsArtifactFound[static_cast<ItemId>(410)]); // Unfound artifacts stay unfound.

    // Returning to New Sorpigal reloads its delta from the save rather than respawning the map:
    // the peasant is still dead.
    MapId newSorpigal = pMapStats->GetMapInfo("oute3.odm");
    ASSERT_NE(newSorpigal, MAP_INVALID);
    game.teleportTo(newSorpigal, Vec3f(-9728, -11319, 160), 0);
    game.tick(1);
    auto deadPeasant = std::ranges::find_if(pActors, isFirstPeasant);
    ASSERT_NE(deadPeasant, pActors.end());
    EXPECT_EQ(deadPeasant->aiState, Dead);
    EXPECT_EQ(deadPeasant->hp, 0);
}

GAME_TEST(Mm6, SaveMenuListsMm6Saves) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    // The Save menu used to probe hard-coded "save{:03}.mm7" file names instead of the
    // version-keyed saveFileExtension(), so an MM6 session's own saves never showed up:
    // every slot read "Empty Save" right after saving, inviting a silent overwrite.
    ufs->remove("saves");

    game.startNewGame();
    game.tick(1);

    // Save into slot 0 through the real Save menu, naming the save "0". Two slot clicks:
    // the first selects the slot, the second starts the name text input.
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    game.pressGuiButton("GameMenu_SaveGame");
    game.tick(10);
    game.pressGuiButton("SaveMenu_Slot0");
    game.tick(2);
    game.pressGuiButton("SaveMenu_Slot0");
    game.tick(2);
    game.pressAndReleaseKey(PlatformKey::KEY_DIGIT_0);
    game.tick(2);
    game.pressGuiButton("SaveMenu_Save");
    game.tick(10);
    EXPECT_TRUE(ufs->exists("saves/save000.mm6")); // The save itself is version-keyed...

    // ...and reopening the Save menu must list it, not "Empty Save".
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    game.pressGuiButton("GameMenu_SaveGame");
    game.tick(10);
    EXPECT_TRUE(pSavegameList->pSavegameUsedSlots[0]);
    EXPECT_EQ(pSavegameList->pSavegameHeader[0].name, "0");

    // Close the menu so the test ends back on the game screen.
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
}

GAME_TEST(Mm6, EndgameWinAndLose) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // The Hive's event 60 ends the game: step 11 is EnterHouse(600) = Win (party destroyed the
    // reactor with the Ritual of the Void in hand), step 5 is EnterHouse(601) = Lose (without
    // it, the blast consumes the world). Both show the endgame certificate screen.
    MapId hive = pMapStats->GetMapInfo("hive.blv");
    ASSERT_NE(hive, MAP_INVALID);
    game.teleportTo(hive, Vec3f(0, 0, 0), 0);
    ASSERT_FALSE(pIndoor->pSpawnPoints.empty());
    game.teleportTo(hive, pIndoor->pSpawnPoints[0].position, 0);
    game.tick(1);

    // Win: the certificate window opens and the game is NOT over - MM6 lets you play on.
    eventProcessor(60, Pid(), 1, 11);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAMEOVER_WINDOW);
    EXPECT_EQ(uGameState, GAME_STATE_FINAL_WINDOW);
    ASSERT_TRUE(pGameOverWindow);
    game.pressAndReleaseButton(BUTTON_LEFT, 320, 240); // First click shows the credits popup...
    game.tick(2);
    game.pressAndReleaseButton(BUTTON_LEFT, 320, 240); // ...second click closes the window.
    game.tick(2);
    EXPECT_FALSE(pGameOverWindow);
    EXPECT_EQ(uGameState, GAME_STATE_PLAYING);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
    EXPECT_EQ(engine->_currentLoadedMapId, hive); // Still in the Hive, game continues.

    // Lose: certificate again, but closing it ends the session - back to the main menu.
    eventProcessor(60, Pid(), 1, 5);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAMEOVER_WINDOW);
    ASSERT_TRUE(pGameOverWindow);
    game.pressAndReleaseButton(BUTTON_LEFT, 320, 240);
    game.tick(2);
    game.pressAndReleaseButton(BUTTON_LEFT, 320, 240);
    for (int i = 0; i < 50 && GetCurrentMenuID() != MENU_MAIN; i++)
        game.tick(1);
    EXPECT_EQ(GetCurrentMenuID(), MENU_MAIN);
}

GAME_TEST(Mm6, Mm7MapPinsDoNotMisfire) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // MM6 map ids collide with MM7's MapId enum, so MM7's hardcoded per-map special cases must
    // not fire in an MM6 session. New Sorpigal's map id is MM7's MAP_SHOALS - walking off the
    // east map edge must not offer MM7 foot travel to Avlee. (MM6 border travel is a separate,
    // not yet implemented model.)
    EXPECT_EQ(pOutdoor->getTravelDestination(100000, 0), MAP_INVALID);

    // Gharik's Forge has MM7's MAP_BREEDING_ZONE map id - its monsters must not have their
    // exp and loot zeroed by MM7's spawning-grounds rule.
    MapId gharik = pMapStats->GetMapInfo("d18.blv");
    ASSERT_NE(gharik, MAP_INVALID);
    game.teleportTo(gharik, Vec3f(0, 0, 0), 0);
    ASSERT_FALSE(pIndoor->pSpawnPoints.empty());
    game.teleportTo(gharik, pIndoor->pSpawnPoints[0].position, 0);
    game.tick(1);
    ASSERT_FALSE(pActors.empty());
    EXPECT_TRUE(std::ranges::any_of(pActors, [](const Actor &actor) { return actor.monsterInfo.exp > 0; }));
}

GAME_TEST(Mm6, DeathRespawnsInNewSorpigal) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // Die somewhere far from home - in Goblinwatch.
    MapId goblinwatch = pMapStats->GetMapInfo("d01.blv");
    ASSERT_NE(goblinwatch, MAP_INVALID);
    game.teleportTo(goblinwatch, Vec3f(0, 0, 0), 0);
    ASSERT_FALSE(pIndoor->pSpawnPoints.empty());
    game.teleportTo(goblinwatch, pIndoor->pSpawnPoints[0].position, 0);
    game.tick(1);
    for (Character &character : pParty->pCharacters)
        character.conditions.set(CONDITION_DEAD, pParty->GetPlayingTime());
    game.tick(10);

    // MM6 death respawn: back at the New Sorpigal new-game start pose, not MM7's
    // Harmondale/Emerald Isle logic (whose map ids are ordinary MM6 maps).
    EXPECT_EQ(uGameState, GAME_STATE_PLAYING);
    EXPECT_EQ(pParty->uNumDeaths, 1);
    EXPECT_EQ(pMapStats->pInfos[engine->_currentLoadedMapId].fileName, "oute3.odm");
    EXPECT_EQ(pParty->pos.x, -9728);
    EXPECT_EQ(pParty->pos.y, -11319);

    // The death grants MM6's own counted award row, not MM7's AWARD_DEATHS (85), which is the
    // "Squire Arena Victories" row in MM6's awards.txt.
    for (const Character &character : pParty->pCharacters) {
        EXPECT_TRUE(character._achievedAwardsBits[static_cast<AwardId>(82)]); // awards.txt 82 "%u Deaths".
        EXPECT_FALSE(character._achievedAwardsBits[static_cast<AwardId>(85)]);
    }
}

// Dying in the Hive after the reactor is destroyed (qbit 180) but before the win hand-in (qbit 237, set by
// hive.evt event 60) is the LOSE ending, not a respawn: MM6.EXE's defeat handler (0x453bbd / 0x454109)
// checks the current map against "hive.blv" and runs event 601 - the lose certificate - instead of the
// losegame movie, penalties and New Sorpigal respawn.
GAME_TEST(Mm6, HiveDeathLosesGame) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    MapId hive = pMapStats->GetMapInfo("hive.blv");
    ASSERT_NE(hive, MAP_INVALID);

    auto enterHive = [&] {
        game.teleportTo(hive, Vec3f(0, 0, 0), 0);
        ASSERT_FALSE(pIndoor->pSpawnPoints.empty());
        game.teleportTo(hive, pIndoor->pSpawnPoints[0].position, 0);
        game.tick(1);
    };
    auto killWholeParty = [&] {
        for (Character &character : pParty->pCharacters)
            character.conditions.set(CONDITION_DEAD, pParty->GetPlayingTime());
        game.tick(10);
    };

    // Control: dying in the Hive BEFORE the reactor is destroyed is an ordinary defeat - respawn in
    // New Sorpigal like anywhere else.
    enterHive();
    killWholeParty();
    EXPECT_EQ(uGameState, GAME_STATE_PLAYING);
    EXPECT_EQ(pMapStats->pInfos[engine->_currentLoadedMapId].fileName, "oute3.odm");

    // Back into the Hive, this time mid-meltdown: reactor destroyed, win not handed in.
    enterHive();
    pParty->_questBits.set(static_cast<QuestBit>(180));
    killWholeParty();

    // The lose certificate shows instead of a respawn - the party is still in the Hive behind it - and
    // closing it (first click = credits popup, second = release) quits to the main menu.
    EXPECT_EQ(current_screen_type, SCREEN_GAMEOVER_WINDOW);
    ASSERT_TRUE(pGameOverWindow);
    EXPECT_EQ(engine->_currentLoadedMapId, hive);
    game.pressAndReleaseButton(BUTTON_LEFT, 320, 240);
    game.tick(2);
    game.pressAndReleaseButton(BUTTON_LEFT, 320, 240);
    for (int i = 0; i < 50 && GetCurrentMenuID() != MENU_MAIN; i++)
        game.tick(1);
    EXPECT_EQ(GetCurrentMenuID(), MENU_MAIN);
}

GAME_TEST(Mm6, FootTravelAcrossBorders) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame(); // New Sorpigal, oute3 = grid cell E3.

    // MM6 border travel is grid arithmetic on the "out<column><row>.odm" file name: E3 connects
    // west to D3 (Castle Ironfist) and north to E2 (Misty Islands); east and south are off-grid.
    MapId ironfist = pMapStats->GetMapInfo("outd3.odm");
    MapId mist = pMapStats->GetMapInfo("oute2.odm");
    MapId bootlegBay = pMapStats->GetMapInfo("outd2.odm");
    ASSERT_NE(ironfist, MAP_INVALID);
    ASSERT_NE(mist, MAP_INVALID);
    ASSERT_NE(bootlegBay, MAP_INVALID);
    EXPECT_EQ(pOutdoor->getTravelDestination(-23000, 0), ironfist);
    EXPECT_EQ(pOutdoor->getTravelDestination(23000, 0), MAP_INVALID);
    EXPECT_EQ(pOutdoor->getTravelDestination(0, 23000), mist);
    EXPECT_EQ(pOutdoor->getTravelDestination(0, -23000), MAP_INVALID);
    // Both axes are checked independently, so corner crossings go diagonally: off the
    // northwest corner of E3 lies D2 (Bootleg Bay); the other three corners are off-grid.
    EXPECT_EQ(pOutdoor->getTravelDestination(-23000, 23000), bootlegBay);
    EXPECT_EQ(pOutdoor->getTravelDestination(23000, 23000), MAP_INVALID);
    EXPECT_EQ(pOutdoor->getTravelDestination(-23000, -23000), MAP_INVALID);
    EXPECT_EQ(pOutdoor->getTravelDestination(23000, -23000), MAP_INVALID);
    EXPECT_EQ(getTravelTime(), 5); // Walking always takes 5 days in MM6.

    // Find dry land on the west border - the travel prompt won't open over water.
    float borderY = 0.0f;
    float borderZ = 0.0f;
    bool found = false;
    for (int y = -20000; y <= 20000 && !found; y += 512) {
        bool isOnWater = false;
        int floorFaceId = -1;
        float z = ODM_GetFloorLevel(Vec3f(-22400, y, 3000), &isOnWater, &floorFaceId);
        if (!isOnWater) {
            borderY = y;
            borderZ = z;
            found = true;
        }
    }
    ASSERT_TRUE(found);

    game.teleportTo(engine->_currentLoadedMapId, Vec3f(-22400, borderY, borderZ), 270);
    game.tick(3);

    // Step across the border - the on-foot travel prompt opens, Y confirms.
    Time timeBefore = pParty->GetPlayingTime();
    pParty->pos.x = -22700;
    game.tick(3);
    ASSERT_EQ(current_screen_type, SCREEN_CHANGE_LOCATION);
    game.pressAndReleaseKey(PlatformKey::KEY_Y);
    game.tick(10);

    // 5 days later the party is at the opposite (east) border of Castle Ironfist,
    // north-south position preserved.
    EXPECT_EQ(pMapStats->pInfos[engine->_currentLoadedMapId].fileName, "outd3.odm");
    EXPECT_EQ(uCurrentlyLoadedLevelType, LEVEL_OUTDOOR);
    EXPECT_GT(pParty->pos.x, 20000);
    EXPECT_NEAR(pParty->pos.y, borderY, 1500);
    EXPECT_EQ((pParty->GetPlayingTime() - timeBefore).days(), 5);
    game.tick(10);
}

// Walks up to the outdoor door face wired to the given local event and opens it with SPACE.
static void enterHouseThroughDoor(EngineController &game, int eventId) {
    const BLVFace *door = nullptr;
    for (const BSPModel &model : pOutdoor->pBModels) {
        for (const BLVFace &face : model.faces) {
            if (face.eventId == eventId && face.Clickable()) {
                door = &face;
                break;
            }
        }
    }
    ASSERT_NE(door, nullptr);
    Vec3f doorCenter = door->boundingBox.center();
    Vec3f pos = doorCenter + door->facePlane.normal * 130;
    pos.z = door->boundingBox.z1;
    int yawDegrees = TrigLUT.atan2(doorCenter.x - pos.x, doorCenter.y - pos.y) * 90 / 512;
    game.teleportTo(engine->_currentLoadedMapId, pos, yawDegrees);
    game.tick(1);
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_HOUSE);
}

// Advances whole days until the given transport house has an active route today.
static void advanceToTravelDay(EngineController &game, HouseId houseId) {
    for (int i = 0; i < 8 && !isTravelAvailable(houseId); i++) {
        pParty->GetPlayingTime() += Duration::fromDays(1);
        game.tick(1);
    }
    ASSERT_TRUE(isTravelAvailable(houseId));
}

GAME_TEST(Mm6, TravelByCoachAndBoat) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    pParty->SetGold(2000);

    // New Sorpigal's transport houses from 2dEvents: 48 = stables, 57 = dock.
    ASSERT_EQ(houseTable[HouseId(48)].uType, HOUSE_TYPE_STABLE);
    ASSERT_EQ(houseTable[HouseId(57)].uType, HOUSE_TYPE_BOAT);

    // Ride the coach to Castle Ironfist (schedule entry 0: Mon/Wed/Fri, 2 days).
    advanceToTravelDay(game, HouseId(48));
    enterHouseThroughDoor(game, 15); // oute3 event 15 = EnterHouse(48), the stables door.
    ASSERT_NE(window_SpeakInHouse, nullptr);
    ASSERT_EQ(window_SpeakInHouse->houseId(), HouseId(48));

    int goldBefore = pParty->GetGold();
    Time timeBefore = pParty->GetPlayingTime();
    clickProprietorOption(game, DIALOGUE_TRANSPORT_SCHEDULE_1);
    game.tick(10);

    EXPECT_EQ(pMapStats->pInfos[engine->_currentLoadedMapId].fileName, "outd3.odm"); // Castle Ironfist.
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
    EXPECT_NEAR(pParty->pos.x, 14317, 8); // MM6.EXE TravelInfo[0] arrival pose.
    EXPECT_NEAR(pParty->pos.y, 2696, 8);
    EXPECT_LT(pParty->GetGold(), goldBefore);
    EXPECT_EQ((pParty->GetPlayingTime() - timeBefore).days(), 2);

    // Back to New Sorpigal and sail to the Misty Islands (schedule entry 15: Tue/Thu/Sat, 3 days).
    // The dock's EnterHouse(57) event (oute3 event 29) isn't wired to a clickable building face,
    // so enter the house directly through the same path EVENT_SpeakInHouse takes.
    game.teleportTo(pMapStats->GetMapInfo("oute3.odm"), Vec3f(-9728, -11319, 160), 0);
    game.tick(2);
    advanceToTravelDay(game, HouseId(57));
    ASSERT_TRUE(enterHouse(HouseId(57)));
    createHouseUI(HouseId(57));
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_HOUSE);
    ASSERT_NE(window_SpeakInHouse, nullptr);
    ASSERT_EQ(window_SpeakInHouse->houseId(), HouseId(57));

    goldBefore = pParty->GetGold();
    timeBefore = pParty->GetPlayingTime();
    clickProprietorOption(game, DIALOGUE_TRANSPORT_SCHEDULE_1);
    game.tick(10);

    EXPECT_EQ(pMapStats->pInfos[engine->_currentLoadedMapId].fileName, "oute2.odm"); // Misty Islands.
    EXPECT_NEAR(pParty->pos.x, -4225, 8); // MM6.EXE TravelInfo[15] arrival pose.
    EXPECT_NEAR(pParty->pos.y, -14604, 8);
    EXPECT_LT(pParty->GetGold(), goldBefore);
    EXPECT_EQ((pParty->GetPlayingTime() - timeBefore).days(), 3);
    game.tick(10);
}

GAME_TEST(Mm6, TownPortal) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // Cast Town Portal (all six MM6 towns are open - MM6 has no unlock quest bits) and click
    // towns on MM6's own map image. Clicks are anchored to positions on the picture, not to
    // marker indices, so a wrong rect/destination pairing fails here (MM6.EXE pairs click box i
    // with destination i through the jump table at 0x42F958, not identity).
    engine->config->debug.AllMagic.setValue(true);
    game.castSpell(1, SPELL_WATER_TOWN_PORTAL);
    game.tick(2);
    game.pressAndReleaseButton(BUTTON_LEFT, 377, 295); // The "New Sorpigal" label box on the image.
    game.tick(2);

    // The party starts in New Sorpigal, so this is a same-map teleport straight to the pose.
    EXPECT_EQ(pMapStats->pInfos[engine->_currentLoadedMapId].fileName, "oute3.odm"); // New Sorpigal.
    EXPECT_NEAR(pParty->pos.x, -9705, 8); // MM6.EXE TownPortalInfo[3] fountain pose.
    EXPECT_NEAR(pParty->pos.y, -6858, 8);
    game.tick(10);

    game.castSpell(2, SPELL_WATER_TOWN_PORTAL);
    game.tick(2);
    game.pressAndReleaseButton(BUTTON_LEFT, 248, 171); // The "Free Haven" label box on the image.
    game.tick(2);
    game.skipLoadingScreen();
    game.tick(10);

    EXPECT_EQ(pMapStats->pInfos[engine->_currentLoadedMapId].fileName, "outc2.odm"); // Free Haven.
    EXPECT_NEAR(pParty->pos.x, 6991, 8); // MM6.EXE TownPortalInfo[1] fountain pose.
    EXPECT_NEAR(pParty->pos.y, 13438, 8);
    game.tick(10);
}

GAME_TEST(Mm6, LloydBeaconSaveRoundtrip) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // Beacons serialize their map as a games.lod file index (MM6: 1-based over the sorted map
    // files); both an outdoor and an indoor map id must survive a save/load roundtrip.
    MapId goblinwatch = pMapStats->GetMapInfo("d01.blv");
    MapId newSorpigal = pMapStats->GetMapInfo("oute3.odm");
    ASSERT_NE(goblinwatch, MAP_INVALID);

    LloydBeacon outdoorBeacon;
    outdoorBeacon.uBeaconTime = pParty->GetPlayingTime() + Duration::fromDays(7);
    outdoorBeacon._partyPos = Vec3f(-9728, -11319, 160);
    outdoorBeacon._partyViewYaw = 512;
    outdoorBeacon.mapId = newSorpigal;
    outdoorBeacon.image = GraphicsImage::Create(render->MakeViewportScreenshot(92, 68));
    pParty->pCharacters[0].vBeacons[0] = outdoorBeacon;

    LloydBeacon indoorBeacon = outdoorBeacon;
    indoorBeacon.mapId = goblinwatch;
    pParty->pCharacters[0].vBeacons[4] = indoorBeacon;

    Blob save = game.saveGame();
    game.loadGame(save);
    game.tick(2);

    ASSERT_TRUE(pParty->pCharacters[0].vBeacons[0].has_value());
    EXPECT_EQ(pParty->pCharacters[0].vBeacons[0]->mapId, newSorpigal);
    EXPECT_EQ(pParty->pCharacters[0].vBeacons[0]->_partyPos, Vec3f(-9728, -11319, 160));
    ASSERT_TRUE(pParty->pCharacters[0].vBeacons[4].has_value());
    EXPECT_EQ(pParty->pCharacters[0].vBeacons[4]->mapId, goblinwatch);
}

GAME_TEST(Mm6, DevLeftoverOpcodesAreNoOps) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // MM6.EXE's event dispatch table routes opcodes 20/27/28 (ModifyItem / RndPassword / RndAnswer -
    // dev leftovers, never implemented) straight to the step-advance path, so they are no-ops in the
    // shipped game. Snergle's Iron Mines (d09.blv) has RndPassword at step 0 of switch event 21 whose
    // step 1 opens a door: execution must pass THROUGH the no-op and drive the door.
    MapId snergleMines = pMapStats->GetMapInfo("d09.blv");
    ASSERT_NE(snergleMines, MAP_INVALID);
    game.teleportTo(snergleMines, Vec3f(0, 0, 0), 0);
    ASSERT_FALSE(pIndoor->pSpawnPoints.empty());
    game.teleportTo(snergleMines, pIndoor->pSpawnPoints[0].position, 0);
    game.tick(1);

    ASSERT_TRUE(engine->_localEventMap.hasEvent(21));
    const std::vector<EvtInstruction> &script = engine->_localEventMap.function(21);
    auto rndPassword = std::ranges::find_if(script, [](const EvtInstruction &ir) { return ir.opcode == EVENT_RandomPassword; });
    ASSERT_NE(rndPassword, script.end());
    auto doorStep = std::ranges::find_if(script, [](const EvtInstruction &ir) { return ir.opcode == EVENT_ChangeDoorState; });
    ASSERT_NE(doorStep, script.end());

    BLVDoor *door = nullptr;
    for (BLVDoor &candidate : pIndoor->doors) {
        if (candidate.doorId == static_cast<uint32_t>(doorStep->data.door_descr.door_id)) {
            door = &candidate;
            break;
        }
    }
    ASSERT_NE(door, nullptr);
    DoorState stateBefore = door->state;

    eventProcessor(21, Pid(), 1, 0);
    game.tick(2);
    EXPECT_NE(door->state, stateBefore); // The no-op step didn't halt the script.

    // Event 39 is a bookshelf whose only non-marker step is a RndAnswer - firing it must be a
    // clean no-op ("You thumb through the books, but find nothing of interest.").
    ASSERT_TRUE(engine->_localEventMap.hasEvent(39));
    eventProcessor(39, Pid(), 1, 0);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
}

GAME_TEST(Mm6, OverlaysRenderAndExpire) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // MM6's doverlay.bin binds real sprite framesets (MM7 has the same 96 overlay ids, all pointing
    // at the "null" sprite). Spot-check an impact spark, a portrait buff fx and two status icons.
    auto descById = [](int overlayId) -> const OverlayDesc * {
        for (const OverlayDesc &desc : pOverlayList->pOverlays)
            if (desc.uOverlayID == overlayId)
                return &desc;
        return nullptr;
    };
    EXPECT_EQ(pOverlayList->pOverlays.size(), 96);
    for (int overlayId : {904, 10000, 10009, 10015}) {
        const OverlayDesc *desc = descById(overlayId);
        ASSERT_NE(desc, nullptr);
        ASSERT_NE(desc->uSpriteFramesetID, 0);
        SpriteFrame *frame = pSpriteFrameTable->GetFrame(desc->uSpriteFramesetID, 0_ticks);
        ASSERT_NE(frame, nullptr);
        EXPECT_NE(frame->spriteName, "null");
        ASSERT_NE(frame->sprites[0], nullptr);
    }

    // An elemental hit spawns a one-shot spark overlay attached to the actor; the per-frame update
    // then animates it in the 3D view and frees the slot when the animation ends.
    pActiveOverlayList->Reset();
    ASSERT_FALSE(pActors.empty());
    Actor::AddOnDamageOverlay(0, 1, 100); // Fire damage spark, overlay 904.
    ActiveOverlay &spark = pActiveOverlayList->pOverlays[0];
    EXPECT_EQ(spark.pid, Pid(OBJECT_Actor, 0));
    EXPECT_GT(spark.animLength, 0);
    EXPECT_GT(spark.fpDamageMod, 0);
    EXPECT_EQ(spark.spriteFrameTime, 0);
    game.tick(2);
    EXPECT_GT(spark.spriteFrameTime, 0); // The draw loop ran and advanced the animation.
    for (int i = 0; i < 100 && spark.animLength > 0; i++)
        game.tick(1);
    EXPECT_LE(spark.animLength, 0); // One-shot expired and freed its slot.
    EXPECT_EQ(spark.pid, Pid());

    // Screen-anchored buff fx: anchored over character 0's portrait, deduplicated per anchor, kept
    // alive past its animation length until the owning SpellBuff resets the slot.
    int slotIndex = pActiveOverlayList->addScreenOverlay(10000, 310, 0_ticks, 65536);
    ASSERT_GT(slotIndex, 0);
    ActiveOverlay &buffFx = pActiveOverlayList->pOverlays[slotIndex - 1];
    EXPECT_EQ(buffFx.target, 310);
    EXPECT_EQ(buffFx.screenSpaceX, 19);
    EXPECT_EQ(buffFx.screenSpaceY, 456);
    EXPECT_TRUE(buffFx.flags & OVERLAY_FLAG_BUFF_OWNED);
    EXPECT_EQ(pActiveOverlayList->addScreenOverlay(10000, 310, 0_ticks, 65536), slotIndex);
    game.tick(5);
    EXPECT_GT(buffFx.animLength, 0); // Not expired by the update loop.
    SpellBuff buff;
    buff.Apply(pParty->GetPlayingTime() + Duration::fromHours(1), MASTERY_NOVICE, 5, slotIndex, 0);
    buff.Reset();
    EXPECT_LE(buffFx.animLength, 0); // Freed together with the buff.

    // Party-buff status icons (the y=254 row) draw while the buff is active - smoke-test the draw
    // pass headless.
    pParty->pPartyBuffs[PARTY_BUFF_WIZARD_EYE].Apply(pParty->GetPlayingTime() + Duration::fromHours(1), MASTERY_NOVICE, 5, 0, 0);
    game.tick(3);
    pParty->pPartyBuffs[PARTY_BUFF_WIZARD_EYE].Reset();
}

GAME_TEST(Mm6, SpellCastFx) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    engine->config->debug.AllMagic.setValue(true); // Cast lands every time - no mana/skill/mastery gating.
    game.tick(1);

    // Count active one-shot cast-fx overlays sitting on a character-portrait anchor (target 100..103) that
    // resolve to a given doverlay overlay id.
    auto portraitFxFor = [](int overlayId) {
        int count = 0;
        for (const ActiveOverlay &slot : pActiveOverlayList->pOverlays) {
            if (slot.animLength <= 0 || slot.target < 100 || slot.target > 103)
                continue;
            if (slot.indexToOverlayList < 0 || slot.indexToOverlayList >= static_cast<int>(pOverlayList->pOverlays.size()))
                continue;
            if (pOverlayList->pOverlays[slot.indexToOverlayList].uOverlayID == overlayId)
                count++;
        }
        return count;
    };

    // MM6 spell id 12 is Wizard Eye, a party-wide utility buff. Casting it flashes overlay 2000 over the
    // portraits (MM6.EXE CastSpell dispatch 0x422C93). It applies a party buff without running through the
    // MM7 SetPlayerBuffAnim path, so its cast fx must come from the shared cast tail, not the buff-anim hook.
    pActiveOverlayList->Reset();
    EXPECT_EQ(portraitFxFor(2000), 0); // Fail-first anchor: nothing spawned before the cast.
    pushSpellOrRangedAttack(static_cast<SpellId>(12), 0, CombinedSkillValue::none(), 0, 1);
    game.tick(1);
    // A party-target spell loops the fx over all four portraits (anchors 100..103).
    EXPECT_EQ(portraitFxFor(2000), 4);
    // The cast fx is a one-shot (not a buff-owned persistent overlay), so it expires on its own.
    for (const ActiveOverlay &slot : pActiveOverlayList->pOverlays)
        if (slot.animLength > 0 && slot.target >= 100 && slot.target <= 103)
            EXPECT_FALSE(slot.flags & OVERLAY_FLAG_BUFF_OWNED);

    // A character-targeted spell flashes over only the targeted portrait. MM6 spell id 68 is First Aid,
    // whose effect (Heal) targets a single character; drive it through the target-picking path onto
    // character 2 and check the fx lands on anchor 102 alone.
    pActiveOverlayList->Reset();
    pushSpellOrRangedAttack(static_cast<SpellId>(68), 0, CombinedSkillValue::none(), 0, 0);
    spellTargetPicked(Pid(), 2);
    game.tick(1);
    EXPECT_EQ(portraitFxFor(7010), 1); // Only the targeted portrait.
    bool onChar2 = false;
    for (const ActiveOverlay &slot : pActiveOverlayList->pOverlays)
        if (slot.animLength > 0 && slot.target == 102 &&
                pOverlayList->pOverlays[slot.indexToOverlayList].uOverlayID == 7010)
            onChar2 = true;
    EXPECT_TRUE(onChar2);

    // The MM6-unique spells (Create Food, the stat-buff family, Day of the Gods, ...) run through
    // castMm6UniqueSpell, which applies the cast tail itself - their fx must spawn there too. Day of the
    // Gods (MM6 id 83, overlay 8050) is always party-wide. Casting it right after the picked-target First
    // Aid above also proves a reused cast-queue slot doesn't leak the old target onto a party cast.
    pActiveOverlayList->Reset();
    pushSpellOrRangedAttack(static_cast<SpellId>(83), 0, CombinedSkillValue::none(), 0, 1);
    game.tick(1);
    EXPECT_EQ(portraitFxFor(8050), 4);

    // Power (MM6 id 75) is the one spell with two cast-fx add sites: MM6.EXE spawns a secondary overlay
    // 6030 over character 0's portrait alongside the primary 7080.
    pActiveOverlayList->Reset();
    pushSpellOrRangedAttack(static_cast<SpellId>(75), 0, CombinedSkillValue::none(), 0, 1);
    game.tick(1);
    EXPECT_EQ(portraitFxFor(7080), 4); // AllMagic casts at grandmaster, so the buff is party-wide.
    EXPECT_EQ(portraitFxFor(6030), 1);
    bool secondaryOnChar0 = false;
    for (const ActiveOverlay &slot : pActiveOverlayList->pOverlays)
        if (slot.animLength > 0 && slot.target == 100 &&
                pOverlayList->pOverlays[slot.indexToOverlayList].uOverlayID == 6030)
            secondaryOnChar0 = true;
    EXPECT_TRUE(secondaryOnChar0);

    // Below Master the single-stat buffs pick one character, and the fx follows onto that portrait alone:
    // Lucky Day (MM6 id 48, overlay 5030) picked onto character 1.
    engine->config->debug.AllMagic.setValue(false);
    pActiveOverlayList->Reset();
    pushSpellOrRangedAttack(static_cast<SpellId>(48), 0, CombinedSkillValue(10, MASTERY_NOVICE), 0, 0);
    spellTargetPicked(Pid(), 1);
    game.tick(1);
    EXPECT_EQ(portraitFxFor(5030), 1);
    bool onChar1 = false;
    for (const ActiveOverlay &slot : pActiveOverlayList->pOverlays)
        if (slot.animLength > 0 && slot.target == 101 &&
                pOverlayList->pOverlays[slot.indexToOverlayList].uOverlayID == 5030)
            onChar1 = true;
    EXPECT_TRUE(onChar1);
}

GAME_TEST(Mm6, TurnBasedCombatIcon) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // MM6 has no dift.bin turn-based icons (MM7's "turnstart"/"turnhour"/...), so the overlay falls back to
    // drawing two sprite framesets: newhand1 while the party can act, newglas1 while monsters take their turn
    // (MM6.EXE 0x435F03, at screen anchor (444, 326)). loadIcons() resolved and loaded them.
    ASSERT_TRUE(turnBasedOverlay.usesMm6Sprites());
    int hand = pSpriteFrameTable->FastFindSprite("newhand1");
    int glass = pSpriteFrameTable->FastFindSprite("newglas1");
    ASSERT_GT(hand, 0);
    ASSERT_GT(glass, 0);
    // The framesets were InitializeSprite'd, so their frames carry real (non-null) sprites.
    ASSERT_NE(pSpriteFrameTable->GetFrame(hand, 0_ticks)->sprites[0], nullptr);
    ASSERT_NE(pSpriteFrameTable->GetFrame(glass, 0_ticks)->sprites[0], nullptr);

    // The overlay tracks the turn stage directly (no MM7-style opening-hand phase): hourglass on the
    // monsters' turn, hand on the party's attack or movement steps, nothing when combat ends.
    turnBasedOverlay.update(8_ticks, TE_WAIT);
    EXPECT_EQ(turnBasedOverlay.state(), TURN_BASED_OVERLAY_WAIT);
    turnBasedOverlay.draw(); // Headless smoke test - draws the hourglass without crashing.

    turnBasedOverlay.update(8_ticks, TE_ATTACK);
    EXPECT_EQ(turnBasedOverlay.state(), TURN_BASED_OVERLAY_ATTACK);
    turnBasedOverlay.draw(); // Draws the hand.

    turnBasedOverlay.update(8_ticks, TE_MOVEMENT);
    EXPECT_EQ(turnBasedOverlay.state(), TURN_BASED_OVERLAY_MOVEMENT);

    turnBasedOverlay.update(8_ticks, TE_NONE);
    EXPECT_EQ(turnBasedOverlay.state(), TURN_BASED_OVERLAY_NONE);
    turnBasedOverlay.draw(); // No overlay when combat is over - must be a no-op.

    turnBasedOverlay.reset();
}

GAME_TEST(Mm6, SpellNamesLoad) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // MM6's spells.txt is parsed natively: the same 1..99 spell ids as MM7, but read with MM6's column
    // layout. A few sampled ids: 2 = Flame Arrow, 81 = Slow, 99 = Dark Containment.
    EXPECT_EQ(pSpellStats->pInfos[SPELL_FIRE_FIRE_BOLT].name, "Flame Arrow");
    EXPECT_EQ(pSpellStats->pInfos[SPELL_LIGHT_PARALYZE].name, "Slow");
    EXPECT_FALSE(pSpellStats->pInfos[SPELL_DARK_SOULDRINKER].name.empty()); // MM6 id 99 = Dark Containment
}

// The engine's Class enum follows MM7's 36-slot numbering, but MM6's class.txt has 18 rows in MM6
// class-byte order (base * 3 + tier). Each row must land on the slot classFromMm6ClassByte maps it
// to - zipping the rows onto the enum in file order puts the Cleric description on CLASS_BLACK_KNIGHT
// and leaves 5 of the 6 startable classes with wrong or empty creation-screen hints.
GAME_TEST(Mm6, ClassDescriptions) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // The six startable classes shown on the party-creation screen.
    EXPECT_TRUE(localization->classDescription(CLASS_KNIGHT).starts_with("The Knight class"));
    EXPECT_TRUE(localization->classDescription(CLASS_CLERIC).starts_with("Clerics in Enroth"));
    EXPECT_TRUE(localization->classDescription(CLASS_SORCERER).starts_with("Students of the realm"));
    EXPECT_TRUE(localization->classDescription(CLASS_PALADIN).starts_with("A cross between Knight and Cleric"));
    EXPECT_TRUE(localization->classDescription(CLASS_ARCHER).starts_with("Like Paladins"));
    EXPECT_TRUE(localization->classDescription(CLASS_DRUID).starts_with("Druids are a hybrid"));

    // Promotion tiers occupy the first three slots of each MM7 class line.
    EXPECT_TRUE(localization->classDescription(CLASS_CHAMPION).starts_with("The Champion class"));
    EXPECT_TRUE(localization->classDescription(CLASS_PRIEST_OF_SUN).starts_with("High Priest is"));  // MM6 High Priest.
    EXPECT_TRUE(localization->classDescription(CLASS_WARRIOR_MAGE).starts_with("Battle Mage is"));   // MM6 Battle Mage.
    EXPECT_TRUE(localization->classDescription(CLASS_MASTER_ARCHER).starts_with("Warrior Mage is")); // MM6 Warrior Mage.
    EXPECT_TRUE(localization->classDescription(CLASS_ARCHAMGE).starts_with("Arch Mages are"));
    EXPECT_TRUE(localization->classDescription(CLASS_ARCH_DRUID).starts_with("Arch Druids are"));

    // MM7-only classes have no MM6 rows and must stay empty.
    EXPECT_TRUE(localization->classDescription(CLASS_THIEF).empty());
    EXPECT_TRUE(localization->classDescription(CLASS_MONK).empty());
    EXPECT_TRUE(localization->classDescription(CLASS_BLACK_KNIGHT).empty());
    EXPECT_TRUE(localization->classDescription(CLASS_LICH).empty());
}

// Class NAMES must come from MM6's class.txt column 0 as well - the fixed MM7 global.txt row ids
// that initializeClassNames() uses mean something completely different in MM6's global.txt
// (LSTR_PRIEST_OF_LIGHT = row 44 = "Combat", LSTR_MASTER_ARCHER = row 119 = "Items"), so a
// twice-promoted Cleric displayed as "<name> the Combat" and a twice-promoted Archer as
// "<name> the Items" on the character sheet, status bar and game-over screen.
GAME_TEST(Mm6, ClassNames) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // The six startable classes.
    EXPECT_EQ(localization->className(CLASS_KNIGHT), "Knight");
    EXPECT_EQ(localization->className(CLASS_CLERIC), "Cleric");
    EXPECT_EQ(localization->className(CLASS_SORCERER), "Sorcerer");
    EXPECT_EQ(localization->className(CLASS_PALADIN), "Paladin");
    EXPECT_EQ(localization->className(CLASS_ARCHER), "Archer");
    EXPECT_EQ(localization->className(CLASS_DRUID), "Druid");

    // Promotion tiers, incl. the MM6-specific names that differ from the MM7 slot they map onto.
    EXPECT_EQ(localization->className(CLASS_CAVALIER), "Cavalier");
    EXPECT_EQ(localization->className(CLASS_CHAMPION), "Champion");
    EXPECT_EQ(localization->className(CLASS_PRIEST), "Priest");
    EXPECT_EQ(localization->className(CLASS_PRIEST_OF_SUN), "High Priest");  // MM6 High Priest, not "Combat".
    EXPECT_EQ(localization->className(CLASS_WARRIOR_MAGE), "Battle Mage");   // MM6 Battle Mage, not MM7 "Warrior Mage".
    EXPECT_EQ(localization->className(CLASS_MASTER_ARCHER), "Warrior Mage"); // MM6 Warrior Mage, not "Items".
    EXPECT_EQ(localization->className(CLASS_ARCHAMGE), "Arch Mage");
    EXPECT_EQ(localization->className(CLASS_GREAT_DRUID), "Great Druid");
    EXPECT_EQ(localization->className(CLASS_ARCH_DRUID), "Arch Druid");

    // MM7-only classes have no MM6 rows and must stay empty.
    EXPECT_TRUE(localization->className(CLASS_THIEF).empty());
    EXPECT_TRUE(localization->className(CLASS_MONK).empty());
    EXPECT_TRUE(localization->className(CLASS_BLACK_KNIGHT).empty());
    EXPECT_TRUE(localization->className(CLASS_LICH).empty());
}

// Buff display names are cached from fixed global.txt row ids that were chosen for MM7's row list.
// MM6's global.txt shares most of those rows (Heroism@440..Prot Magic@462, Shield@279, the
// condition-style actor buffs), but the rest hold unrelated MM6 strings ("Backward", "Train",
// "SOUND VOLUME", ...) or don't exist in MM6's 595-row file at all. This gate pins the MM6 names to
// MM6's own rows - named for what translateForCast actually feeds into each engine buff slot - and
// keeps the slots no MM6 cast can reach blank instead of showing MM7-row garbage.
GAME_TEST(Mm6, BuffNames) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // Party buffs whose MM7 row ids hold unrelated strings in MM6's global.txt.
    EXPECT_EQ(localization->partyBuffName(PARTY_BUFF_RESIST_FIRE), "Prot Fire");    // Was "Backward".
    EXPECT_EQ(localization->partyBuffName(PARTY_BUFF_RESIST_AIR), "Prot Elec");     // Was "Set".
    EXPECT_EQ(localization->partyBuffName(PARTY_BUFF_RESIST_WATER), "Prot Cold");   // Was "Select".
    EXPECT_EQ(localization->partyBuffName(PARTY_BUFF_RESIST_BODY), "Prot Poison");  // Was "Shopkeeper".
    EXPECT_EQ(localization->partyBuffName(PARTY_BUFF_DAY_OF_GODS), "Day of the Gods"); // Was "Stocked"; comes from spells.txt.

    // Rows that coincide between the MM6 and MM7 global.txt row lists keep working unchanged.
    EXPECT_EQ(localization->partyBuffName(PARTY_BUFF_TORCHLIGHT), "Torch Light");
    EXPECT_EQ(localization->partyBuffName(PARTY_BUFF_WIZARD_EYE), "Wizard Eye");
    EXPECT_EQ(localization->partyBuffName(PARTY_BUFF_PROTECTION_FROM_MAGIC), "Prot Magic");
    EXPECT_EQ(localization->partyBuffName(PARTY_BUFF_WATER_WALK), "Water Walk");

    // Character buffs, named for the MM6 spell that lands on the slot via translateForCast:
    // Lucky Day -> Fate, Power -> Hammerhands, Guardian Angel -> Preservation.
    EXPECT_EQ(localization->characterBuffName(CHARACTER_BUFF_FATE), "Lucky Day");        // Was "Subtract from Stat".
    EXPECT_EQ(localization->characterBuffName(CHARACTER_BUFF_HAMMERHANDS), "Power");     // Was "Train".
    EXPECT_EQ(localization->characterBuffName(CHARACTER_BUFF_PRESERVATION), "Guardian"); // Was "Use".
    EXPECT_EQ(localization->characterBuffName(CHARACTER_BUFF_BLESS), "Bless");
    EXPECT_EQ(localization->characterBuffName(CHARACTER_BUFF_STONESKIN), "Stoneskin");

    // Actor buffs: MM6 Feeblemind runs the MM7 Berserk handler, and the Day of Protection /
    // Hour of Power names only exist in MM6's spells.txt.
    EXPECT_EQ(localization->actorBuffName(ACTOR_BUFF_BERSERK), "Feebleminded");
    EXPECT_EQ(localization->actorBuffName(ACTOR_BUFF_FATE), "Lucky Day");
    EXPECT_EQ(localization->actorBuffName(ACTOR_BUFF_DAY_OF_PROTECTION), "Day of Protection");
    EXPECT_EQ(localization->actorBuffName(ACTOR_BUFF_HOUR_OF_POWER), "Hour of Power");

    // Buff slots no MM6 cast can reach are blank, not MM7-row garbage.
    EXPECT_TRUE(localization->partyBuffName(PARTY_BUFF_RESIST_EARTH).empty());   // Was "SOUND VOLUME".
    EXPECT_TRUE(localization->partyBuffName(PARTY_BUFF_RESIST_MIND).empty());    // Was "Spell".
    EXPECT_TRUE(localization->partyBuffName(PARTY_BUFF_DETECT_LIFE).empty());    // Was "Stablemaster".
    EXPECT_TRUE(localization->partyBuffName(PARTY_BUFF_INVISIBILITY).empty());   // Was "Starving".
    EXPECT_TRUE(localization->partyBuffName(PARTY_BUFF_IMMOLATION).empty());     // Was "STEP MODE".
    EXPECT_TRUE(localization->characterBuffName(CHARACTER_BUFF_PAIN_REFLECTION).empty()); // Was "Travel to New Area".
    EXPECT_TRUE(localization->characterBuffName(CHARACTER_BUFF_REGENERATION).empty());    // Was "Virgo".
    EXPECT_TRUE(localization->characterBuffName(CHARACTER_BUFF_ACCURACY).empty());        // Was "Walking".
    EXPECT_TRUE(localization->characterBuffName(CHARACTER_BUFF_SPEED).empty());           // Was "High Priest".
    EXPECT_TRUE(localization->actorBuffName(ACTOR_BUFF_PAIN_REFLECTION).empty());         // Was "Travel to New Area".
}

// The MM6 and MM7 spell tables share the identical 9-school x 11-spell id layout, but the spell that sits
// at a given id often differs between the two games. The cast runtime dispatches on MM7-named SpellId
// constants, so an MM6 spell has to be routed through translateForCast to the MM7 spell whose effect (and
// targeting mode) matches. These two gates cast such remapped MM6 spells and check that the intended effect,
// not MM7's same-id effect, runs. (They are the fail-first tests for that wiring: without translateForCast in
// the dispatch path both produce no projectile.)
GAME_TEST(Mm6, CastShiftedSpell) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    engine->config->debug.AllMagic.setValue(true); // Cast lands every time - no mana/skill/mastery gating.

    // MM6 spell id 30 is Acid Burst, a projectile. MM7's id 30 is Enchant Item, an inventory-target spell that
    // launches no projectile and opens the item-enchant window instead. This is one half of MM6's id29/id30
    // swap (Enchant Item and Acid Burst trade slots relative to MM7). Casting the MM6 spell must fire an acid
    // burst, not enter item-enchant targeting.
    // Quick-cast it (nonzero overrideSoundId, as the quick-spell button does) so a projectile spell fires
    // immediately instead of opening an actor-targeting window.
    pushSpellOrRangedAttack(static_cast<SpellId>(30), 0, CombinedSkillValue::none(), 0, 1);
    game.tick(1);

    // The item-enchant targeting mode must NOT have been entered, and a projectile carrying the native MM6
    // spell id 30 must have been launched (the Acid Burst effect ran).
    EXPECT_FALSE(IsEnchantingInProgress);
    int projectiles = std::ranges::count_if(pSpriteObjects, [](const SpriteObject &obj) {
        return obj.uSpellID == static_cast<SpellId>(30) && obj.uObjectDescID != 0;
    });
    EXPECT_GE(projectiles, 1);

    // The projectile sprite stays keyed off the NATIVE id (MM6's dobjlist projectile objects are indexed by
    // the native MM6 spell slot). MM6 Acid Burst is native id 30, whose slot carries the acid projectile
    // object; the effect id (MM7 Acid Burst, id 29) is MM6's Enchant Item slot, which has no projectile object
    // at all - so an effect-keyed sprite would create no projectile here.
    EXPECT_NE(pObjectList->ObjectIDByItemID(SpellSpriteMapping[static_cast<SpellId>(30)]), 0u);
    EXPECT_EQ(pObjectList->ObjectIDByItemID(SpellSpriteMapping[SPELL_WATER_ACID_BURST]), 0u);
}

GAME_TEST(Mm6, CastUniqueSpellAnalog) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    engine->config->debug.AllMagic.setValue(true);

    // MM6 spell id 8 is Fire Blast, a fireball-like projectile with no same-id MM7 equivalent - MM7's id 8 is
    // Immolation, a self-only party buff that launches no projectile. translateForCast maps the MM6 spell to
    // MM7's Fireball projectile effect. Without it, casting id 8 would just apply the Immolation buff and
    // create nothing to observe.
    pushSpellOrRangedAttack(static_cast<SpellId>(8), 0, CombinedSkillValue::none(), 0, 1);
    game.tick(1);

    // A projectile carrying the native MM6 spell id 8 must have been launched (a fire projectile effect ran).
    int projectiles = std::ranges::count_if(pSpriteObjects, [](const SpriteObject &obj) {
        return obj.uSpellID == static_cast<SpellId>(8) && obj.uObjectDescID != 0;
    });
    EXPECT_GE(projectiles, 1);
}

// MM6 monster spell attacks come from MM6's monsters.txt, and the spell that sits at a given id differs from
// MM7's. ParseSpellType resolves each cell the way MM6.EXE does - by matching its FIRST WORD against a
// hardcoded keyword table - into a NATIVE MM6 SpellId; castSpell()'s translateForCast then maps that id to the
// matching MM7 effect. Before this wiring the names were matched against MM7's hardcoded name map, resolving
// them to the wrong id, or (for MM6-unique names) to SPELL_NONE with an "Unknown monster spell" warning.
GAME_TEST(Mm6, MonsterSpellNamesResolve) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // Goblin C (monster id 78) casts "Fire Bolt". In MM6 "Fire Bolt" is native spell id 4 (spells.txt row 4),
    // whereas MM7's id 4 is Fire Aura - so the native id lands on SPELL_FIRE_FIRE_AURA's slot, NOT MM7's id-2
    // Fire Bolt that the old name map returned. translateForCast maps the native id to MM7's Fire Bolt effect.
    const MonsterInfo &goblin = pMonsterStats->infos[MonsterId(78)];
    EXPECT_GT(goblin.spell1UseChance, 0);
    EXPECT_EQ(goblin.spell1Id, SPELL_FIRE_FIRE_AURA); // Native MM6 id 4 = "Fire Bolt".
    EXPECT_TRUE(isRegularSpell(goblin.spell1Id));
    EXPECT_EQ(translateForCast(goblin.spell1Id, GAME_VERSION_MM6), SPELL_FIRE_FIRE_BOLT);

    // Ooze B (id 116) casts "Poison Spray", an MM6-only name absent from MM7's monster-spell map (it resolved
    // to SPELL_NONE plus a warning before the fix). MM6's "Poison Spray" is native id 26.
    const MonsterInfo &ooze = pMonsterStats->infos[MonsterId(116)];
    EXPECT_GT(ooze.spell1UseChance, 0);
    EXPECT_EQ(ooze.spell1Id, SPELL_WATER_ICE_BOLT); // Native MM6 id 26 = "Poison Spray".
    EXPECT_TRUE(isRegularSpell(ooze.spell1Id));

    // MM6's shipped monsters.txt carries two spell-name typos: "Dispell Magic" (doubled L) and "Psychic
    // Shockt". They still resolve in the original game, because MM6.EXE matches only the FIRST WORD of the
    // cell against a hardcoded keyword table - and "Dispell" and "Psychic" are the keywords. ParseSpellType
    // reproduces that table, so these four monsters keep the spell attack they'd otherwise have lost.
    // Maddening Eye (Beholder C), Lich (Lich A) and Greater Lich (Lich B) all ship "Dispell Magic,N,10".
    for (MonsterId id : {MonsterId(12), MonsterId(94), MonsterId(95)}) {
        const MonsterInfo &caster = pMonsterStats->infos[id];
        EXPECT_GT(caster.spell1UseChance, 0);
        EXPECT_EQ(caster.spell1Id, SPELL_LIGHT_DISPEL_MAGIC); // Native MM6 id 80 = "Dispel Magic".
    }
    const MonsterInfo &nobleTitan = pMonsterStats->infos[MonsterId(167)]; // Titan B, cell "Psychic Shockt,M,18".
    EXPECT_GT(nobleTitan.spell1UseChance, 0);
    EXPECT_EQ(nobleTitan.spell1Id, SPELL_MIND_PSYCHIC_SHOCK); // Native MM6 id 65 = "Psychic Shock".

    // Across the whole monster table, every one of MM6's 55 spell-casting monsters resolves to a real
    // regular spell - the keyword table covers every spell cell MM6 ships, typos included.
    int resolved = 0;
    int unresolved = 0;
    for (MonsterId id : pMonsterStats->infos.indices()) {
        const MonsterInfo &info = pMonsterStats->infos[id];
        if (info.spell1UseChance == 0)
            continue;
        if (isRegularSpell(info.spell1Id))
            resolved++;
        else
            unresolved++;
    }
    EXPECT_EQ(resolved, 55);
    EXPECT_EQ(unresolved, 0);
}

// Monsters don't cast through castSpell() - they use Actor::AI_SpellAttack, which switches on the spell id.
// Now that MM6 monster spells resolve to NATIVE MM6 ids (MonsterSpellNamesResolve), AI_SpellAttack has to
// translate that id to the matching MM7 effect for its switch, exactly like castSpell does - otherwise a
// native MM6 id with no MM7 case (e.g. Fire Bolt = native id 4 = SPELL_FIRE_FIRE_AURA's slot) hits the
// switch's default: assert(false) and aborts. Asset/data reads inside the cases stay native (MM6's
// projectile bank is native-indexed).
GAME_TEST(Mm6, MonsterCastsSpell) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // Goblinwatch is indoor, so the projectile's sector lookup is well-defined.
    MapId goblinwatch = pMapStats->GetMapInfo("d01.blv");
    ASSERT_NE(goblinwatch, MAP_INVALID);
    game.teleportTo(goblinwatch, Vec3f(-1850, 4304, -512), 0); // First spawn point of d01.blv.
    ASSERT_FALSE(pActors.empty());

    // Make an actor cast MM6 Goblin C's spell attack ("Fire Bolt" = native spell id 4, a projectile). Its
    // native id is SPELL_FIRE_FIRE_AURA's slot; translateForCast maps it to MM7's Fire Bolt effect.
    Actor &caster = pActors[0];
    caster.monsterInfo = pMonsterStats->infos[MonsterId(78)];
    SpellId nativeSpell = caster.monsterInfo.spell1Id;
    ASSERT_EQ(nativeSpell, SPELL_FIRE_FIRE_AURA); // MM6 native id 4 = "Fire Bolt".
    ASSERT_EQ(translateForCast(nativeSpell, GAME_VERSION_MM6), SPELL_FIRE_FIRE_BOLT);

    size_t spritesBefore = pSpriteObjects.size();

    AIDirection dir;
    dir.uDistance = 1500;
    dir.uDistanceXZ = 1500;
    // Before the AI_SpellAttack fix this aborts on the switch's default: assert(false); after it, the Fire
    // Bolt case launches a projectile carrying the NATIVE spell id with MM6's native-indexed projectile sprite.
    Actor::AI_SpellAttack(0, &dir, nativeSpell, ABILITY_SPELL1, caster.monsterInfo.spell1SkillMastery);

    int projectiles = std::ranges::count_if(pSpriteObjects, [nativeSpell](const SpriteObject &obj) {
        return obj.uSpellID == nativeSpell && obj.uObjectDescID != 0;
    });
    EXPECT_GE(projectiles, 1);
    EXPECT_GT(pSpriteObjects.size(), spritesBefore);
}

// A shifted MM6 damage spell must actually deal impact damage. Two layers are needed and both are exercised
// here: (1) processSpellImpact dispatches on the EFFECT's sprite (the native-slot sprite of e.g. Fire Bolt is
// Fire Aura's, which has no impact case -> the projectile hit the damage-less default), and (2) CalcSpellDamage
// resolves the magnitude via the effect spell (the native-slot data is a 0-damage buff). Missing either leaves
// MM6 Fire Bolt at 0 damage. This drives a player cast into a monster; the same CalcSpellDamage chokepoint and
// processSpellImpact serve the monster->party direction (see Mm6.MonsterCastsSpell for the monster projectile).
GAME_TEST(Mm6, ShiftedSpellDealsImpactDamage) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    engine->config->debug.AllMagic.setValue(true); // Casts always land - no mana/skill gating.

    MapId goblinwatch = pMapStats->GetMapInfo("d01.blv");
    ASSERT_NE(goblinwatch, MAP_INVALID);
    game.teleportTo(goblinwatch, Vec3f(-1850, 4304, -512), 0); // Indoor, so the projectile's sector is defined.
    game.tick(1);

    int monId = -1;
    for (size_t i = 0; i < pActors.size(); i++) {
        if (pActors[i].hp > 0) {
            monId = static_cast<int>(i);
            break;
        }
    }
    ASSERT_NE(monId, -1);

    // Quick-cast a native MM6 fire projectile, then impact it straight onto the monster; return HP lost.
    auto castImpactDamage = [&](int nativeId) -> int {
        pActors[monId].hp = 500;
        pActors[monId].monsterInfo.resFire = 0; // Deterministic: the found monster must not resist the damage.
        pushSpellOrRangedAttack(static_cast<SpellId>(nativeId), 0, CombinedSkillValue::none(), 0, 1);
        game.tick(1);
        int proj = -1;
        for (size_t j = 0; j < pSpriteObjects.size(); j++)
            if (pSpriteObjects[j].uSpellID == static_cast<SpellId>(nativeId))
                proj = static_cast<int>(j);
        EXPECT_NE(proj, -1) << "native spell " << nativeId << " formed no projectile";
        if (proj == -1)
            return 0;
        // The projectile may have clipped scenery during its creating tick; restore its fresh pre-impact native
        // sprite so the impact runs cleanly on the monster (updateSpriteOnImpact asserts a projectile sprite).
        SpriteObject &p = pSpriteObjects[proj];
        p.uSpellID = static_cast<SpellId>(nativeId);
        p.spriteId = SpellSpriteMapping[static_cast<SpellId>(nativeId)];
        p.uObjectDescID = pObjectList->ObjectIDByItemID(p.spriteId);
        pActors[monId].hp = 500;
        processSpellImpact(proj, Pid(OBJECT_Actor, monId));
        return 500 - pActors[monId].hp;
    };

    // Shifted: MM6 Fire Bolt (native id 4) - the most common monster attack spell. 0 before either layer.
    EXPECT_GT(castImpactDamage(4), 0);

    // Aligned: MM6 Flame Arrow (native id 2) already sits on the Fire Bolt sprite/data - damage must still
    // land, proving the central CalcSpellDamage remap didn't break the aligned (identity) path.
    EXPECT_GT(castImpactDamage(2), 0);
}

// AI_SpellAttack's switch only had cases for the effects MM7 monsters cast. MM6 monsters cast more: some are
// damage projectiles now added to the launch group (Ooze Poison Spray, Druidess Deadly Swarm, Cleric Flying
// Fist), while a few (Minotaur's Finger of Death -> Souldrinker, and the two data-typo SPELL_NONE spells) have
// no case and must no-op gracefully instead of hitting the switch's default: assert(false).
GAME_TEST(Mm6, MonsterCastsUncoveredSpell) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    MapId goblinwatch = pMapStats->GetMapInfo("d01.blv");
    ASSERT_NE(goblinwatch, MAP_INVALID);
    game.teleportTo(goblinwatch, Vec3f(-1850, 4304, -512), 0);
    ASSERT_FALSE(pActors.empty());

    Actor &caster = pActors[0];
    AIDirection dir;
    dir.uDistance = 1500;
    dir.uDistanceXZ = 1500;

    // Added case: Ooze B (id 116) casts "Poison Spray" (native id 26 -> effect Poison Spray), now a
    // projectile-launch case, so a projectile carrying the native id is fired.
    caster.monsterInfo = pMonsterStats->infos[MonsterId(116)];
    SpellId oozeSpell = caster.monsterInfo.spell1Id;
    ASSERT_EQ(translateForCast(oozeSpell, GAME_VERSION_MM6), SPELL_WATER_POISON_SPRAY);
    Actor::AI_SpellAttack(0, &dir, oozeSpell, ABILITY_SPELL1, caster.monsterInfo.spell1SkillMastery);
    int poisonProjectiles = std::ranges::count_if(pSpriteObjects, [oozeSpell](const SpriteObject &obj) {
        return obj.uSpellID == oozeSpell && obj.uObjectDescID != 0;
    });
    EXPECT_GE(poisonProjectiles, 1);

    // Still-uncovered effect: a monster whose spell cell doesn't resolve keeps SPELL_NONE, which has no
    // AI_SpellAttack case - the MM6-gated default must no-op it: no abort, no projectile. Reaching the assert
    // below at all means it did not abort on the switch's default: assert(false). MM6's shipped data no longer
    // produces such a monster (ParseSpellType reproduces MM6.EXE's first-word keyword table, which resolves
    // even the "Dispell Magic" / "Psychic Shockt" typos), so drive the default with SPELL_NONE directly.
    // (Finger of Death used to sit here; it is now a real monster cast - see Mm6.MonsterCastsFingerOfDeath.)
    ASSERT_FALSE(isRegularSpell(SPELL_NONE));
    size_t spritesBefore = pSpriteObjects.size();
    Actor::AI_SpellAttack(0, &dir, SPELL_NONE, ABILITY_SPELL1, caster.monsterInfo.spell1SkillMastery);
    EXPECT_EQ(pSpriteObjects.size(), spritesBefore);
}

// MM6 Finger of Death is also a monster spell (Minotaur C among others). Monsters cast through AI_SpellAttack,
// whose switch now handles Souldrinker (Finger of Death's effect id) for MM6: the monster tries to slay one
// party member outright, 3/4/5% per point of skill. It used to be a documented no-op (see the earlier form of
// Mm6.MonsterCastsUncoveredSpell). No MM7 monster casts Souldrinker, so the case is MM6-only.
GAME_TEST(Mm6, MonsterCastsFingerOfDeath) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    MapId goblinwatch = pMapStats->GetMapInfo("d01.blv");
    ASSERT_NE(goblinwatch, MAP_INVALID);
    game.teleportTo(goblinwatch, Vec3f(-1850, 4304, -512), 0);
    ASSERT_FALSE(pActors.empty());

    for (const Character &character : pParty->pCharacters)
        ASSERT_FALSE(character.conditions.has(CONDITION_DEAD)); // All four start alive.

    Actor &caster = pActors[0];
    caster.monsterInfo = pMonsterStats->infos[MonsterId(108)]; // Minotaur C casts Finger of Death.
    SpellId fingerSpell = caster.monsterInfo.spell1Id;
    ASSERT_EQ(translateForCast(fingerSpell, GAME_VERSION_MM6), SPELL_DARK_SOULDRINKER);

    AIDirection dir;
    dir.uDistance = 1500;
    dir.uDistanceXZ = 1500;
    // Skill 20 Master -> 5% * 20 = 100% success, so exactly one party member is slain.
    Actor::AI_SpellAttack(0, &dir, fingerSpell, ABILITY_SPELL1, CombinedSkillValue(20, MASTERY_MASTER));

    int dead = 0;
    for (const Character &character : pParty->pCharacters)
        if (character.conditions.has(CONDITION_DEAD))
            dead++;
    EXPECT_EQ(dead, 1);
}

// MM6 Golden Touch (native id 79) converts a chosen inventory item into gold. It has no MM7 counterpart -
// translateForCast maps it onto Dispel Magic, which would dispel every creature in sight instead. It is an
// inventory-target spell (like Enchant Item): the MM6 targeting override routes it into the item picker, and
// castMm6UniqueSpell runs the conversion once an item is chosen.
GAME_TEST(Mm6, GoldenTouch) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);
    pParty->SetGold(0);

    // Put a plain, valuable item in character 0's backpack to convert.
    Character &caster = pParty->pCharacters[0];
    InventoryEntry item = caster.inventory.add(Item(ItemId(84)));
    ASSERT_TRUE(item);
    int value = item->GetValue();
    ASSERT_GT(value, 0);
    int itemIndex = item.index();

    // Casting non-quick (overrideSoundId 0) must enter the inventory item picker, proving the MM6 targeting
    // override routed Golden Touch to item targeting rather than Dispel's targetless mass cast.
    pushSpellOrRangedAttack(static_cast<SpellId>(79), 0, CombinedSkillValue(10, MASTERY_MASTER), 0, 0);
    game.tick(1);
    ASSERT_TRUE(IsEnchantingInProgress);

    // Mirror the inventory-item click (UICharacter.cpp): hand the queued cast its item target and clear the
    // enchant-in-progress state so castSpell runs it next tick.
    CastSpellInfo *info = pGUIWindow_CastTargetedSpell->spellInfo();
    info->flags &= ~ON_CAST_TargetedEnchantment;
    info->targetCharacterIndex = 0;
    info->targetInventoryIndex = itemIndex;
    IsEnchantingInProgress = false;
    game.tick(1);

    // Skill 10 -> 100% success; Master -> 80% of the item's value in gold, and the item is consumed.
    EXPECT_EQ(pParty->GetGold(), value * 80 / 100);
    EXPECT_FALSE(caster.inventory.entry(itemIndex));
}

// MM6 Enchant Item (native id 29) is castable from Novice up - spells.txt row 29 tiers read "Weak
// enchantments only" / "Stronger enchantments" / "Allows enchantment of weapons", and the spellbook learn
// gate accordingly admits it at Expert. But it translates onto MM7's SPELL_WATER_ENCHANT_ITEM, a
// Master-only spell whose handler assert(false)ed on Novice/Expert casts - so a legitimate MM6 Expert
// cast aborted a Debug build (and burned mana on a guaranteed "Spell failed" in Release). The MM6 gates
// never caught it because debug.AllMagic forces Grandmaster mastery.
GAME_TEST(Mm6, EnchantItemTiers) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    Character &caster = pParty->pCharacters[0];
    caster.mana = 500;

    // Pick items from MM6's own table: passive equipment worth >= 450 (below that the low-value rule
    // breaks it) and a weapon worth >= 250 for the Master weapon tier.
    ItemId armorId = ITEM_NULL, armorId2 = ITEM_NULL, weaponId = ITEM_NULL;
    for (ItemId id : pItemTable->items.indices()) {
        if (!isRegular(id))
            continue;
        Item item(id);
        if (isPassiveEquipment(item.type()) && item.GetValue() >= 450) {
            if (armorId == ITEM_NULL)
                armorId = id;
            else if (armorId2 == ITEM_NULL)
                armorId2 = id;
        }
        if (weaponId == ITEM_NULL && isWeapon(item.type()) && item.GetValue() >= 250)
            weaponId = id;
    }
    ASSERT_NE(armorId, ITEM_NULL);
    ASSERT_NE(armorId2, ITEM_NULL);
    ASSERT_NE(weaponId, ITEM_NULL);

    // Casts Enchant Item (native id 29) on the given inventory slot, mirroring the item-picker click
    // exactly like the GoldenTouch test above.
    auto castEnchantOn = [&](int itemIndex, Mastery mastery) {
        caster.timeToRecovery = 0_ticks; // Clear the previous cast's recovery so the next cast goes through.
        caster.mana = 500;
        pushSpellOrRangedAttack(static_cast<SpellId>(29), 0, CombinedSkillValue(10, mastery), 0, 0);
        game.tick(1);
        ASSERT_TRUE(IsEnchantingInProgress) << "mastery " << std::to_underlying(mastery);
        CastSpellInfo *info = pGUIWindow_CastTargetedSpell->spellInfo();
        info->flags &= ~ON_CAST_TargetedEnchantment;
        info->targetCharacterIndex = 0;
        info->targetInventoryIndex = itemIndex;
        IsEnchantingInProgress = false;
        // The real click also schedules this Escape, which closes the inventory screen and releases the
        // targeted-spell window - without it the stale window blocks the next cast's item picker. A
        // 1-tick timeout replaces the interactive 1-second one.
        AfterEnchClickEventId = UIMSG_Escape;
        AfterEnchClickEventSecondParam = 0;
        AfterEnchClickEventTimeout = Duration::fromTicks(1);
        game.tick(3);
    };

    // Expert on plain armor: skill 10 -> the success roll can't miss, so the item must come out
    // enchanted (standard or special, depending on the roll) and unbroken. Aborted via assert(false)
    // before the fix.
    InventoryEntry armor = caster.inventory.add(Item(armorId));
    ASSERT_TRUE(armor);
    castEnchantOn(armor.index(), MASTERY_EXPERT);
    EXPECT_TRUE(armor->standardEnchantment || armor->specialEnchantment != ITEM_ENCHANTMENT_NULL);
    EXPECT_FALSE(armor->IsBroken());

    // Novice on plain armor: also a legal MM6 cast ("Weak enchantments only"), also enchants.
    InventoryEntry armor2 = caster.inventory.add(Item(armorId2));
    ASSERT_TRUE(armor2);
    castEnchantOn(armor2.index(), MASTERY_NOVICE);
    EXPECT_TRUE(armor2->standardEnchantment || armor2->specialEnchantment != ITEM_ENCHANTMENT_NULL);
    EXPECT_FALSE(armor2->IsBroken());

    // Expert on a weapon: weapons need Master ("Allows enchantment of weapons"), so the cast fails
    // cleanly - no enchantment, and the item is NOT broken.
    InventoryEntry weapon = caster.inventory.add(Item(weaponId));
    ASSERT_TRUE(weapon);
    castEnchantOn(weapon.index(), MASTERY_EXPERT);
    EXPECT_FALSE(weapon->standardEnchantment.has_value());
    EXPECT_EQ(weapon->specialEnchantment, ITEM_ENCHANTMENT_NULL);
    EXPECT_FALSE(weapon->IsBroken());

    // Master on the same weapon: now allowed, and weapons always roll a special enchantment.
    castEnchantOn(weapon.index(), MASTERY_MASTER);
    EXPECT_NE(weapon->specialEnchantment, ITEM_ENCHANTMENT_NULL);
    EXPECT_FALSE(weapon->IsBroken());
}

GAME_TEST(Mm6, SpellManaCosts) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // pSpellDatas is statically initialized with MM7 numbers; applyMm6SpellDatas() must have replaced the
    // mana costs and recovery times with the MM6 values extracted from MM6.EXE's SpellInfo table (VA
    // 0x4BDD70). These native ids assert values that DIFFER from MM7, proving the MM6 table was applied.
    // Cross-validated against tartarus.rpgclassics.com/mm6 and the MM7-vs-EXE decode. In MM6, unlike MM7,
    // mana can drop with mastery and Light/Dark magic is far more expensive.

    // Native id 2 = MM6 "Flame Arrow": mana 2/1/0 across Novice/Expert/Master (MM7 Fire Bolt is a flat 2).
    EXPECT_EQ(pSpellDatas[SPELL_FIRE_FIRE_BOLT].mana_per_skill[MASTERY_NOVICE], 2);
    EXPECT_EQ(pSpellDatas[SPELL_FIRE_FIRE_BOLT].mana_per_skill[MASTERY_EXPERT], 1);
    EXPECT_EQ(pSpellDatas[SPELL_FIRE_FIRE_BOLT].mana_per_skill[MASTERY_MASTER], 0);
    // MM6 recovery for that spell is 100/90/80 (MM7 is 110/110/100).
    EXPECT_EQ(pSpellDatas[SPELL_FIRE_FIRE_BOLT].recovery_per_skill[MASTERY_NOVICE], Duration::fromTicks(100));
    EXPECT_EQ(pSpellDatas[SPELL_FIRE_FIRE_BOLT].recovery_per_skill[MASTERY_MASTER], Duration::fromTicks(80));

    // Native id 78 = first Light spell: 20 mana in MM6 vs 5 in MM7, recovery 100 vs MM7's 110.
    EXPECT_EQ(pSpellDatas[SPELL_LIGHT_LIGHT_BOLT].mana_per_skill[MASTERY_NOVICE], 20);
    EXPECT_EQ(pSpellDatas[SPELL_LIGHT_LIGHT_BOLT].recovery_per_skill[MASTERY_NOVICE], Duration::fromTicks(100));

    // Native id 89 = first Dark spell: 20 mana in MM6 vs 10 in MM7.
    EXPECT_EQ(pSpellDatas[SPELL_DARK_REANIMATE].mana_per_skill[MASTERY_NOVICE], 20);

    // MM6 has no Grandmaster tier, so the GM slot must mirror the Master value.
    EXPECT_EQ(pSpellDatas[SPELL_DARK_SOULDRINKER].mana_per_skill[MASTERY_GRANDMASTER],
              pSpellDatas[SPELL_DARK_SOULDRINKER].mana_per_skill[MASTERY_MASTER]);
    EXPECT_EQ(pSpellDatas[SPELL_DARK_SOULDRINKER].mana_per_skill[MASTERY_NOVICE], 200); // MM7 is 60.
}

GAME_TEST(Mm6, SpellLearnMastery) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // MM6 caps mastery at Master (no Grandmaster tier). applyMm6SpellDatas() must clamp the min-mastery of
    // each school's 11th spell down from MM7's Grandmaster to Master; otherwise the spellbook learn gate
    // (Character.cpp, requiredMastery > val.mastery()) would leave those 9 spells permanently unlearnable.
    // Native id 11 = Incinerate, the 11th Fire spell (MASTERY_GRANDMASTER in MM7's pSpellDatas).
    EXPECT_EQ(pSpellDatas[SPELL_FIRE_INCINERATE].skillMastery, MASTERY_MASTER);
    EXPECT_EQ(pSpellDatas[SPELL_DARK_SOULDRINKER].skillMastery, MASTERY_MASTER); // native id 99, also GM in MM7.

    // Non-top spells keep their existing (sub-Grandmaster) tier - the clamp only touches Grandmaster rows.
    EXPECT_EQ(pSpellDatas[SPELL_FIRE_TORCH_LIGHT].skillMastery, MASTERY_NOVICE); // native id 1.
}

// MM6 Create Food (native id 78, the first Light spell) has no MM7 counterpart - translateForCast maps it onto
// First Aid, which would heal a targeted character instead of stocking the party's food. castMm6UniqueSpell
// runs the real effect: it fills the party's food up to 1 day + 1/2/3 days per 10 skill (Novice/Expert/Master),
// but only when the current supply is lower.
GAME_TEST(Mm6, CreateFood) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // Master, skill 10 -> 1 + 3 * (10 / 10) = 4 days of food. Quick-cast (nonzero overrideSoundId) so it casts
    // immediately with no target picker; the MM6 targeting override keeps this a party-wide, targetless cast.
    pParty->SetFood(0);
    pushSpellOrRangedAttack(static_cast<SpellId>(78), 0, CombinedSkillValue(10, MASTERY_MASTER), 0, 1);
    game.tick(1);
    EXPECT_EQ(pParty->GetFood(), 4);

    // Casting again with more food already on hand than the spell would create is a no-op - it fills up to the
    // amount, never adds on top.
    pParty->SetFood(20);
    pushSpellOrRangedAttack(static_cast<SpellId>(78), 0, CombinedSkillValue(10, MASTERY_MASTER), 0, 1);
    game.tick(1);
    EXPECT_EQ(pParty->GetFood(), 20);
}

// MM6 Finger of Death (native id 95) tries to instantly slay a single creature, 3/4/5% per point of skill at
// Novice/Expert/Master. translateForCast maps it onto Souldrinker (a viewport-wide life-drain AoE), so without
// castMm6UniqueSpell it would drain the whole room instead of gambling on one target. On success the target
// dies outright and the party is rewarded exactly like any other kill.
GAME_TEST(Mm6, FingerOfDeath) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    MapId goblinwatch = pMapStats->GetMapInfo("d01.blv");
    ASSERT_NE(goblinwatch, MAP_INVALID);
    game.teleportTo(goblinwatch, Vec3f(-1850, 4304, -512), 0); // First spawn point of d01.blv.
    game.tick(1);

    int monId = -1;
    for (size_t i = 0; i < pActors.size(); i++) {
        if (pActors[i].CanAct() && pActors[i].hp > 0 && pActors[i].monsterInfo.exp > 0) {
            monId = static_cast<int>(i);
            break;
        }
    }
    ASSERT_NE(monId, -1);

    int expBefore = 0;
    for (const Character &character : pParty->pCharacters)
        expBefore += character.experience;

    // Point at the target so castSpell picks it, then cast at skill 20 Master -> 5% * 20 = 100% success (a
    // guaranteed kill). Quick-cast so no targeting window opens; the mouse target is used directly.
    mouse->uPointingObjectID = Pid(OBJECT_Actor, monId);
    pushSpellOrRangedAttack(static_cast<SpellId>(95), 0, CombinedSkillValue(20, MASTERY_MASTER), 0, 1);
    game.tick(1);

    EXPECT_EQ(pActors[monId].aiState, Dying); // Slain outright.
    EXPECT_LE(pActors[monId].hp, 0);

    int expAfter = 0;
    for (const Character &character : pParty->pCharacters)
        expAfter += character.experience;
    EXPECT_GT(expAfter, expBefore); // The kill rewarded party experience.
}

// MM6's single-stat buff family - Lucky Day (48, Luck), Meditation (56, Intellect+Personality),
// Precision (59, Accuracy), Speed (73, Speed) and Power (75, Might+Endurance) - has no MM7 counterpart,
// so translateForCast runs each as an unrelated analog. castMm6UniqueSpell instead applies the real
// per-stat character buff: +(10 + 2/skill at Novice, 3/skill at Expert & Master) for one hour per skill
// point (MM6.EXE CastSpell dispatch 0x422C93; the duration lea-chain is a flat 3600*L game-ticks for
// every mastery, the same unit that makes Torch Light "1 hour per point of skill"). Novice/Expert buff a
// single chosen character; Master hits the whole party (spells.txt "Spell affects entire party").
GAME_TEST(Mm6, SingleStatBuffs) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // Precision at Novice, skill 10 -> +(2*10 + 10) = 30 Accuracy for 10 hours, on the picked character
    // (char 2) alone. Passing a CombinedSkillValue makes it a free, exact-mastery cast; the MM6 targeting
    // override opens the character picker below Master, driven here with spellTargetPicked.
    Character &c0 = pParty->pCharacters[0];
    Character &c2 = pParty->pCharacters[2];
    int accBefore = c2.GetActualAccuracy();
    Time castStart = pParty->GetPlayingTime();
    pushSpellOrRangedAttack(static_cast<SpellId>(59), 0, CombinedSkillValue(10, MASTERY_NOVICE), 0, 0);
    spellTargetPicked(Pid(), 2);
    game.tick(1);
    Time castEnd = pParty->GetPlayingTime();
    EXPECT_TRUE(c2.pCharacterBuffs[CHARACTER_BUFF_ACCURACY].Active());
    EXPECT_EQ(c2.pCharacterBuffs[CHARACTER_BUFF_ACCURACY].power, 30);
    EXPECT_GE(c2.pCharacterBuffs[CHARACTER_BUFF_ACCURACY].expireTime, castStart + Duration::fromHours(10));
    EXPECT_LE(c2.pCharacterBuffs[CHARACTER_BUFF_ACCURACY].expireTime, castEnd + Duration::fromHours(10));
    EXPECT_EQ(c2.GetActualAccuracy(), accBefore + 30); // The buff feeds the actual stat.
    EXPECT_FALSE(c0.pCharacterBuffs[CHARACTER_BUFF_ACCURACY].Active()); // Single target - char 0 untouched.

    // Expert bumps the per-skill bonus to 3: +(3*10 + 10) = 40, still single target.
    pushSpellOrRangedAttack(static_cast<SpellId>(59), 0, CombinedSkillValue(10, MASTERY_EXPERT), 0, 0);
    spellTargetPicked(Pid(), 2);
    game.tick(1);
    EXPECT_EQ(c2.pCharacterBuffs[CHARACTER_BUFF_ACCURACY].power, 40);
    EXPECT_FALSE(c0.pCharacterBuffs[CHARACTER_BUFF_ACCURACY].Active());

    // Master keeps the 3/skill bonus (40) but affects the entire party - no picker.
    pushSpellOrRangedAttack(static_cast<SpellId>(59), 0, CombinedSkillValue(10, MASTERY_MASTER), 0, 1);
    game.tick(1);
    for (Character &character : pParty->pCharacters)
        EXPECT_EQ(character.pCharacterBuffs[CHARACTER_BUFF_ACCURACY].power, 40);

    // Each family member buffs its own attribute(s). Cast every one at Master, skill 10 -> power 40; the
    // five spells touch disjoint stats, so all buffs coexist on the party.
    struct StatBuff { int id; std::vector<CharacterBuff> stats; };
    std::vector<StatBuff> family = {
        {48, {CHARACTER_BUFF_LUCK}},
        {56, {CHARACTER_BUFF_INTELLIGENCE, CHARACTER_BUFF_PERSONALITY}},
        {59, {CHARACTER_BUFF_ACCURACY}},
        {73, {CHARACTER_BUFF_SPEED}},
        {75, {CHARACTER_BUFF_STRENGTH, CHARACTER_BUFF_ENDURANCE}},
    };
    for (const StatBuff &spell : family) {
        pushSpellOrRangedAttack(static_cast<SpellId>(spell.id), 0, CombinedSkillValue(10, MASTERY_MASTER), 0, 1);
        game.tick(1);
    }
    for (Character &character : pParty->pCharacters)
        for (const StatBuff &spell : family)
            for (CharacterBuff buff : spell.stats)
                EXPECT_EQ(character.pCharacterBuffs[buff].power, 40)
                    << "spell " << spell.id << " buff " << std::to_underlying(buff);
}

// MM6 Day of the Gods (83) casts the whole single-stat buff family - Power, Meditation, Speed, Lucky Day
// and Precision - on the entire party at an effective strength of 2x/3x/4x Light skill (Novice/Expert/
// Master), i.e. +(mult*L + 10) to each of the seven attributes for mult*L hours (MM6.EXE 0x428A43). It maps
// onto MM7's Day of the Gods, whose case asserts(false) on Novice (MM7 has no Novice Day of the Gods) - so
// without the MM6 handling a Novice cast aborts. (MM6 also folds in Guardian Angel; no OpenEnroth buff yet.)
GAME_TEST(Mm6, DayOfTheGods) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    const std::array<CharacterBuff, 7> stats = {
        CHARACTER_BUFF_STRENGTH, CHARACTER_BUFF_ENDURANCE, CHARACTER_BUFF_INTELLIGENCE,
        CHARACTER_BUFF_PERSONALITY, CHARACTER_BUFF_ACCURACY, CHARACTER_BUFF_SPEED, CHARACTER_BUFF_LUCK};

    // (mastery, effective multiplier). Novice does NOT abort - that is the bug this fixes.
    struct Tier { Mastery mastery; int mult; };
    for (const Tier &tier : {Tier{MASTERY_NOVICE, 2}, Tier{MASTERY_EXPERT, 3}, Tier{MASTERY_MASTER, 4}}) {
        int expectedPower = tier.mult * 10 + 10;
        Duration expectedDuration = Duration::fromHours(tier.mult * 10);
        Time castStart = pParty->GetPlayingTime();
        pushSpellOrRangedAttack(static_cast<SpellId>(83), 0, CombinedSkillValue(10, tier.mastery), 0, 1);
        game.tick(1);
        Time castEnd = pParty->GetPlayingTime();
        for (Character &character : pParty->pCharacters) {
            for (CharacterBuff buff : stats) {
                EXPECT_TRUE(character.pCharacterBuffs[buff].Active());
                EXPECT_EQ(character.pCharacterBuffs[buff].power, expectedPower)
                    << "mastery " << std::to_underlying(tier.mastery) << " buff " << std::to_underlying(buff);
                EXPECT_GE(character.pCharacterBuffs[buff].expireTime, castStart + expectedDuration);
                EXPECT_LE(character.pCharacterBuffs[buff].expireTime, castEnd + expectedDuration);
            }
        }
    }
}

// MM6 Day of Protection (94) casts the protection family - Protection from Fire, Cold, Electricity,
// Poison and Magic, plus Feather Fall and Wizard Eye - on the party at 2x/3x/4x Dark skill power for
// Novice/Expert/Master, lasting skill+4 hours (MM6.EXE 0x4295fe). It translates onto MM7's Day of
// Protection, whose effect is Master-only - a legal MM6 Novice/Expert cast hit its assert(false) (and
// silently got the Master 4x numbers in Release) - and whose buff set is MM7-shaped (Mind/Earth
// resistances instead of Protection from Magic).
GAME_TEST(Mm6, DayOfProtectionTiers) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    const std::array<PartyBuff, 7> buffs = {
        PARTY_BUFF_RESIST_FIRE, PARTY_BUFF_RESIST_WATER, PARTY_BUFF_RESIST_AIR, PARTY_BUFF_RESIST_BODY,
        PARTY_BUFF_PROTECTION_FROM_MAGIC, PARTY_BUFF_FEATHER_FALL, PARTY_BUFF_WIZARD_EYE};

    // (mastery, power multiplier). Novice/Expert do NOT abort - that is the bug this fixes. Duration is
    // skill+4 hours at every mastery.
    struct Tier { Mastery mastery; int mult; };
    for (const Tier &tier : {Tier{MASTERY_NOVICE, 2}, Tier{MASTERY_EXPERT, 3}, Tier{MASTERY_MASTER, 4}}) {
        for (SpellBuff &buff : pParty->pPartyBuffs)
            buff.Reset();
        Duration expectedDuration = Duration::fromHours(10 + 4);
        Time castStart = pParty->GetPlayingTime();
        pushSpellOrRangedAttack(static_cast<SpellId>(94), 0, CombinedSkillValue(10, tier.mastery), 0, 1);
        game.tick(1);
        Time castEnd = pParty->GetPlayingTime();
        for (PartyBuff buff : buffs) {
            EXPECT_TRUE(pParty->pPartyBuffs[buff].Active())
                << "mastery " << std::to_underlying(tier.mastery) << " buff " << std::to_underlying(buff);
            EXPECT_EQ(pParty->pPartyBuffs[buff].power, tier.mult * 10)
                << "mastery " << std::to_underlying(tier.mastery) << " buff " << std::to_underlying(buff);
            EXPECT_GE(pParty->pPartyBuffs[buff].expireTime, castStart + expectedDuration);
            EXPECT_LE(pParty->pPartyBuffs[buff].expireTime, castEnd + expectedDuration);
        }
        // MM6 has no Mind/Earth protection spells - the MM7 effect's set must not leak through.
        EXPECT_FALSE(pParty->pPartyBuffs[PARTY_BUFF_RESIST_MIND].Active());
        EXPECT_FALSE(pParty->pPartyBuffs[PARTY_BUFF_RESIST_EARTH].Active());
    }
}

// MM6 computes spell damage with its own per-spell formulas (MM6.EXE CalcSpellDamage @0x47F0A0), keyed on the
// NATIVE spell id and independent of mastery. CalcSpellDamage must reproduce them exactly instead of borrowing
// the MM7 effect spell's numbers through the translateForCast remap. Cross-checked against MM6's spells.txt
// descriptions (which agree with the EXE everywhere except Acid Burst, see below).
GAME_TEST(Mm6, SpellDamageNumbers) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // Deterministic "base + 1 per point of skill" spells at skill 10. All of these differ from the MM7 effect
    // spell's dice (e.g. Ring of Fire rides Inferno's effect, which rolls 12 + skill x d1 = 22, not 16), and
    // MM6 damage never varies with mastery.
    for (Mastery mastery : {MASTERY_NOVICE, MASTERY_EXPERT, MASTERY_MASTER}) {
        EXPECT_EQ(CalcSpellDamage(static_cast<SpellId>(7), 10, mastery, 0), 16);   // Ring of Fire: 6 + skill.
        EXPECT_EQ(CalcSpellDamage(static_cast<SpellId>(9), 10, mastery, 0), 18);   // Meteor Shower: 8 + skill.
        EXPECT_EQ(CalcSpellDamage(static_cast<SpellId>(10), 10, mastery, 0), 22);  // Inferno: 12 + skill.
        EXPECT_EQ(CalcSpellDamage(static_cast<SpellId>(15), 10, mastery, 0), 12);  // Sparks: 2 + skill.
        EXPECT_EQ(CalcSpellDamage(static_cast<SpellId>(22), 10, mastery, 0), 30);  // Starburst: 20 + skill.
        EXPECT_EQ(CalcSpellDamage(static_cast<SpellId>(43), 10, mastery, 0), 30);  // Death Blossom: 20 + skill.
        EXPECT_EQ(CalcSpellDamage(static_cast<SpellId>(84), 10, mastery, 0), 35);  // Prismatic Light: 25 + skill.
        EXPECT_EQ(CalcSpellDamage(static_cast<SpellId>(99), 10, mastery, 0), 60);  // Dark Containment: 50 + skill.
    }

    // Flat-dice spells ignore skill entirely - even at skill 30 they stay within their fixed range.
    for (int i = 0; i < 64; i++) {
        int flameArrow = CalcSpellDamage(static_cast<SpellId>(2), 30, MASTERY_MASTER, 0);   // 1d8.
        EXPECT_GE(flameArrow, 1);
        EXPECT_LE(flameArrow, 8);
        int staticCharge = CalcSpellDamage(static_cast<SpellId>(13), 30, MASTERY_MASTER, 0); // 2-6.
        EXPECT_GE(staticCharge, 2);
        EXPECT_LE(staticCharge, 6);
        int coldBeam = CalcSpellDamage(static_cast<SpellId>(24), 30, MASTERY_MASTER, 0);     // 2d3 = 2-6.
        EXPECT_GE(coldBeam, 2);
        EXPECT_LE(coldBeam, 6);
        int magicArrow = CalcSpellDamage(static_cast<SpellId>(35), 30, MASTERY_MASTER, 0);   // 3-8.
        EXPECT_GE(magicArrow, 3);
        EXPECT_LE(magicArrow, 8);
        int spiritArrow = CalcSpellDamage(static_cast<SpellId>(45), 30, MASTERY_MASTER, 0);  // 1d6.
        EXPECT_GE(spiritArrow, 1);
        EXPECT_LE(spiritArrow, 6);
    }

    // Per-skill dice: Fire Bolt (native id 4) rolls skill x d4, so [10, 40] at skill 10 - the current MM7
    // remap can't produce values this low once skill dice differ.
    for (int i = 0; i < 64; i++) {
        int fireBolt = CalcSpellDamage(static_cast<SpellId>(4), 10, MASTERY_NOVICE, 0);
        EXPECT_GE(fireBolt, 10);
        EXPECT_LE(fireBolt, 40);
        int sunRay = CalcSpellDamage(static_cast<SpellId>(87), 10, MASTERY_NOVICE, 0);       // 20 + skill x d20.
        EXPECT_GE(sunRay, 30);
        EXPECT_LE(sunRay, 220);
    }

    // Acid Burst (native id 30): the EXE rolls 9 + skill x (0..8) - a 0-based die, unlike the "9 plus 1-9 per
    // point of skill" its description claims. At skill 1 the range is [9, 17] and the 0 face makes min == 9
    // (an MM7-style 9 + 1d9 would bottom out at 10); over 256 rolls P(no 0 seen) = (8/9)^256 ~ 8e-14.
    int acidMin = 1000, acidMax = 0;
    for (int i = 0; i < 256; i++) {
        int acid = CalcSpellDamage(static_cast<SpellId>(30), 1, MASTERY_NOVICE, 0);
        acidMin = std::min(acidMin, acid);
        acidMax = std::max(acidMax, acid);
    }
    EXPECT_EQ(acidMin, 9);
    EXPECT_LE(acidMax, 17);

    // Mass Distortion (native id 44): 25% of the target's current HP plus 2% per point of skill, deterministic.
    EXPECT_EQ(CalcSpellDamage(static_cast<SpellId>(44), 10, MASTERY_NOVICE, 1000), 450);

    // Non-damage spells stay at zero (native id 5 = MM6 Haste; the EXE default case returns 0).
    EXPECT_EQ(CalcSpellDamage(static_cast<SpellId>(5), 10, MASTERY_MASTER, 0), 0);
}

// MM6 heal spells cure their native spells.txt amounts, not their MM7 effect analogs': Healing Touch (47)
// heals a random 3-7/5-9/7-11 at Novice/Expert/Master, First Aid (68) a flat 5/7/10, Cure Wounds (71)
// 5 plus 2 per point of skill, Power Cure (77) 10 plus 2 per point of skill on every character, and
// Shared Life (54) adds 1/2/3 points per point of skill to the pooled party health. The mechanics
// (targeting, fx, the Shared Life redistribution) stay the shared engine paths - only the amounts differ.
GAME_TEST(Mm6, SpellHealNumbers) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    Character &c2 = pParty->pCharacters[2];

    // Healing Touch (47): a random 3-7/5-9/7-11 by mastery, independent of skill. Pin the exact dice by
    // sampling - 48 casts miss a face of a 5-face die with P ~ 1e-4, and the game RNG is deterministic
    // anyway. The MM7 analog (First Aid) would heal 2*10 + 5 = 25 at Novice.
    struct HealRange { Mastery mastery; int lo; int hi; };
    for (const HealRange &tier : {HealRange{MASTERY_NOVICE, 3, 7}, HealRange{MASTERY_EXPERT, 5, 9},
                                  HealRange{MASTERY_MASTER, 7, 11}}) {
        int healMin = 1000, healMax = 0;
        for (int i = 0; i < 48; i++) {
            c2.health = 1;
            pushSpellOrRangedAttack(static_cast<SpellId>(47), 0, CombinedSkillValue(10, tier.mastery), 0, 0);
            spellTargetPicked(Pid(), 2);
            game.tick(1);
            healMin = std::min(healMin, c2.health - 1);
            healMax = std::max(healMax, c2.health - 1);
        }
        EXPECT_EQ(healMin, tier.lo) << "mastery " << std::to_underlying(tier.mastery);
        EXPECT_EQ(healMax, tier.hi) << "mastery " << std::to_underlying(tier.mastery);
    }

    // First Aid (68): a flat 5/7/10 by mastery, independent of skill (MM7's own First Aid at this id
    // scales with skill - 2/3/4 * L + 5).
    struct FlatHeal { Mastery mastery; int amount; };
    for (const FlatHeal &tier : {FlatHeal{MASTERY_NOVICE, 5}, FlatHeal{MASTERY_EXPERT, 7},
                                 FlatHeal{MASTERY_MASTER, 10}}) {
        c2.health = 1;
        pushSpellOrRangedAttack(static_cast<SpellId>(68), 0, CombinedSkillValue(10, tier.mastery), 0, 0);
        spellTargetPicked(Pid(), 2);
        game.tick(1);
        EXPECT_EQ(c2.health, 1 + tier.amount) << "mastery " << std::to_underlying(tier.mastery);
    }

    // Cure Wounds (71): 5 + 2 per point of skill at every mastery. (At Novice this coincides with the
    // MM7 analog's 2L+5 - Expert and Master are what the analog would get wrong: 26/33 at skill 7.)
    for (Mastery mastery : {MASTERY_NOVICE, MASTERY_EXPERT, MASTERY_MASTER}) {
        c2.health = 1;
        pushSpellOrRangedAttack(static_cast<SpellId>(71), 0, CombinedSkillValue(7, mastery), 0, 0);
        spellTargetPicked(Pid(), 2);
        game.tick(1);
        EXPECT_EQ(c2.health, 1 + 5 + 2 * 7) << "mastery " << std::to_underlying(mastery);
    }

    // Power Cure (77): 10 + 2 per point of skill to every character, at every mastery (the MM7 spell
    // heals 5L + 10 = 35 at skill 5, capping several starting characters at max health).
    for (Mastery mastery : {MASTERY_NOVICE, MASTERY_EXPERT, MASTERY_MASTER}) {
        for (Character &character : pParty->pCharacters)
            character.health = 1;
        pushSpellOrRangedAttack(static_cast<SpellId>(77), 0, CombinedSkillValue(5, mastery), 0, 1);
        game.tick(1);
        for (Character &character : pParty->pCharacters)
            EXPECT_EQ(character.health, 1 + 10 + 2 * 5) << "mastery " << std::to_underlying(mastery);
    }

    // Shared Life (54): pools current party health plus 1/2/3 points per point of skill at N/E/M and
    // redistributes it evenly (MM7 adds a flat 3L below Grandmaster, so Novice and Expert differ).
    struct PoolAdd { Mastery mastery; int add; };
    for (const PoolAdd &tier : {PoolAdd{MASTERY_NOVICE, 10}, PoolAdd{MASTERY_EXPERT, 20}}) {
        for (Character &character : pParty->pCharacters)
            character.health = 10;
        pushSpellOrRangedAttack(static_cast<SpellId>(54), 0, CombinedSkillValue(10, tier.mastery), 0, 1);
        game.tick(1);
        for (Character &character : pParty->pCharacters)
            EXPECT_EQ(character.health, (4 * 10 + tier.add) / 4) << "mastery " << std::to_underlying(tier.mastery);
    }
}

// MM6 Mass Curse (native id 91, the third Dark spell) has no MM7 counterpart, so translateForCast runs it as
// Toxic Cloud - a poison AoE. Its real effect (spells.txt: "Inflicts the cursed condition on all monsters in
// the sight of the caster"; MM6.EXE 0x42928b) is to curse every monster in the caster's line of sight for
// 2/3/4 minutes per point of skill at Novice/Expert/Master. A cursed monster misses every attack
// (Actor::ActorHitOrMiss returns false) until the curse expires. The state is a transient
// Actor::cursedExpireTime, not a persisted buff - MM7 never curses monsters, so the hook is a no-op there.
GAME_TEST(Mm6, MassCurse) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    MapId goblinwatch = pMapStats->GetMapInfo("d01.blv");
    ASSERT_NE(goblinwatch, MAP_INVALID);
    game.teleportTo(goblinwatch, Vec3f(-1850, 4304, -512), 0); // First spawn point of d01.blv, a goblin in view.
    game.tick(1);

    // Mass Curse only reaches monsters in the caster's line of sight, so work with whatever is actually in the
    // viewport rather than teleporting a monster around (indoor rendering culls by sector). Respawn placement
    // is RNG-dependent, so the spawn view can start empty - but with MM6 hostility live the dungeon's monsters
    // pursue the party on sight, so ticking a little always brings some into view.
    std::vector<Actor *> inView = render->getActorsInViewport(4096);
    for (int i = 0; i < 300 && inView.empty(); i++) {
        game.tick(1);
        inView = render->getActorsInViewport(4096);
    }
    ASSERT_FALSE(inView.empty());
    Actor *mon = inView[0];
    Character &target = pParty->pCharacters[0];

    // Effect hook: a cursed monster has a flat 50% chance to miss each attack (MM6.EXE 0x431c48 -
    // rand() % 100 < 50 -> miss), so over many attempts it lands roughly HALF the hits of an un-cursed
    // one - but it still lands some (it is not an auto-miss).
    mon->cursedExpireTime = Time();
    int hitsWhenUncursed = 0;
    for (int i = 0; i < 500; i++)
        if (mon->ActorHitOrMiss(&target))
            hitsWhenUncursed++;
    EXPECT_GT(hitsWhenUncursed, 0);

    mon->cursedExpireTime = pParty->GetPlayingTime() + Duration::fromMinutes(10);
    int hitsWhenCursed = 0;
    for (int i = 0; i < 500; i++)
        if (mon->ActorHitOrMiss(&target))
            hitsWhenCursed++;
    EXPECT_GT(hitsWhenCursed, 0);
    EXPECT_GT(hitsWhenCursed, hitsWhenUncursed / 5);      // True ratio is 1/2; these bounds are ~5 sigma.
    EXPECT_LT(hitsWhenCursed, hitsWhenUncursed * 4 / 5);

    mon->cursedExpireTime = Time(); // Clear before exercising the real cast.

    // The saving throw (MM6.EXE 0x421e90, damage type 1 = magic): level 0 / res 0 always sticks
    // (rand() % 30 < 30 is a tautology), and magic resistance >= 200 is outright immune.
    int monLevel = mon->monsterInfo.level;
    int monResMind = mon->monsterInfo.resMind;
    mon->monsterInfo.level = 0;
    mon->monsterInfo.resMind = 0;
    EXPECT_TRUE(mon->mm6MagicEffectSticks());
    mon->monsterInfo.resMind = 200;
    EXPECT_FALSE(mon->mm6MagicEffectSticks());
    mon->monsterInfo.level = monLevel;
    mon->monsterInfo.resMind = monResMind;

    // Cast Mass Curse at Novice, skill 10 -> 2 min/skill * 10 = 20 minutes of curse on every monster in sight
    // that fails its magic save - zero the in-view monsters' level and magic resistance first so the save
    // deterministically sticks. Nothing moves the party or monsters between this snapshot and the cast, so the
    // same actors are in view.
    for (Actor *actor : inView) {
        actor->monsterInfo.level = 0;
        actor->monsterInfo.resMind = 0;
    }
    Time castStart = pParty->GetPlayingTime();
    pushSpellOrRangedAttack(static_cast<SpellId>(91), 0, CombinedSkillValue(10, MASTERY_NOVICE), 0, 1);
    game.tick(1);
    Time castEnd = pParty->GetPlayingTime();

    // At least the monsters that were in view got cursed, and every cursed monster has exactly the Novice
    // duration (2 minutes per skill point).
    Actor *cursedMon = nullptr;
    for (Actor *actor : inView) {
        if (actor->cursedExpireTime > castStart) {
            cursedMon = actor;
            EXPECT_GE(actor->cursedExpireTime, castStart + Duration::fromMinutes(20));
            EXPECT_LE(actor->cursedExpireTime, castEnd + Duration::fromMinutes(20));
        }
    }
    ASSERT_NE(cursedMon, nullptr) << "Mass Curse should have cursed at least one monster in view";
    // End to end: the cast cursed it, so the 50% curse roll now produces misses (100 straight hits would
    // need every curse coin-flip AND every base to-hit roll to succeed - P < 1e-30).
    bool anyMiss = false;
    for (int i = 0; i < 100 && !anyMiss; i++)
        anyMiss = !cursedMon->ActorHitOrMiss(&target);
    EXPECT_TRUE(anyMiss);
}

// MM6's Dark Containment (native id 99, the last Dark spell) deals NO damage at all - MM6.EXE's impact case
// (0x45d0fc, the object-id 9100 entry of the impact jump table) inflicts monster debuffs instead: each of
// Shrink/Stoned/Paralyze/Curse/Slow/Charm/Fear/Feeblemind is rolled INDEPENDENTLY against the monster's magic
// saving throw (immune at res >= 200, otherwise it sticks with chance 30 / (level + res + 30)) for a random
// (rand() % 30 + 1) * 128 tick (30 game-seconds to 15 minute) duration at power rand() % 3 + 2. Feeblemind is
// the MM6 condition that blocks monster SPELL-CASTING (the cast-state entry falls back to pursuing, MM6.EXE
// 0x4041e4); the curse only imposes the 50% attack-miss chance.
GAME_TEST(Mm6, DarkContainment) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    engine->config->debug.AllMagic.setValue(true);

    MapId goblinwatch = pMapStats->GetMapInfo("d01.blv");
    ASSERT_NE(goblinwatch, MAP_INVALID);
    game.teleportTo(goblinwatch, Vec3f(-1850, 4304, -512), 0); // Indoor, so the projectile's sector is defined.
    game.tick(1);

    int monId = -1;
    for (size_t i = 0; i < pActors.size(); i++) {
        if (pActors[i].hp > 0) {
            monId = static_cast<int>(i);
            break;
        }
    }
    ASSERT_NE(monId, -1);
    Actor &mon = pActors[monId];

    static constexpr std::array<ActorBuff, 6> kContainmentBuffs = {
        ACTOR_BUFF_SHRINK, ACTOR_BUFF_STONED, ACTOR_BUFF_PARALYZED,
        ACTOR_BUFF_SLOWED, ACTOR_BUFF_CHARM, ACTOR_BUFF_AFRAID};

    // Quick-cast Dark Containment and impact its projectile straight onto the monster (the pattern from
    // ShiftedSpellDealsImpactDamage - the projectile may clip scenery during its creating tick).
    auto castAndImpact = [&] {
        for (SpriteObject &object : pSpriteObjects) // Retire any earlier cast's projectile so the scan
            if (object.uSpellID == static_cast<SpellId>(99)) // below finds only the fresh one.
                object.uSpellID = SPELL_NONE;
        pushSpellOrRangedAttack(static_cast<SpellId>(99), 0, CombinedSkillValue(10, MASTERY_NOVICE), 0, 1);
        game.tick(1);
        int proj = -1;
        for (size_t j = 0; j < pSpriteObjects.size(); j++)
            if (pSpriteObjects[j].uSpellID == static_cast<SpellId>(99))
                proj = static_cast<int>(j);
        ASSERT_NE(proj, -1) << "Dark Containment formed no projectile";
        SpriteObject &p = pSpriteObjects[proj];
        p.spriteId = SpellSpriteMapping[static_cast<SpellId>(99)];
        p.uObjectDescID = pObjectList->ObjectIDByItemID(p.spriteId);
        processSpellImpact(proj, Pid(OBJECT_Actor, monId));
    };

    // Deterministic saves: level 0 / magic res 0 makes every effect stick (rand() % 30 < 30 always).
    mon.monsterInfo.level = 0;
    mon.monsterInfo.resMind = 0;
    mon.hp = 500;
    Time castStart = pParty->GetPlayingTime();
    castAndImpact();
    Time castEnd = pParty->GetPlayingTime();

    EXPECT_EQ(mon.hp, 500) << "Dark Containment must deal no damage";
    for (ActorBuff debuff : kContainmentBuffs) {
        EXPECT_TRUE(mon.buffs[debuff].Active()) << "debuff " << std::to_underlying(debuff);
        // The buffs are stamped at impact time (== castEnd; nothing ticks in between): (rand()%30+1)*128.
        EXPECT_GE(mon.buffs[debuff].expireTime, castStart + Duration::fromTicks(128));
        EXPECT_LE(mon.buffs[debuff].expireTime, castEnd + Duration::fromTicks(30 * 128));
        EXPECT_GE(mon.buffs[debuff].power, 2);
        EXPECT_LE(mon.buffs[debuff].power, 4);
    }
    EXPECT_GT(mon.cursedExpireTime, castStart);          // Curse and Feeblemind have no ACTOR_BUFF_* slot -
    EXPECT_GT(mon.mm6FeeblemindExpireTime, castStart);   // they live in the transient Actor fields.

    // Feeblemind blocks spell-casting: the cast-state entry (AI_SpellAttack1, the analog of MM6.EXE 0x404160)
    // falls back to pursuing while the buff is active. Establish the control FIRST - find a monster that,
    // un-feebled, actually enters the cast state (i.e. has line of sight to the party; the first d01 spawn
    // always has monsters in view) - so the feebleminded outcome below can only be the gate's doing.
    int casterId = -1;
    for (size_t i = 0; i < pActors.size() && casterId == -1; i++) {
        if (pActors[i].aiState == Dead || pActors[i].aiState == Removed)
            continue;
        AIDirection dir;
        Actor::GetDirectionInfo(Pid(OBJECT_Actor, i), Pid(OBJECT_Character, 0), &dir, 0);
        pActors[i].mm6FeeblemindExpireTime = Time();
        Actor::AI_SpellAttack1(i, Pid(OBJECT_Character, 0), &dir);
        if (pActors[i].aiState == AttackingRanged3)
            casterId = static_cast<int>(i);
    }
    ASSERT_NE(casterId, -1) << "no monster with line of sight to the party could enter the cast state";
    pActors[casterId].mm6FeeblemindExpireTime = pParty->GetPlayingTime() + Duration::fromMinutes(10);
    AIDirection casterDir;
    Actor::GetDirectionInfo(Pid(OBJECT_Actor, casterId), Pid(OBJECT_Character, 0), &casterDir, 0);
    Actor::AI_SpellAttack1(casterId, Pid(OBJECT_Character, 0), &casterDir);
    // The gate falls back to AI_Pursue1, which picks its own maneuver (Pursuing or Standing) - the
    // one thing it must NOT do is enter the cast state the un-feebled control just reached.
    EXPECT_NE(pActors[casterId].aiState, AttackingRanged3);
    pActors[casterId].mm6FeeblemindExpireTime = Time();

    // Immunity: magic resistance >= 200 shrugs the whole cascade off.
    for (ActorBuff debuff : kContainmentBuffs)
        mon.buffs[debuff].Reset();
    mon.cursedExpireTime = Time();
    mon.mm6FeeblemindExpireTime = Time();
    mon.monsterInfo.resMind = 200;
    mon.hp = 500;
    castAndImpact();
    EXPECT_EQ(mon.hp, 500);
    for (ActorBuff debuff : kContainmentBuffs)
        EXPECT_FALSE(mon.buffs[debuff].Active()) << "debuff " << std::to_underlying(debuff);
    EXPECT_EQ(mon.cursedExpireTime, Time());
    EXPECT_EQ(mon.mm6FeeblemindExpireTime, Time());
}

// MM6 Moon Ray (native id 96) is an outdoors-at-night-only spell: ONE roll of 1-4 per point of skill
// (mastery plays no part) is subtracted raw - no resistance, no magic save - from every monster in the
// caster's sight, and every character that is not Dead/Eradicated heals by the same amount (MM6.EXE
// 0x4297a1). A monster dies only when its hp goes strictly negative. Indoors the cast fails with MM6
// global.txt row 498 "Can't cast MoonRay indoors!", in daylight (hour 5..20) with a plain "Spell failed".
// It used to cast the Sunray analog - a single-target projectile with neither the AoE nor the heal.
GAME_TEST(Mm6, MoonRay) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);
    Character &healed = pParty->pCharacters[0];

    // Daylight (a new game starts in the morning): the cast fails and nobody is healed.
    ASSERT_TRUE(pParty->uCurrentHour >= 5 && pParty->uCurrentHour < 21);
    healed.health = 1;
    pushSpellOrRangedAttack(static_cast<SpellId>(96), 0, CombinedSkillValue(3, MASTERY_NOVICE), 0, 1);
    game.tick(1);
    EXPECT_EQ(healed.health, 1);
    EXPECT_EQ(engine->_statusBar->get(), localization->str(LSTR_SPELL_FAILED));

    // Advance the clock to 22:00 - night.
    pParty->GetPlayingTime() += Duration::fromHours((22 - pParty->GetPlayingTime().toCivilTime().hour + 24) % 24);
    game.tick(1);
    ASSERT_EQ(pParty->uCurrentHour, 22);

    // Work with whatever is in sight (New Sorpigal's street peasants; tick until something is).
    std::vector<Actor *> inView = render->getActorsInViewport(4096);
    for (int i = 0; i < 300 && inView.empty(); i++) {
        game.tick(1);
        inView = render->getActorsInViewport(4096);
    }
    ASSERT_FALSE(inView.empty());
    std::map<int, int> hpBefore; // Bump everything in sight to 500 hp so nothing dies of the ray.
    for (Actor *actor : inView) {
        actor->hp = 500;
        hpBefore[actor->id] = actor->hp;
    }

    healed.health = 1;
    pushSpellOrRangedAttack(static_cast<SpellId>(96), 0, CombinedSkillValue(3, MASTERY_NOVICE), 0, 1);
    game.tick(1);

    // The heal and every monster hit share ONE roll: skill 3 at Novice = 3-12 (well below Roderick's max hp,
    // so the heal isn't clamped and measures the roll exactly).
    int amount = healed.health - 1;
    EXPECT_GE(amount, 3);
    EXPECT_LE(amount, 12);
    for (Actor *actor : inView) { // Anything still in sight at the cast lost exactly the shared roll;
        int lost = hpBefore[actor->id] - actor->hp; // anything that wandered out of view lost nothing.
        EXPECT_TRUE(lost == 0 || lost == amount) << "actor " << actor->id << " lost " << lost << ", roll was " << amount;
    }

    // The kill rule is strict: hp must go NEGATIVE (an actor left at exactly 0 survives), so 1 hp - a
    // 3+ roll always dies.
    Actor *sacrifice = inView[0];
    sacrifice->hp = 1;
    pushSpellOrRangedAttack(static_cast<SpellId>(96), 0, CombinedSkillValue(3, MASTERY_NOVICE), 0, 1);
    game.tick(1);
    EXPECT_TRUE(sacrifice->aiState == Dying || sacrifice->aiState == Dead)
        << "actor " << sacrifice->id << " in state " << std::to_underlying(sacrifice->aiState);

    // Indoors the cast fails outright, even at night.
    MapId goblinwatch = pMapStats->GetMapInfo("d01.blv");
    ASSERT_NE(goblinwatch, MAP_INVALID);
    game.teleportTo(goblinwatch, Vec3f(-1850, 4304, -512), 0);
    game.tick(1);
    healed.health = 1;
    pushSpellOrRangedAttack(static_cast<SpellId>(96), 0, CombinedSkillValue(3, MASTERY_NOVICE), 0, 1);
    game.tick(1);
    EXPECT_EQ(healed.health, 1);
    // MM6 global.txt row 498; MM7 reuses that row for "Herbalist", so there is no LSTR_ name.
    EXPECT_EQ(engine->_statusBar->get(), localization->str(static_cast<LstrId>(498)));
}

// MM6 Spirit Arrow (native id 45) is a plain single targeted projectile - MM6.EXE's cast handler (0x4230e1)
// is the shared projectile launch tail - dealing a flat 1d6 of Spirit damage. Its former Spirit Lash analog
// forms NO projectile (Spirit Lash is a close-range direct hit), so the spell silently did nothing; it now
// translates to Harm, whose effect is exactly "launch one projectile at the target".
GAME_TEST(Mm6, SpiritArrowProjectile) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    MapId goblinwatch = pMapStats->GetMapInfo("d01.blv");
    ASSERT_NE(goblinwatch, MAP_INVALID);
    game.teleportTo(goblinwatch, Vec3f(-1850, 4304, -512), 0); // Indoor, so the projectile's sector is defined.
    game.tick(1);

    int monId = -1;
    for (size_t i = 0; i < pActors.size(); i++) {
        if (pActors[i].hp > 0) {
            monId = static_cast<int>(i);
            break;
        }
    }
    ASSERT_NE(monId, -1);

    pushSpellOrRangedAttack(static_cast<SpellId>(45), 0, CombinedSkillValue(10, MASTERY_NOVICE), 0, 1);
    game.tick(1);
    int proj = -1;
    for (size_t j = 0; j < pSpriteObjects.size(); j++)
        if (pSpriteObjects[j].uSpellID == static_cast<SpellId>(45))
            proj = static_cast<int>(j);
    ASSERT_NE(proj, -1) << "Spirit Arrow formed no projectile";

    // Impact it straight onto a monster (the ShiftedSpellDealsImpactDamage pattern - restore the fresh
    // pre-impact sprite in case the projectile clipped scenery during its creating tick).
    SpriteObject &p = pSpriteObjects[proj];
    p.uSpellID = static_cast<SpellId>(45);
    p.spriteId = SpellSpriteMapping[static_cast<SpellId>(45)];
    p.uObjectDescID = pObjectList->ObjectIDByItemID(p.spriteId);
    pActors[monId].hp = 500;
    pActors[monId].monsterInfo.resSpirit = 0; // Deterministic: the found monster must not resist the damage.
    processSpellImpact(proj, Pid(OBJECT_Actor, monId));
    EXPECT_GT(500 - pActors[monId].hp, 0);
}

// MM6 Shrapmetal (native id 92) fires 3/5/7 pieces at Novice/Expert/Master (MM6.EXE 0x429445) - one fewer
// per tier than the MM7 Sharpmetal effect it casts through.
GAME_TEST(Mm6, ShrapmetalFan) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    MapId goblinwatch = pMapStats->GetMapInfo("d01.blv");
    ASSERT_NE(goblinwatch, MAP_INVALID);
    game.teleportTo(goblinwatch, Vec3f(-1850, 4304, -512), 0);
    game.tick(1);

    auto countPieces = [&](Mastery mastery) -> int {
        for (SpriteObject &object : pSpriteObjects) // Retire earlier casts' pieces so the scan below
            if (object.uSpellID == static_cast<SpellId>(92)) // finds only the fresh fan.
                object.uSpellID = SPELL_NONE;
        pushSpellOrRangedAttack(static_cast<SpellId>(92), 0, CombinedSkillValue(5, mastery), 0, 1);
        game.tick(1);
        int pieces = 0;
        for (const SpriteObject &object : pSpriteObjects)
            if (object.uSpellID == static_cast<SpellId>(92))
                pieces++;
        return pieces;
    };

    EXPECT_EQ(countPieces(MASTERY_NOVICE), 3);
    EXPECT_EQ(countPieces(MASTERY_EXPERT), 5);
    EXPECT_EQ(countPieces(MASTERY_MASTER), 7);
}

// MM6 has no hostile.txt and no monster factions (MMExtension defines HostileTxt and the IsAgainst relation
// method for MM7+ only): a monster's aggression toward the party is its own monsters.txt "Hst" column - 4 for
// every regular monster, 0 for the true Peasant rows - kept as mutable per-actor state (Charm zeroes it,
// damage escalates it), and monsters NEVER fight each other (MM6 has no Berserk/Enslave spells and no
// infighting; MM6.EXE's AI target is always the party, Pid 4 verbatim in the engage path @0x40203A). With
// hostile.txt absent the relations table is all-friendly, which used to pacify every MM6 monster permanently:
// _SelectTarget never picked the party, and UpdateActorAI then reset the actor's hostility every frame.
GAME_TEST(Mm6, MonsterHostility) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // Street peasants (New Sorpigal's oute3 is full of them) are harmless: monsters.txt hostility 0.
    auto peasant = std::ranges::find_if(pActors, [](Actor &actor) { return actor.IsPeasant(); });
    ASSERT_NE(peasant, pActors.end());
    EXPECT_EQ(peasant->GetActorsRelation(nullptr), HOSTILITY_FRIENDLY);

    MapId goblinwatch = pMapStats->GetMapInfo("d01.blv");
    ASSERT_NE(goblinwatch, MAP_INVALID);
    game.teleportTo(goblinwatch, Vec3f(-1850, 4304, -512), 0); // First spawn point of d01.blv, a goblin in view.
    game.tick(1);

    // Pick the nearest live monster with line of sight to the party. Respawn placement is RNG-dependent
    // (and the RNG stream now differs run-to-run with street hostiles fighting on oute3), so don't
    // depend on any specific spawn layout or on the rendered viewport.
    Actor *mon = nullptr;
    int monId = -1;
    float bestDist = 5120; // _SelectTarget targets the party inside ranges[HOSTILITY_LONG] = 5120.
    for (size_t i = 0; i < pActors.size(); i++) {
        Actor &actor = pActors[i];
        if (actor.aiState == Dead || actor.aiState == Removed)
            continue;
        float dist = (actor.pos - pParty->pos).length();
        if (dist < bestDist && Detect_Between_Objects(Pid(OBJECT_Actor, i), Pid(OBJECT_Character, 0))) {
            bestDist = dist;
            mon = &actor;
            monId = i;
        }
    }
    ASSERT_NE(mon, nullptr);

    // Every regular MM6 monster is Hst 4 in monsters.txt, so its relation to the party is hostile...
    EXPECT_EQ(mon->GetActorsRelation(nullptr), HOSTILITY_LONG);

    // ...but monsters are always friendly to each other, even across families (rats vs goblins here).
    Actor *otherFamily = nullptr;
    for (Actor &actor : pActors) {
        if (monsterTypeForMonsterId(actor.monsterInfo.id) != monsterTypeForMonsterId(mon->monsterInfo.id)) {
            otherFamily = &actor;
            break;
        }
    }
    ASSERT_NE(otherFamily, nullptr);
    EXPECT_EQ(mon->GetActorsRelation(otherFamily), HOSTILITY_FRIENDLY);
    EXPECT_EQ(otherFamily->GetActorsRelation(mon), HOSTILITY_FRIENDLY);

    // Target selection picks the party for the monster in sight.
    Pid target;
    Actor::_SelectTarget(monId, &target, true);
    EXPECT_EQ(target, Pid(OBJECT_Character, 0));

    // And the live AI actually engages: the goblin (or a cave-mate) pursues or attacks within a few
    // seconds, without the party having thrown a single punch.
    bool aggro = false;
    for (int i = 0; i < 150 && !aggro; i++) {
        game.tick(1);
        for (const Actor &actor : pActors) {
            if (actor.aiState == Pursuing || actor.aiState == AttackingMelee ||
                    actor.aiState == AttackingRanged1 || actor.aiState == AttackingRanged2 ||
                    actor.aiState == AttackingRanged3 || actor.aiState == AttackingRanged4) {
                aggro = true;
                break;
            }
        }
    }
    EXPECT_TRUE(aggro);
}

// MM6 Guardian Angel (native id 50) has no MM7 counterpart, so translateForCast runs it as Preservation
// (which turns a lethal blow into unconsciousness). Its real effect (spells.txt) is a whole-party compact:
// while it is active, a total party defeat resurrects the party for HALF its gold instead of the normal
// all-gold-lost respawn, restoring 1 / half / full HP per character at Novice/Expert/Master. It lasts 1 hour
// per point of skill at every mastery (MM6.EXE 0x426b97, the same 3600*L tick chain as the stat buffs). The
// state is a transient party field (Party::_mm6GuardianAngelExpireTime), like Actor::cursedExpireTime for
// Mass Curse - MM7 never sets it, so nothing here fires in an MM7 game.
GAME_TEST(Mm6, GuardianAngel) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // Cast: whole party (no target picker, like Day of the Gods), 1 hour per skill point at every mastery,
    // with the mastery stored to pick the resurrect HP tier. skill 10 -> 10 hours.
    Time castStart = pParty->GetPlayingTime();
    pushSpellOrRangedAttack(static_cast<SpellId>(50), 0, CombinedSkillValue(10, MASTERY_MASTER), 0, 1);
    game.tick(1);
    Time castEnd = pParty->GetPlayingTime();
    EXPECT_GT(pParty->_mm6GuardianAngelExpireTime, castEnd); // Active.
    EXPECT_GE(pParty->_mm6GuardianAngelExpireTime, castStart + Duration::fromHours(10));
    EXPECT_LE(pParty->_mm6GuardianAngelExpireTime, castEnd + Duration::fromHours(10));
    EXPECT_EQ(pParty->_mm6GuardianAngelMastery, MASTERY_MASTER);

    auto killWholeParty = [&]() {
        for (Character &character : pParty->pCharacters)
            character.conditions.set(CONDITION_DEAD, pParty->GetPlayingTime());
        game.tick(10);
    };

    // While Guardian Angel is active, a total party defeat resurrects everyone for HALF the party's gold (not
    // all of it) with HP by mastery, and the game keeps playing. Novice = 1 HP, Expert = half HP, Master =
    // full HP. The week-long time penalty applies either way (MM6.EXE 0x453d70 adds it before the buff is
    // even read), and the buff is CONSUMED by the resurrect (0x453e11 wipes the whole party buff array,
    // Guardian Angel included) - one cast protects against one defeat.
    auto expectResurrect = [&](Mastery mastery, auto hpForMax) {
        pParty->_mm6GuardianAngelExpireTime = pParty->GetPlayingTime() + Duration::fromDays(30);
        pParty->_mm6GuardianAngelMastery = mastery;
        pParty->SetGold(1000);
        std::array<int, 4> maxHealth;
        for (int i = 0; i < 4; i++)
            maxHealth[i] = pParty->pCharacters[i].GetMaxHealth();
        Time timeBeforeDeath = pParty->GetPlayingTime();
        killWholeParty();
        EXPECT_EQ(uGameState, GAME_STATE_PLAYING);
        EXPECT_EQ(pParty->GetGold(), 500) << "mastery " << std::to_underlying(mastery);
        EXPECT_GE(pParty->GetPlayingTime(), timeBeforeDeath + Duration::fromDays(7)); // Penalty is unconditional.
        EXPECT_EQ(pParty->_mm6GuardianAngelExpireTime, Time()) << "the resurrect should consume the buff";
        for (int i = 0; i < 4; i++) {
            EXPECT_TRUE(pParty->pCharacters[i].CanAct());
            EXPECT_EQ(pParty->pCharacters[i].health, hpForMax(maxHealth[i]))
                << "mastery " << std::to_underlying(mastery) << " char " << i;
        }
    };
    expectResurrect(MASTERY_NOVICE, [](int) { return 1; });               // Novice: 1 HP each.
    expectResurrect(MASTERY_EXPERT, [](int maxHp) { return maxHp / 2; }); // Expert: half HP.
    expectResurrect(MASTERY_MASTER, [](int maxHp) { return maxHp; });     // Master: full HP.

    // Control: with no Guardian Angel active, the same defeat loses ALL the gold and leaves everyone at 1 HP.
    pParty->_mm6GuardianAngelExpireTime = Time();
    pParty->SetGold(1000);
    killWholeParty();
    EXPECT_EQ(uGameState, GAME_STATE_PLAYING);
    EXPECT_EQ(pParty->GetGold(), 0);
    for (Character &character : pParty->pCharacters)
        EXPECT_EQ(character.health, 1);
}

// Opens the proprietor dialogue in a freshly entered house. Houses with a single occupant open it
// automatically; houses that also lodge npcdata NPCs (like MM6's town halls) show a portrait row
// instead, and the proprietor - always first in houseNpcs - must be clicked.
static void openProprietorDialogue(EngineController &game) {
    if (pDialogueWindow != nullptr)
        return;
    ASSERT_FALSE(houseNpcs.empty());
    ASSERT_EQ(houseNpcs[0].type, HOUSE_PROPRIETOR);
    ASSERT_NE(houseNpcs[0].button, nullptr);
    Recti portrait = houseNpcs[0].button->rect;
    game.pressAndReleaseButton(BUTTON_LEFT, portrait.x + portrait.w / 2, portrait.y + portrait.h / 2);
    game.tick(2);
    ASSERT_NE(pDialogueWindow, nullptr);
}

// Escapes out of any open house dialogue and the house itself, back to the game screen.
static void leaveHouse(EngineController &game) {
    for (int i = 0; i < 4 && current_screen_type != SCREEN_GAME; i++) {
        game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
        game.tick(2);
    }
    ASSERT_EQ(current_screen_type, SCREEN_GAME);
}

// Finds a proprietor-dialogue option button by its DialogueId, or nullptr when not offered.
// Option buttons are re-laid-out to rendered-text metrics on draw - locate by message param only.
static const GUIButton *findProprietorOption(DialogueId option) {
    if (!pDialogueWindow)
        return nullptr;
    for (const GUIButton *button : pDialogueWindow->vButtons)
        if (button->msg == UIMSG_SelectProprietorDialogueOption && button->msg_param == std::to_underlying(option))
            return button;
    return nullptr;
}

// MM6 taverns have a flat four-option menu (MM6.EXE option factory tavern case @0x498828): Rent
// Room(15) / Fill Packs(16) / Have a Drink(25) / Tip Barkeep(26) - no Arcomage, no skill teaching.
// A drink (handler @0x49F45C) costs a flat 1 gold and marks the tavern as drunk-in; half the time
// the drinker hiccups and 1-in-3 of those turn Drunk, otherwise 1-in-4 grants an until-rest +5..10
// bonus to a random stat. A tip (@0x49F716) needs a prior drink in THIS tavern ("Have a Drink
// first..."), costs 1 gold, and tells a rumor from the regional-news pool - rolled once per tavern
// and repeated verbatim on later tips.
GAME_TEST(Mm6, TavernDrinksAndTip) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    EXPECT_EQ(houseTable[HouseId(92)].uType, HOUSE_TYPE_TAVERN);
    enterLonelyKnightTavern(game); // A Lonely Knight, house 92.
    ASSERT_NE(window_SpeakInHouse, nullptr);
    openProprietorDialogue(game);

    EXPECT_EQ(listProprietorOptions(), (std::vector<DialogueId>{DIALOGUE_TAVERN_REST, DIALOGUE_TAVERN_BUY_FOOD,
                                                                DIALOGUE_TAVERN_MM6_DRINKS, DIALOGUE_TAVERN_MM6_TIP}));

    pParty->SetGold(1000);

    // Tipping before drinking here is refused: no gold spent, no rumor told.
    clickProprietorOption(game, DIALOGUE_TAVERN_MM6_TIP);
    EXPECT_EQ(pParty->GetGold(), 1000);
    EXPECT_FALSE(pParty->_mm6TavernRumors.contains(HouseId(92)));

    // Drinks: 1 gold each. Over 64 rounds the random effects must materialize on the drinker -
    // Drunk lands at 1/6 per drink and a stat bonus at 1/8, so P(neither, 64 rounds) ~ 2.6e-10.
    Character &drinker = pParty->activeCharacter();
    bool anyEffect = false;
    for (int i = 0; i < 64; i++) {
        int goldBefore = pParty->GetGold();
        clickProprietorOption(game, DIALOGUE_TAVERN_MM6_DRINKS);
        EXPECT_EQ(pParty->GetGold(), goldBefore - 1);
        anyEffect = anyEffect || drinker.conditions.has(CONDITION_DRUNK);
        for (Attribute stat : drinker._statBonuses.indices())
            anyEffect = anyEffect || drinker._statBonuses[stat] > 0;
    }
    EXPECT_TRUE(pParty->_mm6TavernsDrunkIn.contains(HouseId(92)));
    EXPECT_TRUE(anyEffect);

    // Tip: 1 gold, and the barkeep tells a news-pool rumor. It is cached for this tavern - a
    // second tip costs another gold but repeats the same line.
    int goldBefore = pParty->GetGold();
    clickProprietorOption(game, DIALOGUE_TAVERN_MM6_TIP);
    EXPECT_EQ(pParty->GetGold(), goldBefore - 1);
    ASSERT_TRUE(pParty->_mm6TavernRumors.contains(HouseId(92)));
    std::string rumor = pParty->_mm6TavernRumors[HouseId(92)];
    EXPECT_FALSE(rumor.empty());
    clickProprietorOption(game, DIALOGUE_TAVERN_MM6_TIP);
    EXPECT_EQ(pParty->GetGold(), goldBefore - 2);
    EXPECT_EQ(pParty->_mm6TavernRumors[HouseId(92)], rumor);

    leaveHouse(game);
    game.tick(5);
}

// MM6's fighter and thief guilds are membership skill-teaching houses. The model, from MM6.EXE:
// - 2dEvents "Merc Guild" rows are houses 141-146, "Thieves Guild" rows 147-152. Every membership
//   house (magic guilds 119-140 included) maps to a per-house membership award bit via the word
//   pair table @0x4C3CB8: Blades' End 141/145 -> bit 69, Duelists' Edge 142/144 -> 70, Berserkers'
//   Fury 143/146 -> 71, Buccaneers' Lair 147/148 -> 66, Protection Services 149/150 -> 67,
//   Smugglers' Guild 151/152 -> 68 (awards.txt rows 64-80 are the "Joined the ..." strings).
// - Joining happens through npcdata NPC topics 381..397 (npctopic "<Guild> Membership"): topic id
//   381+idx -> award bit 64+idx for ALL FOUR characters at the price from the dword table
//   @0x4C3E10 (Blades' End 25 gold), and the NPC's topic slot is cleared (0x496a96).
// - Inside the house, a member is offered the house's taught skills (house-id-keyed lists in the
//   option builder @0x498ec4/0x4991e3; house 141 = Sword/Axe/Spear/Staff/Leather, 147 =
//   Dagger/Merchant/IdentifyItem/Perception/DisarmTrap), filtered by the class-can-learn table
//   @0x4C2694 and by not-already-knowing the skill. Learning costs trunc(base x 2dEvents price
//   multiplier), base 100 for "Merc Guild" rows and 250 for "Thieves Guild" rows (0x49c4cd),
//   merchant-discounted with a floor of a third, and sets the skill to novice 1 (0x49c712).
// - A non-member (and any plain type-18 house) gets NO options - it must not crash: unmapped MM6
//   type strings used to fall into the MM7-shaped award array read that aborted on draw.
GAME_TEST(Mm6, MercGuildJoinAndLearnSkills) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    pParty->SetGold(2000);

    EXPECT_EQ(houseTable[HouseId(141)].name, "Blades' End");
    EXPECT_EQ(houseTable[HouseId(147)].name, "Buccaneers' Lair");
    EXPECT_EQ(houseTable[HouseId(141)].fPriceMultiplier, 1.5f);
    EXPECT_EQ(houseTable[HouseId(147)].fPriceMultiplier, 1.5f);

    // Not a member: Blades' End opens safely and offers no skill training.
    ASSERT_TRUE(enterHouse(HouseId(141)));
    createHouseUI(HouseId(141));
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_HOUSE);
    openProprietorDialogue(game);
    for (const GUIButton *button : pDialogueWindow->vButtons)
        if (button->msg == UIMSG_SelectProprietorDialogueOption)
            EXPECT_FALSE(IsSkillLearningDialogue(static_cast<DialogueId>(button->msg_param)));
    leaveHouse(game);

    // Join Blade's End through its recruiter: Harold Hess in New Sorpigal's House P1 (473), whose
    // third npcdata topic is 386 ("Blade's End Membership").
    NPCData *recruiter = &pNPCStats->pNPCData[33];
    EXPECT_EQ(recruiter->name, "Harold Hess");
    ASSERT_EQ(recruiter->house, HouseId(473));
    ASSERT_EQ(recruiter->dialogue_3_evt_id, 386);
    EXPECT_EQ(pNPCTopics[386].pTopic, "Blade's End Membership");
    ASSERT_TRUE(enterHouse(HouseId(473)));
    createHouseUI(HouseId(473));
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_HOUSE);
    clickHouseNpcPortrait(game, recruiter);
    int goldBefore = pParty->GetGold();
    selectScriptedTopic(game, DIALOGUE_SCRIPTED_LINE_3);
    selectScriptedTopic(game, DIALOGUE_MAGIC_GUILD_JOIN);
    EXPECT_EQ(pParty->GetGold(), goldBefore - 25); // MM6.EXE join price table 0x4C3E10, Blades' End = 25.
    for (const Character &character : pParty->pCharacters)
        EXPECT_TRUE(character._achievedAwardsBits[static_cast<AwardId>(69)]); // awards.txt 69 "Joined the Blade's End Guild".
    EXPECT_EQ(recruiter->dialogue_3_evt_id, 0); // The recruiter no longer offers the topic.
    leaveHouse(game);

    // As a member, Blades' End teaches. Roderick the Paladin already knows Sword, so it is
    // filtered out; Staff is learnable (class-can-learn table, Paladin/Staff nonzero).
    ASSERT_TRUE(enterHouse(HouseId(141)));
    createHouseUI(HouseId(141));
    game.tick(2);
    openProprietorDialogue(game);
    EXPECT_EQ(pParty->activeCharacter().name, pParty->pCharacters[0].name); // Roderick is active.
    EXPECT_NE(findProprietorOption(DIALOGUE_LEARN_STAFF), nullptr);
    EXPECT_NE(findProprietorOption(DIALOGUE_LEARN_AXE), nullptr);
    EXPECT_NE(findProprietorOption(DIALOGUE_LEARN_SPEAR), nullptr);
    EXPECT_NE(findProprietorOption(DIALOGUE_LEARN_LEATHER), nullptr);
    EXPECT_EQ(findProprietorOption(DIALOGUE_LEARN_MERCHANT), nullptr); // Not taught here.
    // Sword is taught here but Roderick already knows it: the option button stays (like MM7's
    // learn dialogues its label just goes blank) and clicking it is a no-op.
    EXPECT_NE(findProprietorOption(DIALOGUE_LEARN_SWORD), nullptr);
    goldBefore = pParty->GetGold();
    clickProprietorOption(game, DIALOGUE_LEARN_SWORD);
    EXPECT_EQ(pParty->GetGold(), goldBefore);
    goldBefore = pParty->GetGold();
    EXPECT_FALSE(pParty->pCharacters[0].pActiveSkills[SKILL_STAFF]);
    clickProprietorOption(game, DIALOGUE_LEARN_STAFF);
    EXPECT_EQ(pParty->GetGold(), goldBefore - 150); // trunc(100 x 1.5), no merchant discount on a fresh party.
    EXPECT_EQ(pParty->pCharacters[0].pActiveSkills[SKILL_STAFF], CombinedSkillValue::novice());
    leaveHouse(game);

    // Thieves guilds share the model at learn base price 250. Grant Buccaneers' membership (bit 66)
    // directly and have Alexis the Archer learn Dagger at house 147: trunc(250 x 1.5) = 375.
    for (Character &character : pParty->pCharacters)
        character._achievedAwardsBits.set(static_cast<AwardId>(66), true);
    pParty->setActiveCharacterIndex(2); // Alexis.
    pParty->GetPlayingTime() += Duration::fromHours(10); // Thieves keep night hours (open 18-6).
    game.tick(1);
    ASSERT_TRUE(enterHouse(HouseId(147)));
    createHouseUI(HouseId(147));
    game.tick(2);
    openProprietorDialogue(game);
    EXPECT_EQ(pParty->activeCharacter().name, pParty->pCharacters[1].name);
    EXPECT_NE(findProprietorOption(DIALOGUE_LEARN_DAGGER), nullptr);
    goldBefore = pParty->GetGold();
    EXPECT_FALSE(pParty->pCharacters[1].pActiveSkills[SKILL_DAGGER]);
    clickProprietorOption(game, DIALOGUE_LEARN_DAGGER);
    EXPECT_EQ(pParty->GetGold(), goldBefore - 375);
    EXPECT_EQ(pParty->pCharacters[1].pActiveSkills[SKILL_DAGGER], CombinedSkillValue::novice());
    leaveHouse(game);
}

// MM6 has three town halls (2dEvents houses 89-91: New Sorpigal, Castle Ironfist, Silver Cove) whose
// bounty state lives in three slots keyed as houseId - 89 (MM6.EXE 0x4A31AF); the engine's bounty arrays
// are keyed by MM7's five town-hall house ids, so the MM6 town halls map onto the first three slots.
// The monthly bounty is a uniform roll over monster ids 1-171 that re-rolls the non-monster rows
// (88-90 VARN guardians, 103-105 Merchants, 121-126 + 133-135 true peasants, 148-150 VARN robots;
// MM6.EXE 0x4A3238). Claiming pays 100 gold per monster level, brands every character with MM6's award
// 81 ("Collected %u bounties"), counts ONE bounty (MM6's counter is a count, not gold like MM7's), and
// nudges reputation toward notorious by the monster's level - killing for money (MM6.EXE 0x4A32DC).
// The reply texts are MM6's own npctext rows 368-370.
GAME_TEST(Mm6, TownHallBountyHunt) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // All three MM6 town halls parse as town halls.
    ASSERT_EQ(houseTable[HouseId(89)].uType, HOUSE_TYPE_TOWN_HALL);
    ASSERT_EQ(houseTable[HouseId(90)].uType, HOUSE_TYPE_TOWN_HALL);
    ASSERT_EQ(houseTable[HouseId(91)].uType, HOUSE_TYPE_TOWN_HALL);

    auto isExcludedFromBounties = [](int id) {
        return (id >= 88 && id <= 90) || (id >= 103 && id <= 105) || (id >= 121 && id <= 126) ||
               (id >= 133 && id <= 135) || (id >= 148 && id <= 150);
    };

    // The roll never produces an out-of-range or excluded monster.
    for (int i = 0; i < 500; i++) {
        int roll = std::to_underlying(GUIWindow_TownHall::randomMonsterForHunting(HouseId(89)));
        ASSERT_GE(roll, 1);
        ASSERT_LE(roll, 171);
        ASSERT_FALSE(isExcludedFromBounties(roll)) << "rolled excluded monster id " << roll;
    }

    // Town halls open at 10:00 and the game starts at 9:00 - move to opening hours, then walk in through
    // the same path EVENT_SpeakInHouse takes (the New Sorpigal town hall is house 89, on oute3).
    pParty->GetPlayingTime() += Duration::fromHours(2);
    game.tick(1);
    ASSERT_TRUE(enterHouse(HouseId(89)));
    createHouseUI(HouseId(89));
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_HOUSE);
    ASSERT_NE(window_SpeakInHouse, nullptr);
    ASSERT_EQ(window_SpeakInHouse->houseId(), HouseId(89));
    openProprietorDialogue(game);

    // Asking for the bounty posts this month's hunt: a valid target, not yet killed, regenerating at the
    // start of next month, announced with MM6's npctext row 368 naming the monster.
    clickProprietorOption(game, DIALOGUE_TOWNHALL_BOUNTY_HUNT);
    HouseId slot = HOUSE_TOWN_HALL_HARMONDALE; // MM6 slot 0 = house 89.
    MonsterId target = pParty->monster_id_for_hunting[slot];
    ASSERT_NE(target, MONSTER_INVALID);
    EXPECT_FALSE(isExcludedFromBounties(std::to_underlying(target)));
    EXPECT_FALSE(pParty->monster_for_hunting_killed[slot]);
    EXPECT_EQ(pParty->PartyTimes.bountyHuntNextGenTime[slot],
              Time::fromMonths(pParty->GetPlayingTime().toMonths() + 1));
    EXPECT_TRUE(current_npc_text.contains(pMonsterStats->infos[target].name));
    EXPECT_TRUE(current_npc_text.contains("bounty"));

    // Kill a monster of the hunted kind through the real death path - Actor::Die registers the kill.
    // Escaping unwinds sub-dialogue -> main dialogue -> portrait row -> out of the house.
    for (int i = 0; i < 5 && current_screen_type != SCREEN_GAME; i++) {
        game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
        game.tick(2);
    }
    ASSERT_EQ(current_screen_type, SCREEN_GAME);
    ASSERT_FALSE(pActors.empty());
    pActors[0].monsterInfo.id = target;
    Actor::Die(0);
    game.tick(2);
    EXPECT_TRUE(pParty->monster_for_hunting_killed[slot]);

    // Claim the reward: 100 gold per level of the hunted monster, one bounty counted, award 81 on every
    // character, reputation up (toward notorious) by the monster's level, and the slot retires for the
    // rest of the month.
    int level = pMonsterStats->infos[target].level;
    pParty->SetGold(1000);
    int bountiesBefore = pParty->uNumBountiesCollected;
    int repBefore = currentLocationInfo().reputation;
    ASSERT_TRUE(enterHouse(HouseId(89)));
    createHouseUI(HouseId(89));
    game.tick(2);
    openProprietorDialogue(game);
    clickProprietorOption(game, DIALOGUE_TOWNHALL_BOUNTY_HUNT);
    EXPECT_EQ(pParty->GetGold(), 1000 + 100 * level);
    EXPECT_EQ(pParty->uNumBountiesCollected, bountiesBefore + 1);
    EXPECT_EQ(currentLocationInfo().reputation, repBefore + level);
    for (Character &character : pParty->pCharacters)
        EXPECT_TRUE(character._achievedAwardsBits[static_cast<AwardId>(81)]);
    EXPECT_EQ(pParty->monster_id_for_hunting[slot], MONSTER_INVALID);
    EXPECT_FALSE(pParty->monster_for_hunting_killed[slot]);
    EXPECT_TRUE(current_npc_text.contains("Congratulations"));

    // Asking again the same month: someone has already claimed the bounty (npctext row 370).
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE); // Back to the main dialogue.
    game.tick(2);
    clickProprietorOption(game, DIALOGUE_TOWNHALL_BOUNTY_HUNT);
    EXPECT_TRUE(current_npc_text.contains("already claimed"));

    // Next month a fresh bounty is posted (MM months are exactly 28 days).
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    pParty->GetPlayingTime() += Duration::fromDays(28);
    game.tick(1);
    clickProprietorOption(game, DIALOGUE_TOWNHALL_BOUNTY_HUNT);
    EXPECT_NE(pParty->monster_id_for_hunting[slot], MONSTER_INVALID);
    EXPECT_FALSE(pParty->monster_for_hunting_killed[slot]);
    for (int i = 0; i < 5 && current_screen_type != SCREEN_GAME; i++) {
        game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
        game.tick(2);
    }
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
}

// The MM6 in-game HUD draws from MM6's own skin assets (reversed from MM6.EXE: asset loader @0x418090,
// HUD draw cluster @0x417dc0/0x417df0/0x486900): border3/border4 edges around the viewport, a
// time-of-day tapestry (TAP1..4) over the top-right block with the minimap and the scrolling compass
// ribbon showing through its color-keyed holes, the border1.pcx right panel (books /
// medallions / hireling windows), the border2.pcx portrait strip with per-face frame sets
// (malea..maleh / girla..girld, 53 frames each), bottom-anchored HP/SP pillar bars, ready-gems, and the
// footer status bar.
GAME_TEST(Mm6, GameHudSkin) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(3); // DrawGUI runs every frame - the whole HUD draw path is exercised here.

    // The skin globals hold the real MM6 assets, not the old 1x1 placeholder.
    EXPECT_EQ(game_ui_topframe->size(), Sizei(468, 8));            // border3
    EXPECT_EQ(game_ui_leftframe->size(), Sizei(8, 344));           // border4
    EXPECT_EQ(game_ui_bottomframe->size(), Sizei(469, 109));       // border2.pcx portrait strip
    EXPECT_EQ(game_ui_right_panel_frame->size(), Sizei(172, 339)); // border1.pcx right panel
    EXPECT_EQ(game_ui_statusbar->size(), Sizei(483, 24));          // footer
    EXPECT_EQ(game_ui_minimap_frame->size(), Sizei(151, 116));     // mapback
    EXPECT_EQ(game_ui_minimap_compass->size(), Sizei(325, 9));     // compass ribbon
    EXPECT_EQ(game_ui_mm6_facemask->size(), Sizei(63, 83));
    EXPECT_EQ(game_ui_mm6_border5->size(), Sizei(8, 20));
    EXPECT_EQ(game_ui_mm6_border6->size(), Sizei(7, 21));
    for (GraphicsImage *tapestry : game_ui_mm6_tapestries)
        EXPECT_EQ(tapestry->size(), Sizei(172, 142));

    // Tapestry = the sky seen through the arch, picked by the in-game hour (MM6.EXE 0x417960).
    EXPECT_EQ(mm6TapestryForHour(3), game_ui_mm6_tapestries[3]);   // Night.
    EXPECT_EQ(mm6TapestryForHour(5), game_ui_mm6_tapestries[2]);   // Dawn.
    EXPECT_EQ(mm6TapestryForHour(12), game_ui_mm6_tapestries[1]);  // Day.
    EXPECT_EQ(mm6TapestryForHour(20), game_ui_mm6_tapestries[0]);  // Dusk.
    EXPECT_EQ(mm6TapestryForHour(22), game_ui_mm6_tapestries[3]);  // Night again.

    // Compass ribbon scroll: x = 528 - round((2048 - yaw) * 0.1171875).
    EXPECT_EQ(mm6CompassRibbonX(0), 288);
    EXPECT_EQ(mm6CompassRibbonX(1024), 408);
    EXPECT_EQ(mm6CompassRibbonX(2047), 528);

    // HP/SP bars: green/yellow/red per fill range plus the blue mana bar, 6px wide.
    EXPECT_EQ(game_ui_bar_green->size(), Sizei(6, 78));   // hitsfull
    EXPECT_EQ(game_ui_bar_yellow->size(), Sizei(6, 40));  // hitshalf
    EXPECT_EQ(game_ui_bar_red->size(), Sizei(6, 19));     // hitsqtr
    EXPECT_EQ(game_ui_bar_blue->size(), Sizei(6, 78));    // manafull

    // Portraits: the default party is Roderick (face 0 = malea), Alexis (11 = girld), Serena (9 = girlb),
    // Zoltan (7 = maleh) - every face frame resolves to a real 59x79 image, and the condition stand-ins
    // (tombstone / eradicated smear) load from MM6's own entries.
    for (int i = 0; i < 4; i++)
        EXPECT_EQ(game_ui_player_faces[i][0]->size(), Sizei(59, 79));
    EXPECT_EQ(game_ui_player_faces[0][0], assets->getImage_ColorKey("malea01"));
    EXPECT_EQ(game_ui_player_faces[1][0], assets->getImage_ColorKey("girld01"));
    EXPECT_EQ(game_ui_player_faces[2][0], assets->getImage_ColorKey("girlb01"));
    EXPECT_EQ(game_ui_player_faces[3][0], assets->getImage_ColorKey("maleh01"));
    EXPECT_EQ(game_ui_player_face_dead->size(), Sizei(59, 79));

    // MM6 button layout: the medallion row at y=399, the four books at y=263, no history book.
    EXPECT_EQ(pBtn_CastSpell->rect.topLeft(), Pointi(491, 399));
    EXPECT_EQ(pBtn_Rest->rect.topLeft(), Pointi(525, 399));
    EXPECT_EQ(pBtn_QuickReference->rect.topLeft(), Pointi(560, 399));
    EXPECT_EQ(pBtn_GameSettings->rect.topLeft(), Pointi(594, 399));
    EXPECT_EQ(pBtn_Quests->rect.topLeft(), Pointi(495, 263));
    EXPECT_EQ(pBtn_Calendar->rect.topLeft(), Pointi(588, 263));
    EXPECT_EQ(pBtn_History, nullptr);

    // The history-book message is a no-op in MM6 (there is no journal).
    engine->_messageQueue->addMessageCurrentFrame(UIMSG_OpenHistoryBook, 0, 0);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);

    // The wizard-eye dots path draws without crashing (map content + object/actor dots).
    pParty->pPartyBuffs[PARTY_BUFF_WIZARD_EYE].Apply(pParty->GetPlayingTime() + Duration::fromHours(1), MASTERY_MASTER, 5, 0, 0);
    game.tick(3);
    pParty->pPartyBuffs[PARTY_BUFF_WIZARD_EYE].Reset();
    game.tick(1);

    // Milestone 71: persistent buff fx (MM6.EXE 0x436010, drawn from the portrait pass while the buffs run).
    // The overlay ids involved resolve to real sprites in MM6's doverlay.bin - unlike MM7, where every
    // doverlay entry is the null sprite.
    auto overlayHasRealSprite = [](int overlayId) {
        for (const OverlayDesc &desc : pOverlayList->pOverlays)
            if (desc.uOverlayID == overlayId)
                return desc.uSpriteFramesetID != 0;
        return false;
    };
    for (int overlayId : {10000, 10001, 10002, 10003, 10004, 10005, 10007, 10008})
        EXPECT_TRUE(overlayHasRealSprite(overlayId)) << "overlay " << overlayId;

    // Light every fx up and run the draw: Bless per character, the four party-level character rows,
    // and the three fixed anchors (Water Walk / Guardian Angel / Fly).
    Time buffEnd = pParty->GetPlayingTime() + Duration::fromHours(1);
    for (Character &character : pParty->pCharacters)
        character.pCharacterBuffs[CHARACTER_BUFF_BLESS].Apply(buffEnd, MASTERY_MASTER, 10, 0, 0);
    for (PartyBuff buff : {PARTY_BUFF_HEROISM, PARTY_BUFF_HASTE, PARTY_BUFF_SHIELD, PARTY_BUFF_STONE_SKIN,
                           PARTY_BUFF_WATER_WALK, PARTY_BUFF_FLY})
        pParty->pPartyBuffs[buff].Apply(buffEnd, MASTERY_MASTER, 10, 0, 1); // Caster is 1-based; Fly/Water Walk dereference it.
    pParty->_mm6GuardianAngelExpireTime = buffEnd;
    pParty->_mm6GuardianAngelMastery = MASTERY_NOVICE;
    game.tick(3);
    for (Character &character : pParty->pCharacters)
        character.pCharacterBuffs[CHARACTER_BUFF_BLESS].Reset();
    for (PartyBuff buff : {PARTY_BUFF_HEROISM, PARTY_BUFF_HASTE, PARTY_BUFF_SHIELD, PARTY_BUFF_STONE_SKIN,
                           PARTY_BUFF_WATER_WALK, PARTY_BUFF_FLY})
        pParty->pPartyBuffs[buff].Reset();
    pParty->_mm6GuardianAngelExpireTime = Time();
    pParty->_mm6GuardianAngelMastery = MASTERY_NONE;
    game.tick(1);

    // Milestone 71: turn-based ready gems follow the turn queue (MM6.EXE 0x486b92) - toggling turn-based
    // mode exercises that draw branch; the monsters'-turn stage draws none.
    game.pressAndReleaseKey(PlatformKey::KEY_RETURN);
    game.tick(3);
    EXPECT_TRUE(pParty->bTurnBasedModeOn);
    game.pressAndReleaseKey(PlatformKey::KEY_RETURN);
    game.tick(3);
    EXPECT_FALSE(pParty->bTurnBasedModeOn);

    // Milestone 71: creation-screen stat colors anchor on the CLASS base (MM6.EXE table 0x4C2668; MM6 has
    // no races). Roderick is a Paladin - Might base 14.
    Character &roderick = pParty->pCharacters[0];
    ASSERT_EQ(roderick.classType, CLASS_PALADIN);
    int savedMight = roderick._stats[ATTRIBUTE_MIGHT];
    roderick._stats[ATTRIBUTE_MIGHT] = 14;
    Color defaultColor = roderick.GetStatColor(ATTRIBUTE_MIGHT);
    roderick._stats[ATTRIBUTE_MIGHT] = 15;
    Color buffedColor = roderick.GetStatColor(ATTRIBUTE_MIGHT);
    roderick._stats[ATTRIBUTE_MIGHT] = 12;
    Color debuffedColor = roderick.GetStatColor(ATTRIBUTE_MIGHT);
    roderick._stats[ATTRIBUTE_MIGHT] = savedMight;
    EXPECT_EQ(defaultColor, ui_character_stat_default_color);
    EXPECT_EQ(buffedColor, ui_character_stat_buffed_color);
    EXPECT_EQ(debuffedColor, ui_character_stat_debuffed_color);
}

// The MM6 quick-reference screen: MM6's quikref bitmap is only the box frame (teal-keyed interior)
// and goes at (0,0) - the EXE first fills the viewport with LEATHER + the (7,8)/(461,8) corner
// patches (MM6.EXE 0x416884..0x4168d3), draws the character columns at 91+94*i (0x416919), and
// blits the buttexi1 exit button at (391,316) every frame (0x417387). OE used to draw only the
// frame, at MM7's (8,8) - a black screen with 8px-misaligned text and no visible exit button
// (play-test report).
GAME_TEST(Mm6, QuickReferenceScreen) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(3);

    engine->_messageQueue->addMessageCurrentFrame(UIMSG_QuickReference, 0, 0);
    game.tick(3); // Opens the screen and runs its draw path.
    EXPECT_EQ(current_screen_type, SCREEN_QUICK_REFERENCE);
    // Every piece resolves to real MM6 art of the original dimensions.
    EXPECT_EQ(assets->getImage_ColorKey("quikref")->size(), Sizei(460, 346)); // The frame-only bitmap.
    EXPECT_EQ(ui_leather_mm7->size(), Sizei(460, 344));                       // The fill under it.
    EXPECT_EQ(assets->getImage_Solid("buttexi1")->size(), Sizei(64, 32));     // The exit-button art.

    // The exit hitbox matches the EXE's CreateButton (MM6.EXE 0x42ceb0: 391,316 + 75x33 as a
    // closed interval, stored half-open as +1).
    EXPECT_EQ(pBtn_ExitCancel->rect, Recti(391, 316, 76, 34));

    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
}

// MM6 shows the corner minimap at all times. MM6.EXE calls DrawMinimap (0x437220) unconditionally
// from the present loop (@0x43524c, rect x [480,632) x y [25,140)); the wizard-eye buff (plus the
// Cartographer hireling @0x4372c2) gates only the object/actor dots inside it. MAPBACK is the navy
// cloud backing the INDOOR outline map (blitted @0x417e05 when the level-type global 0x6107d4 says
// indoor), not a wizard-eye parchment. OE used to model the whole minimap as wizard-eye-only, which
// left the tapestry window showing its black backing fill in normal play (play-test report #6).
GAME_TEST(Mm6, MinimapAlwaysVisible) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(3);
    EXPECT_FALSE(pParty->wizardEyeActive());

    // Outdoors: the map content source is the real per-map minimap texture from MM6's icons.lod
    // (oute3 for New Sorpigal), not the "pending" placeholder.
    ASSERT_NE(viewparams->location_minimap, nullptr);
    EXPECT_EQ(viewparams->location_minimap->size(), Sizei(512, 512));

    // Indoors: the minimap outline pass runs every frame WITHOUT Wizard Eye, marking seen outlines -
    // the observable side effect of the map content path actually drawing.
    MapId goblinwatch = pMapStats->GetMapInfo("d01.blv");
    ASSERT_NE(goblinwatch, MAP_INVALID);
    game.teleportTo(goblinwatch, Vec3f(-1850, 4304, -512), 0); // First spawn point of d01.blv.
    game.tick(3);
    bool anyOutlineSeen = false;
    for (char flags : pIndoor->_visible_outlines)
        anyOutlineSeen = anyOutlineSeen || flags != 0;
    EXPECT_TRUE(anyOutlineSeen);
}

// Milestone 72: the quest/autonotes/map/calendar books draw from MM6's own assets - the shared
// `book` base + `tabexit` close tab (MM6.EXE book dispatcher 0x40ebd0), per-book quest_bg /
// note_bg / time_bg parchments at (47,22), the tab+/tab-- page tabs at (415,13)/(415,48) (top =
// next page in MM6), five autonote category tabs (no teacher notes), zoom+/zoom- and N/S/W/E map
// tabs, and the calendar's moon-phase image at (266,198). Previously these screens loaded MM7
// asset names (sbquiknot/sbautnot/sbmap/sbdate-time/tab-an-*) that MM6's data does not have.
GAME_TEST(Mm6, BookScreens) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // The MM6 book assets all resolve with their real dimensions.
    EXPECT_EQ(assets->getImage_Solid("book")->size(), Sizei(460, 344));      // Fills the viewport.
    EXPECT_EQ(assets->getImage_Alpha("tabexit")->size(), Sizei(55, 17));
    EXPECT_EQ(assets->getImage_Solid("quest_bg")->size(), Sizei(360, 300));
    EXPECT_EQ(assets->getImage_Solid("note_bg")->size(), Sizei(360, 300));
    EXPECT_EQ(assets->getImage_Solid("time_bg")->size(), Sizei(360, 300));
    EXPECT_EQ(assets->getImage_Alpha("tab+on")->size(), Sizei(39, 36));
    EXPECT_EQ(assets->getImage_Alpha("tab--off")->size(), Sizei(39, 36));
    EXPECT_EQ(assets->getImage_Alpha("anot1on")->size(), Sizei(39, 36));
    EXPECT_EQ(assets->getImage_Alpha("zoom+on")->size(), Sizei(39, 36));
    EXPECT_EQ(assets->getImage_Solid("lb_pl_bg")->size(), Sizei(411, 306));
    EXPECT_EQ(assets->getImage_Solid("lb_go_bg")->size(), Sizei(411, 306));

    // MM6's autonote.txt type column parses into the same five categories its EXE tab table
    // (0x4bc1f8) groups: potion recipes 54-78, fountain stats 1-53, obelisks 79-93, seer/quest
    // hints 108-116, misc 97-107/117-128.
    EXPECT_EQ(pAutonoteTxt[54].eType, AUTONOTE_POTION_RECIPE);
    EXPECT_EQ(pAutonoteTxt[1].eType, AUTONOTE_STAT_HINT);
    EXPECT_EQ(pAutonoteTxt[79].eType, AUTONOTE_OBELISK);
    EXPECT_EQ(pAutonoteTxt[116].eType, AUTONOTE_SEER);
    EXPECT_EQ(pAutonoteTxt[100].eType, AUTONOTE_MISC);

    // The pressed book-button sprite is NOT drawn at the button rect (y=263): MM6.EXE's open-book
    // handlers (0x42e996/0x42ea12/0x42eaab/0x42eb22) place the overlay at y=270, with autonotes and
    // maps also 1px left of their buttons (play-test report: pressed tomes sat ~5px too high).
    auto booksOverlayPos = []() -> Pointi {
        for (GUIWindow *window : lWindowList)
            if (window->eWindowType == WINDOW_BooksButtonOverlay)
                return window->frameRect.topLeft();
        return Pointi(-1, -1);
    };

    // Quest book: opens, MM6 page-tab buttons at (415,13)/(415,48) (CreateButton stores w+1/h+1),
    // page-flip messages run the MM6 draw, Escape closes.
    engine->_messageQueue->addMessageCurrentFrame(UIMSG_OpenQuestBook, 0, 0);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_BOOKS);
    EXPECT_EQ(booksOverlayPos(), Pointi(495, 270));
    EXPECT_EQ(pBtn_Book_1->rect, Recti(415, 13, 51, 35));
    EXPECT_EQ(pBtn_Book_2->rect, Recti(415, 48, 51, 35));
    engine->_messageQueue->addMessageCurrentFrame(UIMSG_ClickBooksBtn, std::to_underlying(BOOK_NEXT_PAGE), 0);
    game.tick(2);
    engine->_messageQueue->addMessageCurrentFrame(UIMSG_ClickBooksBtn, std::to_underlying(BOOK_PREV_PAGE), 0);
    game.tick(2);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_GAME);

    // Grant a few notes across categories, then drive the autonotes book through every MM6 tab.
    pParty->_autonoteBits.set(54);   // A potion recipe.
    pParty->_autonoteBits.set(1);    // A fountain hint.
    pParty->_autonoteBits.set(79);   // An obelisk piece.
    engine->_messageQueue->addMessageCurrentFrame(UIMSG_OpenAutonotes, 0, 0);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_BOOKS);
    EXPECT_EQ(booksOverlayPos(), Pointi(526, 270));
    for (BookButtonAction action : {BOOK_NOTES_POTION, BOOK_NOTES_FOUNTAIN, BOOK_NOTES_OBELISK, BOOK_NOTES_SEER, BOOK_NOTES_MISC}) {
        engine->_messageQueue->addMessageCurrentFrame(UIMSG_ClickBooksBtn, std::to_underlying(action), 0);
        game.tick(2);
    }
    EXPECT_EQ(autonoteBookDisplayType, AUTONOTE_MISC);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);

    // Map book: MM6 tab order runs N/S/W/E with West above East; zoom buttons sit at the page-tab
    // spots. Exercise a zoom and a scroll.
    engine->_messageQueue->addMessageCurrentFrame(UIMSG_OpenMapBook, 0, 0);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_BOOKS);
    EXPECT_EQ(booksOverlayPos(), Pointi(557, 270));
    EXPECT_EQ(pBtn_Book_1->rect, Recti(415, 13, 51, 35));
    engine->_messageQueue->addMessageCurrentFrame(UIMSG_ClickBooksBtn, std::to_underlying(BOOK_ZOOM_IN), 0);
    game.tick(2);
    engine->_messageQueue->addMessageCurrentFrame(UIMSG_ClickBooksBtn, std::to_underlying(BOOK_SCROLL_LEFT), 0);
    game.tick(2);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);

    // Calendar: the moon-phase image draws at (266,198) for the current day.
    engine->_messageQueue->addMessageCurrentFrame(UIMSG_OpenCalendar, 0, 0);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_BOOKS);
    EXPECT_EQ(booksOverlayPos(), Pointi(588, 270));
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_GAME);
}

// Milestone 73: the spellbook screen draws from MM6's own assets - the shared `book` parchment +
// `pagemask`, TABSPELL/TABEXIT tabs at the page's bottom edge, school tabs at (414/421, 13+35*i),
// and each school page composed from per-spell patch images ({prefix}000 emblem + {prefix}NNN per
// known spell) with the spell name drawn under each icon. Previously it loaded MM7's SBxB00 page
// backgrounds and SBxS##/SBxC## icons, which MM6's data does not have (MM6.EXE ctor 0x40ce30,
// draw 0x40ddc0, prefix table 0x4bc3d8, position tables 0x4bc75c/0x4bc90c/0x4bc3fc/0x4bc5ac).
GAME_TEST(Mm6, SpellbookScreen) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // The MM6 spellbook assets resolve; the per-spell patches exist for every school prefix.
    EXPECT_EQ(assets->getImage_Alpha("pagemask")->size(), Sizei(460, 344));
    EXPECT_EQ(assets->getImage_Alpha("tabspell")->size(), Sizei(55, 17));
    for (const char *name : {"fire000", "fire001", "fire011", "air001", "wtr001", "earth001",
                             "sprt001", "mind001", "body001", "lite001", "dark001"})
        EXPECT_GT(assets->getImage_Alpha(name)->width(), 1) << name;

    // Zoltan the sorcerer (4th character) knows Torch Light (fire #1); make him active and open
    // the spellbook.
    pParty->setActiveCharacterIndex(4);
    ASSERT_EQ(pParty->activeCharacter().classType, CLASS_SORCERER);
    ASSERT_TRUE(pParty->activeCharacter().bHaveSpell[SPELL_FIRE_TORCH_LIGHT]);

    engine->_messageQueue->addMessageCurrentFrame(UIMSG_SpellBookWindow, 0, 0);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_SPELL_BOOK);
    game.tick(2); // Draw smoke through the MM6 page composition.

    // The known spell got a button at its MM6 slot position (fire slot 1 icon at (198,32)) and the
    // fire school tab button sits at (410,13).
    GUIButton *spellButton = nullptr;
    bool foundSchoolButton = false;
    for (GUIButton *button : pGUIWindow_CurrentMenu->vButtons) {
        if (button->msg == UIMSG_SelectSpell && button->msg_param == std::to_underlying(SPELL_FIRE_TORCH_LIGHT)) {
            EXPECT_EQ(button->rect.topLeft(), Pointi(198, 32));
            spellButton = button;
        }
        if (button->msg == UIMSG_OpenSpellbookPage && button->msg_param == std::to_underlying(MAGIC_SCHOOL_FIRE)) {
            EXPECT_EQ(button->rect.topLeft(), Pointi(410, 13));
            foundSchoolButton = true;
        }
    }
    ASSERT_NE(spellButton, nullptr);
    EXPECT_TRUE(foundSchoolButton);

    // MM6 interaction model (MM6.EXE 0x42cc75/0x42ca8d): opening the book starts with nothing
    // selected (0x40ccca resets the selected-slot flag, quickspell included); the first click on
    // a spell selects it, TABSPELL installs the selection as the quickspell and closes the book,
    // and the school-emblem click region clears the installed quickspell.
    EXPECT_EQ(spellbookSelectedSpell, SPELL_NONE);
    Pointi spellCenter = spellButton->rect.center();
    game.pressAndReleaseButton(BUTTON_LEFT, spellCenter.x, spellCenter.y);
    game.tick(2);
    EXPECT_EQ(spellbookSelectedSpell, SPELL_FIRE_TORCH_LIGHT); // First click selects...
    EXPECT_EQ(current_screen_type, SCREEN_SPELL_BOOK);         // ...it does not cast.

    game.pressAndReleaseButton(BUTTON_LEFT, 301 + 27, 332 + 8); // TABSPELL (301,332,55x17).
    game.tick(2);
    EXPECT_EQ(pParty->activeCharacter().uQuickSpell, SPELL_FIRE_TORCH_LIGHT);
    EXPECT_EQ(current_screen_type, SCREEN_GAME); // Installing the quickspell closes the book.

    engine->_messageQueue->addMessageCurrentFrame(UIMSG_SpellBookWindow, 0, 0);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_SPELL_BOOK);
    EXPECT_EQ(spellbookSelectedSpell, SPELL_NONE); // Reset on open despite the installed quickspell.
    game.pressAndReleaseButton(BUTTON_LEFT, 48 + 63, 18 + 41); // The school-emblem click region.
    game.tick(2);
    EXPECT_EQ(pParty->activeCharacter().uQuickSpell, SPELL_NONE);

    // A second click on the selected spell casts it and closes the book.
    game.pressAndReleaseButton(BUTTON_LEFT, spellCenter.x, spellCenter.y);
    game.tick(2);
    EXPECT_EQ(spellbookSelectedSpell, SPELL_FIRE_TORCH_LIGHT);
    game.pressAndReleaseButton(BUTTON_LEFT, spellCenter.x, spellCenter.y);
    game.tick(5);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
    EXPECT_TRUE(pParty->pPartyBuffs[PARTY_BUFF_TORCHLIGHT].Active()); // Torch Light landed.
}

// Milestone 42: the character screen draws from MM6's own skin - leather + fr_* parchments,
// BUTT* tab buttons at MM6's rects, and the MM6 paper doll (per-face body/arms, armor doll
// variants, items at their items.txt anchors) with the BACKHAND rings view.
GAME_TEST(Mm6, CharacterScreenSkin) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // Dress the active character to exercise every doll branch: bow behind the body, cape back
    // side, chain-mail doll variant (CHN2), closed helm, belt, boots at their anchors - plus the
    // starting equipped weapon.
    Character &active = pParty->activeCharacter();
    auto equipInto = [&](ItemSlot slot, int itemId) {
        if (InventoryEntry existing = active.inventory.entry(slot))
            active.inventory.take(existing);
        ASSERT_TRUE(active.inventory.equip(slot, Item(ItemId(itemId))));
    };
    equipInto(ITEM_SLOT_BOW, 43);      // bow2.
    equipInto(ITEM_SLOT_CLOAK, 106);   // cape2a -> CAPE2B back side.
    equipInto(ITEM_SLOT_ARMOUR, 72);   // chn2icon -> CHN2 BOD/ARM1/ARM2 doll art.
    equipInto(ITEM_SLOT_HELMET, 90);   // hlm2icon -> HELM2 doll art.
    equipInto(ITEM_SLOT_BELT, 101);    // belt2a -> BELT2B doll art.
    equipInto(ITEM_SLOT_BOOTS, 116);   // boots2, drawn at its items.txt anchor.

    game.pressAndReleaseKey(PlatformKey::KEY_I);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_CHARACTERS);

    // The backgrounds are MM6's own: fr_* share MM7's names but have MM6's dimensions, and the
    // doll panel assets are BACKDOLL/BACKHAND/MAGNIF-B from MM6 icons.lod.
    EXPECT_EQ(ui_character_stats_background->size(), Sizei(443, 295));
    EXPECT_EQ(ui_character_skills_background->size(), Sizei(443, 299));
    EXPECT_EQ(ui_character_awards_background->size(), Sizei(443, 299));
    EXPECT_EQ(ui_character_inventory_background->size(), Sizei(449, 289));
    EXPECT_EQ(ui_character_inventory_paperdoll_background->size(), Sizei(173, 353));
    EXPECT_EQ(assets->getImage_Alpha("backhand")->size(), Sizei(153, 353));
    EXPECT_EQ(assets->getImage_Alpha("magnif-b")->size(), Sizei(33, 51));
    EXPECT_EQ(ui_leather_mm7->size(), Sizei(460, 344));  // LEATHER fills the viewport.

    // Doll art for the default party resolves per face (Roderick face 0 = mla*), and the
    // hand overlays are MM6's.
    EXPECT_EQ(assets->getImage_Alpha("mlabod")->size(), Sizei(114, 298));
    EXPECT_EQ(assets->getImage_Alpha("rthand")->size(), Sizei(18, 19));
    EXPECT_EQ(assets->getImage_Alpha("lefthand")->size(), Sizei(20, 19));

    // MM6 tab buttons: 75x33 at y=318, x=22/115/208/301/394 (MM6.EXE 0x41fa60), BUTT*1 up /
    // BUTT*2 pressed. CreateButton stores w+1/h+1 (closed-interval heritage).
    EXPECT_EQ(pCharacterScreen_StatsBtn->rect, Recti(22, 318, 76, 34));
    EXPECT_EQ(pCharacterScreen_SkillsBtn->rect, Recti(115, 318, 76, 34));
    EXPECT_EQ(pCharacterScreen_InventoryBtn->rect, Recti(208, 318, 76, 34));
    EXPECT_EQ(pCharacterScreen_AwardsBtn->rect, Recti(301, 318, 76, 34));
    EXPECT_EQ(pCharacterScreen_ExitBtn->rect, Recti(394, 318, 76, 34));
    EXPECT_EQ(pCharacterScreen_StatsBtn->vTextures[0], assets->getImage_Solid("buttsta1"));
    EXPECT_EQ(pCharacterScreen_StatsBtn->vTextures[1], assets->getImage_Solid("buttsta2"));
    EXPECT_EQ(pCharacterScreen_ExitBtn->vTextures[0], assets->getImage_Solid("buttexi1"));

    // Every tab draws with the doll beside it (the doll composes on all tabs in MM6).
    for (UIMessageType message : {UIMSG_ClickStatsBtn, UIMSG_ClickSkillsBtn, UIMSG_ClickAwardsBtn, UIMSG_ClickInventoryBtn}) {
        engine->_messageQueue->addMessageCurrentFrame(message, 0, 0);
        game.tick(2);
    }

    // The magnifier at (600,300) toggles the rings view (BACKHAND + guy_up + accessories). In the
    // rings view the toggle button moves to guy_up's own spot - (527,300) at guy_up's 64x32 size,
    // centered on the BACKHAND panel (MM6.EXE 0x42cfda) - and clicking it there toggles back.
    game.pressAndReleaseButton(BUTTON_LEFT, 615, 315);
    game.tick(2);
    EXPECT_TRUE(ringscreenactive());
    EXPECT_EQ(pCharacterScreen_DetalizBtn->rect, Recti(527, 300, 65, 33));
    game.pressAndReleaseButton(BUTTON_LEFT, 540, 315);
    game.tick(2);
    EXPECT_FALSE(ringscreenactive());
    EXPECT_EQ(pCharacterScreen_DetalizBtn->rect, Recti(600, 300, 31, 31));

    // A two-handed main-hand weapon switches the doll to the grip pose (arm2 + ARM2 sleeve).
    if (InventoryEntry offhand = active.inventory.entry(ITEM_SLOT_OFF_HAND))
        active.inventory.take(offhand);
    equipInto(ITEM_SLOT_MAIN_HAND, 6);  // A two-handed weapon (Weapon2 row).
    game.tick(2);

    // Every doll-visible artifact/relic draws through its own branch: chain artifacts reuse the
    // CHN5 doll variant, plates PL3, crowns CROWN3B, and the three dual-wieldable blades use the
    // EXE's per-id left-hand coords. Fit within BACKDOLL eyeballed via an offline composite of
    // the same assets at the same coordinates (milestone 77).
    equipInto(ITEM_SLOT_MAIN_HAND, 403);  // Excalibur.
    equipInto(ITEM_SLOT_OFF_HAND, 408);   // Valeria shield.
    equipInto(ITEM_SLOT_ARMOUR, 406);     // Galahad chain -> CHN5.
    equipInto(ITEM_SLOT_HELMET, 409);     // Arthur crown -> CROWN3B.
    equipInto(ITEM_SLOT_CLOAK, 410);      // Pendragon cape.
    equipInto(ITEM_SLOT_BOOTS, 411);      // Lucius boots.
    equipInto(ITEM_SLOT_BOW, 405);        // Percival bow.
    game.tick(2);
    equipInto(ITEM_SLOT_MAIN_HAND, 415);  // Hades sword.
    equipInto(ITEM_SLOT_OFF_HAND, 423);   // Aegis shield.
    equipInto(ITEM_SLOT_ARMOUR, 407);     // Pellinore plate -> PL3.
    equipInto(ITEM_SLOT_HELMET, 424);     // Odin crown -> CROWN3B.
    game.tick(2);
    for (int armor : {421, 422}) {        // Apollo chain / Zeus plate.
        equipInto(ITEM_SLOT_ARMOUR, armor);
        game.tick(1);
    }
    for (int blade : {400, 403, 415}) {   // Dual-wield left-hand coords: Mordred/Excalibur/Hades.
        equipInto(ITEM_SLOT_OFF_HAND, blade);
        game.tick(1);
    }
    if (InventoryEntry offhand = active.inventory.entry(ITEM_SLOT_OFF_HAND))
        active.inventory.take(offhand);
    for (int grip : {402, 404, 419, 417}) {  // Conan/Merlin/Hercules/Poseidon in the grip pose.
        equipInto(ITEM_SLOT_MAIN_HAND, grip);
        game.tick(1);
    }
    ASSERT_EQ(current_screen_type, SCREEN_CHARACTERS);

    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
}

// Milestone 68: the character-sheet tab CONTENT is MM6-shaped. awards.txt loads despite having
// no priority column (every row used to be skipped as "truncated"), award colors come from
// MM6's id bands (MM6.EXE 0x4164a4), the awards arrows sit at MM6's rects (0x4152d8/0x41534a),
// and the stats tab draws MM6's five-resistance block (0x413938).
GAME_TEST(Mm6, CharacterSheetTabContent) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // awards.txt parses: two columns only, so the MM7 parse (which requires a third priority
    // column) would leave every entry empty. Priorities are MM6's id bands.
    EXPECT_EQ(pAwards[static_cast<AwardId>(1)].pText, "Returned the Prince");
    EXPECT_EQ(pAwards[static_cast<AwardId>(81)].pText, "Collected %u bounties");
    EXPECT_EQ(pAwards[static_cast<AwardId>(82)].pText, "%u Deaths");
    EXPECT_EQ(pAwards[static_cast<AwardId>(83)].pText, "Served %u Prison Terms");
    EXPECT_EQ(pAwards[static_cast<AwardId>(1)].uPriority, 0);   // Quest awards: Magenta.
    EXPECT_EQ(pAwards[static_cast<AwardId>(8)].uPriority, 1);   // Promotions: Malibu.
    EXPECT_EQ(pAwards[static_cast<AwardId>(32)].uPriority, 2);  // Obelisk/specials: MoonRaker.
    EXPECT_EQ(pAwards[static_cast<AwardId>(37)].uPriority, 3);  // ScreaminGreen.
    EXPECT_EQ(pAwards[static_cast<AwardId>(64)].uPriority, 4);  // Guild memberships: Canary.
    EXPECT_EQ(pAwards[static_cast<AwardId>(81)].uPriority, 5);  // Counted awards: Mimosa.

    // Give the party a mix of awards spanning the bands, including counted ones.
    pParty->uNumDeaths = 3;
    pParty->uNumBountiesCollected = 2;
    for (Character &character : pParty->pCharacters) {
        character._achievedAwardsBits.set(static_cast<AwardId>(1), true);
        character._achievedAwardsBits.set(static_cast<AwardId>(64), true);
        character._achievedAwardsBits.set(static_cast<AwardId>(81), true);
        character._achievedAwardsBits.set(static_cast<AwardId>(82), true);
    }

    game.pressAndReleaseKey(PlatformKey::KEY_I);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_CHARACTERS);

    // Every tab draws its MM6 content - the stats tab runs the five-resistance MM6 layout, the
    // awards tab formats the counted rows and colors by band.
    for (UIMessageType message : {UIMSG_ClickStatsBtn, UIMSG_ClickSkillsBtn, UIMSG_ClickAwardsBtn, UIMSG_ClickInventoryBtn}) {
        engine->_messageQueue->addMessageCurrentFrame(message, 0, 0);
        game.tick(2);
    }

    // The awards scrollbar arrows anchor at MM6's rects and load the palette-0-transparent
    // arrow art (the strip click zone at (440,62) 16x232 is shared with MM7).
    engine->_messageQueue->addMessageCurrentFrame(UIMSG_ClickAwardsBtn, 0, 0);
    game.tick(2);
    ASSERT_NE(pBtn_Up, nullptr);
    ASSERT_NE(pBtn_Down, nullptr);
    EXPECT_EQ(pBtn_Up->rect.topLeft(), Pointi(440, 45));
    EXPECT_EQ(pBtn_Down->rect.topLeft(), Pointi(440, 294));
    EXPECT_EQ(pBtn_Up->vTextures[0], assets->getImage_Alpha("ar_up_up"));
    EXPECT_EQ(pBtn_Down->vTextures[1], assets->getImage_Alpha("ar_dn_dn"));

    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
}

GAME_TEST(Mm6, MainMenuSkin) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.goToMainMenu();

    // MM6's title.pcx has the four buttons baked in along the top right; MM6.EXE creates 132x44
    // hitboxes at x=482, y=9/71/133/195 (CreateButton calls @0x4507bf-0x4508c5). Each button owns
    // the seven frames of its glow ramp ("start%02d%c" @0x4508f9), which the menu loop pulses
    // through while the cursor is over the button.
    auto buttonById = [](std::string_view id) -> GUIButton * {
        for (GUIWindow *window : lWindowList)
            for (GUIButton *button : window->vButtons)
                if (button->id == id)
                    return button;
        return nullptr;
    };
    struct { const char *id; int y; char letter; } expected[4] = {
        {"MainMenu_NewGame", 9, 'a'},
        {"MainMenu_LoadGame", 71, 'b'},
        {"MainMenu_Credits", 133, 'c'},
        {"MainMenu_ExitGame", 195, 'd'},
    };
    for (const auto &e : expected) {
        GUIButton *button = buttonById(e.id);
        ASSERT_NE(button, nullptr) << e.id;
        // CreateButton stores w+1/h+1: the original engines (MM6.EXE included, hit test @0x450a81)
        // treated button rects as closed intervals, so a 132x44 button covers 133x45 pixels.
        EXPECT_EQ(button->rect, Recti(482, e.y, 133, 45)) << e.id;
        ASSERT_EQ(button->vTextures.size(), 7u) << e.id;
        for (int frame = 0; frame < 7; frame++)
            EXPECT_EQ(button->vTextures[frame], assets->getImage_Alpha(fmt::format("start{:02}{:c}", frame, e.letter))) << e.id << " frame " << frame;
    }

    // The glow is a free-running triangle wave shared by all four buttons: MM6.EXE steps a single
    // counter every 50ms and flips its direction below 1 and above 5 (@0x450b52), so the ramp runs
    // 0..6 and back down over 12 steps.
    const int expectedFrames[12] = {0, 1, 2, 3, 4, 5, 6, 5, 4, 3, 2, 1};
    for (int step = 0; step < 12; step++) {
        EXPECT_EQ(mm6MainMenuGlowFrame(50 * step), expectedFrames[step]) << "step " << step;
        EXPECT_EQ(mm6MainMenuGlowFrame(50 * step + 49), expectedFrames[step]) << "step " << step;
        // The pulse is periodic, and never restarts - hovering a second button doesn't reset it.
        EXPECT_EQ(mm6MainMenuGlowFrame(50 * (step + 12)), expectedFrames[step]) << "step " << step;
    }

    // A click on MM7's button column (x=495 from y=172) below the MM6 buttons is empty scenery
    // and must do nothing.
    game.pressAndReleaseButton(BUTTON_LEFT, 550, 300);
    game.tick(2);
    EXPECT_EQ(GetCurrentMenuID(), MENU_MAIN);

    // A click dead center on the NEW button opens the new-game prologue screen (MM6.EXE 0x452BD0),
    // and party creation is one more click away, on its Create Party button.
    game.pressAndReleaseButton(BUTTON_LEFT, 548, 31);
    game.tick(2);
    game.pressGuiButton("Mm6Segue_CreateParty");
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_PARTY_CREATION);
}

// Deliberately NOT guarded on gameVersion(), unlike every other Mm6 test here: mm6SegueScrollY()
// is a pure function of elapsed time and needs no MM6 data, so it can and should run in upstream
// CI, which has MM7 assets but no MM6 install. Don't "fix" the odd one out by adding a skip.
GAME_TEST(Mm6, SegueScroll) {
    // MM6.EXE holds the crawl still for 29s (start + 0x7148 @0x452fba), then pans it down one
    // pixel every 50ms (0x32 @0x453217), clamped at 900 - 320 = 580 (@0x4531fd), which parks the
    // bottom of seg_scrl.pcx - the gate scene - in the window of the static segue_bg.pcx frame.
    EXPECT_EQ(mm6SegueScrollY(0), 0);
    EXPECT_EQ(mm6SegueScrollY(28999), 0);
    EXPECT_EQ(mm6SegueScrollY(29000), 0);
    EXPECT_EQ(mm6SegueScrollY(29049), 0);
    EXPECT_EQ(mm6SegueScrollY(29050), 1);
    EXPECT_EQ(mm6SegueScrollY(29000 + 50 * 100), 100);

    // Negative elapsed is safe (early return), and must stay that way.
    EXPECT_EQ(mm6SegueScrollY(-1), 0);

    // Clamped at the bottom, and it stays there.
    EXPECT_EQ(mm6SegueScrollY(58000), 580); // The crawl's full length: 29000 + 580 * 50.
    EXPECT_EQ(mm6SegueScrollY(29000 + 50 * 580), 580);
    EXPECT_EQ(mm6SegueScrollY(29000 + 50 * 581), 580);
    EXPECT_EQ(mm6SegueScrollY(10'000'000), 580);

    // Monotonic, never out of range.
    int previous = 0;
    for (int64_t ms = 0; ms <= 70'000; ms += 37) {
        int y = mm6SegueScrollY(ms);
        EXPECT_GE(y, previous);
        EXPECT_LE(y, 580);
        previous = y;
    }
}

// Also pure, also runs without MM6 data - and this one is the reason the geometry is a function at
// all. The crawl's draw coordinates used to live in `static constexpr`s inside UIMm6Segue.cpp, which
// meant nothing could assert them, which is exactly how they came to be off by (10, 20). Pin them.
GAME_TEST(Mm6, SegueLayout) {
    // MM6's text routine (@0x443a40) halves the left edge it's handed, and the segue hands it 0x14
    // (@0x453128) - so the crawl is inset 10px, not 20, and 10px is the margin on *each* side of a
    // rect that's the full 512 less all 20 (@0x45311d). Top is the literal 0 pushed @0x45312f.
    EXPECT_EQ(mm6SegueTextWidth(), 492);

    // The viewport is at (64, 104), so the first line of the crawl starts at (64 + 10, 104 + 0)
    // while the pan is still held at the top.
    constexpr int spacing = 17; // quick.fnt is 20 tall, and MM6 steps height - 3 (@0x443b0a-0x443b12).
    EXPECT_EQ(mm6SegueLinePos(0, 0, spacing, 0), Pointi(74, 104));

    // Successive lines step down by the line spacing, and the whole block rides the pan up.
    EXPECT_EQ(mm6SegueLinePos(1, 0, spacing, 0), Pointi(74, 121));
    EXPECT_EQ(mm6SegueLinePos(10, 0, spacing, 0), Pointi(74, 274));
    EXPECT_EQ(mm6SegueLinePos(0, 0, spacing, 580), Pointi(74, -476));
    EXPECT_EQ(mm6SegueLinePos(10, 0, spacing, 580), Pointi(74, -306));

    // A line's centering offset shifts it right, and only right - the whole point of the x fix is
    // that a line as wide as the rect sits 10px from the window's left edge, not 20.
    EXPECT_EQ(mm6SegueLinePos(0, 0, spacing, 0).x, 64 + 10);
    EXPECT_EQ(mm6SegueLinePos(0, 246, spacing, 0).x, 64 + 10 + 246);

    // And the margins are symmetric: a line of width w centered in the 492-wide rect leaves the same
    // gap on both sides of the 512-wide window. This is the property the old {20, 20} broke - it put
    // the crawl's widest line 23px from the left edge and 3px from the right.
    for (int w : {0, 1, 107, 300, 486, 491, 492}) {
        int x = mm6SegueLinePos(0, (492 - w) / 2, spacing, 0).x;
        int leftMargin = x - 64;
        int rightMargin = 64 + 512 - (x + w);
        EXPECT_LE(std::abs(leftMargin - rightMargin), 1) << "line width " << w; // Odd widths round.
        EXPECT_GE(leftMargin, 10) << "line width " << w;
    }
}

// Unlike `Mm6.SegueScroll` above, this one does need MM6 data - everything it checks is an asset.
GAME_TEST(Mm6, SegueAssets) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    // The window that the crawl shows through, at (64, 104) - see kMm6SegueViewport.
    constexpr Sizei viewport = {512, 320};

    // The prologue screen's static frame: the MM6 logo banner, the window that the crawl shows
    // through, and both buttons' resting faces, all painted in.
    EXPECT_EQ(assets->getImage_PCXFromIconsLOD("segue_bg.pcx")->size(), Sizei(640, 480));

    // The crawl's backdrop, sky at the top and the New Sorpigal gate at the bottom. Its height is
    // load-bearing: mm6SegueScrollY() hardcodes a clamp of 580, which is this 900 less the viewport.
    // If the asset ever changes, that clamp is wrong - which is what this assertion guards.
    GraphicsImage *scroll = assets->getImage_PCXFromIconsLOD("seg_scrl.pcx");
    EXPECT_EQ(scroll->size(), Sizei(viewport.w, 900));
    EXPECT_EQ(scroll->height() - viewport.h, mm6SegueScrollY(1'000'000));

    // The two pressed frames - the only button art the screen ships. They're 218x40 against
    // MM6.EXE's 217x41 hitboxes (@0x452ec7 / @0x452ef1): a pixel wider and a pixel shorter.
    EXPECT_EQ(assets->getImage_Alpha("creat_dn")->size(), Sizei(218, 40));
    EXPECT_EQ(assets->getImage_Alpha("quick_dn")->size(), Sizei(218, 40));

    // intro.str is a run of NUL-terminated paragraphs, not a plain string - MM6.EXE rewrites every
    // NUL into a newline before drawing (@0x452dfe-0x452e0a). Read it as a plain string and the
    // crawl is one paragraph cut off at the first NUL, so the NULs have to be there.
    std::string prologue{engine->resources()->eventsData("intro.str").str()};
    EXPECT_FALSE(prologue.empty());
    EXPECT_GT(std::ranges::count(prologue, '\0'), 1);
    EXPECT_TRUE(prologue.contains("Having cheated death"));
    EXPECT_TRUE(prologue.contains("Good Luck"));
    std::ranges::replace(prologue, '\0', '\n');

    // quick.fnt is 20 tall, so the crawl steps 20 - 3 = 17px per line - MM6 reads the same height
    // byte out of the same .fnt header and subtracts the same 3 (@0x443b0a-0x443b12).
    std::unique_ptr<GUIFont> font = GUIFont::LoadFont("quick.fnt");
    EXPECT_EQ(font->GetHeight(), 20);

    // GUIFont::WrapText gives up and returns the string *unwrapped* the moment it sees a '\r'
    // (GUIFont.cpp, "this return is very sus"). Today's intro.str has none, but a localized or
    // patched one with CRLF paragraph breaks would silently run the crawl off the right edge of
    // the window, and nothing else in the suite would notice. Pin the wrapped layout instead:
    // every line fits the width the window wraps to, and the whole crawl fits inside seg_scrl.pcx.
    // As of MM6 1.0 that's 38 lines, the widest 486px, ending at y = 20 + 17 * 38 = 666.
    //
    // The lines are centered in that rect, not left-aligned - MM6.EXE @0x443ac1, see UIMm6Segue.cpp.
    // We don't hand-roll that arithmetic, we call `GUIFont::AlignText_Center`, so there's no segue
    // helper to test; pin the helper we depend on instead, on the crawl's own line widths. Every
    // centered line has to stay inside the window: offset >= 0, and offset + width <= the rect.
    //
    // The rect is the image width less MM6's 0x14 (@0x45311d), and the crawl is inset by *half* of
    // that - the text routine halves the left edge it's handed. `Mm6.SegueLayout` pins that; here we
    // just consume it, on the real line widths.
    constexpr int textInset = 10;                        // Half of MM6.EXE's 0x14 - see kMm6SegueTextOrigin.
    const int textWidth = mm6SegueTextWidth();           // @0x45311d: image width less all of the 0x14.
    const int lineSpacing = font->GetHeight() - 3;
    EXPECT_EQ(textWidth, viewport.w - 2 * textInset);
    std::string wrapped = font->WrapText(prologue, textWidth, 0);
    int lines = 0;
    int widest = 0;
    std::string widestLine;
    for (std::string_view line : split(wrapped).by('\n')) {
        int lineWidth = font->GetLineWidth(line);
        int offsetX = font->AlignText_Center(textWidth, line);
        EXPECT_LE(lineWidth, textWidth) << "unwrapped crawl line: " << line;
        EXPECT_EQ(offsetX, (textWidth - lineWidth) / 2) << "off-center crawl line: " << line;
        EXPECT_GE(offsetX, 0) << "crawl line drawn off the left edge: " << line;
        EXPECT_LE(textInset + offsetX + lineWidth, viewport.w) << "crawl line drawn off the right edge: " << line;
        if (lineWidth > widest) {
            widest = lineWidth;
            widestLine = line;
        }
        lines++;
    }
    EXPECT_LE(lineSpacing * lines, scroll->height()); // The crawl starts at row 0 of seg_scrl.pcx.

    // The two ends of the range the crawl actually produces: its widest line is nudged by 3px, and
    // its short last line lands near the middle of the window - which is the whole visible effect of
    // the centering, "Good Luck" resting above the gate's archway instead of off in the corner.
    EXPECT_EQ(widest, 486);
    EXPECT_EQ(font->AlignText_Center(textWidth, widestLine), 3);
    EXPECT_EQ(font->GetLineWidth("Good Luck"), 107);
    EXPECT_EQ(font->AlignText_Center(textWidth, "Good Luck"), 192);

    // Which is what the crawl's real screen coordinates come out as. The widest line sits 13px in
    // from *both* edges of the window - 10 of inset plus 3 of centering - and that symmetry is the
    // clearest confirmation that the inset really is 10 and not the 20 we used to draw it at.
    constexpr Recti window = {64, 104, viewport.w, viewport.h};
    int widestX = mm6SegueLinePos(0, font->AlignText_Center(textWidth, widestLine), lineSpacing, 0).x;
    EXPECT_EQ(widestX - window.x, 13);
    EXPECT_EQ(window.x + window.w - (widestX + widest), 13);

    // And "Good Luck" - the crawl's last line, the one left in the window when the pan stops - is
    // centered on the gate's archway, i.e. on the middle of the window, to within a pixel.
    int goodLuckX = mm6SegueLinePos(0, font->AlignText_Center(textWidth, "Good Luck"), lineSpacing, 0).x;
    EXPECT_EQ(goodLuckX + font->GetLineWidth("Good Luck") / 2, window.x + window.w / 2 - 1);

    // And a line wider than the rect clamps to 0 rather than going negative and drawing off the left
    // edge of the window. WrapText means the crawl never hits this, but the clamp is the reason we
    // can call `AlignText_Center` without checking its result.
    std::string overflowing(200, 'W');
    EXPECT_GT(font->GetLineWidth(overflowing), textWidth);
    EXPECT_EQ(font->AlignText_Center(textWidth, overflowing), 0);
}

GAME_TEST(Mm6, CreditsKeepMainMenuMusic) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    // MM6's credits are a movie clip routed through VideoState (MM6.EXE 0x4A6C90), whose enter()
    // pauses the menu music and the event timer. exit() must undo both: back in the menu,
    // MainMenuState::enter()'s MusicPlayTrack is swallowed by the currentMusicTrack identity check
    // (the track never changed), so nothing else ever resumes the music and the menu stays silent
    // for the rest of the session.
    game.goToMainMenu();
    ASSERT_TRUE(pAudioPlayer->isMusicPlaying());

    // The test harness runs with debug.NoVideo, which makes VideoState::enter() bail out before it
    // pauses anything - let the credits clip really play. prepareForNextTest resets the flag for
    // the tests that follow, but restore it below anyway so this test's own trailing ticks match
    // the harness defaults.
    engine->config->debug.NoVideo.setValue(false);

    game.pressGuiButton("MainMenu_Credits");
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_VIDEO);
    EXPECT_FALSE(pAudioPlayer->isMusicPlaying()); // Paused while the clip is up.

    // Any key skips the clip and drops back to the main menu.
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    engine->config->debug.NoVideo.setValue(true);
    ASSERT_EQ(GetCurrentMenuID(), MENU_MAIN);
    ASSERT_NE(current_screen_type, SCREEN_VIDEO);

    EXPECT_TRUE(pAudioPlayer->isMusicPlaying());
    EXPECT_FALSE(pEventTimer->isPaused());
}

// The prologue ("segue") screen's window, or nullptr if the screen isn't up. It's an fsm-owned
// window, so it isn't in pGUIWindow_CurrentMenu - only in the window list.
static GUIWindow *findMm6SegueWindow() {
    for (GUIWindow *window : lWindowList)
        if (window->eWindowType == WINDOW_Mm6Segue)
            return window;
    return nullptr;
}

GAME_TEST(Mm6, SegueSkin) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    // New Game opens the prologue screen (MM6.EXE 0x452bd0), not party creation.
    game.goToMainMenu();
    game.pressGuiButton("MainMenu_NewGame");
    game.tick(2);
    GUIWindow *segue = findMm6SegueWindow();
    ASSERT_NE(segue, nullptr);

    auto findButton = [&](UIMessageType msg) -> GUIButton * {
        for (GUIButton *button : segue->vButtons)
            if (button->msg == msg)
                return button;
        return nullptr;
    };

    // The two buttons, at MM6.EXE's own hitboxes: Create Party @0x452ec7, Quick Start @0x452ef1,
    // both 217x41 at y=434. Their resting faces are painted into segue_bg.pcx, so these coordinates
    // are all the screen has - get them wrong and the buttons are invisibly misplaced. CreateButton
    // stores w+1/h+1 (closed-interval heritage), hence the 218x42 below.
    GUIButton *createParty = findButton(UIMSG_Mm6Segue_CreateParty);
    GUIButton *quickStart = findButton(UIMSG_Mm6Segue_QuickStart);
    ASSERT_NE(createParty, nullptr);
    ASSERT_NE(quickStart, nullptr);
    EXPECT_EQ(createParty->rect, Recti(74, 434, 218, 42));
    EXPECT_EQ(quickStart->rect, Recti(350, 434, 218, 42));

    // And Create Party is what opens the creation screen.
    game.pressGuiButton("Mm6Segue_CreateParty");
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_PARTY_CREATION);
    EXPECT_EQ(findMm6SegueWindow(), nullptr); // The prologue is gone.
}

GAME_TEST(Mm6, SegueEscape) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    // Escape backs out of the prologue to the main menu. MM6 binds Escape on this screen (@0x452f32)
    // - where its handler goes there wasn't traced, so the destination is our choice, not a port.
    game.goToMainMenu();
    game.pressGuiButton("MainMenu_NewGame");
    game.tick(2);
    ASSERT_NE(findMm6SegueWindow(), nullptr);

    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    EXPECT_EQ(GetCurrentMenuID(), MENU_MAIN);
    EXPECT_EQ(findMm6SegueWindow(), nullptr); // Really gone, not just covered.

    // New Game opens it again.
    game.pressGuiButton("MainMenu_NewGame");
    game.tick(2);
    EXPECT_NE(findMm6SegueWindow(), nullptr);
}

GAME_TEST(Mm6, SegueQuickStart) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    // Quick Start skips the creation screen and starts with MM6's fully-built template party.
    // MM6.EXE's handler (@0x42fe0e) sets screen id 10 and its caller (@0x453664) fills the party
    // in from global.txt rows 506-509 (the fill at 0x485540) - NOT the half-blank SetClass default
    // party the creation screen opens with. The MM6 fidelity of the template party is what
    // Mm6.NewGameDefaults pins, against new.lod's party.bin.
    game.goToMainMenu();
    game.pressGuiButton("MainMenu_NewGame");
    game.tick(2);
    ASSERT_NE(findMm6SegueWindow(), nullptr);

    game.pressGuiButton("Mm6Segue_QuickStart");
    game.skipLoadingScreen();
    game.tick(2);

    // In the game, in New Sorpigal - no creation screen in between.
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
    EXPECT_EQ(pMapStats->pInfos[engine->_currentLoadedMapId].fileName, "oute3.odm");
    EXPECT_EQ(pParty->pCharacters[0].name, "Roderick");
    EXPECT_EQ(pParty->pCharacters[3].name, "Zoltan");
    EXPECT_EQ(pParty->pCharacters[0].classType, CLASS_PALADIN);
    EXPECT_EQ(pParty->pCharacters[3].classType, CLASS_SORCERER);

    // ...and the starting items are granted, exactly as on the Create Party path: Roderick's
    // skill-derived longsword and chain armor, plus The Letter for the opening delivery quest.
    const Character &roderick = pParty->pCharacters[0];
    ASSERT_TRUE(roderick.inventory.entry(ITEM_SLOT_MAIN_HAND));
    EXPECT_EQ(roderick.inventory.entry(ITEM_SLOT_MAIN_HAND)->itemId, static_cast<ItemId>(1));
    ASSERT_TRUE(roderick.inventory.entry(ITEM_SLOT_ARMOUR));
    EXPECT_EQ(roderick.inventory.entry(ITEM_SLOT_ARMOUR)->itemId, static_cast<ItemId>(71));
    EXPECT_TRUE(roderick.inventory.find(static_cast<ItemId>(505)));
}

GAME_TEST(Mm6, SegueFromInGameMenu) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    // New Game from the *in-game* menu shows the prologue too, not just New Game from the main menu.
    // MM6.EXE's in-game menu handler (@0x42b3ec) sets exit reason 4, which the outer dispatcher turns
    // into screen id 1 (@0x4536aa -> @0x4536c4), which its jump table @0x453854 sends to @0x4535e3 ->
    // `call 0x452bd0`, the segue. See newGameOutOfGameMenuFsmState() in Game.cpp. This path used to
    // drop straight onto party creation - the very bug the prologue screen was built to fix, at a
    // second entry point. MM7 has no prologue and still goes straight to creation (Issues.Issue790).
    game.startNewGame();
    ASSERT_EQ(current_screen_type, SCREEN_GAME);
    ASSERT_EQ(findMm6SegueWindow(), nullptr);

    // Escape into the in-game menu, then New Game - which wants confirming, so it takes two clicks.
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_MENU);
    game.pressGuiButton("GameMenu_NewGame");
    game.tick(1);
    game.pressGuiButton("GameMenu_NewGame");

    // Tearing the game down and restarting the fsm takes a few frames. Bounded, so a regression here
    // fails the test instead of hanging it.
    for (int i = 0; i < 100 && !findMm6SegueWindow(); i++)
        game.tick(1);
    ASSERT_NE(findMm6SegueWindow(), nullptr);

    // And it's the real prologue screen, not a husk - Create Party takes us on to party creation,
    // exactly as it does on the main-menu path.
    game.pressGuiButton("Mm6Segue_CreateParty");
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_PARTY_CREATION);
    EXPECT_EQ(findMm6SegueWindow(), nullptr);
}

GAME_TEST(Mm6, GameMenuSkin) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    // MM6's escape menu is laid out differently from MM7's: Resume/New/Save down the left column,
    // Controls/Load/Quit down the right - that's what the `options` background art shows. MM6.EXE's
    // button setup (@0x42bf29..0x42c018) puts the left column at x=18 sized 219x39, the right column
    // at x=242 sized 215x39, on rows y=160/213/266. MM7's hitboxes over MM6's background left five
    // of six buttons on the wrong labels.
    game.startNewGame();
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_MENU);

    // CreateButton takes the originals' closed-interval sizes and stores w+1/h+1, hence 220x40/216x40.
    EXPECT_EQ(pBtn_Resume->rect, Recti(18, 160, 220, 40));
    EXPECT_EQ(pBtn_NewGame->rect, Recti(18, 213, 220, 40));
    EXPECT_EQ(pBtn_SaveGame->rect, Recti(18, 266, 220, 40));
    EXPECT_EQ(pBtn_GameControls->rect, Recti(242, 160, 216, 40));
    EXPECT_EQ(pBtn_LoadGame->rect, Recti(242, 213, 216, 40));
    EXPECT_EQ(pBtn_QuitGame->rect, Recti(242, 266, 216, 40));

    // Each button carries its own pressed image. MM6's controls art is "control1", not MM7's
    // "controls1" - that name isn't in MM6's icons.lod and came back as the pending placeholder.
    ASSERT_EQ(pBtn_Resume->vTextures.size(), 1u);
    EXPECT_EQ(pBtn_Resume->vTextures[0]->name(), "resume1");
    ASSERT_EQ(pBtn_NewGame->vTextures.size(), 1u);
    EXPECT_EQ(pBtn_NewGame->vTextures[0]->name(), "new1");
    ASSERT_EQ(pBtn_SaveGame->vTextures.size(), 1u);
    EXPECT_EQ(pBtn_SaveGame->vTextures[0]->name(), "save1");
    ASSERT_EQ(pBtn_GameControls->vTextures.size(), 1u);
    EXPECT_EQ(pBtn_GameControls->vTextures[0]->name(), "control1");
    ASSERT_EQ(pBtn_LoadGame->vTextures.size(), 1u);
    EXPECT_EQ(pBtn_LoadGame->vTextures[0]->name(), "load1");
    ASSERT_EQ(pBtn_QuitGame->vTextures.size(), 1u);
    EXPECT_EQ(pBtn_QuitGame->vTextures[0]->name(), "quit1");

    // Resume closes the menu.
    game.pressGuiButton("GameMenu_Resume");
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
}

GAME_TEST(Mm6, PartyCreationSkin) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    // Open the creation screen without confirming it. New Game now lands on the prologue screen
    // first (MM6.EXE 0x452bd0), so it takes its Create Party button to get to the creation screen.
    game.goToMainMenu();
    game.pressGuiButton("MainMenu_NewGame");
    game.tick(2);
    game.pressGuiButton("Mm6Segue_CreateParty");
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_PARTY_CREATION);

    auto findButton = [](UIMessageType msg, auto param) -> GUIButton * {
        for (GUIButton *button : pGUIWindow_CurrentMenu->vButtons)
            if (button->msg == msg && button->msg_param == static_cast<unsigned int>(param))
                return button;
        return nullptr;
    };
    auto countButtons = [](UIMessageType msg) {
        return std::ranges::count_if(pGUIWindow_CurrentMenu->vButtons,
                                     [&](GUIButton *button) { return button->msg == msg; });
    };

    // The skin is MM6's own: makeme.pcx body (640x457, drawn under the MAKETOP band), 12 face
    // stills, six class icons, and the pillar flame animations (MM6.EXE loader 0x451d00).
    EXPECT_EQ(assets->getImage_PCXFromIconsLOD("makeme.pcx")->size(), Sizei(640, 457));
    EXPECT_EQ(assets->getImage_Solid("ccmalea")->size(), Sizei(59, 79));
    EXPECT_EQ(assets->getImage_Solid("ccgirld")->size(), Sizei(59, 79));
    GraphicsImage *knightIcon = assets->getImage_Alpha("IC_KNIG");
    EXPECT_EQ(knightIcon->size(), Sizei(44, 44));
    // The icon background is palette index 0 holding MM6's VGA-scaled teal (0,252,252), which the
    // (0,255,255) colorkey misses - the icons must load as Alpha or draw as sky-blue boxes.
    EXPECT_EQ(knightIcon->rgba()[0][0].a, 0);
    EXPECT_EQ(assets->getImage_Alpha("fl1")->size(), Sizei(33, 79));
    EXPECT_EQ(assets->getImage_Alpha("fr29")->size(), Sizei(40, 80));
    // The focus arrows blit through MM6.EXE 0x40b0c0, which skips pixels whose 16-bit color is 0 -
    // a BLACK colorkey. Their background is palette index 3 (the only black entry); index 0 is an
    // unused magenta sentinel, so palette-0 alpha leaves an opaque black box around the arrow.
    // arrowl1 has exactly 269 background pixels of 20x16, and no black pixels inside the art.
    GraphicsImage *arrow = assets->getImage_ColorKey("arrowl1", colorTable.Black, true);
    ASSERT_EQ(arrow->size(), Sizei(20, 16));
    int transparentPixels = 0;
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 20; x++)
            transparentPixels += arrow->rgba()[y][x].a == 0;
    EXPECT_EQ(arrow->rgba()[0][0].a, 0);
    EXPECT_EQ(transparentPixels, 269);
    // The selected-portrait flame frameset resolves pal002 from the frame table (dsft.bin DOES
    // fill per-frame palette ids - the earlier "all zeroes" reading was the misaligned MM6 record
    // layout picking up the always-zero paletteIndex field). A frame left at paletteId 0 hits the
    // renderer's "not paletted" sentinel and draws the raw palette indices through the red
    // channel - the "red oval" from the play-test report.
    int aframeId = pSpriteFrameTable->FastFindSprite("aframe1");
    ASSERT_GT(aframeId, 0);
    EXPECT_EQ(pSpriteFrameTable->GetFrame(aframeId, 0_ticks)->paletteId, 2);

    // MM6 buttons (MM6.EXE 0x451ff4-0x452670): a lone BUTTMAKE OK scroll at (511,438) - no Clear
    // button - MAKEMINU/MAKEPLUS point-buy buttons, 32x16 face arrows, and no voice arrows at all.
    // CreateButton stores w+1/h+1 (closed-interval heritage).
    GUIButton *okButton = findButton(UIMSG_PlayerCreationClickOK, 0);
    ASSERT_NE(okButton, nullptr);
    EXPECT_EQ(okButton->rect, Recti(511, 438, 64, 30));
    EXPECT_EQ(okButton->vTextures[0], assets->getImage_Solid("BUTTMAKE"));
    GUIButton *minusButton = findButton(UIMSG_PlayerCreationClickMinus, 0);
    GUIButton *plusButton = findButton(UIMSG_PlayerCreationClickPlus, 1);
    ASSERT_NE(minusButton, nullptr);
    ASSERT_NE(plusButton, nullptr);
    EXPECT_EQ(minusButton->rect, Recti(482, 392, 21, 36));
    EXPECT_EQ(plusButton->rect, Recti(580, 392, 23, 36));
    EXPECT_EQ(countButtons(UIMSG_PlayerCreationClickReset), 0);
    EXPECT_EQ(countButtons(UIMSG_PlayerCreation_VoicePrev), 0);
    EXPECT_EQ(countButtons(UIMSG_PlayerCreation_VoiceNext), 0);
    GUIButton *facePrev0 = findButton(UIMSG_PlayerCreation_FacePrev, 0);
    ASSERT_NE(facePrev0, nullptr);
    EXPECT_EQ(facePrev0->rect, Recti(86, 31, 33, 17));

    // Six class buttons on the LEFT (x=60/140, MM6.EXE 0x45248f): Knight/Cleric/Sorcerer down the
    // first column, Paladin/Archer/Druid down the second; no Thief/Monk/Ranger.
    EXPECT_EQ(countButtons(UIMSG_PlayerCreationSelectClass), 6);
    EXPECT_EQ(findButton(UIMSG_PlayerCreationSelectClass, CLASS_KNIGHT)->rect.x, 60);
    EXPECT_EQ(findButton(UIMSG_PlayerCreationSelectClass, CLASS_CLERIC)->rect.x, 60);
    EXPECT_EQ(findButton(UIMSG_PlayerCreationSelectClass, CLASS_SORCERER)->rect.x, 60);
    EXPECT_EQ(findButton(UIMSG_PlayerCreationSelectClass, CLASS_PALADIN)->rect.x, 140);
    EXPECT_EQ(findButton(UIMSG_PlayerCreationSelectClass, CLASS_ARCHER)->rect.x, 140);
    EXPECT_EQ(findButton(UIMSG_PlayerCreationSelectClass, CLASS_DRUID)->rect.x, 140);
    EXPECT_EQ(findButton(UIMSG_PlayerCreationSelectClass, CLASS_KNIGHT)->rect.y, 417);
    EXPECT_EQ(findButton(UIMSG_PlayerCreationSelectClass, CLASS_PALADIN)->rect.y, 417);
    EXPECT_EQ(findButton(UIMSG_PlayerCreationSelectClass, CLASS_THIEF), nullptr);
    EXPECT_EQ(findButton(UIMSG_PlayerCreationSelectClass, CLASS_MONK), nullptr);
    EXPECT_EQ(findButton(UIMSG_PlayerCreationSelectClass, CLASS_RANGER), nullptr);
    // Nine available-skill buttons in the bottom-center grid at x=230+80*(i/3).
    EXPECT_EQ(countButtons(UIMSG_PlayerCreationSelectActiveSkill), 9);
    EXPECT_EQ(findButton(UIMSG_PlayerCreationSelectActiveSkill, 0)->rect.x, 230);
    EXPECT_EQ(findButton(UIMSG_PlayerCreationSelectActiveSkill, 8)->rect.x, 390);

    auto activeSkills = [](const Character &character) {
        std::vector<Skill> result;
        for (Skill skill : allVisibleSkills())
            if (character.pActiveSkills[skill])
                result.push_back(skill);
        return result;
    };

    // The screen opens with the SetClass default party, NOT the new.lod template: New Game runs
    // MM6's Party::Reset (MM6.EXE 0x485f40, called at 0x4535fd BEFORE the prologue), which is four
    // SetClass calls plus faces 0/11/9/7 and the global.txt row 506-509 names - class-base stats
    // with the whole 50-point pool unspent, and only each class's two fixed skills. The fully-built
    // template party belongs to Quick Start alone (its fill lives at 0x485540).
    EXPECT_EQ(CharacterCreation_GetUnspentAttributePointCount(), 50);
    EXPECT_EQ(pParty->pCharacters[0].classType, CLASS_PALADIN);
    EXPECT_EQ(pParty->pCharacters[1].classType, CLASS_ARCHER);
    EXPECT_EQ(pParty->pCharacters[2].classType, CLASS_CLERIC);
    EXPECT_EQ(pParty->pCharacters[3].classType, CLASS_SORCERER);
    EXPECT_EQ(pParty->pCharacters[0].uCurrentFace, 0);
    EXPECT_EQ(pParty->pCharacters[1].uCurrentFace, 11);
    EXPECT_EQ(pParty->pCharacters[2].uCurrentFace, 9);
    EXPECT_EQ(pParty->pCharacters[3].uCurrentFace, 7);
    EXPECT_EQ(pParty->pCharacters[3].name, "Zoltan");
    for (Character &character : pParty->pCharacters)
        EXPECT_EQ(activeSkills(character).size(), 2u);

    // Class change is a full MM6 reset (MM6.EXE SetClass 0x483d90): base stats from the class
    // table, the class's 2 fixed skills, first spell of any granted school, exp/birth-year reroll.
    Character &zoltan = pParty->pCharacters[3];
    engine->_messageQueue->addMessageCurrentFrame(UIMSG_PlayerCreation_SelectAttribute, 3, 0);
    game.tick(1);
    engine->_messageQueue->addMessageCurrentFrame(UIMSG_PlayerCreationSelectClass, std::to_underlying(CLASS_KNIGHT), 0);
    game.tick(1);
    EXPECT_EQ(zoltan.classType, CLASS_KNIGHT);
    EXPECT_EQ(zoltan._stats[ATTRIBUTE_MIGHT], 14);
    EXPECT_EQ(zoltan._stats[ATTRIBUTE_INTELLIGENCE], 7);
    EXPECT_EQ(zoltan._stats[ATTRIBUTE_PERSONALITY], 7);
    EXPECT_EQ(zoltan._stats[ATTRIBUTE_ENDURANCE], 14);
    EXPECT_EQ(zoltan._stats[ATTRIBUTE_ACCURACY], 11);
    EXPECT_EQ(zoltan._stats[ATTRIBUTE_SPEED], 11);
    EXPECT_EQ(zoltan._stats[ATTRIBUTE_LUCK], 9);
    EXPECT_EQ(zoltan.uLevel, 1);
    EXPECT_GE(zoltan.experience, 251);
    EXPECT_LE(zoltan.experience, 350);
    EXPECT_GE(zoltan.uBirthYear, 1139);
    EXPECT_LE(zoltan.uBirthYear, 1144);
    EXPECT_EQ(activeSkills(zoltan), (std::vector<Skill>{SKILL_SWORD, SKILL_LEATHER}));
    EXPECT_TRUE(std::ranges::none_of(zoltan.bHaveSpell, [](bool have) { return have; }));
    // The other three are still at their class bases, so the whole pool is back.
    EXPECT_EQ(CharacterCreation_GetUnspentAttributePointCount(), 50);

    engine->_messageQueue->addMessageCurrentFrame(UIMSG_PlayerCreationSelectClass, std::to_underlying(CLASS_CLERIC), 0);
    game.tick(1);
    EXPECT_EQ(zoltan.classType, CLASS_CLERIC);
    EXPECT_EQ(activeSkills(zoltan), (std::vector<Skill>{SKILL_MACE, SKILL_BODY}));
    EXPECT_TRUE(zoltan.bHaveSpell[static_cast<SpellId>(67)]);   // Body school's first spell.
    EXPECT_FALSE(zoltan.bHaveSpell[static_cast<SpellId>(1)]);
    EXPECT_EQ(zoltan._stats[ATTRIBUTE_MIGHT], 7);               // Cleric bases.
    EXPECT_EQ(zoltan._stats[ATTRIBUTE_PERSONALITY], 14);
    EXPECT_EQ(zoltan._stats[ATTRIBUTE_SPEED], 7);

    // MM6 point buy: +/-1 per click against the CLASS base (not MM7's race StatTable) - floor
    // base-2, cap 25, every point worth 1 (MM6.EXE 0x484450/0x484270/0x4848d0).
    zoltan.DecreaseAttribute(ATTRIBUTE_MIGHT);
    zoltan.DecreaseAttribute(ATTRIBUTE_MIGHT);
    EXPECT_EQ(zoltan._stats[ATTRIBUTE_MIGHT], 5);
    EXPECT_EQ(CharacterCreation_GetUnspentAttributePointCount(), 52);
    zoltan.DecreaseAttribute(ATTRIBUTE_MIGHT); // Floor: base 7 - 2.
    EXPECT_EQ(zoltan._stats[ATTRIBUTE_MIGHT], 5);
    EXPECT_EQ(CharacterCreation_GetUnspentAttributePointCount(), 52);
    for (int i = 0; i < 30; i++)
        zoltan.IncreaseAttribute(ATTRIBUTE_LUCK); // Cap: 25, from the base of 14.
    EXPECT_EQ(zoltan._stats[ATTRIBUTE_LUCK], 25);
    EXPECT_EQ(CharacterCreation_GetUnspentAttributePointCount(), 41);

    // Skill picks: 2 choices from the class's 9 creation options, same message flow as MM7.
    // Cleric options in display order: Staff, Shield, Leather, Spirit, Mind, Id Item, Repair,
    // Meditation, Diplomacy.
    engine->_messageQueue->addMessageCurrentFrame(UIMSG_PlayerCreationSelectActiveSkill, 0, 0);
    game.tick(1);
    engine->_messageQueue->addMessageCurrentFrame(UIMSG_PlayerCreationSelectActiveSkill, 4, 0);
    game.tick(1);
    EXPECT_EQ(activeSkills(zoltan), (std::vector<Skill>{SKILL_STAFF, SKILL_MACE, SKILL_MIND, SKILL_BODY}));
    engine->_messageQueue->addMessageCurrentFrame(UIMSG_PlayerCreationSelectActiveSkill, 8, 0);
    game.tick(1); // Both optional slots taken - refused.
    EXPECT_EQ(activeSkills(zoltan).size(), 4u);
    EXPECT_FALSE(zoltan.pActiveSkills[SKILL_DIPLOMACY]);
    engine->_messageQueue->addMessageCurrentFrame(UIMSG_PlayerCreationRemoveUpSkill, 3, 0);
    game.tick(1); // Removes the first picked skill (Staff).
    EXPECT_FALSE(zoltan.pActiveSkills[SKILL_STAFF]);
    EXPECT_EQ(activeSkills(zoltan).size(), 3u);
    engine->_messageQueue->addMessageCurrentFrame(UIMSG_PlayerCreationSelectActiveSkill, 0, 0);
    game.tick(1);
    EXPECT_EQ(activeSkills(zoltan).size(), 4u);

    // Faces: 12 stills, ids 0-7 male / 8-11 female; sex follows the face and the name rerolls
    // from npcnames.txt (MM6.EXE 0x482cd0). Class and skills are untouched.
    EXPECT_EQ(zoltan.uCurrentFace, 7);
    EXPECT_EQ(zoltan.uSex, SEX_MALE);
    engine->_messageQueue->addMessageCurrentFrame(UIMSG_PlayerCreation_FaceNext, 3, 0);
    game.tick(1);
    EXPECT_EQ(zoltan.uCurrentFace, 8);
    EXPECT_EQ(zoltan.uSex, SEX_FEMALE);
    EXPECT_NE(zoltan.name, "Zoltan");
    EXPECT_EQ(zoltan.classType, CLASS_CLERIC);
    EXPECT_EQ(activeSkills(zoltan).size(), 4u);
    engine->_messageQueue->addMessageCurrentFrame(UIMSG_PlayerCreation_FacePrev, 3, 0);
    game.tick(1);
    EXPECT_EQ(zoltan.uCurrentFace, 7);
    EXPECT_EQ(zoltan.uSex, SEX_MALE);
    engine->_messageQueue->addMessageCurrentFrame(UIMSG_PlayerCreation_FacePrev, 0, 0);
    game.tick(1); // Wraps 0 -> 11 (a female face).
    EXPECT_EQ(pParty->pCharacters[0].uCurrentFace, 11);
    EXPECT_EQ(pParty->pCharacters[0].uSex, SEX_FEMALE);

    // OK is refused while bonus points remain unspent (MM6.EXE 0x42ff14)...
    game.pressGuiButton("PartyCreation_OK");
    game.tick(5);
    EXPECT_EQ(current_screen_type, SCREEN_PARTY_CREATION);
    // ...and proceeds once the pool hits zero with 4 skills on everyone. Characters 0-2 still
    // carry only their two fixed class skills, so give each its two creation picks first.
    for (int i = 0; i < 3; i++) {
        engine->_messageQueue->addMessageCurrentFrame(UIMSG_PlayerCreation_SelectAttribute, i, 0);
        game.tick(1);
        engine->_messageQueue->addMessageCurrentFrame(UIMSG_PlayerCreationSelectActiveSkill, 0, 0);
        game.tick(1);
        engine->_messageQueue->addMessageCurrentFrame(UIMSG_PlayerCreationSelectActiveSkill, 4, 0);
        game.tick(1);
    }
    for (int i = 0; i < 4 && CharacterCreation_GetUnspentAttributePointCount(); i++)
        for (Attribute stat : allStatAttributes())
            while (CharacterCreation_GetUnspentAttributePointCount() && pParty->pCharacters[i]._stats[stat] < 25)
                pParty->pCharacters[i].IncreaseAttribute(stat);
    EXPECT_EQ(CharacterCreation_GetUnspentAttributePointCount(), 0);
    game.pressGuiButton("PartyCreation_OK");
    game.skipLoadingScreen();
    game.tick(2);
    EXPECT_EQ(pMapStats->pInfos[engine->_currentLoadedMapId].fileName, "oute3.odm");
    EXPECT_EQ(pParty->pCharacters[3].classType, CLASS_CLERIC);
    EXPECT_TRUE(pParty->pCharacters[3].pActiveSkills[SKILL_MIND]);
}


GAME_TEST(Mm6, MazeInfoPopup) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // Right-clicking the minimap area shows the map's "maze info" - the level string referenced by the map's
    // LocationName event record, NOT the mapstats name (MM6.EXE 0x439F10, right-click dispatcher 0x41152D).
    EXPECT_EQ(GameUI_GetMinimapHintText(), "New Sorpigal");

    // Exercise the real popup path: popups draw while the right button is held (press/tick/release).
    game.pressButton(BUTTON_RIGHT, 550, 80); // Minimap zone: x past the viewport, y < 140.
    game.tick(2);
    game.releaseButton(BUTTON_RIGHT, 550, 80);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);

    // Indoors the string still comes from the map's own .evt/.str pair.
    MapId caverns = pMapStats->GetMapInfo("cd1.blv");
    ASSERT_NE(caverns, MAP_INVALID);
    game.teleportTo(caverns, Vec3f(-3136, 2240, 224), 0); // A known-valid cd1 position.
    EXPECT_EQ(GameUI_GetMinimapHintText(), "Castle Alamos");
}

GAME_TEST(Mm6, EnterCastleThroneRoom) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // Castle Ironfist's castle door (outd3 event 43): MoveToMap(house 153, exit pic 2, "0") shows the entry
    // prompt named after the "Castle Entrance" 2dEvents row, and the follow-up step SpeakInHouse(154) opens
    // the Throne Room house. The 2dEvents "Throne" / "2D 154" exit columns are editor annotations - MM6.EXE's
    // parser atoi's them to junk (0x439596/0x4395a5) and the whole chain lives in the map script instead.
    MapId ironfist = pMapStats->GetMapInfo("outd3.odm");
    ASSERT_NE(ironfist, MAP_INVALID);
    game.teleportTo(ironfist, Vec3f(0, 0, 512), 0);
    game.tick(1);

    const BLVFace *door = nullptr;
    for (const BSPModel &model : pOutdoor->pBModels)
        for (const BLVFace &face : model.faces)
            if (face.eventId == 43 && face.Clickable())
                door = &face;
    ASSERT_NE(door, nullptr);
    Vec3f doorCenter = door->boundingBox.center();
    Vec3f pos = doorCenter + door->facePlane.normal * 130;
    pos.z = door->boundingBox.z1;
    int yawDegrees = TrigLUT.atan2(doorCenter.x - pos.x, doorCenter.y - pos.y) * 90 / 512;
    game.teleportTo(ironfist, pos, yawDegrees);
    game.tick(1);
    Vec3f posAtDoor = pParty->pos;

    // Interacting with the door opens the entry prompt, skinned with MM6's "dungeon" exit picture and the
    // trans.txt blurb keyed by the entrance-house id (row 153 mentions the castle's resident regent).
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_INPUT_BLV);
    ASSERT_NE(transition_ui_icon, nullptr);
    EXPECT_EQ(transition_ui_icon->name(), "dungeon"); // Event 43 passes exit-pic id 2 = MM6's "dungeon".
    EXPECT_NE(pTransitionStrings[153].find("Wilbur Humphrey"), std::string::npos);

    // Confirming must NOT move the party - the event's MoveToMap target is all-zero, which in MM6 means
    // "stay put" (MM6.EXE 0x43de3d ORs all six components; zero = keep current). The event resumes at the
    // next step, which opens the Throne Room house.
    game.pressAndReleaseKey(PlatformKey::KEY_Y);
    game.tick(5);
    EXPECT_EQ(current_screen_type, SCREEN_HOUSE);
    ASSERT_NE(window_SpeakInHouse, nullptr);
    EXPECT_EQ(window_SpeakInHouse->houseId(), HouseId(154));
    EXPECT_EQ(engine->_currentLoadedMapId, ironfist);
    EXPECT_EQ(pParty->pos.x, posAtDoor.x);
    EXPECT_EQ(pParty->pos.y, posAtDoor.y);

    // Wilbur Humphrey holds court in the throne room (npcdata "2D Location" = 154).
    bool humphreyPresent = false;
    for (const HouseNpcDesc &npc : houseNpcs)
        if (npc.type == HOUSE_NPC && npc.npc && npc.npc->name == "Wilbur Humphrey")
            humphreyPresent = true;
    EXPECT_TRUE(humphreyPresent);

    // Esc leaves the castle back into the game world, party still at the door.
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
    EXPECT_EQ(pParty->pos.x, posAtDoor.x);
    EXPECT_EQ(pParty->pos.y, posAtDoor.y);
}

// MM6 magic guilds (2dEvents houses 119-140, an Initiate + an Adept house per organization) run the
// same membership model as the fighter/thief guilds on the house side (award-bit table @0x4C3CB8;
// joining happens at recruiter NPC topics 389-397 + 381/382, covered by MercGuildJoinAndLearnSkills):
// non-members are turned away with npctext row 172 ("You must be a member of this guild to study
// here") and zero options; members get Buy Spells plus the guild's taught skills (option factory
// @0x498a15..0x498ec4: school skill + Learning for fire/air/water/earth guilds, + Meditation for
// spirit/mind/body, school skill only for light/dark; the MM6-only Element guild teaches all four
// elemental schools and the Self guild all three self schools). Learning costs trunc(500 x 2dEvents
// multiplier), merchant-discounted with a floor of a third (@0x49b854), and requires the class-can-
// learn table @0x4C2694. Buy Spells restocks 12 identified spellbooks when the 2dEvents interval
// elapses (generator @0x4a4320): item = 300 + school*11 + rand % N, where N comes from the per-house
// word table @0x4C48B0 (7/11 for the school guilds, 6/10 light+dark, 4/8 element+self - the 2dEvents
// "Spells = 1-N" annotations agree) and the combined guilds roll the school per slot. The engine's
// guild shelf arrays are keyed by MM7's magic-guild house ids 139-170, so MM6 houses 119-140 map
// onto slots houseId + 20 (the save format is untouched).
GAME_TEST(Mm6, MagicGuildMembershipAndSpellbooks) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    pParty->SetGold(20000);

    // MM6's guild rows parse with their own types, including the MM6-only combined guilds.
    ASSERT_EQ(houseTable[HouseId(119)].uType, HOUSE_TYPE_FIRE_GUILD);
    ASSERT_EQ(houseTable[HouseId(137)].uType, HOUSE_TYPE_ELEMENTAL_GUILD);
    ASSERT_EQ(houseTable[HouseId(139)].uType, HOUSE_TYPE_SELF_GUILD);
    EXPECT_EQ(houseTable[HouseId(137)].name, "Initiate Guild of the Elements");
    EXPECT_EQ(houseTable[HouseId(137)].fPriceMultiplier, 1.5f);
    EXPECT_EQ(houseTable[HouseId(119)].fPriceMultiplier, 2.0f);

    // Non-member: New Sorpigal's Element guild opens safely and offers nothing.
    ASSERT_TRUE(enterHouse(HouseId(137)));
    createHouseUI(HouseId(137));
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_HOUSE);
    openProprietorDialogue(game);
    game.tick(2);
    EXPECT_EQ(findProprietorOption(DIALOGUE_GUILD_BUY_BOOKS), nullptr);
    for (const GUIButton *button : pDialogueWindow->vButtons)
        if (button->msg == UIMSG_SelectProprietorDialogueOption)
            EXPECT_FALSE(IsSkillLearningDialogue(static_cast<DialogueId>(button->msg_param)));
    leaveHouse(game);

    // Membership in the Elemental-guild organization is award bit 64 on the active character.
    for (Character &character : pParty->pCharacters)
        character._achievedAwardsBits.set(static_cast<AwardId>(64), true);

    // A member is offered Buy Spells + the four elemental school skills (MM6.EXE factory 0x498a15).
    ASSERT_TRUE(enterHouse(HouseId(137)));
    createHouseUI(HouseId(137));
    game.tick(2);
    openProprietorDialogue(game);
    EXPECT_NE(findProprietorOption(DIALOGUE_GUILD_BUY_BOOKS), nullptr);
    EXPECT_NE(findProprietorOption(DIALOGUE_LEARN_FIRE), nullptr);
    EXPECT_NE(findProprietorOption(DIALOGUE_LEARN_AIR), nullptr);
    EXPECT_NE(findProprietorOption(DIALOGUE_LEARN_WATER), nullptr);
    EXPECT_NE(findProprietorOption(DIALOGUE_LEARN_EARTH), nullptr);
    EXPECT_EQ(findProprietorOption(DIALOGUE_LEARN_LEARNING), nullptr);
    EXPECT_EQ(findProprietorOption(DIALOGUE_LEARN_SPIRIT), nullptr);

    // Zoltan the Sorcerer learns Air Magic for trunc(500 x 1.5) = 750 (no merchant discount on a
    // fresh party). Fire is taught here too but he knows it from creation: blank no-op option.
    pParty->setActiveCharacterIndex(4);
    EXPECT_EQ(pParty->activeCharacter().name, pParty->pCharacters[3].name);
    EXPECT_TRUE(pParty->pCharacters[3].pActiveSkills[SKILL_FIRE]);
    int goldBefore = pParty->GetGold();
    clickProprietorOption(game, DIALOGUE_LEARN_FIRE);
    EXPECT_EQ(pParty->GetGold(), goldBefore);
    EXPECT_FALSE(pParty->pCharacters[3].pActiveSkills[SKILL_AIR]);
    clickProprietorOption(game, DIALOGUE_LEARN_AIR);
    EXPECT_EQ(pParty->GetGold(), goldBefore - 750);
    EXPECT_EQ(pParty->pCharacters[3].pActiveSkills[SKILL_AIR], CombinedSkillValue::novice());

    // Roderick the Paladin can learn no magic at all (class-can-learn table @0x4C2694): no-op.
    pParty->setActiveCharacterIndex(1);
    goldBefore = pParty->GetGold();
    clickProprietorOption(game, DIALOGUE_LEARN_WATER);
    EXPECT_EQ(pParty->GetGold(), goldBefore);
    EXPECT_FALSE(pParty->pCharacters[0].pActiveSkills[SKILL_WATER]);

    // Buy Spells: the Initiate shelves stock 12 identified books rolled from the four elemental
    // schools' first FOUR spells, and the refresh clock starts ticking.
    clickProprietorOption(game, DIALOGUE_GUILD_BUY_BOOKS);
    game.tick(2);
    EXPECT_GT(pParty->PartyTimes.guildNextRefreshTime[HouseId(137 + 20)], pParty->GetPlayingTime());
    std::array<Item, 12> &shelf = pParty->spellBooksInGuilds[HouseId(137 + 20)];
    for (const Item &book : shelf) {
        int id = std::to_underlying(book.itemId);
        int school = (id - 300) / 11;
        EXPECT_GE(school, 0) << "book id " << id;
        EXPECT_LE(school, 3) << "book id " << id;
        EXPECT_LT((id - 300) % 11, 4) << "book id " << id;
        EXPECT_TRUE(book.IsIdentified());
    }

    // Buying the first shelf book pays the standard buying price and moves it into the active
    // character's inventory; the slot empties until the next restock.
    Item firstBook = shelf[0];
    goldBefore = pParty->GetGold();
    int price = PriceCalculator::itemBuyingPriceForPlayer(&pParty->activeCharacter(), firstBook.GetValue(),
                                                          houseTable[HouseId(137)].fPriceMultiplier);
    game.pressAndReleaseButton(BUTTON_LEFT, 36, 94);
    game.tick(2);
    EXPECT_EQ(pParty->GetGold(), goldBefore - price);
    EXPECT_EQ(shelf[0].itemId, ITEM_NULL);
    bool bookInInventory = false;
    for (InventoryConstEntry entry : pParty->activeCharacter().inventory.entries())
        if (entry->itemId == firstBook.itemId)
            bookInInventory = true;
    EXPECT_TRUE(bookInInventory);
    leaveHouse(game);

    // The Self guild (house 139, organization award 65) teaches and stocks the three self schools.
    // Serena the Cleric knows Mind and Body from creation and learns Spirit here.
    for (Character &character : pParty->pCharacters)
        character._achievedAwardsBits.set(static_cast<AwardId>(65), true);
    pParty->setActiveCharacterIndex(3);
    ASSERT_TRUE(enterHouse(HouseId(139)));
    createHouseUI(HouseId(139));
    game.tick(2);
    openProprietorDialogue(game);
    EXPECT_NE(findProprietorOption(DIALOGUE_LEARN_SPIRIT), nullptr);
    EXPECT_NE(findProprietorOption(DIALOGUE_LEARN_MIND), nullptr);
    EXPECT_NE(findProprietorOption(DIALOGUE_LEARN_BODY), nullptr);
    EXPECT_EQ(findProprietorOption(DIALOGUE_LEARN_FIRE), nullptr);
    EXPECT_EQ(findProprietorOption(DIALOGUE_LEARN_MEDITATION), nullptr);
    goldBefore = pParty->GetGold();
    EXPECT_FALSE(pParty->pCharacters[2].pActiveSkills[SKILL_SPIRIT]);
    clickProprietorOption(game, DIALOGUE_LEARN_SPIRIT);
    EXPECT_EQ(pParty->GetGold(), goldBefore - 750);
    EXPECT_EQ(pParty->pCharacters[2].pActiveSkills[SKILL_SPIRIT], CombinedSkillValue::novice());
    clickProprietorOption(game, DIALOGUE_GUILD_BUY_BOOKS);
    game.tick(2);
    for (const Item &book : pParty->spellBooksInGuilds[HouseId(139 + 20)]) {
        int id = std::to_underlying(book.itemId);
        int school = (id - 300) / 11;
        EXPECT_GE(school, 4) << "book id " << id;
        EXPECT_LE(school, 6) << "book id " << id;
        EXPECT_LT((id - 300) % 11, 4) << "book id " << id;
    }
    leaveHouse(game);

    // A single-school guild teaches its school + Learning and stocks spells 1-7 at Initiate level:
    // the Fire guild (house 119, organization award 74, multiplier 2 -> Learning costs 1000).
    for (Character &character : pParty->pCharacters)
        character._achievedAwardsBits.set(static_cast<AwardId>(74), true);
    ASSERT_TRUE(enterHouse(HouseId(119)));
    createHouseUI(HouseId(119));
    game.tick(2);
    openProprietorDialogue(game);
    EXPECT_NE(findProprietorOption(DIALOGUE_GUILD_BUY_BOOKS), nullptr);
    EXPECT_NE(findProprietorOption(DIALOGUE_LEARN_FIRE), nullptr);
    EXPECT_NE(findProprietorOption(DIALOGUE_LEARN_LEARNING), nullptr);
    EXPECT_EQ(findProprietorOption(DIALOGUE_LEARN_AIR), nullptr);
    goldBefore = pParty->GetGold();
    EXPECT_FALSE(pParty->pCharacters[2].pActiveSkills[SKILL_LEARNING]);
    clickProprietorOption(game, DIALOGUE_LEARN_LEARNING);
    EXPECT_EQ(pParty->GetGold(), goldBefore - 1000);
    EXPECT_EQ(pParty->pCharacters[2].pActiveSkills[SKILL_LEARNING], CombinedSkillValue::novice());
    clickProprietorOption(game, DIALOGUE_GUILD_BUY_BOOKS);
    game.tick(2);
    for (const Item &book : pParty->spellBooksInGuilds[HouseId(119 + 20)]) {
        int id = std::to_underlying(book.itemId);
        EXPECT_GE(id, 300) << "book id " << id;
        EXPECT_LE(id, 306) << "book id " << id;
    }
    leaveHouse(game);
}

// MM6's NPC skill teachers: dialogue topics 200-259 are reserved for expert/master skill promotion
// (MM6.EXE topic dispatch @0x496b42, gate computation @0x496c90, learn execute @0x4969c4).
// Topic = 200 + 2 x skill slot + parity (even = expert, odd = master); slot 28 = Thievery is
// skipped (a skill index above 27 is incremented), so topics 256-259 teach Disarm Traps and
// Learning. The offer prose is npctext row [topic]; the refusal texts are npctext rows 260-266;
// the clickable option label is "Learn" (global.txt 535) once every gate passes: a conscious
// active character, the skill known, the tier not already held, expert-first for masters,
// expert = rank 4 + price (weapons/shield 2000, armor + elemental schools 1000, most misc 500),
// master = a per-skill price and prerequisite (rank 8/10/12 tiers, stats >= 30/40, class
// promotions or their award bits, reputation >= 1000 (Saintly - MM6 positive reputation is good)
// for Light / <= -1000 for Dark, fame >= 200 for Diplomacy, a carried blaster for Ancient
// Weapons), and the gold check last. Learning promotes the ACTIVE character's skill mastery and
// takes the gold; the teacher's topic is never retired.
GAME_TEST(Mm6, NpcSkillTeachers) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    pParty->GetPlayingTime() += Duration::fromHours(2); // 11:00, inside every dwelling's open hours.
    game.tick(1);

    Character &roderick = pParty->pCharacters[0];

    // Escapes any open dialogue, back to the house screen with clickable portraits.
    auto escapeToHouseScreen = [&] {
        for (int i = 0; i < 5 && pDialogueWindow; i++) {
            game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
            game.tick(2);
        }
        ASSERT_EQ(pDialogueWindow, nullptr);
        ASSERT_EQ(current_screen_type, SCREEN_HOUSE);
    };
    // Opens the teacher's topic from the house screen, checks the offer prose, clicks Learn
    // (a no-op when a gate refuses) and returns to the house screen.
    auto attemptLearn = [&](NPCData *npc, DialogueId slot, int topicId) {
        clickHouseNpcPortrait(game, npc);
        selectScriptedTopic(game, slot);
        EXPECT_EQ(current_npc_text, pNPCTopics[topicId - 1].pText); // The offer prose = npctext row [topic].
        selectScriptedTopic(game, DIALOGUE_MASTERY_TEACHER_LEARN);
        escapeToHouseScreen();
    };

    // Expert Staff: Calvin Black, npcdata 31, house 460, first topic 200 ("...an intermediate
    // skill in the staff (Rank 4) and 2000 gold").
    NPCData *calvin = &pNPCStats->pNPCData[31];
    EXPECT_EQ(calvin->name, "Calvin Black");
    ASSERT_EQ(calvin->house, HouseId(460));
    ASSERT_EQ(calvin->dialogue_1_evt_id, 200);
    EXPECT_TRUE(std::string_view(pNPCTopics[200].pTopic).starts_with("Expert Staff Defense"));
    ASSERT_TRUE(enterHouse(HouseId(460)));
    createHouseUI(HouseId(460));
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_HOUSE);
    EXPECT_EQ(pParty->activeCharacter().name, roderick.name);

    // Roderick doesn't know Staff at all: refused, nothing charged.
    pParty->SetGold(10000);
    attemptLearn(calvin, DIALOGUE_SCRIPTED_LINE_1, 200);
    EXPECT_EQ(pParty->GetGold(), 10000);
    EXPECT_FALSE(roderick.pActiveSkills[SKILL_STAFF]);

    // Rank 3 is below the expert requirement of rank 4.
    roderick.setSkillValue(SKILL_STAFF, CombinedSkillValue(3, MASTERY_NOVICE));
    attemptLearn(calvin, DIALOGUE_SCRIPTED_LINE_1, 200);
    EXPECT_EQ(pParty->GetGold(), 10000);
    EXPECT_EQ(roderick.getSkillValue(SKILL_STAFF).mastery(), MASTERY_NOVICE);

    // Rank 4 but 1999 gold: the gold gate refuses.
    roderick.setSkillValue(SKILL_STAFF, CombinedSkillValue(4, MASTERY_NOVICE));
    pParty->SetGold(1999);
    attemptLearn(calvin, DIALOGUE_SCRIPTED_LINE_1, 200);
    EXPECT_EQ(pParty->GetGold(), 1999);
    EXPECT_EQ(roderick.getSkillValue(SKILL_STAFF).mastery(), MASTERY_NOVICE);

    // Rank 4 + 2500 gold: promoted to expert for exactly 2000.
    pParty->SetGold(2500);
    attemptLearn(calvin, DIALOGUE_SCRIPTED_LINE_1, 200);
    EXPECT_EQ(pParty->GetGold(), 500);
    EXPECT_EQ(roderick.getSkillValue(SKILL_STAFF), CombinedSkillValue(4, MASTERY_EXPERT));

    // Already an expert: the repeat visit refuses and charges nothing.
    attemptLearn(calvin, DIALOGUE_SCRIPTED_LINE_1, 200);
    EXPECT_EQ(pParty->GetGold(), 500);
    EXPECT_EQ(roderick.getSkillValue(SKILL_STAFF), CombinedSkillValue(4, MASTERY_EXPERT));
    leaveHouse(game);

    // Master Merchant: Will Ottoman, npcdata 29, house 397, first topic 245 - rank 7,
    // Personality >= 30, 4000 gold ("...for 4000 gold, provided you have the skill (Rank 7)...").
    NPCData *ottoman = &pNPCStats->pNPCData[29];
    EXPECT_EQ(ottoman->name, "Will Ottoman");
    ASSERT_EQ(ottoman->house, HouseId(397));
    ASSERT_EQ(ottoman->dialogue_1_evt_id, 245);
    ASSERT_TRUE(enterHouse(HouseId(397)));
    createHouseUI(HouseId(397));
    game.tick(2);
    pParty->SetGold(10000);

    // Rank 7 but no expert tier yet: masters teach experts only.
    roderick.setSkillValue(SKILL_MERCHANT, CombinedSkillValue(7, MASTERY_NOVICE));
    attemptLearn(ottoman, DIALOGUE_SCRIPTED_LINE_1, 245);
    EXPECT_EQ(pParty->GetGold(), 10000);
    EXPECT_EQ(roderick.getSkillValue(SKILL_MERCHANT).mastery(), MASTERY_NOVICE);

    // Expert at rank 7 but not charming enough: Personality below 30 refuses.
    roderick.setSkillValue(SKILL_MERCHANT, CombinedSkillValue(7, MASTERY_EXPERT));
    int personalityBefore = roderick._stats[ATTRIBUTE_PERSONALITY];
    roderick._stats[ATTRIBUTE_PERSONALITY] = 1;
    ASSERT_LT(roderick.GetActualPersonality(), 30);
    attemptLearn(ottoman, DIALOGUE_SCRIPTED_LINE_1, 245);
    EXPECT_EQ(pParty->GetGold(), 10000);
    EXPECT_EQ(roderick.getSkillValue(SKILL_MERCHANT).mastery(), MASTERY_EXPERT);

    // Personality 30: promoted to master for exactly 4000.
    roderick._stats[ATTRIBUTE_PERSONALITY] = 30;
    ASSERT_GE(roderick.GetActualPersonality(), 30);
    attemptLearn(ottoman, DIALOGUE_SCRIPTED_LINE_1, 245);
    EXPECT_EQ(pParty->GetGold(), 6000);
    EXPECT_EQ(roderick.getSkillValue(SKILL_MERCHANT), CombinedSkillValue(7, MASTERY_MASTER));
    roderick._stats[ATTRIBUTE_PERSONALITY] = personalityBefore;
    leaveHouse(game);

    // Master of Light: Ki Lo Nee, npcdata 272, house 444, first topic 239 - free, but only for a
    // Saintly party (reputation >= 1000; MM6's scale is positive = good, MM6.EXE titles @0x489c60).
    NPCData *kiLoNee = &pNPCStats->pNPCData[272];
    EXPECT_EQ(kiLoNee->name, "Ki Lo Nee");
    ASSERT_EQ(kiLoNee->house, HouseId(444));
    ASSERT_EQ(kiLoNee->dialogue_1_evt_id, 239);
    ASSERT_TRUE(enterHouse(HouseId(444)));
    createHouseUI(HouseId(444));
    game.tick(2);
    roderick.setSkillValue(SKILL_LIGHT, CombinedSkillValue(8, MASTERY_EXPERT));

    currentLocationInfo().reputation = 0;
    attemptLearn(kiLoNee, DIALOGUE_SCRIPTED_LINE_1, 239);
    EXPECT_EQ(roderick.getSkillValue(SKILL_LIGHT).mastery(), MASTERY_EXPERT);

    currentLocationInfo().reputation = 1000;
    attemptLearn(kiLoNee, DIALOGUE_SCRIPTED_LINE_1, 239);
    EXPECT_EQ(pParty->GetGold(), 6000); // Free.
    EXPECT_EQ(roderick.getSkillValue(SKILL_LIGHT), CombinedSkillValue(8, MASTERY_MASTER));
    leaveHouse(game);

    // Master of Ancient Weapons: Rexella, npcdata 130, house 223, first topic 215 - 5000 gold and
    // the one non-stat prerequisite in the set: the character must be CARRYING a blaster
    // (an inventory item whose items.txt skill column is Blaster; MM6.EXE 0x496e8f).
    NPCData *rexella = &pNPCStats->pNPCData[130];
    EXPECT_EQ(rexella->name, "Rexella "); // Trailing space verbatim from npcdata.txt.
    ASSERT_EQ(rexella->house, HouseId(223));
    ASSERT_EQ(rexella->dialogue_1_evt_id, 215);
    ASSERT_TRUE(enterHouse(HouseId(223)));
    createHouseUI(HouseId(223));
    game.tick(2);
    roderick.setSkillValue(SKILL_BLASTER, CombinedSkillValue(4, MASTERY_EXPERT));
    pParty->SetGold(6000);

    attemptLearn(rexella, DIALOGUE_SCRIPTED_LINE_1, 215);
    EXPECT_EQ(pParty->GetGold(), 6000);
    EXPECT_EQ(roderick.getSkillValue(SKILL_BLASTER).mastery(), MASTERY_EXPERT);

    ASSERT_TRUE(roderick.inventory.tryAdd(Item(ItemId(64)))); // items.txt 64 raygun1 "Blaster".
    attemptLearn(rexella, DIALOGUE_SCRIPTED_LINE_1, 215);
    EXPECT_EQ(pParty->GetGold(), 1000);
    EXPECT_EQ(roderick.getSkillValue(SKILL_BLASTER), CombinedSkillValue(4, MASTERY_MASTER));
    leaveHouse(game);
}

// Finds a dialogue-option button in an open street NPC dialogue (GUIWindow_Dialogue, message
// UIMSG_SelectNPCDialogueOption - the house-NPC variant above uses a different message id).
static const GUIButton *findStreetDialogueOption(DialogueId topic) {
    if (!pDialogueWindow)
        return nullptr;
    for (const GUIButton *button : pDialogueWindow->vButtons)
        if (button->msg == UIMSG_SelectNPCDialogueOption && button->msg_param == std::to_underlying(topic))
            return button;
    return nullptr;
}

static void selectStreetDialogueOption(EngineController &game, DialogueId topic) {
    const GUIButton *option = findStreetDialogueOption(topic);
    ASSERT_NE(option, nullptr);
    game.pressAndReleaseButton(BUTTON_LEFT, option->rect.x + option->rect.w / 2,
                               option->rect.y + option->rect.h / 2);
    game.tick(2);
}

// MM6's street-dialogue gates: citizens with a reputation requirement refuse to talk and offer
// only Beg / Threaten / Bribe, whose outcomes are keyed to the NPC's npcprof.txt personality via
// npcbtb.txt. Begging works once, a bribe must be paid again on every visit (at a rising price),
// and a successful threat opens the normal menu forever. All three cost reputation, less with
// better Diplomacy. Fame below the NPC's requirement shuts the conversation down entirely.
GAME_TEST(Mm6, StreetBegBribeThreaten) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // The parsed personality/BTB tables: npcprof.txt's Personality column keys npcbtb.txt's flags.
    EXPECT_EQ(pNPCStats->mm6PersonalityByProfession[Smith], PERSONALITY_MERCHANT);
    EXPECT_EQ(pNPCStats->mm6PersonalityByProfession[Alchemist], PERSONALITY_SORCERER);
    EXPECT_EQ(pNPCStats->mm6PersonalityByProfession[FollowerOfBaa], PERSONALITY_EVIL_FANATIC);
    EXPECT_FALSE(pNPCStats->mm6PersonalityAcceptsBeg[PERSONALITY_MERCHANT]); // Merchant: BT - bribe & threat only.
    EXPECT_TRUE(pNPCStats->mm6PersonalityAcceptsBribe[PERSONALITY_MERCHANT]);
    EXPECT_TRUE(pNPCStats->mm6PersonalityAcceptsThreat[PERSONALITY_MERCHANT]);
    EXPECT_TRUE(pNPCStats->mm6PersonalityAcceptsBeg[PERSONALITY_PEASANT]); // Peasant: BTB - all three.
    EXPECT_FALSE(pNPCStats->mm6PersonalityAcceptsBribe[PERSONALITY_SORCERER]); // Sorcerer: TB - no bribes.
    EXPECT_FALSE(pNPCStats->mm6PersonalityAcceptsThreat[PERSONALITY_PALADIN]); // Paladin: Be - beg only.
    for (NpcPersonality personality : Segment(PERSONALITY_FIRST, PERSONALITY_LAST)) {
        EXPECT_FALSE(pNPCStats->mm6BtbTexts[1][personality].empty()); // Greeting rows cover every personality.
        EXPECT_FALSE(pNPCStats->mm6BtbTexts[13][personality].empty()); // "You aren't good enough".
    }
    EXPECT_EQ(pNPCStats->mm6ProfText[Smith][0].topic, "Rest"); // Smith's Sunday small talk.
    // npcdata's fame/rep requirement columns parse (fixed NPCs; the BTB gate itself is street-only).
    EXPECT_EQ(pNPCStats->pOriginalNPCData[22].rep, 400); // Benito Tellman.
    EXPECT_EQ(pNPCStats->pOriginalNPCData[24].fame, 800); // John Tuck.
    EXPECT_EQ(pNPCStats->pOriginalNPCData[255].rep, -1000); // Su Lang Manchu, the Notorious-only teacher.

    // Talk to a peasant to generate a citizen, then shape it: a Smith (Merchant personality) that
    // demands display reputation above +200 - a fresh party (reputation 0) gets refused.
    auto peasant = std::ranges::find_if(pActors, [](const Actor &actor) {
        return isPeasant(actor.monsterInfo.id, GAME_VERSION_MM6) && actor.CanAct();
    });
    ASSERT_NE(peasant, pActors.end());
    Vec3f peasantPos = peasant->pos;
    Vec3f pos = peasantPos + Vec3f(-160, 0, 0);
    int yawDegrees = TrigLUT.atan2(peasantPos.x - pos.x, peasantPos.y - pos.y) * 90 / 512;
    game.teleportTo(engine->_currentLoadedMapId, pos, yawDegrees);
    game.tick(1);
    pParty->setActiveCharacterIndex(1);
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_NPC_DIALOGUE);
    ASSERT_GE(speakingNpcId, 5000);
    NPCData *citizen = getNPCData(speakingNpcId);
    citizen->profession = Smith;
    citizen->rep = 200;
    citizen->flags = 0;
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);

    // Reopen: the refusal menu is exactly Beg / Threaten / Bribe, with the bribe price on its label.
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);
    auto *dialogue = static_cast<GUIWindow_Dialogue *>(pDialogueWindow.get());
    EXPECT_EQ(dialogue->mm6StreetPage(), MM6_STREET_PAGE_BTB);
    EXPECT_EQ(findStreetDialogueOption(DIALOGUE_HIRE_FIRE), nullptr);
    ASSERT_NE(findStreetDialogueOption(DIALOGUE_STREET_MM6_BEG), nullptr);
    ASSERT_NE(findStreetDialogueOption(DIALOGUE_STREET_MM6_THREATEN), nullptr);
    ASSERT_NE(findStreetDialogueOption(DIALOGUE_STREET_MM6_BRIBE), nullptr);
    int diplomacy = mm6DiplomacyBonus(pParty->activeCharacter());
    int expectedCost = std::max(10, (100 - diplomacy) * (pParty->_mm6NpcBribeCount + 1) / 2);
    EXPECT_EQ(mm6BribeCost(), expectedCost);
    EXPECT_TRUE(findStreetDialogueOption(DIALOGUE_STREET_MM6_BRIBE)->sLabel.contains(std::to_string(expectedCost)));

    // Begging a Merchant personality fails: no state change, no reputation loss, the refuse line shows.
    int reputationBefore = currentLocationInfo().reputation;
    selectStreetDialogueOption(game, DIALOGUE_STREET_MM6_BEG);
    EXPECT_EQ(dialogue->getDisplayedDialogueType(), DIALOGUE_STREET_MM6_BEG);
    EXPECT_EQ(currentLocationInfo().reputation, reputationBefore);
    EXPECT_EQ(std::to_underlying(citizen->flags) & 0x7f, 1); // Just "talked to", not "begged".

    // Bribing works: gold down by the price, the party-wide bribe counter up, greet state 3,
    // reputation down by max(0, 20 - diplomacy bonus).
    pParty->SetGold(1000);
    selectStreetDialogueOption(game, DIALOGUE_STREET_MM6_BRIBE);
    EXPECT_EQ(pParty->GetGold(), 1000 - expectedCost);
    EXPECT_EQ(pParty->_mm6NpcBribeCount, 1);
    EXPECT_EQ(std::to_underlying(citizen->flags) & 0x7f, 3);
    EXPECT_EQ(currentLocationInfo().reputation, reputationBefore - std::max(0, 20 - diplomacy));
    reputationBefore = currentLocationInfo().reputation;

    // A bribed NPC wants paying again next time: the reopened dialogue is the BTB menu again, and
    // the next bribe is pricier (the counter scales the price).
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);
    dialogue = static_cast<GUIWindow_Dialogue *>(pDialogueWindow.get());
    EXPECT_EQ(dialogue->mm6StreetPage(), MM6_STREET_PAGE_BTB);
    EXPECT_EQ(mm6BribeCost(), std::max(10, (100 - diplomacy) * 2 / 2));

    // Threatening a Merchant personality works and is permanent: greet state 4, reputation down by
    // max(0, 50 - diplomacy bonus), and the next visit gets the normal talk menu.
    selectStreetDialogueOption(game, DIALOGUE_STREET_MM6_THREATEN);
    EXPECT_EQ(std::to_underlying(citizen->flags) & 0x7f, 4);
    EXPECT_EQ(currentLocationInfo().reputation, reputationBefore - std::max(0, 50 - diplomacy));
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);
    dialogue = static_cast<GUIWindow_Dialogue *>(pDialogueWindow.get());
    EXPECT_EQ(dialogue->mm6StreetPage(), MM6_STREET_PAGE_TALK);
    EXPECT_NE(findStreetDialogueOption(DIALOGUE_STREET_MM6_PROF_TOPIC), nullptr);
    EXPECT_NE(findStreetDialogueOption(DIALOGUE_HIRE_FIRE), nullptr);
    EXPECT_NE(findStreetDialogueOption(DIALOGUE_STREET_MM6_NEWS), nullptr);

    // Fame gate: it outranks even a successful threat, and offers nothing at all.
    citizen->fame = 1 << 20;
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);
    dialogue = static_cast<GUIWindow_Dialogue *>(pDialogueWindow.get());
    EXPECT_EQ(dialogue->mm6StreetPage(), MM6_STREET_PAGE_FAME_REFUSAL);
    EXPECT_EQ(findStreetDialogueOption(DIALOGUE_STREET_MM6_BEG), nullptr);
    EXPECT_EQ(findStreetDialogueOption(DIALOGUE_STREET_MM6_PROF_TOPIC), nullptr);
    citizen->fame = 0;

    // Begging a Peasant personality works - once. The second beg is brushed off back to the
    // greeting, and reopening still shows the BTB menu (begging never re-opens the talk menu).
    reputationBefore = currentLocationInfo().reputation;
    citizen->profession = Peasant;
    citizen->flags = 0;
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);
    dialogue = static_cast<GUIWindow_Dialogue *>(pDialogueWindow.get());
    EXPECT_EQ(dialogue->mm6StreetPage(), MM6_STREET_PAGE_BTB);
    selectStreetDialogueOption(game, DIALOGUE_STREET_MM6_BEG);
    EXPECT_EQ(std::to_underlying(citizen->flags) & 0x7f, 2);
    EXPECT_EQ(currentLocationInfo().reputation, reputationBefore - std::max(0, 10 - diplomacy));
    reputationBefore = currentLocationInfo().reputation;
    selectStreetDialogueOption(game, DIALOGUE_STREET_MM6_BEG);
    EXPECT_EQ(dialogue->getDisplayedDialogueType(), DIALOGUE_MAIN); // Brushed off, no double dip.
    EXPECT_EQ(currentLocationInfo().reputation, reputationBefore);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);
    dialogue = static_cast<GUIWindow_Dialogue *>(pDialogueWindow.get());
    EXPECT_EQ(dialogue->mm6StreetPage(), MM6_STREET_PAGE_BTB);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
}

GAME_TEST(Mm6, SeerPilgrimageAndLostItems) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    pParty->GetPlayingTime() += Duration::fromHours(2); // 11:00, inside the Seer's open hours.
    game.tick(1);

    // The Seer (npcdata 9, house 169 in the Castle Ironfist region) carries MM6's two reserved
    // Seer topics: 41 "Pilgrimage" and 45 "I lost it" (plus the ordinary hint script 46).
    NPCData *seer = &pNPCStats->pNPCData[9];
    EXPECT_EQ(seer->name, "The Seer");
    ASSERT_EQ(seer->house, HouseId(169));
    ASSERT_EQ(seer->dialogue_1_evt_id, 41);
    ASSERT_EQ(seer->dialogue_2_evt_id, 45);
    EXPECT_EQ(pNPCTopics[41].pTopic, "Pilgrimage");
    EXPECT_EQ(pNPCTopics[45].pTopic, "I lost it");

    ASSERT_TRUE(enterHouse(HouseId(169)));
    createHouseUI(HouseId(169));
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_HOUSE);

    auto escapeToHouseScreen = [&] {
        for (int i = 0; i < 5 && pDialogueWindow; i++) {
            game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
            game.tick(2);
        }
        ASSERT_EQ(pDialogueWindow, nullptr);
        ASSERT_EQ(current_screen_type, SCREEN_HOUSE);
    };

    // "I lost it" with nothing missing: the party still carries its starting Letter (item 505,
    // armed by quest bit 181), so the reply is npctext row 175.
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(181)]);
    ASSERT_TRUE(pParty->hasItem(ItemId(505)));
    clickHouseNpcPortrait(game, seer);
    selectScriptedTopic(game, DIALOGUE_SCRIPTED_LINE_2);
    EXPECT_EQ(current_npc_text, pNPCTopics[174].pText); // "You never found it."
    escapeToHouseScreen();

    // Lose the Letter: the Seer replaces it (quest bit 181 + item 505 is the first table pair).
    InventoryEntry letter = pParty->pCharacters[0].inventory.find(ItemId(505));
    ASSERT_TRUE(letter);
    pParty->pCharacters[0].inventory.take(letter);
    ASSERT_FALSE(pParty->hasItem(ItemId(505)));
    clickHouseNpcPortrait(game, seer);
    selectScriptedTopic(game, DIALOGUE_SCRIPTED_LINE_2);
    EXPECT_EQ(pParty->pPickedItem.itemId, ItemId(505)); // Handed over "in hands", like event item grants.
    EXPECT_NE(current_npc_text, pNPCTopics[174].pText); // Row 176: "Here is the %s you have misplaced..."
    EXPECT_TRUE(current_npc_text.contains(pItemTable->items[ItemId(505)].unidentifiedName));
    ASSERT_TRUE(pParty->pCharacters[0].inventory.add(pParty->pPickedItem));
    pParty->takeHoldingItem();
    escapeToHouseScreen();

    // "Pilgrimage" names the current month's shrine ("This is %s, the month of %s. Journey to the
    // Shrine of %s...", npctext row 54 - months 0-6 are the stat shrines in display order).
    int month = pParty->uCurrentMonth;
    std::string shrine = month <= 6
        ? localization->attributeName(static_cast<Attribute>(month))
        : localization->str(static_cast<LstrId>(std::array<int, 5>{87, 71, 43, 166, 138}[month - 7]));
    std::string expected = fmt::sprintf(pNPCTopics[53].pText, localization->monthName(month), shrine, shrine); // NOLINT: this is not ::sprintf.
    clickHouseNpcPortrait(game, seer);
    selectScriptedTopic(game, DIALOGUE_SCRIPTED_LINE_1);
    EXPECT_EQ(current_npc_text, expected);
    escapeToHouseScreen();

    // With this month's blessing already taken (quest bit 206), the Seer tells the party to wait.
    pParty->_questBits[static_cast<QuestBit>(206)] = true;
    clickHouseNpcPortrait(game, seer);
    selectScriptedTopic(game, DIALOGUE_SCRIPTED_LINE_1);
    EXPECT_EQ(current_npc_text, pNPCTopics[54].pText); // "You must wait until the new month..."
    escapeToHouseScreen();
    leaveHouse(game);

    // A Seer visit in a NEW month resets the pilgrimage bits (MM6.EXE @0x4A2F20 recomputes the
    // start-of-next-month timestamp and clears bits 205/206).
    pParty->GetPlayingTime() += Duration::fromDays(28); // MM months are exactly 28 days.
    game.tick(2);
    ASSERT_NE(pParty->uCurrentMonth, month);
    ASSERT_TRUE(enterHouse(HouseId(169)));
    createHouseUI(HouseId(169));
    game.tick(2);
    clickHouseNpcPortrait(game, seer);
    selectScriptedTopic(game, DIALOGUE_SCRIPTED_LINE_1);
    EXPECT_FALSE(pParty->_questBits[static_cast<QuestBit>(206)]);
    EXPECT_TRUE(current_npc_text.contains(localization->monthName(pParty->uCurrentMonth)));
    escapeToHouseScreen();
    leaveHouse(game);

    // The shrine end of the loop: New Sorpigal's shrine (oute3 event 261) is the month-6 Luck
    // shrine - Cmp(MonthIs, 6), then the whole party gets +10 permanent Luck once (quest bit 213)
    // and the monthly blessing gate (bit 206) closes until the next Seer visit in a new month.
    while (pParty->uCurrentMonth != 6) {
        pParty->GetPlayingTime() += Duration::fromDays(28);
        game.tick(1);
    }
    pParty->_questBits[static_cast<QuestBit>(206)] = false;
    std::array<int, 4> luckBefore;
    for (int i = 0; i < 4; i++)
        luckBefore[i] = pParty->pCharacters[i]._stats[ATTRIBUTE_LUCK];
    eventProcessor(261, Pid(), 1);
    for (int i = 0; i < 4; i++)
        EXPECT_EQ(pParty->pCharacters[i]._stats[ATTRIBUTE_LUCK], luckBefore[i] + 10);
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(206)]);
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(213)]);

    // Praying again the same month: blocked by bit 206, nothing changes.
    eventProcessor(261, Pid(), 1);
    for (int i = 0; i < 4; i++)
        EXPECT_EQ(pParty->pCharacters[i]._stats[ATTRIBUTE_LUCK], luckBefore[i] + 10);
    game.tick(2);
}

GAME_TEST(Mm6, TownHallBountyTopic) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    pParty->GetPlayingTime() += Duration::fromHours(2); // 11:00, town halls open at 10:00.
    game.tick(1);

    // Janice, the New Sorpigal town-hall proprietor, carries the reserved bounty topic 399 in her
    // third npcdata slot (MM6.EXE routes it to the bounty handler @0x4A30B0) - talking to her as a
    // house occupant must open the same bounty interaction as the town-hall house option, not run
    // global script 399.
    NPCData *janice = &pNPCStats->pNPCData[291];
    EXPECT_EQ(janice->name, "Janice");
    ASSERT_EQ(janice->house, HouseId(89));
    ASSERT_EQ(janice->dialogue_3_evt_id, 399);

    ASSERT_TRUE(enterHouse(HouseId(89)));
    createHouseUI(HouseId(89));
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_HOUSE);

    clickHouseNpcPortrait(game, janice);
    selectScriptedTopic(game, DIALOGUE_SCRIPTED_LINE_3);

    // The first interaction generates the month's bounty for the New Sorpigal slot and announces it.
    HouseId slot = HOUSE_FIRST_TOWN_HALL;
    MonsterId target = pParty->monster_id_for_hunting[slot];
    ASSERT_NE(target, MONSTER_INVALID);
    EXPECT_TRUE(current_npc_text.contains(pMonsterStats->infos[target].name));
    game.tick(2);
}

GAME_TEST(Mm6, ArenaFightAndPrize) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // MM6's arena is the indoor map zarena.blv; the Arena Master is npcdata 307, reached through
    // the map's SpeakNPC(307) event 5 (he is not a placed actor). His only topic is the reserved
    // arena topic 400 (MM6.EXE @0x4A3350) - MM7's whole arena flow descends from it, so the fight
    // rides the same code with MM6's own numbers.
    NPCData *master = &pNPCStats->pNPCData[307];
    EXPECT_EQ(master->name, "Arena Master");
    ASSERT_EQ(master->dialogue_1_evt_id, 400);

    game.teleportTo(pMapStats->GetMapInfo("zarena.blv"), Vec3f(0, 0, 0), 0);
    game.teleportTo(pMapStats->GetMapInfo("zarena.blv"), Vec3f(3849, 5770, 1), 0);
    game.tick(5);
    ASSERT_EQ(pActors.size(), 0u); // The arena is empty until a fight is arranged.
    ASSERT_EQ(pParty->arenaState, ARENA_STATE_INITIAL);

    auto talkToArenaMaster = [&] {
        eventProcessor(5, Pid(), 1); // zarena event 5 = SpeakNPC(307).
        game.tick(2);
        ASSERT_NE(pDialogueWindow, nullptr);
    };

    // First talk: the welcome screen offers the four difficulty tiers (labels are MM6's own
    // global.txt rows 578-581, which MM7 kept at the same rows).
    talkToArenaMaster();
    selectStreetDialogueOption(game, DIALOGUE_SCRIPTED_LINE_1);
    EXPECT_NE(findStreetDialogueOption(DIALOGUE_ARENA_SELECT_PAGE), nullptr);
    EXPECT_NE(findStreetDialogueOption(DIALOGUE_ARENA_SELECT_SQUIRE), nullptr);
    EXPECT_NE(findStreetDialogueOption(DIALOGUE_ARENA_SELECT_KNIGHT), nullptr);
    EXPECT_NE(findStreetDialogueOption(DIALOGUE_ARENA_SELECT_LORD), nullptr);

    // Pick Page: 6-8 monsters spawn at the fixed placements, drawn from every monsters.txt row
    // 1-171 whose level fits the tier window - for a level-1 party that's [1, 1] (MM6's floor is 1,
    // not MM7's 2). The dialogue closes and the party stands in the pit.
    int characterMaxLevel = 0;
    for (Character &character : pParty->pCharacters)
        characterMaxLevel = std::max(characterMaxLevel, (int)character.GetActualLevel());
    ASSERT_EQ(characterMaxLevel, 1);
    selectStreetDialogueOption(game, DIALOGUE_ARENA_SELECT_PAGE);
    game.tick(3);
    EXPECT_EQ(pDialogueWindow, nullptr);
    EXPECT_EQ(pParty->arenaState, ARENA_STATE_FIGHTING);
    EXPECT_EQ(pParty->arenaLevel, ARENA_LEVEL_PAGE);
    EXPECT_GE(pActors.size(), 6u);
    EXPECT_LE(pActors.size(), 8u);
    for (const Actor &actor : pActors)
        EXPECT_EQ(pMonsterStats->infos[actor.monsterInfo.id].level, 1);
    EXPECT_EQ(pParty->pos.x, 3849);
    EXPECT_EQ(pParty->pos.y, 5770);

    // Talking mid-fight sends the party back into the pit ("Get back in there you wimps").
    pParty->pos = Vec3f(3000, 5000, 1);
    talkToArenaMaster();
    selectStreetDialogueOption(game, DIALOGUE_SCRIPTED_LINE_1);
    game.tick(3);
    EXPECT_EQ(pParty->pos.x, 3849);
    EXPECT_EQ(pParty->pos.y, 5770);

    // Win: prize = 50 x max party level for Page, every character gets MM6's award 84
    // ("%u Page Arena Victories"), and the win counter ticks.
    for (Actor &actor : pActors)
        actor.aiState = Dead;
    int goldBefore = pParty->GetGold();
    talkToArenaMaster();
    selectStreetDialogueOption(game, DIALOGUE_SCRIPTED_LINE_1);
    EXPECT_EQ(pParty->GetGold(), goldBefore + 50 * characterMaxLevel);
    EXPECT_EQ(pParty->uNumArenaWins[ARENA_LEVEL_PAGE], 1);
    EXPECT_EQ(pParty->arenaState, ARENA_STATE_WON);
    for (Character &character : pParty->pCharacters)
        EXPECT_TRUE(character._achievedAwardsBits[static_cast<AwardId>(84)]);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);

    // Asking again the same trip: no rematch, no second payout.
    talkToArenaMaster();
    selectStreetDialogueOption(game, DIALOGUE_SCRIPTED_LINE_1);
    EXPECT_EQ(pParty->GetGold(), goldBefore + 50 * characterMaxLevel);
    EXPECT_EQ(pParty->uNumArenaWins[ARENA_LEVEL_PAGE], 1);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);

    // Leaving the map resets the arena for the next visit.
    game.teleportTo(pMapStats->GetMapInfo("oute3.odm"), Vec3f(-9728, -11319, 160), 0);
    game.tick(2);
    EXPECT_EQ(pParty->arenaState, ARENA_STATE_INITIAL);
    game.tick(2);
}

// Stashes the item the last event grant left on the cursor (AddVariable(VAR_PlayerItemInHands)
// puts it into pParty->pPickedItem) and returns its id.
static ItemId stashPickedItem() {
    ItemId id = pParty->pPickedItem.itemId;
    if (id != ITEM_NULL) {
        pParty->pCharacters[0].inventory.add(pParty->pPickedItem);
        pParty->takeHoldingItem();
    }
    return id;
}

// MM6's traveling circus (2dEvents house 166) is pure event data: the door events on
// outb2/outc3/outd2 gate entry to one 28-day month per site via Cmp(DayOfYear, d) chains
// (Bootleg Bay = days 308-335), outd1's site is ungated; the game tents (houses 532-537,
// same DayOfYear gates) charge 50 gold and roll a stat-tiered RandomGoTo prize - Lodestone
// 470, Harpy Feather 471, Four Leaf Clover 477 (ev107 = the Might game, all six branches
// pay out at Might >= 200); Blaze the Circus Master (npc 392, topic 105) redeems the
// prizes via VAR_CircusPrises - a computed variable counting Lodestones x1 + Feathers x3 +
// Clovers x5 that MM7's engine inherited from MM6 verbatim - at >= 30 points for the
// Golden Pyramid 472 (an Oracle quest item) and >= 10 for a Keg of Wine 473, then burns
// every prize item in the party.
GAME_TEST(Mm6, CircusGamesAndPrizes) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    // The Mire of the Damned (outc3) hosts the circus in month 7 (door event 14 gates on days
    // 196-223); its game tents are events 15-20.
    MapId mire = pMapStats->GetMapInfo("outc3.odm");
    ASSERT_NE(mire, MAP_INVALID);
    game.teleportTo(mire, Vec3f(0, 0, 512), 0);
    game.tick(1);

    // Out of season the circus door (event 14) just prints "come back later" and stays closed.
    const BLVFace *door = nullptr;
    for (const BSPModel &model : pOutdoor->pBModels)
        for (const BLVFace &face : model.faces)
            if (face.eventId == 14 && face.Clickable())
                door = &face;
    ASSERT_NE(door, nullptr);
    ASSERT_NE(pParty->GetPlayingTime().toCivilTime().month - 1, 7); // A new game starts outside the window.
    Vec3f doorCenter = door->boundingBox.center();
    Vec3f pos = doorCenter + door->facePlane.normal * 130;
    pos.z = door->boundingBox.z1;
    int yawDegrees = TrigLUT.atan2(doorCenter.x - pos.x, doorCenter.y - pos.y) * 90 / 512;
    game.teleportTo(mire, pos, yawDegrees);
    game.tick(1);
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);

    // In month 7 the same door opens the circus.
    advanceToMonth(game, 7);
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_HOUSE);
    ASSERT_NE(window_SpeakInHouse, nullptr);
    EXPECT_EQ(window_SpeakInHouse->houseId(), HouseId(166));

    NPCData *blaze = &pNPCStats->pNPCData[392];
    ASSERT_EQ(blaze->name, "Blaze the Circus Master");
    ASSERT_EQ(blaze->dialogue_2_evt_id, 105u);

    // No prizes: the redemption topic refuses.
    clickHouseNpcPortrait(game, blaze);
    selectScriptedTopic(game, DIALOGUE_SCRIPTED_LINE_2);
    EXPECT_TRUE(current_npc_text.contains("you don't have 10 points"));
    EXPECT_EQ(pParty->pPickedItem.itemId, ITEM_NULL);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);

    // Two Four Leaf Clovers = 10 points: a Keg of Wine, and the clovers are gone.
    ASSERT_TRUE(pParty->pCharacters[0].inventory.add(Item(ItemId(477))));
    ASSERT_TRUE(pParty->pCharacters[1].inventory.add(Item(ItemId(477))));
    clickHouseNpcPortrait(game, blaze);
    selectScriptedTopic(game, DIALOGUE_SCRIPTED_LINE_2);
    EXPECT_TRUE(current_npc_text.contains("keg of wine"));
    EXPECT_EQ(stashPickedItem(), ItemId(473));
    EXPECT_FALSE(pParty->pCharacters[0].inventory.find(ItemId(477)));
    EXPECT_FALSE(pParty->pCharacters[1].inventory.find(ItemId(477)));
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);

    // A 30-point haul (4 clovers + 3 feathers + a lodestone = 32) earns the Golden Pyramid.
    for (int i = 0; i < 4; i++)
        ASSERT_TRUE(pParty->pCharacters[i].inventory.add(Item(ItemId(477))));
    for (int i = 0; i < 3; i++)
        ASSERT_TRUE(pParty->pCharacters[i].inventory.add(Item(ItemId(471))));
    ASSERT_TRUE(pParty->pCharacters[3].inventory.add(Item(ItemId(470))));
    clickHouseNpcPortrait(game, blaze);
    selectScriptedTopic(game, DIALOGUE_SCRIPTED_LINE_2);
    EXPECT_TRUE(current_npc_text.contains("golden pyramid"));
    EXPECT_EQ(stashPickedItem(), ItemId(472));
    for (int i = 0; i < 4; i++) {
        EXPECT_FALSE(pParty->pCharacters[i].inventory.find(ItemId(470)));
        EXPECT_FALSE(pParty->pCharacters[i].inventory.find(ItemId(471)));
        EXPECT_FALSE(pParty->pCharacters[i].inventory.find(ItemId(477)));
    }
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_GAME);

    // The strongman tent (event 15, house 532): 50 gold a game; at Might >= 200 every
    // RandomGoTo branch pays out a prize item. Tent entries can hang off either a model face or
    // a billboard decoration.
    for (Character &character : pParty->pCharacters)
        character._statBonuses[ATTRIBUTE_MIGHT] = 500;
    pParty->SetGold(1000);
    Vec3f tentStand;
    Vec3f tentCenter;
    bool tentFound = false;
    for (const BSPModel &model : pOutdoor->pBModels)
        for (const BLVFace &face : model.faces)
            if (face.eventId == 15 && face.Clickable()) {
                tentCenter = face.boundingBox.center();
                tentStand = tentCenter + face.facePlane.normal * 130;
                tentStand.z = face.boundingBox.z1;
                tentFound = true;
            }
    if (!tentFound) {
        for (const LevelDecoration &decoration : pLevelDecorations)
            if (decoration.uEventID == 15) {
                tentCenter = decoration.vPosition;
                tentStand = tentCenter + Vec3f(0, -120, 0);
                tentFound = true;
            }
    }
    ASSERT_TRUE(tentFound);
    int tentYaw = TrigLUT.atan2(tentCenter.x - tentStand.x, tentCenter.y - tentStand.y) * 90 / 512;
    game.teleportTo(mire, tentStand, tentYaw);
    game.tick(1);
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_HOUSE);
    ASSERT_NE(window_SpeakInHouse, nullptr);
    EXPECT_EQ(window_SpeakInHouse->houseId(), HouseId(532));
    NPCData *tarquin = &pNPCStats->pNPCData[393];
    ASSERT_EQ(tarquin->name, "Sir William Tarquin");
    ASSERT_EQ(tarquin->dialogue_2_evt_id, 107u);
    clickHouseNpcPortrait(game, tarquin);
    selectScriptedTopic(game, DIALOGUE_SCRIPTED_LINE_2);
    EXPECT_EQ(pParty->GetGold(), 950);
    ItemId prize = stashPickedItem();
    EXPECT_TRUE(prize == ItemId(470) || prize == ItemId(471) || prize == ItemId(477));
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
    game.tick(5);
}

// The Free Haven High Council (2dEvents house 165) is a plain house with six councilman
// NPCs (299-304, topics 375-380). Five of them run Cmp(Award, N) quest offers; Slicker
// Silvertongue the traitor (npc 304, topic 380 = global event 380) refuses his vote until
// the party presents the Letter from Zenofex (item 502), which convicts him: the letter is
// taken, the whole party gets award 32, quest bit 168 is set, reputation rises by 200 and
// MoveNPC(304, 0) removes him from the council for good.
GAME_TEST(Mm6, CouncilQuestsAndTraitor) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    MapId freeHaven = pMapStats->GetMapInfo("outc2.odm");
    ASSERT_NE(freeHaven, MAP_INVALID);
    game.teleportTo(freeHaven, Vec3f(0, 0, 512), 0);
    game.tick(1);
    enterHouseThroughDoor(game, 49);
    ASSERT_NE(window_SpeakInHouse, nullptr);
    EXPECT_EQ(window_SpeakInHouse->houseId(), HouseId(165));

    // All six councilmen hold session.
    int councilmen = 0;
    for (const HouseNpcDesc &npc : houseNpcs)
        if (npc.type == HOUSE_NPC)
            councilmen++;
    EXPECT_EQ(councilmen, 6);

    // Preston Steel's topic is a quest offer until Lord Temper's award (4) is earned.
    NPCData *preston = &pNPCStats->pNPCData[299];
    ASSERT_EQ(preston->name, "Preston Steel");
    clickHouseNpcPortrait(game, preston);
    selectScriptedTopic(game, DIALOGUE_SCRIPTED_LINE_1);
    EXPECT_TRUE(current_npc_text.contains("Lord Temper"));
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);

    // Slicker without the letter: a refusal, nothing changes.
    NPCData *slicker = &pNPCStats->pNPCData[304];
    ASSERT_EQ(slicker->name, "Slicker Silvertongue");
    ASSERT_EQ(slicker->dialogue_1_evt_id, 380u);
    clickHouseNpcPortrait(game, slicker);
    selectScriptedTopic(game, DIALOGUE_SCRIPTED_LINE_1);
    EXPECT_TRUE(current_npc_text.contains("cannot give you my vote"));
    EXPECT_FALSE(pParty->_questBits[static_cast<QuestBit>(168)]);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);

    // With the Letter from Zenofex the traitor is convicted and jailed. The script's
    // MoveNPC(304, 0) trips MM6.EXE's council special (0x43cdb0): the dialogue is torn down on
    // the spot and the chamber re-enters playing the one-shot "Citytrtr" clip - Slicker's reply
    // text never gets to show.
    ASSERT_TRUE(pParty->pCharacters[0].inventory.add(Item(ItemId(502))));
    int reputationBefore = pParty->GetPartyReputation();
    clickHouseNpcPortrait(game, slicker);
    const GUIButton *conviction = findScriptedTopicButton(DIALOGUE_SCRIPTED_LINE_1);
    ASSERT_NE(conviction, nullptr);
    game.pressAndReleaseButton(BUTTON_LEFT, conviction->rect.x + conviction->rect.w / 2,
                               conviction->rect.y + conviction->rect.h / 2);
    game.tick(1);
    EXPECT_FALSE(pParty->pCharacters[0].inventory.find(ItemId(502))); // Letter taken as evidence.
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(168)]);
    for (const Character &character : pParty->pCharacters)
        EXPECT_TRUE(character._achievedAwardsBits[static_cast<AwardId>(32)]);
    // The script's Add(Reputation, 200) sits under ForPartyMember(all), and variable ops run once
    // per member - reputation moves by 4 x 200, faithfully to the original interpreters.
    EXPECT_EQ(pParty->GetPartyReputation(), reputationBefore + 800);
    EXPECT_EQ(slicker->house, HouseId(0)); // MoveNPC(304, 0) - gone from the council.
    // The re-entered chamber: the "Citytrtr" one-shot is up, Slicker's seat already empty.
    ASSERT_NE(window_SpeakInHouse, nullptr);
    EXPECT_EQ(window_SpeakInHouse->houseId(), HOUSE_MM6_COUNCIL);
    EXPECT_EQ(pMediaPlayer->currentHouseMovieName(), "Citytrtr");
    councilmen = 0;
    for (const HouseNpcDesc &npc : houseNpcs)
        if (npc.type == HOUSE_NPC)
            councilmen++;
    EXPECT_EQ(councilmen, 5);
    // Headless one-shots are over instantly; the movie-end pass re-enters the chamber loop.
    game.tick(1);
    ASSERT_NE(window_SpeakInHouse, nullptr);
    EXPECT_EQ(window_SpeakInHouse->houseId(), HOUSE_MM6_COUNCIL);
    EXPECT_EQ(pMediaPlayer->currentHouseMovieName(),
              houseAnimDescr(houseTable[HOUSE_MM6_COUNCIL].uAnimationID).video_name);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_GAME);

    // Re-entering through the door finds only five councilmen.
    enterHouseThroughDoor(game, 49);
    councilmen = 0;
    for (const HouseNpcDesc &npc : houseNpcs)
        if (npc.type == HOUSE_NPC)
            councilmen++;
    EXPECT_EQ(councilmen, 5);
    // No Oracle door yet: MM6 shows the extra exit only once its quest bit is EARNED
    // (2dEvents row 165: ExitPic 5, map 49 = Oracle of Enroth, quest bit 167).
    for (const HouseNpcDesc &npc : houseNpcs)
        EXPECT_NE(npc.type, HOUSE_TRANSITION);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);

    // Once all six council quests are done (quest bit 167), the council chamber grows a door
    // leading straight to the Oracle.
    pParty->_questBits[static_cast<QuestBit>(167)] = true;
    enterHouseThroughDoor(game, 49);
    const HouseNpcDesc *oracleDoor = nullptr;
    for (const HouseNpcDesc &npc : houseNpcs)
        if (npc.type == HOUSE_TRANSITION)
            oracleDoor = &npc;
    ASSERT_NE(oracleDoor, nullptr);
    EXPECT_EQ(oracleDoor->targetMapID, pMapStats->GetMapInfo("oracle.blv"));
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    game.tick(5);
}

// The Hermit on the Mountain (2dEvents house 552, Kriegspire) is a plain single-occupant
// house; his one topic (95) is a flavor monologue.
GAME_TEST(Mm6, HermitHouse) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    MapId kriegspire = pMapStats->GetMapInfo("outb1.odm");
    ASSERT_NE(kriegspire, MAP_INVALID);
    game.teleportTo(kriegspire, Vec3f(0, 0, 512), 0);
    game.tick(1);
    enterHouseThroughDoor(game, 14);
    ASSERT_NE(window_SpeakInHouse, nullptr);
    EXPECT_EQ(window_SpeakInHouse->houseId(), HouseId(552));

    NPCData *hermit = &pNPCStats->pNPCData[19];
    ASSERT_EQ(hermit->name, "The Hermit on the Mountain");
    clickHouseNpcPortrait(game, hermit);
    selectScriptedTopic(game, DIALOGUE_SCRIPTED_LINE_1);
    EXPECT_TRUE(current_npc_text.contains("watch the world turn"));
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
    game.tick(5);
}

// The Oracle of Enroth (2dEvents house 170, inside Oracle.Blv). The ONLY working entrance is
// the High Council's quest-bit-167 exit door (2dEvents row 165: map 49 = Oracle of Enroth) -
// outc2's face events stop at 152, so the .evt's MoveToMap event 153 is an unwired dev
// leftover, and the 2dEvents "Access denied / Control Cube" text is a col-24 editor
// annotation the EXE never parses (its 2devents parser stops at col 23). Slicker's refusal
// line ("as long as I am a member of this council, you will not be permitted to visit the
// Oracle") describes exactly this gate. The Control Cube (456) is a fetch-quest hand-in:
// Oracle topic 76 first sends the party after it (award 33, quest bit 166), then trades it
// for 500k exp and award 34 and retires the Oracle's topics to 77/78. Topic 73 kicks off
// the Memory Crystal hunt (quest bits 162-165).
GAME_TEST(Mm6, OracleCrystalsAndCube) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    MapId freeHaven = pMapStats->GetMapInfo("outc2.odm");
    ASSERT_NE(freeHaven, MAP_INVALID);
    game.teleportTo(freeHaven, Vec3f(0, 0, 512), 0);
    game.tick(1);

    // With the council quests done, the council chamber's Oracle door is open.
    pParty->_questBits[static_cast<QuestBit>(167)] = true;
    enterHouseThroughDoor(game, 49);
    const HouseNpcDesc *oracleDoor = nullptr;
    for (const HouseNpcDesc &npc : houseNpcs)
        if (npc.type == HOUSE_TRANSITION)
            oracleDoor = &npc;
    ASSERT_NE(oracleDoor, nullptr);
    ASSERT_NE(oracleDoor->button, nullptr);
    Recti doorRect = oracleDoor->button->rect;
    game.pressAndReleaseButton(BUTTON_LEFT, doorRect.x + doorRect.w / 2, doorRect.y + doorRect.h / 2);
    game.tick(2);
    game.pressAndReleaseKey(PlatformKey::KEY_Y);
    game.tick(10);
    ASSERT_EQ(engine->_currentLoadedMapId, pMapStats->GetMapInfo("oracle.blv"));

    // The Oracle herself is house 170, opened from a face inside (event 3, ev3 st0 =
    // SpeakInHouse(170) with no gate); open the house the way the face script would.
    const BLVFace *console = nullptr;
    for (const BLVFace &face : pIndoor->faces)
        if (face.eventId == 3 && face.Clickable())
            console = &face;
    EXPECT_NE(console, nullptr);
    ASSERT_TRUE(enterHouse(HouseId(170)));
    createHouseUI(HouseId(170));
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_HOUSE);
    ASSERT_NE(window_SpeakInHouse, nullptr);
    EXPECT_EQ(window_SpeakInHouse->houseId(), HouseId(170));

    NPCData *oracle = &pNPCStats->pNPCData[8];
    ASSERT_EQ(oracle->name, "Oracle");
    ASSERT_EQ(oracle->dialogue_1_evt_id, 73u);

    // Topic 73 hands out the Memory Crystal hunt.
    clickHouseNpcPortrait(game, oracle);
    selectScriptedTopic(game, DIALOGUE_SCRIPTED_LINE_1);
    for (int bit : {162, 163, 164, 165})
        EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(bit)]);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);

    // Placing all four crystals rewires topic slot 1 to event 76 (oracle.blv events 5/15/16/17);
    // install it directly and run both of its stages.
    oracle->dialogue_1_evt_id = 76;
    clickHouseNpcPortrait(game, oracle);
    selectScriptedTopic(game, DIALOGUE_SCRIPTED_LINE_1);
    EXPECT_TRUE(current_npc_text.contains("Melian"));
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(166)]);
    for (const Character &character : pParty->pCharacters)
        EXPECT_TRUE(character._achievedAwardsBits[static_cast<AwardId>(33)]);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);

    // Returning with the Control Cube: 500k exp a head, award 34, topics retire to 77/78.
    ASSERT_TRUE(pParty->pCharacters[0].inventory.add(Item(ItemId(456))));
    uint64_t expBefore = pParty->pCharacters[0].experience;
    clickHouseNpcPortrait(game, oracle);
    selectScriptedTopic(game, DIALOGUE_SCRIPTED_LINE_1);
    EXPECT_TRUE(current_npc_text.contains("transported"));
    EXPECT_FALSE(pParty->pCharacters[0].inventory.find(ItemId(456)));
    EXPECT_EQ(pParty->pCharacters[0].experience, expBefore + 500000);
    for (const Character &character : pParty->pCharacters)
        EXPECT_TRUE(character._achievedAwardsBits[static_cast<AwardId>(34)]);
    EXPECT_FALSE(pParty->_questBits[static_cast<QuestBit>(166)]);
    EXPECT_EQ(oracle->dialogue_1_evt_id, 77u);
    EXPECT_EQ(oracle->dialogue_2_evt_id, 78u);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
    game.tick(5);
}

// The King's Library (Castle Ironfist region, outd3 door event 42) is a three-stage house:
// the door opens house 168 (empty reading room) until quest bit 177; MM6.EXE's enterHouse
// additionally scans the party for Tanir's Bell (item 461) and with it chains the entry to
// house 553 - Archibald Ironfist's library, where his topic 30 hands over the Ritual of the
// Void (item 544, +50000 exp each, quest bit 177) and MoveNPC(12, 0) retires him. With the
// bit set the door script itself opens house 554 (the post-Ritual empty stage).
GAME_TEST(Mm6, LibraryArchibaldRitual) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    MapId ironfist = pMapStats->GetMapInfo("outd3.odm");
    ASSERT_NE(ironfist, MAP_INVALID);
    game.teleportTo(ironfist, Vec3f(0, 0, 512), 0);
    game.tick(1);

    // Without Tanir's Bell the library is the empty house 168.
    enterHouseThroughDoor(game, 42);
    ASSERT_NE(window_SpeakInHouse, nullptr);
    EXPECT_EQ(window_SpeakInHouse->houseId(), HouseId(168));
    for (const HouseNpcDesc &npc : houseNpcs)
        EXPECT_NE(npc.type, HOUSE_NPC);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);

    // With the bell aboard the same door plays the one-shot "archie" cutscene over the still-empty
    // house 168 (no redirect at entry), and the movie-end pass (MM6.EXE 0x4a617f) re-enters as
    // house 553 - Archibald's library. Headless one-shots are over instantly.
    ASSERT_TRUE(pParty->pCharacters[0].inventory.add(Item(ItemId(461))));
    ASSERT_TRUE(enterHouse(HOUSE_MM6_LIBRARY));
    EXPECT_EQ(enteredHouseId, HOUSE_MM6_LIBRARY);
    EXPECT_EQ(pMediaPlayer->currentHouseMovieName(), "archie");
    createHouseUI(HOUSE_MM6_LIBRARY);
    game.tick(1);
    ASSERT_NE(window_SpeakInHouse, nullptr);
    EXPECT_EQ(window_SpeakInHouse->houseId(), HOUSE_MM6_LIBRARY_ARCHIBALD);
    EXPECT_EQ(pMediaPlayer->currentHouseMovieName(),
              houseAnimDescr(houseTable[HOUSE_MM6_LIBRARY_ARCHIBALD].uAnimationID).video_name);
    NPCData *archibald = &pNPCStats->pNPCData[12];
    ASSERT_EQ(archibald->name, "Archibald Ironfist");
    uint64_t expBefore = pParty->pCharacters[0].experience;
    clickHouseNpcPortrait(game, archibald);
    selectScriptedTopic(game, DIALOGUE_SCRIPTED_LINE_1);
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(177)]);
    EXPECT_EQ(pParty->pCharacters[0].experience, expBefore + 50000);
    EXPECT_EQ(archibald->house, HOUSE_INVALID); // MoveNPC(12, 0) - he has left the library.
    // That MoveNPC also demotes the room clip to a single pass (MM6.EXE 0x43ce99); once it's out
    // the movie-end pass advances the screen to the empty stage 554, parking the granted Ritual
    // (held on the cursor) into a pack on the way.
    ASSERT_NE(window_SpeakInHouse, nullptr);
    EXPECT_EQ(window_SpeakInHouse->houseId(), HOUSE_MM6_LIBRARY_EMPTY);
    EXPECT_TRUE(std::ranges::any_of(pParty->pCharacters, [](const Character &character) {
        return !!character.inventory.find(ItemId(544)); // The Ritual of the Void.
    }));
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_GAME);

    // Post-Ritual the door script reroutes to the empty stage 554 (bell or not).
    enterHouseThroughDoor(game, 42);
    ASSERT_NE(window_SpeakInHouse, nullptr);
    EXPECT_EQ(window_SpeakInHouse->houseId(), HOUSE_MM6_LIBRARY_EMPTY);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    game.tick(5);
}

// MM6's jail (2dEvents row 167 "Prison", reachable from no door): calling on any castle
// throne room with party reputation at -1000 or below gets the party arrested (MM6.EXE
// 0x43c58c) - the house screen becomes the Prison, a year passes without rest, reputation
// resets, the prison-term counter ticks and everyone earns award 83.
GAME_TEST(Mm6, JailForNotoriety) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    MapId ironfist = pMapStats->GetMapInfo("outd3.odm");
    ASSERT_NE(ironfist, MAP_INVALID);
    game.teleportTo(ironfist, Vec3f(0, 0, 512), 0);
    game.tick(1);
    currentLocationInfo().reputation = -1500;
    int yearBefore = pParty->GetPlayingTime().toCivilTime().year;

    // The Castle Ironfist door chain (event 43): prompt, confirm, throne room... which for a
    // notorious party opens as the Prison instead.
    const BLVFace *door = nullptr;
    for (const BSPModel &model : pOutdoor->pBModels)
        for (const BLVFace &face : model.faces)
            if (face.eventId == 43 && face.Clickable())
                door = &face;
    ASSERT_NE(door, nullptr);
    Vec3f doorCenter = door->boundingBox.center();
    Vec3f pos = doorCenter + door->facePlane.normal * 130;
    pos.z = door->boundingBox.z1;
    int yawDegrees = TrigLUT.atan2(doorCenter.x - pos.x, doorCenter.y - pos.y) * 90 / 512;
    game.teleportTo(ironfist, pos, yawDegrees);
    game.tick(1);
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_INPUT_BLV);
    game.pressAndReleaseKey(PlatformKey::KEY_Y);
    game.tick(5);
    ASSERT_EQ(current_screen_type, SCREEN_HOUSE);
    ASSERT_NE(window_SpeakInHouse, nullptr);
    EXPECT_EQ(window_SpeakInHouse->houseId(), HouseId(167));
    EXPECT_EQ(pParty->GetPlayingTime().toCivilTime().year, yearBefore + 1);
    EXPECT_EQ(currentLocationInfo().reputation, 0);
    EXPECT_EQ(pParty->uNumPrisonTerms, 1);
    for (const Character &character : pParty->pCharacters) {
        EXPECT_TRUE(character._achievedAwardsBits[static_cast<AwardId>(83)]);
        EXPECT_EQ(character.timeToRecovery, 0_ticks);
    }
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);

    // With a clean record the same door reaches Wilbur Humphrey's throne room again.
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_INPUT_BLV);
    game.pressAndReleaseKey(PlatformKey::KEY_Y);
    game.tick(5);
    ASSERT_EQ(current_screen_type, SCREEN_HOUSE);
    EXPECT_EQ(window_SpeakInHouse->houseId(), HouseId(154));
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    game.tick(5);
}

// Six ordinary-looking Free Haven houses (2dEvents rows 286/290/316/323/331/528) hide the only
// entrances to the Free Haven Sewer: their exit column says map 47 = Sewer.Blv with a NEGATIVE
// quest-bit value selecting a fixed arrival pose (MM6.EXE tables @0x4BE3B8..0x4BE400). The door
// is always available; sewer.evt events 20-25 are the ladders back up.
GAME_TEST(Mm6, SewerEntranceTeleport) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    MapId freeHaven = pMapStats->GetMapInfo("outc2.odm");
    ASSERT_NE(freeHaven, MAP_INVALID);
    game.teleportTo(freeHaven, Vec3f(0, 0, 512), 0);
    game.tick(1);

    // House 286 (door event 59) carries pose index 2.
    ASSERT_EQ(houseTable[HouseId(286)].mm6ExitPoseIndex, 2);
    ASSERT_EQ(houseTable[HouseId(286)].uExitMapID, pMapStats->GetMapInfo("sewer.blv"));
    enterHouseThroughDoor(game, 59);
    ASSERT_NE(window_SpeakInHouse, nullptr);
    EXPECT_EQ(window_SpeakInHouse->houseId(), HouseId(286));

    const HouseNpcDesc *hatch = nullptr;
    int hatchIndex = -1;
    for (int i = 0; i < houseNpcs.size(); i++) {
        if (houseNpcs[i].type == HOUSE_TRANSITION) {
            hatch = &houseNpcs[i];
            hatchIndex = i;
        }
    }
    ASSERT_NE(hatch, nullptr);
    ASSERT_NE(hatch->button, nullptr);

    // Click the hatch, then confirm the transition - down into the sewer at the fixed pose.
    Recti hatchRect = hatch->button->rect;
    game.pressAndReleaseButton(BUTTON_LEFT, hatchRect.x + hatchRect.w / 2, hatchRect.y + hatchRect.h / 2);
    game.tick(2);
    game.pressAndReleaseKey(PlatformKey::KEY_Y);
    game.tick(10);
    EXPECT_EQ(engine->_currentLoadedMapId, pMapStats->GetMapInfo("sewer.blv"));
    EXPECT_NEAR(pParty->pos.x, 4234, 128); // Pose 2 from the EXE tables; physics may nudge the party.
    EXPECT_NEAR(pParty->pos.y, 13224, 128);
    game.tick(5);
}

// Kills a quest monster through the real damage pipeline (hp whittled to 1, then party melee)
// so that Actor::Die - and MM6's hardcoded death behavior - runs the way real combat reaches it.
static void killQuestMonster(EngineController &game, int actorId) {
    Actor &actor = pActors[actorId];
    for (int i = 0; i < 64 && actor.aiState != Dying && actor.aiState != Dead; i++) {
        actor.hp = 1;
        Actor::DamageMonsterFromParty(Pid(OBJECT_Character, 0), actorId, Vec3f());
    }
    ASSERT_TRUE(actor.aiState == Dying || actor.aiState == Dead)
        << "aiState=" << std::to_underlying(actor.aiState) << " hp=" << actor.hp
        << " attributes=" << std::hex << std::to_underlying(actor.attributes);
    game.tick(2);
}

static int findActorByMonsterId(int monsterId) {
    for (int i = 0; i < pActors.size(); i++)
        if (std::to_underlying(pActors[i].monsterInfo.id) == monsterId && pActors[i].aiState != Removed)
            return i;
    return -1;
}

// Clears the map of every actor except the given one, so a scripted showdown can run without
// the local wildlife mauling the party mid-assert.
static void removeAllActorsExcept(int actorId) {
    for (int i = 0; i < pActors.size(); i++)
        if (i != actorId)
            pActors[i].aiState = Removed;
}

// The reactor is immune to everything except Energy (its monsters.txt row is Imm across the
// board) - MM6 blasters are the one thing that hurts it. Fires real blaster shots until it dies.
static void blastReactor(EngineController &game, int reactorId) {
    Character &gunner = pParty->pCharacters[0];
    if (InventoryEntry oldWeapon = gunner.inventory.entry(ITEM_SLOT_MAIN_HAND))
        gunner.inventory.take(oldWeapon);
    ASSERT_TRUE(gunner.inventory.equip(ITEM_SLOT_MAIN_HAND, Item(ItemId(64)))); // A Blaster.
    gunner.setSkillValue(SKILL_BLASTER, CombinedSkillValue::novice());
    Actor &reactor = pActors[reactorId];
    // Test scaffolding: the reactor answers with 20D5+20 Energy shots that would flatten a
    // level-1 party long before its 11k HP ran out - hold it still while we whittle.
    reactor.buffs[ACTOR_BUFF_PARALYZED].Apply(pParty->GetPlayingTime() + Duration::fromHours(1), MASTERY_NOVICE, 0, 0, 0);
    int lasersSeen = 0;
    for (int i = 0; i < 32 && reactor.aiState != Dying && reactor.aiState != Dead; i++) {
        reactor.hp = 1;
        gunner.timeToRecovery = 0_ticks;
        pParty->setActiveCharacterIndex(1); // The gunner shoots; heals/reloads may have unset the active slot.
        Vec3f pos = reactor.pos + Vec3f(-400, 0, 0);
        int yawDegrees = TrigLUT.atan2(reactor.pos.x - pos.x, reactor.pos.y - pos.y) * 90 / 512;
        game.teleportTo(engine->_currentLoadedMapId, pos, yawDegrees);
        game.tick(1);
        game.pressAndReleaseKey(PlatformKey::KEY_A);
        for (int j = 0; j < 4; j++) {
            game.tick(1);
            for (const SpriteObject &sprite : pSpriteObjects)
                if (sprite.uSpellID == SPELL_LASER_PROJECTILE)
                    lasersSeen++;
        }
    }
    ASSERT_TRUE(reactor.aiState == Dying || reactor.aiState == Dead)
        << "aiState=" << std::to_underlying(reactor.aiState) << " hp=" << reactor.hp
        << " gunnerHp=" << gunner.health << " canAct=" << gunner.CanAct()
        << " active=" << pParty->activeCharacterIndex() << " lasersSeen=" << lasersSeen;
    game.tick(2);
}

// MM6's Hive reactor (monsters.txt row 173 "zReactor") is a killable monster with a hardcoded
// ending in the EXE's actor-death handler (@0x4031f1): destroying it without the Ritual of the
// Void (544) anywhere in the party is the Lose ending; with it, the party survives the blast -
// quest bit 180 goes up, sixteen tier-3 monsters pour out of the walls, the escape doors flip
// (the set hive.evt event 100 re-applies on reload), the party is thrown to (3328, 25920) facing
// east, and comes to rested and healed. Once bit 180 is up the reactor stays gone on map reload.
GAME_TEST(Mm6, ReactorMeltdown) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    MapId hive = pMapStats->GetMapInfo("hive.blv");
    ASSERT_NE(hive, MAP_INVALID);
    game.teleportTo(hive, Vec3f(0, 0, 0), 0);
    ASSERT_FALSE(pIndoor->pSpawnPoints.empty());
    game.teleportTo(hive, pIndoor->pSpawnPoints[0].position, 0);
    game.tick(1);

    // Survival leg: the Ritual is aboard, a character is wounded and blessed. The rest of the
    // hive is cleared out so the showdown runs undisturbed.
    int reactorId = findActorByMonsterId(173);
    ASSERT_NE(reactorId, -1);
    removeAllActorsExcept(reactorId);
    ASSERT_TRUE(pParty->pCharacters[0].inventory.add(Item(ITEM_MM6_RITUAL_OF_THE_VOID)));
    pParty->pCharacters[0].health = 1;
    pParty->pCharacters[0].pCharacterBuffs[CHARACTER_BUFF_BLESS].Apply(
        pParty->GetPlayingTime() + Duration::fromHours(1), MASTERY_NOVICE, 5, 0, 0);

    blastReactor(game, reactorId);

    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(180)]);
    EXPECT_EQ(current_screen_type, SCREEN_GAME); // No game over - the party lives.
    EXPECT_NEAR(pParty->pos.x, 3328, 128); // Blown clear to the exit passage; physics may nudge.
    EXPECT_NEAR(pParty->pos.y, 25920, 128);
    EXPECT_EQ(pParty->_viewYaw, 512);
    int blastSpawns = 0; // The blast wave: sixteen tier-3 spawns pour out of the walls.
    for (const Actor &actor : pActors)
        if (actor.CanAct())
            blastSpawns++;
    EXPECT_GE(blastSpawns, 8);
    EXPECT_FALSE(pParty->pCharacters[0].pCharacterBuffs[CHARACTER_BUFF_BLESS].Active());
    EXPECT_EQ(pParty->pCharacters[0].health, pParty->pCharacters[0].GetMaxHealth()); // RestAndHeal.
    EXPECT_TRUE(pParty->pCharacters[0].inventory.find(ITEM_MM6_RITUAL_OF_THE_VOID)); // Not consumed.

    // Reloading the map leaves the reactor gone for good (map-load despawn on quest bit 180).
    // A same-map teleport doesn't reload - bounce through New Sorpigal for a real map load.
    game.teleportTo(pMapStats->GetMapInfo("oute3.odm"), Vec3f(-9728, -11319, 160), 0);
    game.tick(1);
    game.teleportTo(hive, Vec3f(0, 0, 0), 0);
    game.teleportTo(hive, pIndoor->pSpawnPoints[0].position, 0);
    game.tick(1);
    EXPECT_EQ(findActorByMonsterId(173), -1);

    // Lose leg: reset the quest state, drop the Ritual, and blow the reactor up again on a
    // freshly respawned map.
    pParty->_questBits[static_cast<QuestBit>(180)] = false;
    InventoryEntry ritual = pParty->pCharacters[0].inventory.find(ITEM_MM6_RITUAL_OF_THE_VOID);
    ASSERT_TRUE(ritual);
    pParty->pCharacters[0].inventory.take(ritual);
    game.teleportTo(pMapStats->GetMapInfo("oute3.odm"), Vec3f(-9728, -11319, 160), 0);
    game.tick(1);
    game.teleportTo(hive, Vec3f(0, 0, 0), 0);
    game.teleportTo(hive, pIndoor->pSpawnPoints[0].position, 0);
    game.tick(1);
    reactorId = findActorByMonsterId(173);
    ASSERT_NE(reactorId, -1);
    removeAllActorsExcept(reactorId);
    blastReactor(game, reactorId);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAMEOVER_WINDOW); // The blast consumes the world.
    ASSERT_TRUE(pGameOverWindow);
    game.pressAndReleaseButton(BUTTON_LEFT, 320, 240);
    game.tick(2);
    game.pressAndReleaseButton(BUTTON_LEFT, 320, 240);
    for (int i = 0; i < 50 && GetCurrentMenuID() != MENU_MAIN; i++)
        game.tick(1);
    EXPECT_EQ(GetCurrentMenuID(), MENU_MAIN);
}

// MM6's rest screen (MM6.EXE ctor @0x41d560): every button sits 3px right and 7-11px below MM7's,
// and the three wait buttons are 27px tall, not 33.
GAME_TEST(Mm6, RestScreenLayout) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    removeAllActorsExcept(-1); // No hostiles nearby, so resting is allowed.
    game.tick(1);

    engine->_messageQueue->addMessageCurrentFrame(UIMSG_RestWindow, 0, 0);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_REST);

    // CreateButton stores the EXE's closed-interval width/height as w+1 / h+1.
    EXPECT_EQ(pButton_RestUI_Main->rect, Recti(27, 161, 226, 38));
    EXPECT_EQ(pButton_RestUI_WaitUntilDawn->rect, Recti(64, 243, 155, 28));
    EXPECT_EQ(pButton_RestUI_Wait1Hour->rect, Recti(64, 275, 155, 28));
    EXPECT_EQ(pButton_RestUI_Wait5Minutes->rect, Recti(64, 307, 155, 28));
    EXPECT_EQ(pButton_RestUI_Exit->rect, Recti(283, 308, 155, 38));

    engine->_messageQueue->addMessageCurrentFrame(UIMSG_ExitRest, 0, 0);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
}

// Enters a castle through its real outdoor door face, answering the entry prompt if one comes up.
static void enterCastleThroughDoor(EngineController &game, int eventId) {
    const BLVFace *door = nullptr;
    for (const BSPModel &model : pOutdoor->pBModels)
        for (const BLVFace &face : model.faces)
            if (face.eventId == eventId && face.Clickable())
                door = &face;
    ASSERT_NE(door, nullptr);
    Vec3f doorCenter = door->boundingBox.center();
    Vec3f pos = doorCenter + door->facePlane.normal * 130;
    pos.z = door->boundingBox.z1;
    int yawDegrees = TrigLUT.atan2(doorCenter.x - pos.x, doorCenter.y - pos.y) * 90 / 512;
    game.teleportTo(engine->_currentLoadedMapId, pos, yawDegrees);
    game.tick(1);
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);
    if (current_screen_type == SCREEN_INPUT_BLV) {
        game.pressAndReleaseKey(PlatformKey::KEY_Y);
        game.tick(5);
    }
    ASSERT_EQ(current_screen_type, SCREEN_HOUSE);
}

// Opens a quest giver's house screen directly (the stable idiom for face-scripted houses).
static void visitQuestGiver(EngineController &game, const NPCData *npc) {
    ASSERT_NE(npc->house, HOUSE_INVALID);
    ASSERT_TRUE(enterHouse(npc->house));
    createHouseUI(npc->house);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_HOUSE);
}

// Clicks the NPC's portrait, runs their scripted topic, and escapes back to the portraits.
static void runNpcTopic(EngineController &game, NPCData *npc, DialogueId topicLine = DIALOGUE_SCRIPTED_LINE_1) {
    clickHouseNpcPortrait(game, npc);
    selectScriptedTopic(game, topicLine);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
}

static bool everyoneHasAward(int awardId) {
    return std::ranges::all_of(pParty->pCharacters, [&](const Character &character) {
        return character._achievedAwardsBits[static_cast<AwardId>(awardId)];
    });
}

// The whole MM6 main quest, driven end-to-end through the real event chains - the only elisions
// are dungeon chest/drop loot (Kilburn's Shield 499, the Hourglass 433, the Devil Plans 506, the
// Letter from Zenofex 502, the Control Cube 456), granted straight to the inventory, and the
// Prince of Thieves joining as a hireling directly (his capture topic is a plain message; the
// join is the standard hire flow). Everything else - every offer, hand-in, quest bit, topic
// rewire, door gate and ending - runs the same scripts and hardcoded EXE behavior a player hits.
GAME_TEST(Mm6, MainQuestEndToEnd) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // --- The Letter: Andover Potbello points the party at Castle Ironfist (global event 1).
    ASSERT_TRUE(pParty->pCharacters[0].inventory.find(ItemId(505)));
    int gold = pParty->GetGold();
    enterLonelyKnightTavern(game);
    NPCData *andover = &pNPCStats->pNPCData[1];
    runNpcTopic(game, andover);
    EXPECT_EQ(pParty->GetGold(), gold + 1000);
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(82)]);
    EXPECT_TRUE(pParty->pCharacters[0].inventory.find(ItemId(505))); // Andover reads it, keeps nothing.
    leaveHouse(game);

    // --- Wilbur Humphrey, Castle Ironfist: letter delivery (event 9), then Lord Kilburn's
    // Shield - offer (10, quest bit 86) and hand-in (11, award 2).
    MapId ironfist = pMapStats->GetMapInfo("outd3.odm");
    ASSERT_NE(ironfist, MAP_INVALID);
    game.teleportTo(ironfist, Vec3f(0, 0, 512), 0);
    game.tick(1);
    enterCastleThroughDoor(game, 43);
    NPCData *humphrey = &pNPCStats->pNPCData[4];
    ASSERT_EQ(humphrey->name, "Wilbur Humphrey");
    ASSERT_EQ(humphrey->dialogue_1_evt_id, 9u);
    gold = pParty->GetGold();
    runNpcTopic(game, humphrey);
    EXPECT_EQ(pParty->GetGold(), gold + 5000);
    EXPECT_FALSE(pParty->pCharacters[0].inventory.find(ItemId(505))); // The Letter reaches its addressee.
    EXPECT_TRUE(everyoneHasAward(58));
    EXPECT_EQ(humphrey->dialogue_1_evt_id, 10u);
    runNpcTopic(game, humphrey); // The shield offer.
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(86)]);
    EXPECT_EQ(humphrey->dialogue_1_evt_id, 11u);
    ASSERT_TRUE(pParty->pCharacters[0].inventory.add(Item(ItemId(499)))); // Dungeon loot, elided.
    runNpcTopic(game, humphrey);
    EXPECT_TRUE(everyoneHasAward(2));
    EXPECT_FALSE(pParty->pCharacters[0].inventory.find(ItemId(499)));
    leaveHouse(game);

    // --- Albert Newton: the Hourglass of Time (offer 51, hand-in 52 with a fake-item branch -
    // Gharik's laboratory key 487 is mistaken loot the script calls out and refuses).
    NPCData *newton = &pNPCStats->pNPCData[5];
    ASSERT_EQ(newton->name, "Albert Newton");
    visitQuestGiver(game, newton);
    runNpcTopic(game, newton);
    EXPECT_EQ(newton->dialogue_1_evt_id, 52u);
    ASSERT_TRUE(pParty->pCharacters[0].inventory.add(Item(ItemId(487))));
    runNpcTopic(game, newton);
    EXPECT_FALSE(everyoneHasAward(3)); // The wrong item entirely - the script refuses it.
    InventoryEntry fake = pParty->pCharacters[0].inventory.find(ItemId(487));
    ASSERT_TRUE(fake);
    pParty->pCharacters[0].inventory.take(fake);
    ASSERT_TRUE(pParty->pCharacters[0].inventory.add(Item(ItemId(433))));
    runNpcTopic(game, newton);
    EXPECT_TRUE(everyoneHasAward(3));
    EXPECT_FALSE(pParty->pCharacters[0].inventory.find(ItemId(433)));
    leaveHouse(game);

    // --- Osric Temper: the Devil Plans (offer 61, hand-in 62, award 4).
    NPCData *temper = &pNPCStats->pNPCData[6];
    ASSERT_EQ(temper->name, "Osric Temper");
    visitQuestGiver(game, temper);
    runNpcTopic(game, temper);
    ASSERT_TRUE(pParty->pCharacters[0].inventory.add(Item(ItemId(506)))); // Dungeon loot, elided.
    runNpcTopic(game, temper);
    EXPECT_TRUE(everyoneHasAward(4));
    leaveHouse(game);

    // --- Anthony Stone: bring him the Prince of Thieves (offer 32, hand-in 33 checks the
    // hireling roster and takes the Prince off it).
    NPCData *stone = &pNPCStats->pNPCData[16];
    ASSERT_EQ(stone->name, "Anthony Stone");
    NPCData *prince = &pNPCStats->pNPCData[17];
    ASSERT_EQ(prince->name, "The Prince of Thieves");
    visitQuestGiver(game, stone);
    runNpcTopic(game, stone);
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(98)]);
    runNpcTopic(game, stone); // Empty-handed: a hint about Free Haven, nothing changes.
    EXPECT_FALSE(everyoneHasAward(5));
    prince->flags |= NPC_HIRED; // The capture itself is the standard hire flow, elided.
    pParty->CountHirelings();
    ASSERT_TRUE(prince->Hired());
    runNpcTopic(game, stone);
    EXPECT_TRUE(everyoneHasAward(5));
    EXPECT_FALSE(prince->Hired()); // "The package" is delivered.
    leaveHouse(game);

    // --- Loretta Fleise: Price Fixing (offer 79 sets quest bit 116; all nine coach companies
    // must sign up - MM6.EXE grows a "Price Fixing" option in every stable's menu, each sets
    // quest bit houseId+99, the ninth arms bit 117; hand-in 80 pays by local reputation).
    ASSERT_TRUE(enterHouse(HouseId(48)));
    createHouseUI(HouseId(48));
    game.tick(2);
    openProprietorDialogue(game);
    EXPECT_EQ(findProprietorOption(DIALOGUE_TRANSPORT_MM6_PRICE_FIXING), nullptr); // Not before the quest.
    leaveHouse(game);
    NPCData *loretta = &pNPCStats->pNPCData[14];
    ASSERT_EQ(loretta->name, "Loretta Fleise");
    visitQuestGiver(game, loretta);
    runNpcTopic(game, loretta);
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(116)]);
    leaveHouse(game);
    for (int house = 48; house <= 56; house++) {
        EXPECT_FALSE(pParty->_questBits[static_cast<QuestBit>(117)]);
        ASSERT_TRUE(enterHouse(HouseId(house)));
        createHouseUI(HouseId(house));
        game.tick(2);
        openProprietorDialogue(game);
        ASSERT_NE(findProprietorOption(DIALOGUE_TRANSPORT_MM6_PRICE_FIXING), nullptr);
        clickProprietorOption(game, DIALOGUE_TRANSPORT_MM6_PRICE_FIXING);
        EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(house + 99)]);
        EXPECT_EQ(findProprietorOption(DIALOGUE_TRANSPORT_MM6_PRICE_FIXING), nullptr); // Already signed up.
        leaveHouse(game);
    }
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(117)]);
    visitQuestGiver(game, loretta);
    gold = pParty->GetGold();
    runNpcTopic(game, loretta);
    EXPECT_TRUE(everyoneHasAward(6));
    EXPECT_EQ(pParty->GetGold(), gold + 25000); // Local reputation below 31 pays the full purse.
    leaveHouse(game);

    // --- Erik Von Stromgard: end the eternal winter (offer 87 arms the Hermit on the Mountain,
    // whose topic 96 breaks the weather and arms the hand-in 89).
    NPCData *stromgard = &pNPCStats->pNPCData[15];
    ASSERT_EQ(stromgard->name, "Erik Von Stromgard");
    NPCData *hermit = &pNPCStats->pNPCData[19];
    visitQuestGiver(game, stromgard);
    runNpcTopic(game, stromgard);
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(120)]);
    EXPECT_EQ(hermit->dialogue_1_evt_id, 96u);
    leaveHouse(game);
    visitQuestGiver(game, hermit);
    runNpcTopic(game, hermit);
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(228)]);
    EXPECT_EQ(stromgard->dialogue_1_evt_id, 89u);
    leaveHouse(game);
    visitQuestGiver(game, stromgard);
    runNpcTopic(game, stromgard);
    EXPECT_TRUE(everyoneHasAward(7));
    leaveHouse(game);

    // All six council quests are in - but the Oracle door stays shut until the traitor falls:
    // every hand-in's quest-bit-167 chain checks award 32 first.
    EXPECT_FALSE(pParty->_questBits[static_cast<QuestBit>(167)]);

    // --- Slicker Silvertongue's conviction (event 380): the Letter from Zenofex convicts him,
    // and with awards 2-7 all present ITS award chain finally raises quest bit 167.
    MapId freeHaven = pMapStats->GetMapInfo("outc2.odm");
    ASSERT_NE(freeHaven, MAP_INVALID);
    game.teleportTo(freeHaven, Vec3f(0, 0, 512), 0);
    game.tick(1);
    enterHouseThroughDoor(game, 49);
    NPCData *slicker = &pNPCStats->pNPCData[304];
    ASSERT_EQ(slicker->name, "Slicker Silvertongue");
    ASSERT_TRUE(pParty->pCharacters[0].inventory.add(Item(ItemId(502)))); // Dungeon drop, elided.
    runNpcTopic(game, slicker);
    EXPECT_TRUE(everyoneHasAward(32));
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(168)]);
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(167)]); // The Oracle door arms - for real.
    leaveHouse(game);

    // --- Mid-quest save/load roundtrip: the whole quest state survives.
    Blob save = game.saveGame();
    game.loadGame(save);
    game.tick(1);
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(167)]);
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(117)]);
    for (int award : {2, 3, 4, 5, 6, 7, 32, 58})
        EXPECT_TRUE(everyoneHasAward(award));

    // --- The Oracle, reached through the council's newly-armed exit door. Topic 73 starts the
    // Memory Crystal hunt.
    enterHouseThroughDoor(game, 49);
    const HouseNpcDesc *oracleDoor = nullptr;
    for (const HouseNpcDesc &npc : houseNpcs)
        if (npc.type == HOUSE_TRANSITION)
            oracleDoor = &npc;
    ASSERT_NE(oracleDoor, nullptr);
    ASSERT_NE(oracleDoor->button, nullptr);
    Recti doorRect = oracleDoor->button->rect;
    game.pressAndReleaseButton(BUTTON_LEFT, doorRect.x + doorRect.w / 2, doorRect.y + doorRect.h / 2);
    game.tick(2);
    game.pressAndReleaseKey(PlatformKey::KEY_Y);
    game.tick(10);
    ASSERT_EQ(engine->_currentLoadedMapId, pMapStats->GetMapInfo("oracle.blv"));
    NPCData *oracle = &pNPCStats->pNPCData[8];
    ASSERT_EQ(oracle->dialogue_1_evt_id, 73u);
    ASSERT_TRUE(enterHouse(HouseId(170)));
    createHouseUI(HouseId(170));
    game.tick(2);
    runNpcTopic(game, oracle);
    for (int bit : {162, 163, 164, 165})
        EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(bit)]);
    leaveHouse(game);

    // --- The four Memory Crystals come from their real granting events: the Superior Temple
    // of Baa and the castles Alamos, Darkmoor and Kriegspire.
    struct CrystalSource {
        const char *map;
        int eventId;
        int itemId;
    };
    for (const CrystalSource &source : {CrystalSource{"t6.blv", 32, 550}, CrystalSource{"cd1.blv", 59, 551},
                                        CrystalSource{"cd2.blv", 57, 552}, CrystalSource{"cd3.blv", 62, 553}}) {
        MapId map = pMapStats->GetMapInfo(source.map);
        ASSERT_NE(map, MAP_INVALID);
        game.teleportTo(map, Vec3f(0, 0, 0), 0);
        ASSERT_FALSE(pIndoor->pSpawnPoints.empty());
        game.teleportTo(map, pIndoor->pSpawnPoints[0].position, 0);
        game.tick(1);
        eventProcessor(source.eventId, Pid(), 1);
        EXPECT_EQ(stashPickedItem(), ItemId(source.itemId));
    }

    // --- Back through the council door; power up the Oracle (event 14 - the pedestals gate on
    // MapVar6) and place all four crystals. The fourth placement rewires the Oracle's topics to
    // the Control Cube stage 76 - for real, through oracle.evt's own chain.
    game.teleportTo(freeHaven, Vec3f(0, 0, 512), 0);
    game.tick(1);
    enterHouseThroughDoor(game, 49);
    oracleDoor = nullptr;
    for (const HouseNpcDesc &npc : houseNpcs)
        if (npc.type == HOUSE_TRANSITION)
            oracleDoor = &npc;
    ASSERT_NE(oracleDoor, nullptr);
    doorRect = oracleDoor->button->rect;
    game.pressAndReleaseButton(BUTTON_LEFT, doorRect.x + doorRect.w / 2, doorRect.y + doorRect.h / 2);
    game.tick(2);
    game.pressAndReleaseKey(PlatformKey::KEY_Y);
    game.tick(10);
    ASSERT_EQ(engine->_currentLoadedMapId, pMapStats->GetMapInfo("oracle.blv"));
    eventProcessor(14, Pid(), 1); // The power switch.
    for (int pedestal : {5, 15, 16, 17})
        eventProcessor(pedestal, Pid(), 1);
    for (int item : {550, 551, 552, 553})
        EXPECT_FALSE(pParty->pCharacters[0].inventory.find(ItemId(item)));
    for (int bit : {100, 101, 102, 103})
        EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(bit)]);
    EXPECT_EQ(oracle->dialogue_1_evt_id, 76u);
    EXPECT_EQ(oracle->dialogue_2_evt_id, 0u);

    // --- The Control Cube: stage 1 sends the party after it (award 33), the hand-in pays
    // 500k exp and retires the topics to 77/78; topic 77 opens the Control Center door and
    // arms Nicolai's Tanir's-Bell chain.
    ASSERT_TRUE(enterHouse(HouseId(170)));
    createHouseUI(HouseId(170));
    game.tick(2);
    runNpcTopic(game, oracle);
    EXPECT_TRUE(everyoneHasAward(33));
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(166)]);
    ASSERT_TRUE(pParty->pCharacters[0].inventory.add(Item(ItemId(456)))); // Dungeon loot, elided.
    uint64_t expBefore = pParty->pCharacters[0].experience;
    runNpcTopic(game, oracle);
    EXPECT_EQ(pParty->pCharacters[0].experience, expBefore + 500000);
    EXPECT_TRUE(everyoneHasAward(34));
    EXPECT_FALSE(pParty->pCharacters[0].inventory.find(ItemId(456)));
    ASSERT_EQ(oracle->dialogue_1_evt_id, 77u);
    NPCData *nicolai = &pNPCStats->pNPCData[13];
    ASSERT_EQ(nicolai->name, "Nicolai Ironfist");
    runNpcTopic(game, oracle); // Topic 77.
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(169)]); // The Control Center door arms.
    EXPECT_EQ(nicolai->dialogue_2_evt_id, 26u);
    leaveHouse(game);

    // --- Nicolai trades Tanir's Bell for The Third Eye, which sits behind its own real map
    // event on outd3 - gated on quest bit 96, which only Nicolai's topic 26 raises.
    visitQuestGiver(game, nicolai);
    runNpcTopic(game, nicolai, DIALOGUE_SCRIPTED_LINE_2);
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(96)]);
    EXPECT_EQ(nicolai->dialogue_2_evt_id, 27u);
    leaveHouse(game);
    game.teleportTo(ironfist, Vec3f(0, 0, 512), 0);
    game.tick(1);
    eventProcessor(231, Pid(), 1);
    EXPECT_EQ(stashPickedItem(), ItemId(446)); // The Third Eye.
    visitQuestGiver(game, nicolai);
    clickHouseNpcPortrait(game, nicolai);
    selectScriptedTopic(game, DIALOGUE_SCRIPTED_LINE_2);
    EXPECT_FALSE(pParty->pCharacters[0].inventory.find(ItemId(446)));
    EXPECT_EQ(stashPickedItem(), ItemId(461)); // Tanir's Bell - stash before Escape parks the cursor.
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(197)]);
    leaveHouse(game);

    // --- The King's Library: with the bell aboard the door plays the "archie" one-shot and the
    // movie-end pass chains to Archibald Ironfist (house 553), whose topic 30 hands over the
    // Ritual of the Void; his MoveNPC(12, 0) then runs the clip out and the screen advances to
    // the empty stage 554, parking the granted Ritual into a pack.
    enterHouseThroughDoor(game, 42);
    ASSERT_NE(window_SpeakInHouse, nullptr);
    ASSERT_EQ(window_SpeakInHouse->houseId(), HOUSE_MM6_LIBRARY_ARCHIBALD);
    NPCData *archibald = &pNPCStats->pNPCData[12];
    clickHouseNpcPortrait(game, archibald);
    selectScriptedTopic(game, DIALOGUE_SCRIPTED_LINE_1);
    ASSERT_NE(window_SpeakInHouse, nullptr);
    EXPECT_EQ(window_SpeakInHouse->houseId(), HOUSE_MM6_LIBRARY_EMPTY);
    EXPECT_TRUE(std::ranges::any_of(pParty->pCharacters, [](const Character &character) {
        return !!character.inventory.find(ITEM_MM6_RITUAL_OF_THE_VOID);
    }));
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(177)]);
    leaveHouse(game);

    // --- The Hive. The reactor refuses the Ritual while the Demon Queen lives (quest bit 202
    // comes only from her death - the hardcoded EXE special); with her dead, the Ritual wins
    // the game, and MM6 lets the party play on afterwards.
    MapId hive = pMapStats->GetMapInfo("hive.blv");
    ASSERT_NE(hive, MAP_INVALID);
    game.teleportTo(hive, Vec3f(0, 0, 0), 0);
    ASSERT_FALSE(pIndoor->pSpawnPoints.empty());
    game.teleportTo(hive, pIndoor->pSpawnPoints[0].position, 0);
    game.tick(1);
    eventProcessor(60, Pid(), 1); // "The Queen's psychic energies protect the reactor."
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
    EXPECT_FALSE(pParty->_questBits[static_cast<QuestBit>(237)]);
    int queenId = findActorByMonsterId(172);
    ASSERT_NE(queenId, -1);
    removeAllActorsExcept(queenId);
    killQuestMonster(game, queenId);
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(202)]);
    eventProcessor(60, Pid(), 1);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAMEOVER_WINDOW); // The Win certificate.
    EXPECT_EQ(uGameState, GAME_STATE_FINAL_WINDOW);
    EXPECT_TRUE(pParty->_questBits[static_cast<QuestBit>(237)]);
    EXPECT_TRUE(everyoneHasAward(36));
    EXPECT_TRUE(std::ranges::none_of(pParty->pCharacters, [](const Character &character) {
        return !!character.inventory.find(ITEM_MM6_RITUAL_OF_THE_VOID); // Consumed by the win.
    }));
    ASSERT_TRUE(pGameOverWindow);
    game.pressAndReleaseButton(BUTTON_LEFT, 320, 240);
    game.tick(2);
    game.pressAndReleaseButton(BUTTON_LEFT, 320, 240);
    game.tick(2);
    EXPECT_EQ(uGameState, GAME_STATE_PLAYING); // MM6 plays on after the ending.
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
    EXPECT_EQ(engine->_currentLoadedMapId, hive);
    game.tick(5);
}

// Milestone 58: MM6 monster projectiles come from MM6's own dobjlist bank - object id =
// 490 + 10 * the monsters.txt missile code (arrow 500, fire arrow 510, fire 520, electric 530,
// cold 540, poison 550, energy 560, magic 570, rock 580, laser 590; MM6.EXE ranged-attack
// dispatch @0x404f59) - not MM7's SpriteId values, half of which don't exist in MM6 data at all
// (arrows resolved to the missing object 545 and despawned with "Item not found").
GAME_TEST(Mm6, MonsterProjectiles) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    // monsters.txt missile columns parse with MM6.EXE's per-column keyword sets (attack1
    // @0x447afa, attack2 @0x447e34, both exact stricmp): flaming arrow is spelled "FireAr" in
    // the attack2 set only, Magic/Rock are real MM6-only projectiles, "Pois" does NOT match the
    // attack2 set's "POISON" keyword, and "Dagger" matches nothing in either set.
    EXPECT_EQ(pMonsterStats->infos[MonsterId(2)].attack1MissileType, MONSTER_PROJECTILE_ARROW);          // Master Archer.
    EXPECT_EQ(pMonsterStats->infos[MonsterId(2)].attack2MissileType, MONSTER_PROJECTILE_FLAMING_ARROW);  // "FireAr".
    EXPECT_EQ(pMonsterStats->infos[MonsterId(45)].attack1MissileType, MONSTER_PROJECTILE_MM6_MAGIC);     // Grand Druid.
    EXPECT_EQ(pMonsterStats->infos[MonsterId(52)].attack2MissileType, MONSTER_PROJECTILE_MM6_ROCK);      // Rock Beast.
    EXPECT_EQ(pMonsterStats->infos[MonsterId(148)].attack1MissileType, MONSTER_PROJECTILE_ENERGY_BOLT);  // Patrol Unit.
    EXPECT_EQ(pMonsterStats->infos[MonsterId(37)].attack1MissileType, MONSTER_PROJECTILE_EARTH_BOLT);    // Land Dragon, "Pois" attack1.
    EXPECT_EQ(pMonsterStats->infos[MonsterId(86)].attack2MissileType, MONSTER_PROJECTILE_NONE);          // Venomous Hydra, "Pois" attack2.
    EXPECT_EQ(pMonsterStats->infos[MonsterId(163)].attack2MissileType, MONSTER_PROJECTILE_NONE);         // Thief, "Dagger".

    // The MM6-only bank members count as monster projectiles for the hit-or-miss / shield handling.
    EXPECT_TRUE(isMonsterProjectileSprite(SPRITE_MM6_PROJECTILE_ENERGY));
    EXPECT_TRUE(isMonsterProjectileSprite(SPRITE_MM6_PROJECTILE_MAGIC));
    EXPECT_TRUE(isMonsterProjectileSprite(SPRITE_MM6_PROJECTILE_ROCK));
    EXPECT_TRUE(isMonsterProjectileSprite(SPRITE_MM6_PROJECTILE_LASER));

    // Actor::AI_RangedAttack resolves every missile type onto a live dobjlist object.
    ASSERT_FALSE(pActors.empty()); // New Sorpigal street peasants.
    auto countProjectiles = [](SpriteId sprite) {
        int result = 0;
        for (const SpriteObject &object : pSpriteObjects)
            if (object.spriteId == sprite && object.uObjectDescID != 0)
                result++;
        return result;
    };
    auto shoot = [](MonsterProjectile type) {
        AIDirection dir;
        dir.uDistance = 1000;
        Actor::AI_RangedAttack(0, &dir, type, ABILITY_ATTACK1);
    };
    std::initializer_list<std::pair<MonsterProjectile, SpriteId>> bank = {
        {MONSTER_PROJECTILE_ARROW, SPRITE_MM6_PROJECTILE_ARROW},
        {MONSTER_PROJECTILE_FLAMING_ARROW, SPRITE_MM6_PROJECTILE_FIRE_ARROW},
        {MONSTER_PROJECTILE_FIRE_BOLT, SPRITE_MM6_PROJECTILE_FIRE},
        {MONSTER_PROJECTILE_AIR_BOLT, SPRITE_MM6_PROJECTILE_ELECTRIC},
        {MONSTER_PROJECTILE_WATER_BOLT, SPRITE_MM6_PROJECTILE_COLD},
        {MONSTER_PROJECTILE_EARTH_BOLT, SPRITE_MM6_PROJECTILE_POISON},
        {MONSTER_PROJECTILE_ENERGY_BOLT, SPRITE_MM6_PROJECTILE_ENERGY},
        {MONSTER_PROJECTILE_MM6_MAGIC, SPRITE_MM6_PROJECTILE_MAGIC},
        {MONSTER_PROJECTILE_MM6_ROCK, SPRITE_MM6_PROJECTILE_ROCK},
    };
    for (const auto &[missile, sprite] : bank) {
        int before = countProjectiles(sprite);
        shoot(missile);
        EXPECT_EQ(countProjectiles(sprite), before + 1) << "missile type " << std::to_underlying(missile);
    }

    // The VARN robots' energy shots are laser bolts (MM6.EXE @0x404f84).
    MonsterId monsterBefore = pActors[0].monsterInfo.id;
    pActors[0].monsterInfo.id = MonsterId(150); // Terminator Unit.
    int lasersBefore = countProjectiles(SPRITE_MM6_PROJECTILE_LASER);
    shoot(MONSTER_PROJECTILE_ENERGY_BOLT);
    EXPECT_EQ(countProjectiles(SPRITE_MM6_PROJECTILE_LASER), lasersBefore + 1);
    pActors[0].monsterInfo.id = monsterBefore;

    // Impact: a bolt that hits the party turns into its "explosion" impact object (id + 1). The
    // poison bolt matters most - its value collides with MM7's flaming arrow, whose impact
    // treatment just despawns without an impact animation.
    auto impactTurnsInto = [&](SpriteId sprite) {
        int index = -1;
        for (size_t i = 0; i < pSpriteObjects.size(); i++)
            if (pSpriteObjects[i].spriteId == sprite && pSpriteObjects[i].uObjectDescID != 0)
                index = i;
        EXPECT_NE(index, -1);
        if (index == -1)
            return SPRITE_NULL;
        processSpellImpact(index, Pid(OBJECT_Character, 0));
        return pSpriteObjects[index].uObjectDescID != 0 ? pSpriteObjects[index].spriteId : SPRITE_NULL;
    };
    EXPECT_EQ(impactTurnsInto(SPRITE_MM6_PROJECTILE_ENERGY), impactSprite(SPRITE_MM6_PROJECTILE_ENERGY));
    EXPECT_EQ(impactTurnsInto(SPRITE_MM6_PROJECTILE_POISON), impactSprite(SPRITE_MM6_PROJECTILE_POISON));
}

// MM6's projectile art ships with missing octant frames - sprites.lod has only arra0 and arra4
// for the arrow frameset, so loadSpriteFrame (Sprites.cpp) leaves SpriteFrame::sprites null for
// the side-on octants. The draw paths must skip those octants instead of dereferencing them.
GAME_TEST(Mm6, ProjectileMissingOctantsDraw) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    ASSERT_FALSE(pActors.empty()); // New Sorpigal street peasants.
    AIDirection dir;
    dir.uDistance = 1000;
    Actor::AI_RangedAttack(0, &dir, MONSTER_PROJECTILE_ARROW, ABILITY_ATTACK1);

    int index = -1;
    for (size_t i = 0; i < pSpriteObjects.size(); i++)
        if (pSpriteObjects[i].spriteId == SPRITE_MM6_PROJECTILE_ARROW && pSpriteObjects[i].uObjectDescID != 0)
            index = i;
    ASSERT_NE(index, -1);

    // The hazard only exists because the arrow frameset really is missing octants.
    SpriteFrame *frame = pSpriteObjects[index].getSpriteFrame();
    EXPECT_TRUE(std::ranges::any_of(frame->sprites, [](const Sprite *sprite) { return sprite == nullptr; }));

    // Park the arrow mid-air in front of the camera and sweep its facing through all 8 octants -
    // the side-on ones resolve to the missing frames while the draw list is being built.
    for (int i = 0; i < 8; i++) {
        SpriteObject &arrow = pSpriteObjects[index];
        ASSERT_NE(arrow.uObjectDescID, 0);
        arrow.vPosition = pParty->pos + Vec3f::fromPolar(300, pParty->_viewYaw, 0) + Vec3f(0, 0, 96);
        arrow.vVelocity = Vec3f(0, 0, 0);
        arrow.uFacing = i * 256;
        game.tick(1);
    }
}

// MM6's reputation is a single GLOBAL party value (MM6.EXE party+0xD8 @0x908D48, positive = good),
// while the engine stores a per-map value in LocationInfo::reputation. The current map's slot stays
// the working copy every consumer reads and writes (and the save format carries), and DoPrepareWorld
// carries it across map switches through a party-side mirror - so reputation earned in one region
// now follows the party everywhere, including through a save/load.
GAME_TEST(Mm6, GlobalReputation) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);
    EXPECT_EQ(currentLocationInfo().reputation, 0);

    // Reputation earned in New Sorpigal is still there in Ironfist...
    currentLocationInfo().reputation = 700;
    MapId ironfist = pMapStats->GetMapInfo("outd3.odm");
    ASSERT_NE(ironfist, MAP_INVALID);
    game.teleportTo(ironfist, Vec3f(0, 0, 512), 0);
    game.tick(1);
    EXPECT_EQ(currentLocationInfo().reputation, 700);

    // ...and mutations made there follow the party back, even into an indoor map.
    currentLocationInfo().reputation = -300;
    MapId goblinwatch = pMapStats->GetMapInfo("d01.blv");
    ASSERT_NE(goblinwatch, MAP_INVALID);
    game.teleportTo(goblinwatch, Vec3f(0, 0, 0), 0);
    game.teleportTo(goblinwatch, pIndoor->pSpawnPoints[0].position, 0);
    game.tick(1);
    EXPECT_EQ(currentLocationInfo().reputation, -300);

    // A save carries the value (via the current map's delta), and a load restores it as the
    // global - after loading, it still follows the party to other maps.
    currentLocationInfo().reputation = 450;
    Blob save = game.saveGame();
    currentLocationInfo().reputation = 0;
    game.loadGame(save);
    game.tick(1);
    EXPECT_EQ(currentLocationInfo().reputation, 450);
    MapId newSorpigal = pMapStats->GetMapInfo("oute3.odm");
    ASSERT_NE(newSorpigal, MAP_INVALID);
    game.teleportTo(newSorpigal, Vec3f(-9728, -11319, 160), 0);
    game.tick(1);
    EXPECT_EQ(currentLocationInfo().reputation, 450);
}

// The MM6 reputation model proper, reversed from MM6.EXE via an xref sweep over the global
// (@0x908D48) and its this-relative getter (0x47D600):
// - Titles (0x489C60): eleven bands over MM6 global.txt rows 510-520, Saintly at +1000 down to
//   Notorious at -1000, steps of 200 both ways.
// - Display value (0x47D600): global +200 for a hired Bard, -200 each for Pirate/Gypsy/Duper/
//   Burglar ("Reputation is decreased by one full category" in npcprof.txt; a category = 200).
// - Killing a true peasant (monsters.txt hostility 0) costs 100 reputation, with NO fine - MM6
//   has no fine mechanic - and arena fights are exempt (0x403086, flag 0x908DBD).
// - Turning a true peasant hostile costs 50 reputation once per actor (0x403778).
// - Temple donation (0x49DDC6): +200 up to a cap of +200; weekday-matched blessing tiers for
//   every display band above 200; Temple Baa (house 78) then takes the 200 back (0x49DF3E).
// - Once per game day, crossing 3 AM, reputation decays toward zero: rep = trunc(rep * 0.99),
//   clamped to +-1500 (0x4881E4, multiplier double @0x4B9550).
// - MM6 merchant math (0x485340) has no reputation term - prices must not move with reputation.
GAME_TEST(Mm6, ReputationModel) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);
    LocationInfo &location = currentLocationInfo();

    // Titles: MM6 global.txt rows 510-520, positive = good.
    auto titleAt = [&](int reputation) {
        location.reputation = reputation;
        return GetReputationString(pParty->GetPartyReputation());
    };
    EXPECT_EQ(titleAt(1000), localization->str(static_cast<LstrId>(510)));  // Saintly
    EXPECT_EQ(titleAt(200), localization->str(static_cast<LstrId>(514)));   // Respectable
    EXPECT_EQ(titleAt(0), localization->str(static_cast<LstrId>(515)));     // Average
    EXPECT_EQ(titleAt(-1), localization->str(static_cast<LstrId>(516)));    // Bad
    EXPECT_EQ(titleAt(-300), localization->str(static_cast<LstrId>(517)));  // Vile
    EXPECT_EQ(titleAt(-1000), localization->str(static_cast<LstrId>(520))); // Notorious

    // Hireling display adjustment: Bard +200, Pirate -200. The raw global is untouched.
    location.reputation = 100;
    NPCData hirelingBefore = pParty->pHirelings[0];
    pParty->pHirelings[0].profession = Bard;
    EXPECT_EQ(pParty->GetPartyReputation(), 300);
    pParty->pHirelings[0].profession = Pirate;
    EXPECT_EQ(pParty->GetPartyReputation(), -100);
    pParty->pHirelings[0] = hirelingBefore;
    EXPECT_EQ(location.reputation, 100);

    // Merchant pricing must not move with reputation (the MM7 formula folds -rep in).
    location.reputation = 1000;
    int merchantAtSaintly = PriceCalculator::playerMerchant(&pParty->pCharacters[0]);
    location.reputation = -1000;
    EXPECT_EQ(PriceCalculator::playerMerchant(&pParty->pCharacters[0]), merchantAtSaintly);

    // Peasant kill: -100, no fine, no fine award; exempt during an arena fight. Monster 123 is a
    // true peasant row (hostility 0).
    auto peasant = std::ranges::find_if(pActors, [](const Actor &actor) {
        return pMonsterStats->infos[actor.monsterId].hostilityType == HOSTILITY_FRIENDLY;
    });
    ASSERT_NE(peasant, pActors.end());
    int peasantId = peasant - pActors.begin();
    location.reputation = 0;
    Actor::ApplyFineForKillingPeasant(peasantId);
    EXPECT_EQ(location.reputation, -100);
    EXPECT_EQ(pParty->uFine, 0);
    for (const Character &character : pParty->pCharacters)
        EXPECT_FALSE(character._achievedAwardsBits[AWARD_FINE]);
    pParty->arenaState = ARENA_STATE_FIGHTING;
    Actor::ApplyFineForKillingPeasant(peasantId);
    EXPECT_EQ(location.reputation, -100);
    pParty->arenaState = ARENA_STATE_INITIAL;

    // Aggroing a true peasant: -50, once per actor.
    auto otherPeasant = std::ranges::find_if(pActors, [&](const Actor &actor) {
        return pMonsterStats->infos[actor.monsterId].hostilityType == HOSTILITY_FRIENDLY &&
               &actor != &*peasant && !(actor.attributes & ACTOR_AGGRESSOR);
    });
    ASSERT_NE(otherPeasant, pActors.end());
    location.reputation = 0;
    Actor::AggroSurroundingPeasants(otherPeasant - pActors.begin(), 1);
    EXPECT_EQ(location.reputation, -50);
    Actor::AggroSurroundingPeasants(otherPeasant - pActors.begin(), 1);
    EXPECT_EQ(location.reputation, -50);

    // Daily 3 AM decay: trunc(rep * 0.99), clamped to +-1500, applied once per crossing.
    location.reputation = 1000;
    restAndHeal(Duration::fromDays(1));
    EXPECT_EQ(location.reputation, 990);
    location.reputation = 2000;
    restAndHeal(Duration::fromDays(1));
    EXPECT_EQ(location.reputation, 1500);
    location.reputation = -50;
    restAndHeal(Duration::fromDays(1));
    EXPECT_EQ(location.reputation, -49);

    // Temple donation at a regular temple: +200 while below the +200 cap, then no further growth.
    pParty->SetGold(20000);
    pParty->setActiveCharacterIndex(1);
    location.reputation = 0;
    ASSERT_TRUE(enterHouse(HouseId(70))); // Temple Stone.
    createHouseUI(HouseId(70));
    openProprietorDialogue(game);
    clickProprietorOption(game, DIALOGUE_TEMPLE_DONATE);
    EXPECT_EQ(location.reputation, 200);
    openProprietorDialogue(game);
    clickProprietorOption(game, DIALOGUE_TEMPLE_DONATE);
    EXPECT_EQ(location.reputation, 200);

    // The weekday-matched blessing: with display reputation above 1000 (990 + Bard's 200) every
    // tier fires on the donation whose counter matches the day of the month mod 7 - among them
    // Wizard Eye, the Day of the Gods stat buffs, and Guardian Angel.
    location.reputation = 990;
    pParty->pHirelings[0].profession = Bard;
    for (int i = 0; i < 7 && !pParty->pPartyBuffs[PARTY_BUFF_WIZARD_EYE].Active(); i++) {
        openProprietorDialogue(game);
        clickProprietorOption(game, DIALOGUE_TEMPLE_DONATE);
        game.tick(2);
    }
    EXPECT_TRUE(pParty->pPartyBuffs[PARTY_BUFF_WIZARD_EYE].Active());
    EXPECT_TRUE(pParty->pCharacters[0].pCharacterBuffs[CHARACTER_BUFF_STRENGTH].Active());
    EXPECT_GT(pParty->_mm6GuardianAngelExpireTime, pParty->GetPlayingTime());
    pParty->pHirelings[0] = hirelingBefore;
    leaveHouse(game);

    // Temple Baa (house 78) takes the 200 right back: a donation there nets nothing while below
    // the cap, and bleeds an already-good reputation down by 200 per donation.
    location.reputation = 0;
    ASSERT_TRUE(enterHouse(HouseId(78)));
    createHouseUI(HouseId(78));
    openProprietorDialogue(game);
    clickProprietorOption(game, DIALOGUE_TEMPLE_DONATE);
    EXPECT_EQ(location.reputation, 0);
    location.reputation = 300;
    openProprietorDialogue(game);
    clickProprietorOption(game, DIALOGUE_TEMPLE_DONATE);
    EXPECT_EQ(location.reputation, 100);
    leaveHouse(game);
}

// Hireling in-party benefits, MM6-audited against MM6.EXE (HasNPCProfession 0x467F30 caller
// sweep): the benefit set and numbers below are the ones the shipped engine actually applies,
// which differ from MM7's implementations in every case asserted here.
GAME_TEST(Mm6, HirelingBenefits) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    Character &roderick = pParty->pCharacters[0];
    NPCData hirelingsBefore0 = pParty->pHirelings[0];
    NPCData hirelingsBefore1 = pParty->pHirelings[1];
    auto hire = [](NpcProfession first, NpcProfession second = NoProfession) {
        pParty->pHirelings[0].profession = first;
        pParty->pHirelings[1].profession = second;
    };

    // Arms Master +2 / Weapons Master +3 boost every weapon skill (they stack), Squire adds +2
    // to weapon AND armor skills; none of them conjure a phantom MM7 Armsmaster skill.
    hire(NoProfession);
    int sword = roderick.actualSkillLevel(SKILL_SWORD);
    int bow = roderick.actualSkillLevel(SKILL_BOW);
    int plate = roderick.actualSkillLevel(SKILL_PLATE);
    int shield = roderick.actualSkillLevel(SKILL_SHIELD);
    hire(Armsmaster, Weaponsmaster);
    EXPECT_EQ(roderick.actualSkillLevel(SKILL_SWORD), sword + 5);
    EXPECT_EQ(roderick.actualSkillLevel(SKILL_BOW), bow + 5);
    EXPECT_EQ(roderick.actualSkillLevel(SKILL_PLATE), plate);
    EXPECT_EQ(roderick.actualSkillLevel(SKILL_ARMSMASTER), 0);
    hire(Squire);
    EXPECT_EQ(roderick.actualSkillLevel(SKILL_SWORD), sword + 2);
    EXPECT_EQ(roderick.actualSkillLevel(SKILL_PLATE), plate + 2);
    EXPECT_EQ(roderick.actualSkillLevel(SKILL_SHIELD), shield + 2);

    // Apprentice/Mystic/Spell Master boost EVERY spell school - the self schools and Light/Dark
    // too, not just MM7's four elemental ones - and MM7's self-school professions do nothing.
    hire(NoProfession);
    int fire = roderick.actualSkillLevel(SKILL_FIRE);
    int spirit = roderick.actualSkillLevel(SKILL_SPIRIT);
    int dark = roderick.actualSkillLevel(SKILL_DARK);
    hire(Apprentice, Spellmaster);
    EXPECT_EQ(roderick.actualSkillLevel(SKILL_FIRE), fire + 6);
    EXPECT_EQ(roderick.actualSkillLevel(SKILL_SPIRIT), spirit + 6);
    EXPECT_EQ(roderick.actualSkillLevel(SKILL_DARK), dark + 6);
    hire(Prelate);
    EXPECT_EQ(roderick.actualSkillLevel(SKILL_SPIRIT), spirit);

    // Negotiator: Merchant +4 on top of its Diplomacy +4.
    hire(NoProfession);
    int merchant = roderick.actualSkillLevel(SKILL_MERCHANT);
    hire(Negotiator);
    EXPECT_EQ(roderick.actualSkillLevel(SKILL_MERCHANT), merchant + 4);

    // Scout's Perception +6 and Psychic's Perception +5 are shipped no-ops in MM6.EXE
    // (Psychic's Luck +10 is real).
    hire(NoProfession);
    int perception = roderick.actualSkillLevel(SKILL_PERCEPTION);
    int luck = roderick.GetActualLuck();
    hire(Scout, Psychic);
    EXPECT_EQ(roderick.actualSkillLevel(SKILL_PERCEPTION), perception);
    EXPECT_EQ(roderick.GetActualLuck(), luck + 10);

    // Enchanter: +20 to the four elemental resistances only - MM6's Magic resistance
    // (Mind/Spirit/Body here) gets nothing.
    hire(NoProfession);
    int fireRes = roderick.GetActualResistance(ATTRIBUTE_RESIST_FIRE);
    int mindRes = roderick.GetActualResistance(ATTRIBUTE_RESIST_MIND);
    hire(Enchanter);
    EXPECT_EQ(roderick.GetActualResistance(ATTRIBUTE_RESIST_FIRE), fireRes + 20);
    EXPECT_EQ(roderick.GetActualResistance(ATTRIBUTE_RESIST_MIND), mindRes);

    // Experience: Teacher +10 / Instructor +15 are real, the Scholar's documented +5 percent is
    // a shipped no-op (its unlimited identification is real and separate).
    hire(NoProfession);
    int learning = roderick.getLearningPercent();
    hire(Teacher, Instructor);
    EXPECT_EQ(roderick.getLearningPercent(), learning + 25);
    hire(Scholar);
    EXPECT_EQ(roderick.getLearningPercent(), learning);

    // Disarm hirelings add AFTER the Thievery doubling: novice skill 4 with a Burglar is
    // 2 x (4 + 8), and with Pendragon equipped 2 x (4 x 2 + 8).
    CombinedSkillValue disarmBefore = roderick.pActiveSkills[SKILL_TRAP_DISARM];
    roderick.pActiveSkills[SKILL_TRAP_DISARM] = CombinedSkillValue(4, MASTERY_NOVICE);
    hire(Burglar);
    EXPECT_EQ(roderick.GetDisarmTrap(), 24);
    if (InventoryEntry existing = roderick.inventory.entry(ITEM_SLOT_CLOAK))
        roderick.inventory.take(existing);
    InventoryEntry pendragon = roderick.inventory.equip(ITEM_SLOT_CLOAK, Item(ItemId(410)));
    EXPECT_EQ(roderick.GetDisarmTrap(), 32);
    roderick.inventory.take(pendragon);
    roderick.pActiveSkills[SKILL_TRAP_DISARM] = disarmBefore;

    // Acolyte & Piper: their once-a-day action is a fixed whole-party buff - Bless / Heroism at
    // power 5, Master, 2 hours - not a real spell cast.
    hire(Acolyte, Piper);
    Time expectedExpiry = pParty->GetPlayingTime() + Duration::fromHours(2);
    UseNPCSkill(Acolyte, 0);
    UseNPCSkill(Piper, 1);
    for (const Character &character : pParty->pCharacters) {
        EXPECT_TRUE(character.pCharacterBuffs[CHARACTER_BUFF_BLESS].Active());
        EXPECT_EQ(character.pCharacterBuffs[CHARACTER_BUFF_BLESS].power, 5);
        EXPECT_EQ(character.pCharacterBuffs[CHARACTER_BUFF_BLESS].skillMastery, MASTERY_MASTER);
        EXPECT_EQ(character.pCharacterBuffs[CHARACTER_BUFF_BLESS].expireTime, expectedExpiry);
        EXPECT_TRUE(character.pCharacterBuffs[CHARACTER_BUFF_HEROISM].Active());
        EXPECT_EQ(character.pCharacterBuffs[CHARACTER_BUFF_HEROISM].power, 5);
    }

    pParty->pHirelings[0] = hirelingsBefore0;
    pParty->pHirelings[1] = hirelingsBefore1;
}

// Character speech comes from MM6's own voice banks: the voice is the face id (12 banks,
// MaleA..MaleH for faces 0-7, GirlA..GirlD for 8-11), each speech event carries up to 2 sound
// variants (MM6.EXE table @0x4C22F0), and the sample id is 5000 + 100 * voice + 2 * variant +
// subvariant (MM6.EXE 0x488CA0), resolved through ordinary dsounds.bin entries ("MaleA01a"...).
GAME_TEST(Mm6, CharacterVoices) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    auto soundsTape = tapes.sounds();

    game.startNewGame();
    test.startTaping();

    // The default party: Roderick MaleA(0), Alexis GirlD(11), Serena GirlB(9), Zoltan MaleH(7).
    // Voice = face, and sex follows the face.
    std::array<int, 4> expectedVoices = {{0, 11, 9, 7}};
    std::array<Sex, 4> expectedSexes = {{SEX_MALE, SEX_FEMALE, SEX_FEMALE, SEX_MALE}};
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(pParty->pCharacters[i].uVoiceID, expectedVoices[i]);
        EXPECT_EQ(pParty->pCharacters[i].GetSexByVoice(), expectedSexes[i]);
        EXPECT_EQ(pParty->pCharacters[i].uSex, expectedSexes[i]);
    }

    // An "oww" reaction (event 24, sound variants 34/35, one subvariant each in every bank) picks
    // the sample from the character's own bank: id 5000 + 100 * voice + {68, 70}.
    for (int i = 0; i < 4; i++) {
        pParty->pCharacters[i].receiveDamage(10, DAMAGE_PHYSICAL);
        game.tick(3);
        int voice = pParty->pCharacters[i].uVoiceID;
        auto played = soundsTape.flatten();
        bool heard = std::ranges::any_of(played, [&](SoundId sound) {
            return sound == static_cast<SoundId>(5068 + 100 * voice) || sound == static_cast<SoundId>(5070 + 100 * voice);
        });
        EXPECT_TRUE(heard) << "no MM6 voice reaction for character " << i << " (voice " << voice << ")";
    }
}

// MM7.EXE remaps every loaded palette through HSV when building its palette LUTs, scaling
// saturation by 0.65 and value by 1.1 (the float pair sits in MM7-Rel.exe's .data at 0xd8868;
// GrayFace exposes it as the MM7+-only PaletteSMul/PaletteVMul patch options). MM6.EXE has no
// such remap - its renderer converts palette bytes to 16-bit as-is - so applying MM7's remap
// washed out MM6's entire 3D world (terrain, models, sky, billboards).
GAME_TEST(Mm6, PalettesDrawRaw) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    // The palette-LUT path used for billboards and 3D-world bitmaps is an identity for MM6.
    LodImage *pal = pBitmaps_LOD->loadTexture("pal002", false);
    ASSERT_NE(pal, nullptr);
    EXPECT_EQ(PaletteManager::createLoadedPalette(pal->palette).colors, pal->palette.colors);

    // And end-to-end through the bitmap loader: a loaded 3D-world texture pixel equals the raw
    // palette entry for its index (sky01 = MM6's first-visit sky).
    LodImage *sky = pBitmaps_LOD->loadTexture("sky01", false);
    ASSERT_NE(sky, nullptr);
    GraphicsImage *skyTex = assets->getBitmap("sky01");
    ASSERT_NE(skyTex, nullptr);
    EXPECT_EQ(skyTex->rgba()[0][0], sky->palette.colors[sky->image[0][0]]);
}

// MM6's wtrdr* shore tiles mark their water region with palette index 0, whose palette color is the
// teal marker (0, 252, 252). The terrain shader draws animated water wherever the shore texture is
// transparent, so index-0 pixels must load with alpha 0 - MM7's hwtrdr* hardware shore set got this
// treatment, but MM6's wtrdr* originals didn't, so every shoreline drew the opaque teal marker.
GAME_TEST(Mm6, ShoreTilesTransparentWater) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    for (const char *name : {"wtrdrn", "wtrdrne", "wtrdrnw", "wtrdrs", "wtrdrse", "wtrdrsw",
                             "wtrdre", "wtrdrw", "wtrdrxne", "wtrdrxnw", "wtrdrxse", "wtrdrxsw"}) {
        LodImage *raw = pBitmaps_LOD->loadTexture(name, false);
        ASSERT_NE(raw, nullptr) << name;
        EXPECT_EQ(raw->palette.colors[0], Color(0, 252, 252)) << name;
        int rawWater = std::ranges::count(raw->image.pixels(), 0);
        EXPECT_GT(rawWater, 0) << name;

        GraphicsImage *tex = assets->getBitmap(name);
        ASSERT_NE(tex, nullptr) << name;
        auto texPixels = tex->rgba().pixels();
        int transparent = std::ranges::count_if(texPixels, [] (Color c) { return c.a == 0; });
        EXPECT_EQ(transparent, rawWater) << name;
    }
}

// MM6's dsft.bin packs the SFT flag bits into 2 bytes (MM7 widened them to 4), and OE read them as
// 4 - shifting every following field of every record by two bytes. frameLength picked up the group
// total time, which MM6 stores only on a group's first frame, so GetFrame() saw zero-length chained
// frames and always returned the first one: every monster and street NPC glided around frozen
// mid-stride. paletteId picked up the always-zero paletteIndex (masking that MM6 recolor tiers
// share textures and differ ONLY in the frame-table palette), and glowRadius picked up the palette
// id, hanging a spurious light on every billboard.
GAME_TEST(Mm6, ActorWalkAnimationsAdvance) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    // The bat-A walk group: six chained frames of 2/16s each, 12/16s total, palette pal159.
    int batA = pSpriteFrameTable->FastFindSprite("bAwlka");
    ASSERT_GT(batA, 0);
    pSpriteFrameTable->InitializeSprite(batA);

    SpriteFrame *first = pSpriteFrameTable->GetFrame(batA, 0_ticks);
    EXPECT_EQ(first->frameLength, Duration::fromTicks(2 * 8));
    EXPECT_EQ(first->animationLength, Duration::fromTicks(12 * 8));
    EXPECT_EQ(first->paletteId, 159);
    EXPECT_EQ(first->glowRadius, 0);

    // The walk cycle actually advances through all six frames and wraps.
    SpriteFrame *prev = first;
    for (int t = 16; t < 96; t += 16) {
        SpriteFrame *cur = pSpriteFrameTable->GetFrame(batA, Duration::fromTicks(t));
        EXPECT_NE(cur, prev) << "walk cycle stuck at t=" << t;
        prev = cur;
    }
    EXPECT_EQ(pSpriteFrameTable->GetFrame(batA, Duration::fromTicks(96)), first);

    // Recolor tiers of the same monster share the bhwlk* textures and differ only in the
    // frame-table palette: bat A/B/C carry pal159/160/161.
    int batB = pSpriteFrameTable->FastFindSprite("bBwlka");
    int batC = pSpriteFrameTable->FastFindSprite("bCwlka");
    ASSERT_GT(batB, 0);
    ASSERT_GT(batC, 0);
    pSpriteFrameTable->InitializeSprite(batB);
    pSpriteFrameTable->InitializeSprite(batC);
    EXPECT_EQ(pSpriteFrameTable->GetFrame(batB, 0_ticks)->paletteId, 160);
    EXPECT_EQ(pSpriteFrameTable->GetFrame(batC, 0_ticks)->paletteId, 161);
}

// Hovering or clicking an interactive decoration indexes the 125-slot decorVars array with the
// decoration's eventVarId (UIGame.cpp / Viewport.cpp / CastSpellInfo.cpp), so every eventVarId
// must stay in bounds. MM6 decorations reconstruct through LevelDecoration_MM7 and then pick up
// the MM7 file format's -75 eventVarId shift; PrepareDecorations() only reassigns the first 124
// interactive decorations, so on a map with more of them (New Sorpigal has hundreds) the rest
// kept eventVarId == -75, and mousing over one tripped an out-of-range assert.
GAME_TEST(Mm6, DecorationEventVarIdsInBounds) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    MapId newSorpigal = pMapStats->GetMapInfo("oute3.odm");
    ASSERT_NE(newSorpigal, MAP_INVALID);
    game.teleportTo(newSorpigal, Vec3f(-9728, -11319, 160), 0);
    game.tick(1);
    ASSERT_EQ(uCurrentlyLoadedLevelType, LEVEL_OUTDOOR);

    int interactiveCount = 0;
    for (LevelDecoration &decor : pLevelDecorations) {
        if (decor.uEventID || !decor.IsInteractive())
            continue;
        interactiveCount++;
        ASSERT_GE(decor.eventVarId, 0) << "decoration desc " << std::to_underlying(decor.uDecorationDescID);
        ASSERT_LT(decor.eventVarId, (int)engine->_persistentVariables.decorVars.size())
            << "decoration desc " << std::to_underlying(decor.uDecorationDescID);
    }
    // With MM6's own interactivity table (see DecorationInteractivityTables below) the count
    // stays within PrepareDecorations()' 124 slots - the bounds asserts above are the actual
    // invariant this test pins.
    EXPECT_GT(interactiveCount, 0);
    EXPECT_LE(interactiveCount, 124);
}

// LevelDecoration::IsInteractive() / GetGlobalEvent() must use MM6's decoration tables under
// --game-version mm6, not MM7's. MM6.EXE classifies exactly 14 ddeclist.bin ids as interactive
// (the descId-0x76 xlat tables at 0x455298/0x4558c8: crystals 118-121, trash heap 146, bag 154,
// bucket 155, flour sack 158, gold bag 162, barrel 163, keg 164, skull pile 166, cook fire 167,
// cauldron 182) and seeds each decoration's decorVar from the switch at 0x455050; hovering /
// clicking then resolves npctopic row / global.evt event = seed + 400 (MM7 uses +380).
GAME_TEST(Mm6, DecorationInteractivityTables) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    MapId newSorpigal = pMapStats->GetMapInfo("oute3.odm");
    ASSERT_NE(newSorpigal, MAP_INVALID);
    game.teleportTo(newSorpigal, Vec3f(-9728, -11319, 160), 0);
    game.tick(1);
    ASSERT_EQ(uCurrentlyLoadedLevelType, LEVEL_OUTDOOR);

    constexpr std::array<int, 14> mm6InteractiveIds = {118, 119, 120, 121, 146, 154, 155, 158, 162, 163, 164, 166, 167, 182};
    auto isMm6Interactive = [&](int descId) {
        return std::find(mm6InteractiveIds.begin(), mm6InteractiveIds.end(), descId) != mm6InteractiveIds.end();
    };
    // Seed windows per desc id, from MM6.EXE 0x455050 (fired event / npctopic row = seed + 400).
    auto seedWindow = [](int descId) -> std::pair<int, int> {
        switch (descId) {
            case 118: case 119: case 120: case 121: return {47, 57}; // Crystals.
            case 146: return {36, 37}; // Trash heap.
            case 154: return {31, 34}; // Large bag.
            case 155: return {43, 46}; // Bucket.
            case 158: return {29, 30}; // Flour sack.
            case 162: return {35, 35}; // Bag of gold.
            case 163: case 164: return {10, 17}; // Barrel / keg.
            case 166: return {39, 42}; // Skull pile.
            case 167: return {25, 28}; // Cook fire.
            case 182: return {19, 23}; // Cauldron (only 19 or 23 occur; a window assert suffices).
            default: return {0, 0};
        }
    };

    int interactiveCount = 0;
    for (LevelDecoration &decor : pLevelDecorations) {
        int descId = std::to_underlying(decor.uDecorationDescID);
        EXPECT_EQ(decor.IsInteractive(), isMm6Interactive(descId)) << "descId " << descId;
        if (decor.uEventID || !decor.IsInteractive() || !isMm6Interactive(descId))
            continue;
        interactiveCount++;
        ASSERT_GE(decor.eventVarId, 0);
        ASSERT_LT(decor.eventVarId, (int)engine->_persistentVariables.decorVars.size());
        int seed = engine->_persistentVariables.decorVars[decor.eventVarId];
        auto [seedLo, seedHi] = seedWindow(descId);
        EXPECT_GE(seed, seedLo) << "descId " << descId;
        EXPECT_LE(seed, seedHi) << "descId " << descId;
        // The hover topic row for this seed is one of npctopic.txt's decoration rows (410-457).
        EXPECT_FALSE(pNPCTopics[seed + 400].pTopic.empty()) << "descId " << descId << " seed " << seed;
    }
    EXPECT_GT(interactiveCount, 0);
    EXPECT_LE(interactiveCount, 124);
}

// End-to-end barrel interaction in New Sorpigal: hovering a barrel names it via npctopic row
// seed + 400, clicking fires global.evt event seed + 400 (a permanent +1 stat point in MM6),
// and the event's ChangeEvent(410) stores the new seed relative to the same +400 base.
GAME_TEST(Mm6, BarrelHoverAndClick) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    MapId newSorpigal = pMapStats->GetMapInfo("oute3.odm");
    ASSERT_NE(newSorpigal, MAP_INVALID);
    game.teleportTo(newSorpigal, Vec3f(-9728, -11319, 160), 0);
    game.tick(1);
    ASSERT_EQ(uCurrentlyLoadedLevelType, LEVEL_OUTDOOR);

    std::vector<int> barrels;
    for (size_t i = 0; i < pLevelDecorations.size(); i++)
        if (std::to_underlying(pLevelDecorations[i].uDecorationDescID) == 163 && !pLevelDecorations[i].uEventID)
            barrels.push_back(static_cast<int>(i));
    ASSERT_FALSE(barrels.empty());

    // Stand west of a barrel facing east (+x) and sweep the crosshair column until the pick
    // lands on it. Some barrels sit against geometry, so try candidates until one hovers.
    int picked = -1;
    int pickX = 238, pickY = -1;
    for (int id : barrels) {
        Vec3f pos = pLevelDecorations[id].vPosition;
        game.teleportTo(newSorpigal, pos - Vec3f(200, 0, 0), 0);
        game.tick(2);
        for (int y = 120; y <= 320; y += 8) {
            game.moveMouse(pickX, y);
            game.tick(1);
            if (mouse->uPointingObjectID.type() == OBJECT_Decoration && (int)mouse->uPointingObjectID.id() == id) {
                picked = id;
                pickY = y;
                break;
            }
        }
        if (picked != -1)
            break;
    }
    ASSERT_NE(picked, -1) << "no barrel in New Sorpigal could be hovered";

    LevelDecoration &barrel = pLevelDecorations[picked];
    ASSERT_GE(barrel.eventVarId, 0);
    ASSERT_LT(barrel.eventVarId, (int)engine->_persistentVariables.decorVars.size());

    // Force the red-liquid barrel seed (event/topic 411) for a deterministic click result.
    engine->_persistentVariables.decorVars[barrel.eventVarId] = 11;
    game.tick(1);
    EXPECT_EQ(engine->_statusBar->get(), "Barrel of Red liquid");

    int mightBefore = 0;
    for (const Character &character : pParty->pCharacters)
        mightBefore += character._stats[ATTRIBUTE_MIGHT];

    game.pressAndReleaseButton(BUTTON_LEFT, pickX, pickY);
    game.tick(3);

    // Global event 411: +1 base Might, then ChangeEvent(410) -> seed 10 ("Empty Barrel").
    EXPECT_EQ(engine->_persistentVariables.decorVars[barrel.eventVarId], 10);
    int mightAfter = 0;
    for (const Character &character : pParty->pCharacters)
        mightAfter += character._stats[ATTRIBUTE_MIGHT];
    EXPECT_EQ(mightAfter, mightBefore + 1);

    // The event's own "+1 Might permanent" status text masks the hover text until it expires.
    engine->_statusBar->clearEvent();
    game.moveMouse(pickX, pickY);
    game.tick(1);
    EXPECT_EQ(engine->_statusBar->get(), "Empty Barrel");
}


GAME_TEST(Mm6, ViewportPillarCapitalTransparency) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // border5/border6, the pillar capitals overhanging the 3D viewport's top corners at (7,8)/(461,8),
    // mark their cut-out with BLACK palette entries (165/182), not index 0. MM6.EXE draws them with the
    // 16bpp-black-keyed blit 0x40a5a0 (via 0x417dc0), so the cut-out must be transparent - with plain
    // palette-0 alpha they drew as opaque black boxes over the sky.
    ASSERT_NE(game_ui_mm6_border5, nullptr);
    ASSERT_NE(game_ui_mm6_border6, nullptr);
    EXPECT_EQ(game_ui_mm6_border5->size(), Sizei(8, 20));
    EXPECT_EQ(game_ui_mm6_border6->size(), Sizei(7, 21));

    // The cut-out corners are transparent, the capital art is opaque.
    EXPECT_EQ(game_ui_mm6_border5->rgba()[19][7].a, 0);  // Palette index 165, rgb (0,0,0).
    EXPECT_NE(game_ui_mm6_border5->rgba()[0][0].a, 0);   // Palette index 239, rgb (83,79,80).
    EXPECT_EQ(game_ui_mm6_border6->rgba()[20][0].a, 0);  // Palette index 182, rgb (0,0,0).
    EXPECT_NE(game_ui_mm6_border6->rgba()[0][6].a, 0);   // Palette index 232, rgb (54,51,50).

    // And a good chunk of each patch is cut out (56/160 resp. 63/147 exactly-black pixels alone).
    auto transparentPixels = [](GraphicsImage *image) {
        int result = 0;
        for (const Color &color : image->rgba().pixels())
            result += color.a == 0;
        return result;
    };
    EXPECT_GE(transparentPixels(game_ui_mm6_border5), 56);
    EXPECT_GE(transparentPixels(game_ui_mm6_border6), 63);
}

GAME_TEST(Mm6, KeyBindingsWindowVirtualSpace) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // All engine UI code works in the virtual 640x480 space; only the renderer maps it to window
    // pixels. The key-bindings window used to size itself from GetPresentDimensions() - window
    // pixels - which is only coincidentally right while the window is exactly 640x480. Resize the
    // window so the two spaces differ.
    game.resizeWindow(1024, 768);
    game.tick(2);
    // Restore unconditionally - a failed ASSERT_* below returns from the test body, and skipping
    // the restore would leak the 1024x768 window into subsequent tests in this process.
    MM_AT_SCOPE_EXIT({
        game.resizeWindow(640, 480);
        game.tick(2);
    });
    ASSERT_EQ(render->GetPresentDimensions(), Sizei(1024, 768));
    ASSERT_EQ(render->GetRenderDimensions(), Sizei(640, 480));

    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_MENU);
    engine->_messageQueue->addMessageCurrentFrame(UIMSG_OpenKeyMappingOptions, 0, 0);
    game.tick(3);
    ASSERT_EQ(current_screen_type, SCREEN_KEYBOARD_OPTIONS);
    EXPECT_EQ(pGUIWindow_CurrentMenu->frameRect, Recti(0, 0, 640, 480));

    // Routes through goToGameOrMainMenu, whose SCREEN_KEYBOARD_OPTIONS escape presses
    // KeyBinding_Default - harmless here because this test changes no bindings.
    game.goToGame();
}

GAME_TEST(Mm6, HouseDialogueWindowVirtualSpace) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // Same wrong-space bug as Mm6.KeyBindingsWindowVirtualSpace, in reinitDialogueWindow: the
    // house dialogue window's width came from GetPresentDimensions() - window pixels - in the
    // virtual-space UI, where it is a text-layout rect. Make the two spaces differ.
    game.resizeWindow(1024, 768);
    game.tick(2);
    // Restore unconditionally - a failed ASSERT_* below returns from the test body, and skipping
    // the restore would leak the 1024x768 window into subsequent tests in this process.
    MM_AT_SCOPE_EXIT({
        game.resizeWindow(640, 480);
        game.tick(2);
    });
    ASSERT_EQ(render->GetPresentDimensions(), Sizei(1024, 768));
    ASSERT_EQ(render->GetRenderDimensions(), Sizei(640, 480));

    // The Knife Shoppe's door face is wired to local event 17, an ungated SpeakInHouse(1); its
    // proprietor greeting runs reinitDialogueWindow.
    const BLVFace *door = nullptr;
    for (const BSPModel &model : pOutdoor->pBModels)
        for (const BLVFace &face : model.faces)
            if (face.eventId == 17 && face.Clickable())
                door = &face;
    ASSERT_NE(door, nullptr);
    Vec3f doorCenter = door->boundingBox.center();
    Vec3f pos = doorCenter + door->facePlane.normal * 130;
    pos.z = door->boundingBox.z1;
    int yawDegrees = TrigLUT.atan2(doorCenter.x - pos.x, doorCenter.y - pos.y) * 90 / 512;
    game.teleportTo(engine->_currentLoadedMapId, pos, yawDegrees);
    game.tick(1);
    game.pressAndReleaseKey(PlatformKey::KEY_SPACE);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_HOUSE);
    ASSERT_NE(pDialogueWindow, nullptr);
    EXPECT_EQ(pDialogueWindow->frameRect, Recti(0, 0, 640, 345));

    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);
}

GAME_TEST(Mm6, NativeResSaveThumbnail) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // Native-resolution mode (render_filter 0) renders the frame at window resolution, but pixel
    // readbacks with a caller-chosen size - save thumbnails above all - must still come out at
    // that size. The mode is snapshotted on Reinitialize, which the resize triggers, so set the
    // filter first. Restore unconditionally - a failed ASSERT_* below returns from the test body,
    // and skipping the restore would leak native-res mode into subsequent tests in this process.
    int oldFilter = engine->config->graphics.RenderFilter.value();
    engine->config->graphics.RenderFilter.setValue(0);
    game.resizeWindow(1024, 768);
    game.tick(2);
    MM_AT_SCOPE_EXIT({
        engine->config->graphics.RenderFilter.setValue(oldFilter);
        game.resizeWindow(640, 480);
        game.tick(2);
    });
    // outputPresent and the native-res mode snapshot are taken together in
    // updateRenderDimensions, so the window size having propagated proves the mode switch did too.
    ASSERT_EQ(render->GetPresentDimensions(), Sizei(1024, 768));
    ASSERT_EQ(render->GetRenderDimensions(), Sizei(640, 480));

    // Readbacks touch the render target, so they run on the game thread (in a non-headless run
    // that's where the GL context is bound). The viewport screenshot contract - an image of
    // exactly the requested size - holds in native-res mode (Lloyd's Beacon asks for 92x68, the
    // gamma preview for 155x117), as does MakeVirtualScreenshot's render-size contract (the
    // endgame certificate is re-drawn as a virtual-space quad at its natural size).
    Sizei viewportShotSize, virtualShotSize;
    game.runGameRoutine([&] {
        viewportShotSize = render->MakeViewportScreenshot(92, 68).size();
        virtualShotSize = render->MakeVirtualScreenshot().size();
    });
    EXPECT_EQ(viewportShotSize, Sizei(92, 68));
    EXPECT_EQ(virtualShotSize, Sizei(640, 480));

    // And a save's thumbnail, captured through the same viewport-screenshot path, decodes at its
    // fixed dimensions.
    Blob save = game.saveGame();
    LodReader reader(std::move(save));
    RgbaImage thumbnail = pcx::decode(reader.read("image.pcx"));
    EXPECT_EQ(thumbnail.size(), Sizei(150, 112));
}

GAME_TEST(Mm6, NativeResHeadlessGate) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();

    // Native-resolution mode (render_filter 0) crash gate: run the frame loop and the full
    // virtual->device->virtual mouse round-trip at a fractional scale - 1000x750 puts the UI
    // transform at 1.5625x, where virtual coordinates land between device pixels. The mode is
    // snapshotted on Reinitialize, which the resize triggers, so set the filter first. Restore
    // unconditionally - a failed ASSERT_* below returns from the test body, and skipping the
    // restore would leak native-res mode into subsequent tests in this process.
    int oldFilter = engine->config->graphics.RenderFilter.value();
    engine->config->graphics.RenderFilter.setValue(0);
    game.resizeWindow(1000, 750);
    game.tick(2);
    MM_AT_SCOPE_EXIT({
        engine->config->graphics.RenderFilter.setValue(oldFilter);
        game.resizeWindow(640, 480);
        game.tick(2);
    });
    // outputPresent and the native-res mode snapshot are taken together in
    // updateRenderDimensions, so the window size having propagated proves the mode switch did too.
    ASSERT_EQ(render->GetPresentDimensions(), Sizei(1000, 750));
    ASSERT_EQ(render->GetRenderDimensions(), Sizei(640, 480));

    // A stretch of ordinary frames, then a click at the center of the screen. Harness clicks are
    // virtual-space: this one goes out through MapToPresent and comes back through MapToRender in
    // the mouse handler - the full round-trip. Center-of-screen, never edge-precise, because the
    // round-trip may drift by a pixel at fractional scales.
    game.tick(10);
    game.pressAndReleaseButton(BUTTON_LEFT, 320, 240);
    game.tick(2);

    // The click came back in virtual space (within round-trip tolerance), and the mode snapshot
    // held through the frames.
    Pointi clickPos = mouse->position();
    EXPECT_GE(clickPos.x, 319);
    EXPECT_LE(clickPos.x, 321);
    EXPECT_GE(clickPos.y, 239);
    EXPECT_LE(clickPos.y, 241);
    EXPECT_EQ(render->GetRenderDimensions(), Sizei(640, 480));
    EXPECT_EQ(render->GetPresentDimensions(), Sizei(1000, 750));

    // Follow-on leg: a window smaller than 640x480. The UI scale clamps to 1 and the virtual
    // frame is centered as a crop (negative offset {-70, -40}) - the crop path's only integration
    // exercise. At the clamped 1x scale the mouse round-trip is exact.
    game.resizeWindow(500, 400);
    game.tick(2);
    ASSERT_EQ(render->GetRenderDimensions(), Sizei(640, 480));
    ASSERT_EQ(render->GetPresentDimensions(), Sizei(500, 400));

    game.tick(5);
    game.pressAndReleaseButton(BUTTON_LEFT, 320, 240);
    game.tick(2);

    clickPos = mouse->position();
    EXPECT_GE(clickPos.x, 319);
    EXPECT_LE(clickPos.x, 321);
    EXPECT_GE(clickPos.y, 239);
    EXPECT_LE(clickPos.y, 241);
}

// The MM7 zombie transformation swaps in face/voice ids 23/24 - MM6 has no zombie art or voice
// bank, and those ids overflow its 12-entry face/voice tables (mm6SpeechSubvariantCounts,
// kMm6DollBodyPos, ...). Zombified MM6 characters must keep their own face and voice.
GAME_TEST(Mm6, ZombieKeepsMm6FaceVoice) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    Character &active = pParty->activeCharacter();
    int face = active.uCurrentFace;
    int voice = active.uVoiceID;
    active.SetCondition(CONDITION_DEAD, 0);
    active.SetCondition(CONDITION_ZOMBIE, 0); // Aborted in playReaction on the MM7 zombie voice id before the fix.
    game.tick(1);
    EXPECT_TRUE(active.conditions.has(CONDITION_ZOMBIE));
    EXPECT_EQ(active.uCurrentFace, face);
    EXPECT_EQ(active.uVoiceID, voice);
}

// MM6 has no zombie mechanic - MM6.EXE's temple Heal case (0x49e027) only checks healable/gold,
// zeroes the condition block and restores HP/mana, with no per-house branch. But MM7's evil-temple
// ids collide with MM6 2dEvents rows (HOUSE_TEMPLE_DEYJA == 78 == Temple Baa), so healing a dead
// character at a Temple Baa ran MM7's zombification - an MM7-only condition plus face/voice ids
// 23/24 that overflow MM6's 12-entry tables.
GAME_TEST(Mm6, TempleBaaHealNoZombie) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    pParty->SetGold(20000);
    ASSERT_TRUE(enterHouse(HouseId(78))); // New Sorpigal's Temple Baa.
    createHouseUI(HouseId(78));

    // Healing a dead character is a plain resurrect - no zombification, no face/voice swap.
    Character &active = pParty->activeCharacter();
    int face = active.uCurrentFace;
    int voice = active.uVoiceID;
    active.conditions.set(CONDITION_DEAD, pParty->GetPlayingTime());
    openProprietorDialogue(game);
    clickProprietorOption(game, DIALOGUE_TEMPLE_HEAL);
    EXPECT_FALSE(active.conditions.has(CONDITION_ZOMBIE));
    EXPECT_FALSE(active.conditions.has(CONDITION_DEAD));
    EXPECT_EQ(active.uCurrentFace, face);
    EXPECT_EQ(active.uVoiceID, voice);
    EXPECT_EQ(active.health, active.GetMaxHealth());

    // A zombie from a contaminated pre-fix save is healable at a Temple Baa like at any other MM6
    // temple: the condition is cleared and the pre-zombie face/voice come back.
    active.uPrevFace = face;
    active.uPrevVoiceID = voice;
    active.uCurrentFace = 23;
    active.uVoiceID = 23;
    active.conditions.set(CONDITION_ZOMBIE, pParty->GetPlayingTime());
    openProprietorDialogue(game);
    game.tick(1); // Re-lay-out the option buttons now that the character is healable again.
    clickProprietorOption(game, DIALOGUE_TEMPLE_HEAL);
    EXPECT_FALSE(active.conditions.has(CONDITION_ZOMBIE));
    EXPECT_EQ(active.uCurrentFace, face);
    EXPECT_EQ(active.uVoiceID, voice);
    leaveHouse(game);
}

// Out-of-range face/voice ids can still reach MM6 characters (the MM7-shaped temple-heal zombie
// path, imported or hand-edited saves). The 12-entry doll/voice table lookups must clamp - the
// way the HUD portrait loader already does - instead of indexing out of bounds.
GAME_TEST(Mm6, OutOfRangeFaceVoiceClamped) {
    if (engine->gameVersion() != GAME_VERSION_MM6)
        GTEST_SKIP() << "MM6 game data required, run with --game-version mm6.";

    game.startNewGame();
    game.tick(1);

    Character &active = pParty->activeCharacter();
    int face = active.uCurrentFace;
    int voice = active.uVoiceID;
    active.uCurrentFace = 23; // MM7's male zombie face - past everything MM6 data has.
    active.uVoiceID = 23;

    active.playReaction(SPEECH_CHEATED_DEATH); // Indexed mm6SpeechSubvariantCounts[...][23] before the fix.
    game.tick(1);

    // The paper doll indexes kMm6DollBodyPos / kMm6DollArm1Pos with the face id on every draw.
    game.pressAndReleaseKey(PlatformKey::KEY_I);
    game.tick(2);
    ASSERT_EQ(current_screen_type, SCREEN_CHARACTERS);
    game.pressAndReleaseKey(PlatformKey::KEY_ESCAPE);
    game.tick(2);
    EXPECT_EQ(current_screen_type, SCREEN_GAME);

    active.uCurrentFace = face;
    active.uVoiceID = voice;
}
