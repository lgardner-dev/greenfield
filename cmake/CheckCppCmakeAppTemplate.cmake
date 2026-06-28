if(NOT DEFINED GREENFIELD_SOURCE_DIR)
    message(FATAL_ERROR "GREENFIELD_SOURCE_DIR is required.")
endif()

if(NOT DEFINED GREENFIELD_BINARY_DIR)
    message(FATAL_ERROR "GREENFIELD_BINARY_DIR is required.")
endif()

if(NOT DEFINED GREENFIELD_BUILD_PROFILE)
    set(GREENFIELD_BUILD_PROFILE developer)
endif()

set(_template_dir "${GREENFIELD_SOURCE_DIR}/templates/cpp-cmake-app")
set(_required_files
    "${_template_dir}/README.md"
    "${_template_dir}/CMakeLists.txt"
    "${_template_dir}/src/main.cpp"
)

foreach(_required_file IN LISTS _required_files)
    if(NOT EXISTS "${_required_file}")
        message(FATAL_ERROR "Missing C++/CMake app-template scaffold file: ${_required_file}")
    endif()
endforeach()

file(READ "${_template_dir}/README.md" _template_readme)
file(READ "${_template_dir}/CMakeLists.txt" _template_cmake)
file(READ "${_template_dir}/src/main.cpp" _template_main)

set(_required_readme_fragments
    "not a project generator"
    "root Greenfield build does not include this template automatically"
    "C++/CMake-first app project"
    "Separate from `apps/sandbox`"
    "GREENFIELD_SOURCE_DIR"
    "add_subdirectory"
    "greenfield_core"
    "greenfield_render"
    "greenfield_ui"
    "greenfield_render_fast2d"
    "Linux as the current primary development path"
    "Windows and browser-hosted WebAssembly as future target/export considerations"
    "does not implement install rules"
    "find_package(Greenfield)"
)

foreach(_required_fragment IN LISTS _required_readme_fragments)
    string(FIND "${_template_readme}" "${_required_fragment}" _fragment_position)
    if(_fragment_position EQUAL -1)
        message(FATAL_ERROR "Template README.md is missing expected guardrail language: ${_required_fragment}")
    endif()
endforeach()

set(_required_cmake_fragments
    "project(GreenfieldCppCmakeApp LANGUAGES CXX)"
    "GREENFIELD_SOURCE_DIR is required"
    [[add_subdirectory("${GREENFIELD_SOURCE_DIR}" greenfield-build EXCLUDE_FROM_ALL)]]
    "add_executable(greenfield_template_app"
    "target_compile_features(greenfield_template_app PRIVATE cxx_std_20)"
    "greenfield_core"
    "greenfield_render"
    "greenfield_ui"
    "greenfield_render_fast2d"
)

foreach(_required_fragment IN LISTS _required_cmake_fragments)
    string(FIND "${_template_cmake}" "${_required_fragment}" _fragment_position)
    if(_fragment_position EQUAL -1)
        message(FATAL_ERROR "Template CMakeLists.txt is missing expected scaffold language: ${_required_fragment}")
    endif()
endforeach()

set(_forbidden_template_patterns
    "install\\("
    "package\\("
    "CPack"
    "Emscripten"
    "WASM"
    "wasm32"
)

foreach(_forbidden_pattern IN LISTS _forbidden_template_patterns)
    string(REGEX MATCH "${_forbidden_pattern}" _forbidden_match "${_template_cmake}")
    if(_forbidden_match)
        message(FATAL_ERROR "Template CMakeLists.txt contains out-of-scope export/platform behavior: ${_forbidden_match}")
    endif()
endforeach()

set(_forbidden_template_fragments
    "apps/sandbox"
    "greenfield_sandbox"
    "greenfield_sdl_platform"
    "greenfield_render_webgpu"
    "find_package(Greenfield"
)

foreach(_forbidden_fragment IN LISTS _forbidden_template_fragments)
    string(FIND "${_template_cmake}" "${_forbidden_fragment}" _cmake_fragment_position)
    string(FIND "${_template_main}" "${_forbidden_fragment}" _main_fragment_position)
    if(NOT _cmake_fragment_position EQUAL -1 OR NOT _main_fragment_position EQUAL -1)
        message(FATAL_ERROR "Template contains out-of-scope dependency or packaging behavior: ${_forbidden_fragment}")
    endif()
endforeach()

set(_forbidden_include_pattern "^[ \t]*#[ \t]*include[ \t]*[<\"][^>\"]*(SDL|SDL3|SDL\\.h|dawn|webgpu|wgpu|ft2build\\.h|freetype|FT_)")
file(STRINGS "${_template_dir}/src/main.cpp" _forbidden_includes REGEX "${_forbidden_include_pattern}")
if(_forbidden_includes)
    list(JOIN _forbidden_includes "\n" _forbidden_include_text)
    message(FATAL_ERROR "Template main.cpp includes concrete platform/render/font dependencies:\n${_forbidden_include_text}")
endif()

set(_template_check_build_dir "${GREENFIELD_BINARY_DIR}/template-cpp-cmake-app-check")
set(_template_executable "${_template_check_build_dir}/bin/greenfield_template_app")
file(REMOVE_RECURSE "${_template_check_build_dir}")

set(_template_configure_command
    "${CMAKE_COMMAND}"
    -S "${_template_dir}"
    -B "${_template_check_build_dir}"
    "-DGREENFIELD_SOURCE_DIR=${GREENFIELD_SOURCE_DIR}"
    "-DGREENFIELD_BUILD_PROFILE=${GREENFIELD_BUILD_PROFILE}"
    "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=${_template_check_build_dir}/bin"
)

if(GREENFIELD_BUILD_PROFILE STREQUAL "developer" AND DEFINED GREENFIELD_CMAKE_TOOLCHAIN_FILE AND NOT "${GREENFIELD_CMAKE_TOOLCHAIN_FILE}" STREQUAL "")
    list(APPEND _template_configure_command "-DCMAKE_TOOLCHAIN_FILE=${GREENFIELD_CMAKE_TOOLCHAIN_FILE}")
endif()

if(GREENFIELD_BUILD_PROFILE STREQUAL "developer" AND EXISTS "${GREENFIELD_SOURCE_DIR}/vcpkg.json")
    list(APPEND _template_configure_command "-DVCPKG_MANIFEST_DIR=${GREENFIELD_SOURCE_DIR}")
endif()

if(DEFINED GREENFIELD_ALLOW_SYSTEM_DEPENDENCIES)
    list(APPEND _template_configure_command "-DGREENFIELD_ALLOW_SYSTEM_DEPENDENCIES=${GREENFIELD_ALLOW_SYSTEM_DEPENDENCIES}")
endif()

execute_process(
    COMMAND ${_template_configure_command}
    RESULT_VARIABLE _template_configure_result
    OUTPUT_VARIABLE _template_configure_output
    ERROR_VARIABLE _template_configure_error
)

if(NOT _template_configure_result EQUAL 0)
    message(FATAL_ERROR
        "Template source-tree configure failed:\n${_template_configure_output}\n${_template_configure_error}"
    )
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${_template_check_build_dir}" --target greenfield_template_app
    RESULT_VARIABLE _template_build_result
    OUTPUT_VARIABLE _template_build_output
    ERROR_VARIABLE _template_build_error
)

if(NOT _template_build_result EQUAL 0)
    message(FATAL_ERROR
        "Template source-tree build failed:\n${_template_build_output}\n${_template_build_error}"
    )
endif()

execute_process(
    COMMAND "${_template_executable}"
    RESULT_VARIABLE _template_run_result
    OUTPUT_VARIABLE _template_run_output
    ERROR_VARIABLE _template_run_error
)

if(NOT _template_run_result EQUAL 0)
    message(FATAL_ERROR
        "Template source-tree run failed:\n${_template_run_output}\n${_template_run_error}"
    )
endif()
