#include <string>
#include <vector>

#include "Testing/Unit/UnitTest.h"

#include "Application/GameConfig.h"

#include "Engine/Components/Trace/EngineTraceStateAccessor.h"

#include "Library/Config/ConfigPatch.h"

UNIT_TEST(EngineTraceStateAccessor, ClassicPinnedKeybindingsAreNotRecorded) {
    // Recording pins the classic bindings for the actions that the modern default scheme rebinds.
    // The pins must not be serialized into the trace's config patch - playback re-pins them
    // unconditionally, and serializing them makes every committed trace non-canonical whenever
    // a shipped default binding changes.
    GameConfig config;
    ConfigPatch patch;
    EngineTraceStateAccessor::prepareForRecording(&config, &patch);
    for (const ConfigPatchEntry &entry : patch.entries())
        EXPECT_NE(entry.section, "keybindings") << "'" << entry.key << "' leaked into the trace config patch";
}

UNIT_TEST(EngineTraceStateAccessor, CustomKeybindingsAreRecorded) {
    // Custom rebinds of non-pinned actions must still be serialized - committed traces
    // (e.g. issue_2018.json) rely on them at playback. Custom values of pinned actions are
    // overwritten by the classic pin and thus never recorded.
    GameConfig config;
    config.keybindings.Screenshot.setValue(PlatformKey::KEY_PRINTSCREEN);
    config.keybindings.Forward.setValue(PlatformKey::KEY_T);

    ConfigPatch patch;
    EngineTraceStateAccessor::prepareForRecording(&config, &patch);

    std::vector<std::string> keybindingKeys;
    for (const ConfigPatchEntry &entry : patch.entries())
        if (entry.section == "keybindings")
            keybindingKeys.push_back(entry.key);
    EXPECT_EQ(keybindingKeys, std::vector<std::string>{"screenshot"});
}
