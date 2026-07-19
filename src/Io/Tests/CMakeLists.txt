cmake_minimum_required(VERSION 3.27 FATAL_ERROR)

if(OE_BUILD_TESTS)
    set(TEST_IO_SOURCES
            KeyboardActionMapping_ut.cpp)

    add_library(test_io OBJECT ${TEST_IO_SOURCES})
    target_check_style(test_io)
    target_link_libraries(test_io PUBLIC testing_unit io)

    target_link_libraries(OpenEnroth_UnitTest PUBLIC test_io)
endif()
