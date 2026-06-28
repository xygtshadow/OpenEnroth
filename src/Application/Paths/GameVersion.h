#pragma once

/**
 * Which Might and Magic game the engine was asked to run. This is an explicit, user-driven
 * selection (e.g. from the command line), as opposed to `LodVersion`, which describes the
 * on-disk format of an individual LOD file.
 */
enum class GameVersion {
    GAME_VERSION_MM6,
    GAME_VERSION_MM7,
};
using enum GameVersion;
