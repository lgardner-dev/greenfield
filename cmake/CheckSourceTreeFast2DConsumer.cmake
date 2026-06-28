if(NOT DEFINED GREENFIELD_SOURCE_DIR)
    message(FATAL_ERROR "GREENFIELD_SOURCE_DIR is required.")
endif()

if(NOT DEFINED GREENFIELD_BINARY_DIR)
    message(FATAL_ERROR "GREENFIELD_BINARY_DIR is required.")
endif()

if(NOT DEFINED GREENFIELD_BUILD_PROFILE)
    set(GREENFIELD_BUILD_PROFILE developer)
endif()

set(_consumer_dir "${GREENFIELD_SOURCE_DIR}/consumers/source-tree-fast2d")
set(_consumer_build_dir "${GREENFIELD_BINARY_DIR}/source-tree-fast2d-consumer-check")
set(_consumer_executable "${_consumer_build_dir}/bin/greenfield_source_tree_fast2d_consumer")

set(_required_files
    "${_consumer_dir}/CMakeLists.txt"
    "${_consumer_dir}/src/main.cpp"
)

foreach(_required_file IN LISTS _required_files)
    if(NOT EXISTS "${_required_file}")
        message(FATAL_ERROR "Missing source-tree Fast2D consumer file: ${_required_file}")
    endif()
endforeach()

file(READ "${_consumer_dir}/CMakeLists.txt" _consumer_cmake)
file(READ "${_consumer_dir}/src/main.cpp" _consumer_main)

set(_required_cmake_fragments
    "project(GreenfieldSourceTreeFast2DConsumer LANGUAGES CXX)"
    "GREENFIELD_SOURCE_DIR is required"
    [[add_subdirectory("${GREENFIELD_SOURCE_DIR}" greenfield-build EXCLUDE_FROM_ALL)]]
    "greenfield_ui"
    "greenfield_render_fast2d"
)

foreach(_required_fragment IN LISTS _required_cmake_fragments)
    string(FIND "${_consumer_cmake}" "${_required_fragment}" _fragment_position)
    if(_fragment_position EQUAL -1)
        message(FATAL_ERROR "Source-tree Fast2D consumer CMakeLists.txt is missing expected contract language: ${_required_fragment}")
    endif()
endforeach()

set(_forbidden_consumer_fragments
    "apps/sandbox"
    "greenfield_sandbox"
    "greenfield_sdl_platform"
    "greenfield_render_webgpu"
    "find_package(Greenfield"
    "install("
    "package("
)

foreach(_forbidden_fragment IN LISTS _forbidden_consumer_fragments)
    string(FIND "${_consumer_cmake}" "${_forbidden_fragment}" _cmake_fragment_position)
    string(FIND "${_consumer_main}" "${_forbidden_fragment}" _main_fragment_position)
    if(NOT _cmake_fragment_position EQUAL -1 OR NOT _main_fragment_position EQUAL -1)
        message(FATAL_ERROR "Source-tree Fast2D consumer contains out-of-scope dependency or packaging behavior: ${_forbidden_fragment}")
    endif()
endforeach()

set(_forbidden_include_pattern "^[ \t]*#[ \t]*include[ \t]*[<\"][^>\"]*(SDL|SDL3|SDL\\.h|dawn|webgpu|wgpu|ft2build\\.h|freetype|FT_)")
file(STRINGS "${_consumer_dir}/src/main.cpp" _forbidden_includes REGEX "${_forbidden_include_pattern}")
if(_forbidden_includes)
    list(JOIN _forbidden_includes "\n" _forbidden_include_text)
    message(FATAL_ERROR "Source-tree Fast2D consumer includes concrete platform/render/font dependencies:\n${_forbidden_include_text}")
endif()

file(REMOVE_RECURSE "${_consumer_build_dir}")

set(_consumer_configure_command
    "${CMAKE_COMMAND}"
    -S "${_consumer_dir}"
    -B "${_consumer_build_dir}"
    "-DGREENFIELD_SOURCE_DIR=${GREENFIELD_SOURCE_DIR}"
    "-DGREENFIELD_BUILD_PROFILE=${GREENFIELD_BUILD_PROFILE}"
    "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=${_consumer_build_dir}/bin"
)

if(GREENFIELD_BUILD_PROFILE STREQUAL "developer" AND DEFINED GREENFIELD_CMAKE_TOOLCHAIN_FILE AND NOT "${GREENFIELD_CMAKE_TOOLCHAIN_FILE}" STREQUAL "")
    list(APPEND _consumer_configure_command "-DCMAKE_TOOLCHAIN_FILE=${GREENFIELD_CMAKE_TOOLCHAIN_FILE}")
endif()

if(GREENFIELD_BUILD_PROFILE STREQUAL "developer" AND EXISTS "${GREENFIELD_SOURCE_DIR}/vcpkg.json")
    list(APPEND _consumer_configure_command "-DVCPKG_MANIFEST_DIR=${GREENFIELD_SOURCE_DIR}")
endif()

if(DEFINED GREENFIELD_ALLOW_SYSTEM_DEPENDENCIES)
    list(APPEND _consumer_configure_command "-DGREENFIELD_ALLOW_SYSTEM_DEPENDENCIES=${GREENFIELD_ALLOW_SYSTEM_DEPENDENCIES}")
endif()

execute_process(
    COMMAND ${_consumer_configure_command}
    RESULT_VARIABLE _consumer_configure_result
    OUTPUT_VARIABLE _consumer_configure_output
    ERROR_VARIABLE _consumer_configure_error
)

if(NOT _consumer_configure_result EQUAL 0)
    message(FATAL_ERROR
        "Source-tree Fast2D consumer configure failed:\n${_consumer_configure_output}\n${_consumer_configure_error}"
    )
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${_consumer_build_dir}" --target greenfield_source_tree_fast2d_consumer
    RESULT_VARIABLE _consumer_build_result
    OUTPUT_VARIABLE _consumer_build_output
    ERROR_VARIABLE _consumer_build_error
)

if(NOT _consumer_build_result EQUAL 0)
    message(FATAL_ERROR
        "Source-tree Fast2D consumer build failed:\n${_consumer_build_output}\n${_consumer_build_error}"
    )
endif()

execute_process(
    COMMAND "${_consumer_executable}"
    RESULT_VARIABLE _consumer_run_result
    OUTPUT_VARIABLE _consumer_run_output
    ERROR_VARIABLE _consumer_run_error
)

if(NOT _consumer_run_result EQUAL 0)
    message(FATAL_ERROR
        "Source-tree Fast2D consumer run failed:\n${_consumer_run_output}\n${_consumer_run_error}"
    )
endif()
