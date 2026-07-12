#include <filesystem>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>

#include "Testing/Unit/UnitTest.h"

#include "Application/Paths/PathResolver.h"

#include "Library/FileSystem/Directory/DirectoryFileSystem.h"

#include "Utility/ScopeGuard.h"

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
