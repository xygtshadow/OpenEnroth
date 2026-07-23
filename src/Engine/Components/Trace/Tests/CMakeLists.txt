cmake_minimum_required(VERSION 3.27 FATAL_ERROR)

if(OE_BUILD_TESTS)
    set(TEST_ENGINE_COMPONENTS_TRACE_SOURCES
            EngineTraceStateAccessor_ut.cpp)

    add_library(test_engine_components_trace OBJECT ${TEST_ENGINE_COMPONENTS_TRACE_SOURCES})
    target_check_style(test_engine_components_trace)
    target_link_libraries(test_engine_components_trace PUBLIC testing_unit engine_components_trace application)

    target_link_libraries(OpenEnroth_GameTest PUBLIC test_engine_components_trace)
endif()
