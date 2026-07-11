cmake_minimum_required(VERSION 3.24)

set(GREENFIELD_BUILD_PROFILE "developer" CACHE STRING "Greenfield build profile: developer or headless-fast2d")
set_property(CACHE GREENFIELD_BUILD_PROFILE PROPERTY STRINGS developer headless-fast2d)

set(_greenfield_supported_build_profiles developer headless-fast2d)
if(NOT GREENFIELD_BUILD_PROFILE IN_LIST _greenfield_supported_build_profiles)
    message(FATAL_ERROR "Invalid GREENFIELD_BUILD_PROFILE '${GREENFIELD_BUILD_PROFILE}'. Supported values are: developer, headless-fast2d.")
endif()

set(GREENFIELD_BUILD_DEVELOPER_PROFILE OFF)
if(GREENFIELD_BUILD_PROFILE STREQUAL "developer")
    set(GREENFIELD_BUILD_DEVELOPER_PROFILE ON)
endif()

project(Greenfield VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(Warnings)

option(GREENFIELD_ALLOW_SYSTEM_DEPENDENCIES "Allow CMake to use system-installed dependencies when vcpkg is not available" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

if(GREENFIELD_BUILD_DEVELOPER_PROFILE)
    if(GREENFIELD_ALLOW_SYSTEM_DEPENDENCIES)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)
        set(GREENFIELD_SDL3_TARGET PkgConfig::SDL3)
    else()
        find_package(SDL3 CONFIG REQUIRED)
        set(GREENFIELD_SDL3_TARGET SDL3::SDL3)
    endif()

    find_package(Dawn CONFIG REQUIRED)
    find_package(Freetype REQUIRED)
endif()

add_library(greenfield_core INTERFACE)
target_include_directories(greenfield_core INTERFACE "${PROJECT_SOURCE_DIR}")
target_compile_features(greenfield_core INTERFACE cxx_std_20)

add_library(greenfield_render INTERFACE)
target_include_directories(greenfield_render INTERFACE "${PROJECT_SOURCE_DIR}")
target_link_libraries(greenfield_render INTERFACE greenfield_core)

add_library(greenfield_visualization
    engine/visualization/Viewport2D.cpp
)
target_include_directories(greenfield_visualization PUBLIC "${PROJECT_SOURCE_DIR}")
target_link_libraries(greenfield_visualization PUBLIC greenfield_core)
greenfield_enable_warnings(greenfield_visualization)

add_library(greenfield_render_fast2d
    engine/render/fast2d/Fast2DRenderer.cpp
)
target_include_directories(greenfield_render_fast2d PUBLIC "${PROJECT_SOURCE_DIR}")
target_link_libraries(greenfield_render_fast2d PUBLIC greenfield_core greenfield_render)
greenfield_enable_warnings(greenfield_render_fast2d)

add_library(greenfield_ui
    engine/ui/UiContext.cpp
)
target_include_directories(greenfield_ui PUBLIC "${PROJECT_SOURCE_DIR}")
target_link_libraries(greenfield_ui PUBLIC greenfield_core greenfield_render)
greenfield_enable_warnings(greenfield_ui)

add_library(greenfield_platform INTERFACE)
target_include_directories(greenfield_platform INTERFACE "${PROJECT_SOURCE_DIR}")
target_link_libraries(greenfield_platform INTERFACE greenfield_core)

if(GREENFIELD_BUILD_DEVELOPER_PROFILE)
    add_library(greenfield_sdl_platform
        engine/platform/SdlRasterPresenter.cpp
        engine/platform/SdlStartupPresenter.cpp
        engine/platform/SdlWindow.cpp
    )
    target_include_directories(greenfield_sdl_platform PUBLIC "${PROJECT_SOURCE_DIR}")
    target_link_libraries(greenfield_sdl_platform PUBLIC greenfield_platform "${GREENFIELD_SDL3_TARGET}")
    greenfield_enable_warnings(greenfield_sdl_platform)

    add_library(greenfield_render_webgpu
        engine/render/webgpu/WebGpuContext.cpp
        engine/render/webgpu/WebGpuRenderer.cpp
    )
    add_library(greenfield_webgpu ALIAS greenfield_render_webgpu)
    target_include_directories(greenfield_render_webgpu PUBLIC "${PROJECT_SOURCE_DIR}")
    # The implemented WebGPU renderer currently owns Dawn/WebGPU and backend-local
    # FreeType text rendering; greenfield_ui and greenfield_render stay renderer-agnostic.
    target_link_libraries(greenfield_render_webgpu PUBLIC greenfield_core greenfield_render greenfield_platform dawn::webgpu_dawn Freetype::Freetype)
    greenfield_enable_warnings(greenfield_render_webgpu)

    add_executable(greenfield_sandbox
        apps/sandbox/main.cpp
    )
    target_include_directories(greenfield_sandbox PRIVATE "${PROJECT_SOURCE_DIR}")
    target_link_libraries(greenfield_sandbox PRIVATE greenfield_core greenfield_render greenfield_ui greenfield_sdl_platform
                                                      greenfield_render_fast2d greenfield_render_webgpu)
    greenfield_enable_warnings(greenfield_sandbox)
endif()

if(BUILD_TESTING)
    add_test(
        NAME greenfield_dependency_boundary_tests
        COMMAND "${CMAKE_COMMAND}"
                "-DGREENFIELD_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
                -P "${PROJECT_SOURCE_DIR}/cmake/CheckDependencyBoundaries.cmake"
    )

    add_test(
        NAME greenfield_cpp_cmake_app_template_tests
        COMMAND "${CMAKE_COMMAND}"
                "-DGREENFIELD_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
                "-DGREENFIELD_BINARY_DIR=${PROJECT_BINARY_DIR}"
                "-DGREENFIELD_CMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
                "-DGREENFIELD_ALLOW_SYSTEM_DEPENDENCIES=${GREENFIELD_ALLOW_SYSTEM_DEPENDENCIES}"
                "-DGREENFIELD_BUILD_PROFILE=${GREENFIELD_BUILD_PROFILE}"
                -P "${PROJECT_SOURCE_DIR}/cmake/CheckCppCmakeAppTemplate.cmake"
    )

    add_test(
        NAME greenfield_source_tree_fast2d_consumer_tests
        COMMAND "${CMAKE_COMMAND}"
                "-DGREENFIELD_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
                "-DGREENFIELD_BINARY_DIR=${PROJECT_BINARY_DIR}"
                "-DGREENFIELD_CMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
                "-DGREENFIELD_ALLOW_SYSTEM_DEPENDENCIES=${GREENFIELD_ALLOW_SYSTEM_DEPENDENCIES}"
                "-DGREENFIELD_BUILD_PROFILE=${GREENFIELD_BUILD_PROFILE}"
                -P "${PROJECT_SOURCE_DIR}/cmake/CheckSourceTreeFast2DConsumer.cmake"
    )

    function(greenfield_add_test test_name source_file)
        add_executable(${test_name}
            ${source_file}
        )
        target_include_directories(${test_name} PRIVATE "${PROJECT_SOURCE_DIR}")
        target_link_libraries(${test_name} PRIVATE greenfield_core greenfield_render greenfield_ui)
        greenfield_enable_warnings(${test_name})
        add_test(NAME ${test_name} COMMAND ${test_name})
    endfunction()

    greenfield_add_test(greenfield_core_tests tests/core_tests.cpp)
    greenfield_add_test(greenfield_render_command_tests tests/render_command_tests.cpp)
    greenfield_add_test(greenfield_renderer_backend_kind_tests tests/renderer_backend_kind_tests.cpp)
    greenfield_add_test(greenfield_layout_tests tests/layout_tests.cpp)
    greenfield_add_test(greenfield_ui_tests tests/ui_tests.cpp)

    add_executable(greenfield_viewport2d_tests
        tests/viewport2d_tests.cpp
    )
    target_include_directories(greenfield_viewport2d_tests PRIVATE "${PROJECT_SOURCE_DIR}")
    target_link_libraries(greenfield_viewport2d_tests PRIVATE greenfield_core greenfield_visualization)
    greenfield_enable_warnings(greenfield_viewport2d_tests)
    add_test(NAME greenfield_viewport2d_tests COMMAND greenfield_viewport2d_tests)

    add_executable(greenfield_fast2d_renderer_tests
        tests/fast2d_renderer_tests.cpp
    )
    target_include_directories(greenfield_fast2d_renderer_tests PRIVATE "${PROJECT_SOURCE_DIR}")
    target_link_libraries(greenfield_fast2d_renderer_tests PRIVATE greenfield_core greenfield_render
                                                                    greenfield_render_fast2d)
    greenfield_enable_warnings(greenfield_fast2d_renderer_tests)
    add_test(NAME greenfield_fast2d_renderer_tests COMMAND greenfield_fast2d_renderer_tests)

    if(TARGET greenfield_sdl_platform)
        add_executable(greenfield_sdl_raster_presenter_tests
            tests/sdl_raster_presenter_tests.cpp
        )
        target_include_directories(greenfield_sdl_raster_presenter_tests PRIVATE "${PROJECT_SOURCE_DIR}")
        target_link_libraries(greenfield_sdl_raster_presenter_tests PRIVATE greenfield_core greenfield_sdl_platform)
        greenfield_enable_warnings(greenfield_sdl_raster_presenter_tests)
        add_test(NAME greenfield_sdl_raster_presenter_tests COMMAND greenfield_sdl_raster_presenter_tests)
    endif()
endif()
