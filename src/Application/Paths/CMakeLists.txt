cmake_minimum_required(VERSION 3.27 FATAL_ERROR)

set(APPLICATION_PATHS_SOURCES
        PathResolver.cpp)

set(APPLICATION_PATHS_HEADERS
        GameVersion.h
        PathResolver.h)

add_library(application_paths STATIC ${APPLICATION_PATHS_SOURCES} ${APPLICATION_PATHS_HEADERS})
target_check_style(application_paths)

target_link_libraries(application_paths
        PRIVATE
        library_environment_interface
        library_filesystem_directory
        library_filesystem_lowercase
        library_logger
        utility)

if(OE_BUILD_TESTS)
    set(TEST_APPLICATION_PATHS_SOURCES Tests/PathResolver_ut.cpp)

    add_library(test_application_paths OBJECT ${TEST_APPLICATION_PATHS_SOURCES})
    target_link_libraries(test_application_paths PUBLIC testing_unit application_paths library_filesystem_directory)

    target_check_style(test_application_paths)

    target_link_libraries(OpenEnroth_UnitTest PUBLIC test_application_paths)
endif()
