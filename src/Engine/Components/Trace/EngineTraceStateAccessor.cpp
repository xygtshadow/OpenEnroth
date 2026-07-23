#include "EngineTraceStateAccessor.h"

#include <array>
#include <string>
#include <utility>

#include "Application/GameConfig.h"

#include "Engine/Party.h"
#include "Engine/Engine.h"
#include "Engine/MapInfo.h"
#include "Engine/mm7_data.h"

#include "Media/Audio/AudioPlayer.h"

#include "Library/Trace/EventTrace.h"

#include "Utility/String/Ascii.h"
#include "Utility/String/Encoding.h"

// The classic (original MM7) bindings for the actions that the modern default scheme rebinds.
// Single source of truth for the pin that `applyClassicKeybindings` applies on both the recording
// and the playback side, and for keeping these entries out of recorded config patches - both sides
// pin them unconditionally, so serializing them would only make every committed trace
// non-canonical whenever one of the shipped defaults changes.
static constexpr std::array<std::pair<GameConfig::Key GameConfig::Keybindings::*, PlatformKey>, 11> classicKeybindings = {{
    {&GameConfig::Keybindings::Forward, PlatformKey::KEY_UP},
    {&GameConfig::Keybindings::Backward, PlatformKey::KEY_DOWN},
    {&GameConfig::Keybindings::StepLeft, PlatformKey::KEY_LEFTBRACKET},
    {&GameConfig::Keybindings::StepRight, PlatformKey::KEY_RIGHTBRACKET},
    {&GameConfig::Keybindings::Jump, PlatformKey::KEY_X},
    {&GameConfig::Keybindings::FlyUp, PlatformKey::KEY_PAGEUP},
    {&GameConfig::Keybindings::FlyDown, PlatformKey::KEY_INSERT},
    {&GameConfig::Keybindings::Attack, PlatformKey::KEY_A},
    {&GameConfig::Keybindings::CastReady, PlatformKey::KEY_S},
    {&GameConfig::Keybindings::EventTrigger, PlatformKey::KEY_SPACE},
    {&GameConfig::Keybindings::Quest, PlatformKey::KEY_Q},
}};

static bool isClassicPinnedKeybinding(const GameConfig *config, const AnyConfigEntry *entry) {
    for (const auto &[binding, key] : classicKeybindings)
        if (&(config->keybindings.*binding) == entry)
            return true;
    return false;
}

static bool shouldSkip(const GameConfig *config, const ConfigSection *section, const AnyConfigEntry *entry) {
    return
        (section == &config->window && entry != &config->window.Width && entry != &config->window.Height) ||
        section == &config->graphics ||
        isClassicPinnedKeybinding(config, entry) ||
        entry == &config->settings.MusicLevel ||
        entry == &config->settings.VoiceLevel ||
        entry == &config->settings.SoundLevel ||
        entry == &config->debug.LogLevel ||
        entry == &config->debug.NoVideo ||
        entry == &config->debug.NoPartyActorCollisions ||
        entry == &config->gameplay.QuickSavesCount;
}

static bool shouldTake(const GameConfig *config, const ConfigSection *section, const AnyConfigEntry *entry) {
    return
        entry->string() != entry->defaultString() ||
        entry == &config->debug.TraceFrameTimeMs ||
        entry == &config->debug.TraceRandomEngine ||
        entry == &config->debug.TraceNoVideo ||
        entry == &config->debug.TraceNoPartyActorCollisions;
}

void EngineTraceStateAccessor::prepareForRecording(GameConfig *config, ConfigPatch *patch) {
    applyClassicKeybindings(config);
    *patch = ConfigPatch::fromConfig(config, [config] (const ConfigSection *section, const AnyConfigEntry *entry) {
        return !shouldSkip(config, section, entry) && shouldTake(config, section, entry);
    });

    config->graphics.FPSLimit.setValue(1000 / config->debug.TraceFrameTimeMs.value());
    config->debug.NoVideo.setValue(config->debug.TraceNoVideo.value());
    config->debug.NoPartyActorCollisions.setValue(config->debug.TraceNoPartyActorCollisions.value());
}

void EngineTraceStateAccessor::prepareForPlayback(GameConfig *config, const ConfigPatch &patch) {
    for (ConfigSection *section : config->sections())
        for (AnyConfigEntry *entry : section->entries())
            if (!shouldSkip(config, section, entry))
                entry->reset();

    applyClassicKeybindings(config); // Raw keypresses in traces assume classic bindings.
    patch.apply(config);

    // We don't set voice & music levels to 0.0 b/c in this case our code doesn't even call into OpenAL, and this is NOT
    // what we want when playing traces, especially when running tests - we want the test code path to be the same as
    // in-game one.
    config->settings.MusicLevel.setValue(1.0);
    config->settings.VoiceLevel.setValue(1.0);
    config->settings.SoundLevel.setValue(1.0);
    config->window.MouseGrab.setValue(false);
    config->graphics.FPSLimit.setValue(0); // Unlimited.
    config->graphics.AlwaysCustomCursor.setValue(true); // We want to see the mouse pointer.
    config->debug.NoVideo.setValue(config->debug.TraceNoVideo.value());
    config->debug.NoPartyActorCollisions.setValue(config->debug.TraceNoPartyActorCollisions.value());
    pAudioPlayer->UpdateVolumeFromConfig();
}

void EngineTraceStateAccessor::applyClassicKeybindings(GameConfig *config) {
    for (const auto &[binding, key] : classicKeybindings)
        (config->keybindings.*binding).setValue(key);
}

EventTraceGameState EngineTraceStateAccessor::makeGameState() {
    auto toDebugString = [](const Item &item) {
        std::string result;
        if (item.isWand()) {
            result = fmt::format("{} [{}/{}]", item.GetIdentifiedName(), item.numCharges, item.maxCharges);
        } else if (item.isPotion() && item.itemId != ITEM_POTION_BOTTLE) {
            std::string name = item.GetIdentifiedName();
            if (name.ends_with("Potion")) {
                result = fmt::format("{} [{}]", name, item.potionPower);
            } else {
                result = fmt::format("{} Potion [{}]", name, item.potionPower);
            }
        } else if (item.isSpellScroll()) {
            result = fmt::format("Scroll of {}", item.GetIdentifiedName());
        } else if (item.standardEnchantment) {
            result = fmt::format("{} [+{}]", item.GetIdentifiedName(), item.standardEnchantmentStrength);
        } else if (item.itemId == ITEM_QUEST_LICH_JAR_FULL) {
            result = fmt::format("{} [{}]", item.GetIdentifiedName(), item.lichJarCharacterIndex);
        } else {
            result = item.GetIdentifiedName();
        }

        if (!item.IsIdentified())
            result += " [UNID]";
        if (item.IsBroken())
            result += " [BROKEN]";

        // TODO(captainurist): drop this call once we have everything in UTF-8.
        return txt::utf8ToEncoded(result, ENCODING_ASCII);
    };

    EventTraceGameState result;
    result.locationName = ascii::toLower(pMapStats->pInfos[engine->_currentLoadedMapId].fileName);
    result.partyPosition = pParty->pos.toInt();
    for (const Character &character : pParty->pCharacters) {
        EventTraceCharacterState &traceCharacter = result.characters.emplace_back();
        traceCharacter.hp = character.health;
        traceCharacter.mp = character.mana;
        traceCharacter.might = character.GetActualMight();
        traceCharacter.intelligence = character.GetActualIntelligence();
        traceCharacter.personality = character.GetActualPersonality();
        traceCharacter.endurance = character.GetActualEndurance();
        traceCharacter.accuracy = character.GetActualAccuracy();
        traceCharacter.speed = character.GetActualSpeed();
        traceCharacter.luck = character.GetActualLuck();

        for (InventoryConstEntry entry : character.inventory.equipment())
            traceCharacter.equipment.emplace_back(toDebugString(*entry));
        for (int y = 0; y < character.inventory.gridSize().h; y++)
            for (int x = 0; x < character.inventory.gridSize().w; x++)
                if (InventoryConstEntry entry = character.inventory.entry(Pointi(x, y)); entry && entry.geometry().topLeft() == Pointi(x, y))
                    traceCharacter.backpack.emplace_back(toDebugString(*entry));
    }
    return result;
}
