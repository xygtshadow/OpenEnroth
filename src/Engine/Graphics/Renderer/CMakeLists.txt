cmake_minimum_required(VERSION 3.27 FATAL_ERROR)

set(ENGINE_GRAPHICS_RENDERER_SOURCES
        BaseRenderer.cpp
        NullRenderer.cpp
        OpenGLRenderer.cpp
        OpenGLShader.cpp
        OpenGLShaderParams.cpp
        Renderer.cpp
        RendererEnums.cpp
        RendererFactory.cpp)

set(ENGINE_GRAPHICS_RENDERER_HEADERS
        BaseRenderer.h
        OpenGLVertexBuffer.h
        NullRenderer.h
        OpenGLRenderer.h
        OpenGLShader.h
        OpenGLShaderParams.h
        Renderer.h
        RendererEnums.h
        RendererFactory.h
        TextureRenderId.h
        UiScaleTransform.h)

add_library(engine_graphics_renderer STATIC ${ENGINE_GRAPHICS_RENDERER_SOURCES} ${ENGINE_GRAPHICS_RENDERER_HEADERS})
target_check_style(engine_graphics_renderer)

target_link_libraries(engine_graphics_renderer
        PUBLIC
        engine
        engine_graphics
        utility
        library_serialization
        library_color
        library_image
        glm::glm
        OpenGL::GL
        engine_graphics
        PRIVATE
        glad
        library_filesystem_sub
        library_preprocessor)

if(OE_BUILD_TESTS)
    set(TEST_ENGINE_GRAPHICS_RENDERER_SOURCES
            Tests/UiScaleTransform_ut.cpp)

    add_library(test_engine_graphics_renderer OBJECT ${TEST_ENGINE_GRAPHICS_RENDERER_SOURCES})
    # These are true unit tests of header-only pure math, so they link only geometry, not the GL
    # renderer — keeps OpenEnroth_UnitTest free of engine/GL dependencies.
    target_link_libraries(test_engine_graphics_renderer PUBLIC testing_unit library_geometry)

    target_check_style(test_engine_graphics_renderer)

    target_link_libraries(OpenEnroth_UnitTest PUBLIC test_engine_graphics_renderer)
endif()
