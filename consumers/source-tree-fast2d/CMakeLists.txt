cmake_minimum_required(VERSION 3.24)

project(GreenfieldSourceTreeFast2DConsumer LANGUAGES CXX)

if(NOT DEFINED GREENFIELD_SOURCE_DIR)
    message(FATAL_ERROR "GREENFIELD_SOURCE_DIR is required for source-tree Greenfield consumption.")
endif()

if(NOT EXISTS "${GREENFIELD_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "GREENFIELD_SOURCE_DIR does not point at a Greenfield source tree: ${GREENFIELD_SOURCE_DIR}")
endif()

if(NOT EXISTS "${GREENFIELD_SOURCE_DIR}/engine/ui/UiContext.h")
    message(FATAL_ERROR "GREENFIELD_SOURCE_DIR is missing expected Greenfield SDK UI headers: ${GREENFIELD_SOURCE_DIR}")
endif()

add_subdirectory("${GREENFIELD_SOURCE_DIR}" greenfield-build EXCLUDE_FROM_ALL)

add_executable(greenfield_source_tree_fast2d_consumer
    src/main.cpp
)

target_compile_features(greenfield_source_tree_fast2d_consumer PRIVATE cxx_std_20)

target_link_libraries(greenfield_source_tree_fast2d_consumer
    PRIVATE
        greenfield_ui
        greenfield_render_fast2d
)
