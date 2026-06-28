cmake_minimum_required(VERSION 3.27 FATAL_ERROR)

set(ENGINE_SERIALIZATION_SOURCES
        CompositeSnapshots.cpp
        EntitySnapshots.cpp
        EnumSnapshots.cpp
        TableSerialization.cpp)

set(ENGINE_SERIALIZATION_HEADERS
        CompositeSnapshots.h
        EntitySnapshots.h
        EnumSnapshots.h
        TableSerialization.h)

add_library(engine_serialization STATIC ${ENGINE_SERIALIZATION_SOURCES} ${ENGINE_SERIALIZATION_HEADERS})
target_link_libraries(engine_serialization PUBLIC engine library_binary library_snapshots)
target_check_style(engine_serialization)

if(OE_BUILD_TESTS)
    set(TEST_ENGINE_SERIALIZATION_SOURCES
            Tests/EntitySnapshots_ut.cpp)

    add_library(test_engine_serialization OBJECT ${TEST_ENGINE_SERIALIZATION_SOURCES})
    target_link_libraries(test_engine_serialization PUBLIC testing_unit engine_serialization)

    target_check_style(test_engine_serialization)

    target_link_libraries(OpenEnroth_GameTest PUBLIC test_engine_serialization)
endif()
