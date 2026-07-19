#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "Library/Platform/Interface/PlatformEnums.h"
#include "Library/Serialization/SerializationFwd.h"

#include "Utility/Segment.h"

#include "InputEnums.h"

inline Segment<InputAction> allInputActions() {
    return {INPUT_ACTION_FIRST_VALID, INPUT_ACTION_LAST_VALID};
}

inline Segment<InputAction> allConfigurableInputActions() {
    return {INPUT_ACTION_FIRST_CONFIGURABLE, INPUT_ACTION_LAST_CONFIGURABLE};
}

std::string GetDisplayName(InputAction action);

InputActionTriggerMode triggerModeForInputAction(InputAction action);

/**
 * Computes which actions in @p bindings conflict - are bound to the same key as another action.
 *
 * A pair sharing a key is deliberately NOT a conflict when both actions are bound to their
 * default keys, so shipped default shares (Space is both Jump and FlyUp) don't lock the
 * key-binding screen. Every user-created duplicate is still flagged: when a user rebinds an
 * action onto an occupied key, at least one action of the resulting pair is on a non-default key.
 *
 * @param bindings                      Bindings to check, e.g. the key-binding screen's working copy
 *                                      (`Io::Keybindings` is this map type).
 * @param defaultBindings               Default bindings for the same set of actions.
 * @return                              Actions of @p bindings that are part of at least one conflicting pair.
 */
std::unordered_set<InputAction> findConflictingKeybindings(const std::unordered_map<InputAction, PlatformKey> &bindings,
                                                           const std::unordered_map<InputAction, PlatformKey> &defaultBindings);

// TODO(captainurist): find a better place for this code
MM_DECLARE_SERIALIZATION_FUNCTIONS(PlatformKey)

std::string GetDisplayName(PlatformKey key);
bool TryParseDisplayName(std::string_view displayName, PlatformKey *outKey);
