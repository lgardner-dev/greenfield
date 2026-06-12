cmake_minimum_required(VERSION 3.24)

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

add_library(greenfield_core INTERFACE)
target_include_directories(greenfield_core INTERFACE "${PROJECT_SOURCE_DIR}")
target_compile_features(greenfield_core INTERFACE cxx_std_20)

add_library(greenfield_render INTERFACE)
target_include_directories(greenfield_render INTERFACE "${PROJECT_SOURCE_DIR}")
target_link_libraries(greenfield_render INTERFACE greenfield_core)

add_library(greenfield_ui
    engine/ui/UiContext.cpp
)
target_include_directories(greenfield_ui PUBLIC "${PROJECT_SOURCE_DIR}")
target_link_libraries(greenfield_ui PUBLIC greenfield_core greenfield_render)
greenfield_enable_warnings(greenfield_ui)

add_library(greenfield_platform
    engine/platform/SdlStartupPresenter.cpp
    engine/platform/SdlWindow.cpp
)
target_include_directories(greenfield_platform PUBLIC "${PROJECT_SOURCE_DIR}")
target_link_libraries(greenfield_platform PUBLIC greenfield_core "${GREENFIELD_SDL3_TARGET}")
greenfield_enable_warnings(greenfield_platform)

add_library(greenfield_webgpu
    engine/render/webgpu/WebGpuContext.cpp
    engine/render/webgpu/WebGpuRenderer.cpp
)
target_include_directories(greenfield_webgpu PUBLIC "${PROJECT_SOURCE_DIR}")
target_link_libraries(greenfield_webgpu PUBLIC greenfield_core greenfield_render greenfield_platform dawn::webgpu_dawn Freetype::Freetype)
greenfield_enable_warnings(greenfield_webgpu)

add_executable(greenfield_sandbox
    apps/sandbox/main.cpp
)
target_include_directories(greenfield_sandbox PRIVATE "${PROJECT_SOURCE_DIR}")
target_link_libraries(greenfield_sandbox PRIVATE greenfield_core greenfield_render greenfield_ui greenfield_platform greenfield_webgpu)
greenfield_enable_warnings(greenfield_sandbox)

if(BUILD_TESTING)
    add_executable(greenfield_core_smoke
        tests/core_smoke.cpp
    )
    target_include_directories(greenfield_core_smoke PRIVATE "${PROJECT_SOURCE_DIR}")
    target_link_libraries(greenfield_core_smoke PRIVATE greenfield_core greenfield_render greenfield_ui)
    greenfield_enable_warnings(greenfield_core_smoke)

    add_test(NAME greenfield_core_smoke COMMAND greenfield_core_smoke)
endif()
