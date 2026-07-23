#include <filesystem>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "Testing/Unit/UnitTest.h"

#include "Application/Paths/PathResolver.h"

#include "Library/Environment/Interface/Environment.h"
#include "Library/FileSystem/Directory/DirectoryFileSystem.h"
#include "Library/Logger/BufferLogSink.h"
#include "Library/Logger/Logger.h"

#include "Utility/ScopeGuard.h"

namespace {
class TestEnvironment : public Environment {
 public:
    std::string queryRegistry(const std::string &path) const override {
        auto pos = registry.find(path);
        return pos == registry.end() ? std::string() : pos->second;
    }

    std::string path(EnvironmentPath path) const override {
        return {};
    }

    std::string getenv(const std::string &key) const override {
        auto pos = env.find(key);
        return pos == env.end() ? std::string() : pos->second;
    }

    void setenv(const std::string &key, const std::string &value) const override {
        env[key] = value;
    }

    mutable std::unordered_map<std::string, std::string> env;
    std::unordered_map<std::string, std::string> registry;
};
} // anonymous namespace

static void createInstall(std::string_view dir, std::initializer_list<const char *> files) {
    DirectoryFileSystem fs(dir);
    for (const char *file : files)
        fs.write(file, Blob());
}

UNIT_TEST(PathResolver, ValidatesCompleteMm7Install) {
    MM_AT_SCOPE_EXIT(std::filesystem::remove_all("tmp_pathresolver_mm7"));
    createInstall("tmp_pathresolver_mm7", {
        "anims/magic7.vid", "anims/might7.vid",
        "data/bitmaps.lod", "data/events.lod", "data/games.lod",
        "data/icons.lod", "data/sprites.lod", "sounds/audio.snd"});

    std::string missingFile;
    EXPECT_TRUE(validateMm7Path("tmp_pathresolver_mm7", &missingFile));
}

UNIT_TEST(PathResolver, ValidatesCompleteMm6Install) {
    MM_AT_SCOPE_EXIT(std::filesystem::remove_all("tmp_pathresolver_mm6"));
    createInstall("tmp_pathresolver_mm6", {
        "anims/anims1.vid", "anims/anims2.vid",
        "data/bitmaps.lod", "data/games.lod",
        "data/icons.lod", "data/sprites.lod", "sounds/audio.snd"});

    std::string missingFile;
    EXPECT_TRUE(validateMm6Path("tmp_pathresolver_mm6", &missingFile));
}

UNIT_TEST(PathResolver, ReportsMissingMm6File) {
    MM_AT_SCOPE_EXIT(std::filesystem::remove_all("tmp_pathresolver_mm6_missing"));
    createInstall("tmp_pathresolver_mm6_missing", {
        // data/bitmaps.lod is intentionally absent.
        "anims/anims1.vid", "anims/anims2.vid",
        "data/games.lod", "data/icons.lod", "data/sprites.lod",
        "sounds/audio.snd"});

    std::string missingFile;
    EXPECT_FALSE(validateMm6Path("tmp_pathresolver_mm6_missing", &missingFile));
    EXPECT_EQ(missingFile, "data/bitmaps.lod");
}

UNIT_TEST(PathResolver, Mm6InstallFailsMm7Validation) {
    MM_AT_SCOPE_EXIT(std::filesystem::remove_all("tmp_pathresolver_cross"));
    createInstall("tmp_pathresolver_cross", {
        "anims/anims1.vid", "anims/anims2.vid",
        "data/bitmaps.lod", "data/games.lod",
        "data/icons.lod", "data/sprites.lod", "sounds/audio.snd"});

    // An MM6 install lacks MM7-only files (e.g. data/events.lod), so it must not
    // pass MM7 validation - this is exactly why MM6 needs its own validator.
    std::string missingFile;
    EXPECT_FALSE(validateMm7Path("tmp_pathresolver_cross", &missingFile));
}

UNIT_TEST(PathResolver, ValidateGamePathRoutesToMm6) {
    MM_AT_SCOPE_EXIT(std::filesystem::remove_all("tmp_gamepath_mm6"));
    createInstall("tmp_gamepath_mm6", {
        "anims/anims1.vid", "anims/anims2.vid",
        "data/bitmaps.lod", "data/games.lod",
        "data/icons.lod", "data/sprites.lod", "sounds/audio.snd"});

    std::string missingFile;
    EXPECT_TRUE(validateGamePath("tmp_gamepath_mm6", GAME_VERSION_MM6, &missingFile));
    EXPECT_FALSE(validateGamePath("tmp_gamepath_mm6", GAME_VERSION_MM7, &missingFile));
}

UNIT_TEST(PathResolver, ValidateGamePathRoutesToMm7) {
    MM_AT_SCOPE_EXIT(std::filesystem::remove_all("tmp_gamepath_mm7"));
    createInstall("tmp_gamepath_mm7", {
        "anims/magic7.vid", "anims/might7.vid",
        "data/bitmaps.lod", "data/events.lod", "data/games.lod",
        "data/icons.lod", "data/sprites.lod", "sounds/audio.snd"});

    std::string missingFile;
    EXPECT_TRUE(validateGamePath("tmp_gamepath_mm7", GAME_VERSION_MM7, &missingFile));
    EXPECT_FALSE(validateGamePath("tmp_gamepath_mm7", GAME_VERSION_MM6, &missingFile));
}

UNIT_TEST(PathResolver, DetectsMm6Install) {
    MM_AT_SCOPE_EXIT(std::filesystem::remove_all("tmp_detect_mm6"));
    createInstall("tmp_detect_mm6", {
        "anims/anims1.vid", "anims/anims2.vid",
        "data/bitmaps.lod", "data/games.lod",
        "data/icons.lod", "data/sprites.lod", "sounds/audio.snd"});

    EXPECT_EQ(detectGameVersion("tmp_detect_mm6"), GAME_VERSION_MM6);
}

UNIT_TEST(PathResolver, DetectsMm7Install) {
    MM_AT_SCOPE_EXIT(std::filesystem::remove_all("tmp_detect_mm7"));
    createInstall("tmp_detect_mm7", {
        "anims/magic7.vid", "anims/might7.vid",
        "data/bitmaps.lod", "data/events.lod", "data/games.lod",
        "data/icons.lod", "data/sprites.lod", "sounds/audio.snd"});

    EXPECT_EQ(detectGameVersion("tmp_detect_mm7"), GAME_VERSION_MM7);
}

UNIT_TEST(PathResolver, DetectsNothingInNonGameFolder) {
    MM_AT_SCOPE_EXIT(std::filesystem::remove_all("tmp_detect_none"));
    createInstall("tmp_detect_none", {"readme.txt"});

    EXPECT_EQ(detectGameVersion("tmp_detect_none"), std::nullopt);
    EXPECT_EQ(detectGameVersion("tmp_detect_nonexistent"), std::nullopt);
}

UNIT_TEST(PathResolver, DetectsMm6InstallFromEnvironmentOverride) {
    // An MM6-only setup announced via OPENENROTH_MM6_PATH must be found even though the
    // current folder holds no game data at all (issue: detection used to probe only cwd
    // and fall back to MM7, so an MM6-only install failed as "missing MM7").
    MM_AT_SCOPE_EXIT(std::filesystem::remove_all("tmp_envdetect_mm6"));
    createInstall("tmp_envdetect_mm6", {
        "anims/anims1.vid", "anims/anims2.vid",
        "data/bitmaps.lod", "data/games.lod",
        "data/icons.lod", "data/sprites.lod", "sounds/audio.snd"});

    BufferLogSink sink;
    Logger testLogger(LOG_TRACE, &sink); // The path-override branch logs, and unit tests have no global logger.

    TestEnvironment environment;
    environment.env[mm6PathOverrideKey] = "tmp_envdetect_mm6";
    EXPECT_EQ(detectGameVersion(&environment), GAME_VERSION_MM6);
}

UNIT_TEST(PathResolver, DetectsMm6InstallFromRegistry) {
    MM_AT_SCOPE_EXIT(std::filesystem::remove_all("tmp_regdetect_mm6"));
    createInstall("tmp_regdetect_mm6", {
        "anims/anims1.vid", "anims/anims2.vid",
        "data/bitmaps.lod", "data/games.lod",
        "data/icons.lod", "data/sprites.lod", "sounds/audio.snd"});

    TestEnvironment environment;
    environment.registry["HKEY_LOCAL_MACHINE/SOFTWARE/WOW6432Node/GOG.com/Games/1207661253/PATH"] =
        "tmp_regdetect_mm6";
    EXPECT_EQ(detectGameVersion(&environment), GAME_VERSION_MM6);
}

UNIT_TEST(PathResolver, DetectsMm7InstallBeforeMm6FromEnvironment) {
    // With complete installs of both games resolvable, MM7 wins - same preference as
    // the single-folder detection and the `--game-version` default.
    MM_AT_SCOPE_EXIT({
        std::filesystem::remove_all("tmp_envdetect_both6");
        std::filesystem::remove_all("tmp_envdetect_both7");
    });
    createInstall("tmp_envdetect_both6", {
        "anims/anims1.vid", "anims/anims2.vid",
        "data/bitmaps.lod", "data/games.lod",
        "data/icons.lod", "data/sprites.lod", "sounds/audio.snd"});
    createInstall("tmp_envdetect_both7", {
        "anims/magic7.vid", "anims/might7.vid",
        "data/bitmaps.lod", "data/events.lod", "data/games.lod",
        "data/icons.lod", "data/sprites.lod", "sounds/audio.snd"});

    BufferLogSink sink;
    Logger testLogger(LOG_TRACE, &sink); // The path-override branch logs, and unit tests have no global logger.

    TestEnvironment environment;
    environment.env[mm6PathOverrideKey] = "tmp_envdetect_both6";
    environment.env[mm7PathOverrideKey] = "tmp_envdetect_both7";
    EXPECT_EQ(detectGameVersion(&environment), GAME_VERSION_MM7);
}

UNIT_TEST(PathResolver, DetectsNothingFromBareEnvironment) {
    // No overrides, no registry keys, and the test process's cwd isn't a game folder.
    TestEnvironment environment;
    EXPECT_EQ(detectGameVersion(&environment), std::nullopt);
}

UNIT_TEST(PathResolver, DetectsMm7WhenBothGamesPresent) {
    // A folder that somehow holds both data sets should resolve to MM7 - same
    // preference as the `--game-version` default.
    MM_AT_SCOPE_EXIT(std::filesystem::remove_all("tmp_detect_both"));
    createInstall("tmp_detect_both", {
        "anims/anims1.vid", "anims/anims2.vid",
        "anims/magic7.vid", "anims/might7.vid",
        "data/bitmaps.lod", "data/events.lod", "data/games.lod",
        "data/icons.lod", "data/sprites.lod", "sounds/audio.snd"});

    EXPECT_EQ(detectGameVersion("tmp_detect_both"), GAME_VERSION_MM7);
}
