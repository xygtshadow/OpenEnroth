#include <unordered_map>
#include <unordered_set>

#include "Testing/Unit/UnitTest.h"

#include "Io/InputEnumFunctions.h"

// Same map type as the Keybindings alias in Io/KeyboardActionMapping.h - spelled out here so
// the test doesn't pull in that header and the GameConfig machinery behind it.
using Keybindings = std::unordered_map<InputAction, PlatformKey>;

UNIT_TEST(KeyboardActionMapping, DefaultKeyShareIsNotAConflict) {
    // Jump & FlyUp deliberately share Space by default - the key-binding screen must not flag
    // the shipped scheme as conflicted, or it would refuse to close after any rebind.
    Keybindings defaults = {
        {INPUT_ACTION_JUMP, PlatformKey::KEY_SPACE},
        {INPUT_ACTION_FLY_UP, PlatformKey::KEY_SPACE},
        {INPUT_ACTION_REST, PlatformKey::KEY_R},
    };
    EXPECT_TRUE(findConflictingKeybindings(defaults, defaults).empty());

    // Rebinding an unrelated action doesn't disturb the intentional share.
    Keybindings bindings = defaults;
    bindings[INPUT_ACTION_REST] = PlatformKey::KEY_G;
    EXPECT_TRUE(findConflictingKeybindings(bindings, defaults).empty());
}

UNIT_TEST(KeyboardActionMapping, RebindOntoSharedDefaultKeyConflicts) {
    // A user rebinding a third action onto the shared default key is a real conflict - all
    // three actions end up flagged.
    Keybindings defaults = {
        {INPUT_ACTION_JUMP, PlatformKey::KEY_SPACE},
        {INPUT_ACTION_FLY_UP, PlatformKey::KEY_SPACE},
        {INPUT_ACTION_REST, PlatformKey::KEY_R},
    };
    Keybindings bindings = defaults;
    bindings[INPUT_ACTION_REST] = PlatformKey::KEY_SPACE;
    EXPECT_EQ(findConflictingKeybindings(bindings, defaults),
              (std::unordered_set<InputAction>{INPUT_ACTION_JUMP, INPUT_ACTION_FLY_UP, INPUT_ACTION_REST}));
}

UNIT_TEST(KeyboardActionMapping, RebindOntoOccupiedKeyConflicts) {
    Keybindings defaults = {
        {INPUT_ACTION_ATTACK, PlatformKey::KEY_Q},
        {INPUT_ACTION_REST, PlatformKey::KEY_R},
        {INPUT_ACTION_YELL, PlatformKey::KEY_Y},
    };

    // Two actions rebound onto one non-default key.
    Keybindings bindings = defaults;
    bindings[INPUT_ACTION_ATTACK] = PlatformKey::KEY_G;
    bindings[INPUT_ACTION_REST] = PlatformKey::KEY_G;
    EXPECT_EQ(findConflictingKeybindings(bindings, defaults),
              (std::unordered_set<InputAction>{INPUT_ACTION_ATTACK, INPUT_ACTION_REST}));

    // One action still on its default key, another rebound onto it.
    bindings = defaults;
    bindings[INPUT_ACTION_REST] = PlatformKey::KEY_Q;
    EXPECT_EQ(findConflictingKeybindings(bindings, defaults),
              (std::unordered_set<InputAction>{INPUT_ACTION_ATTACK, INPUT_ACTION_REST}));
}

UNIT_TEST(KeyboardActionMapping, DistinctKeysNoConflict) {
    Keybindings defaults = {
        {INPUT_ACTION_ATTACK, PlatformKey::KEY_Q},
        {INPUT_ACTION_REST, PlatformKey::KEY_R},
    };
    Keybindings bindings = {
        {INPUT_ACTION_ATTACK, PlatformKey::KEY_G},
        {INPUT_ACTION_REST, PlatformKey::KEY_H},
    };
    EXPECT_TRUE(findConflictingKeybindings(bindings, defaults).empty());
}
