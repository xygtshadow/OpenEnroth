#include <filesystem>
#include <initializer_list>
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
