cmake_minimum_required(VERSION 3.24)

project(GreenfieldCppCmakeApp LANGUAGES CXX)

if(NOT DEFINED GREENFIELD_SOURCE_DIR)
    message(FATAL_ERROR "GREENFIELD_SOURCE_DIR is required for source-tree Greenfield consumption.")
endif()

if(NOT EXISTS "${GREENFIELD_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "GREENFIELD_SOURCE_DIR does not point at a Greenfield source tree: ${GREENFIELD_SOURCE_DIR}")
endif()

add_subdirectory("${GREENFIELD_SOURCE_DIR}" greenfield-build EXCLUDE_FROM_ALL)

add_executable(greenfield_template_app
    src/main.cpp
)

target_compile_features(greenfield_template_app PRIVATE cxx_std_20)

target_link_libraries(greenfield_template_app
    PRIVATE
        greenfield_core
        greenfield_render
        greenfield_ui
        greenfield_render_fast2d
)
