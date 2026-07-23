#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Application/Paths/GameVersion.h"

class Environment;

constexpr char mm6PathOverrideKey[] = "OPENENROTH_MM6_PATH";
constexpr char mm7PathOverrideKey[] = "OPENENROTH_MM7_PATH";
constexpr char mm8PathOverrideKey[] = "OPENENROTH_MM8_PATH";

std::vector<std::string> resolveMm6Paths(Environment *environment);
std::vector<std::string> resolveMm7Paths(Environment *environment);
std::vector<std::string> resolveMm8Paths(Environment *environment);

bool validateMm6Path(std::string_view dataPath, std::string *missingFile);
bool validateMm7Path(std::string_view dataPath, std::string *missingFile);

/**
 * Resolves the list of candidate data paths to try for the given game version, in priority order.
 */
std::vector<std::string> resolveGamePaths(Environment *environment, GameVersion version);

/**
 * Validates that `dataPath` holds a complete data set for the given game version.
 *
 * @param dataPath                  Data path to validate.
 * @param version                   Game version to validate against.
 * @param[out] missingFile          Set to the first required file that's missing, if validation fails.
 * @return                          True if `dataPath` is a valid data folder for `version`.
 */
bool validateGamePath(std::string_view dataPath, GameVersion version, std::string *missingFile);

/**
 * Detects which game's data is stored at `dataPath` by validating it against each supported game.
 * This is what makes dropping OpenEnroth.exe into a game folder and just running it work.
 *
 * @return                          The game version whose data set is present at `dataPath`, preferring MM7 in the
 *                                  unlikely case both are, or `std::nullopt` if there's no complete data set.
 */
std::optional<GameVersion> detectGameVersion(std::string_view dataPath);

/**
 * Detects the game to run when neither `--game-version` nor a data path was given, by walking each supported game's
 * own candidate data paths (`OPENENROTH_*_PATH` override, current folder, registry on Windows, ...) and validating
 * each one. This is what makes an MM6-only install runnable without any command line arguments.
 *
 * @param environment               Environment to resolve the candidate paths against.
 * @return                          Version of the first complete install found, checking all of MM7's candidates
 *                                  before MM6's, or `std::nullopt` if no candidate holds one.
 */
std::optional<GameVersion> detectGameVersion(Environment *environment);

std::string resolveMm7UserPath(Environment *environment);
