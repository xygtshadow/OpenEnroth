#pragma once

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
 * @param[out] missingFile          Set to the first required file that's missing, if validation fails.
 * @return                          True if `dataPath` is a valid data folder for `version`.
 */
bool validateGamePath(std::string_view dataPath, GameVersion version, std::string *missingFile);

std::string resolveMm7UserPath(Environment *environment);
