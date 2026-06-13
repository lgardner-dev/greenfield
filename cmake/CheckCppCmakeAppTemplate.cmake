if(NOT DEFINED GREENFIELD_SOURCE_DIR)
    message(FATAL_ERROR "GREENFIELD_SOURCE_DIR is required.")
endif()

if(NOT DEFINED GREENFIELD_BINARY_DIR)
    message(FATAL_ERROR "GREENFIELD_BINARY_DIR is required.")
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

set(_required_readme_fragments
    "not a project generator"
    "root Greenfield build does not include this template automatically"
    "C++/CMake-first app project"
    "Separate from `apps/sandbox`"
    "Consumes Greenfield SDK/runtime targets"
    "Linux as the current primary development path"
    "Windows and browser-hosted WebAssembly as future target/export considerations"
    "M5 does not implement a CLI"
    "browser-hosted WebAssembly support"
)

foreach(_required_fragment IN LISTS _required_readme_fragments)
    string(FIND "${_template_readme}" "${_required_fragment}" _fragment_position)
    if(_fragment_position EQUAL -1)
        message(FATAL_ERROR "Template README.md is missing expected guardrail language: ${_required_fragment}")
    endif()
endforeach()

set(_required_cmake_fragments
    "project(GreenfieldCppCmakeApp LANGUAGES CXX)"
    "add_executable(greenfield_template_app"
    "target_compile_features(greenfield_template_app PRIVATE cxx_std_20)"
    "Greenfield SDK/runtime targets are not available"
    "not a standalone build"
    "greenfield_core"
    "greenfield_render"
    "greenfield_ui"
    "greenfield_sdl_platform"
    "greenfield_render_webgpu"
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

set(_forbidden_include_pattern "^[ \t]*#[ \t]*include[ \t]*[<\"][^>\"]*(SDL|SDL3|SDL\\.h|dawn|webgpu|wgpu|ft2build\\.h|freetype|FT_)")
file(STRINGS "${_template_dir}/src/main.cpp" _forbidden_includes REGEX "${_forbidden_include_pattern}")
if(_forbidden_includes)
    list(JOIN _forbidden_includes "\n" _forbidden_include_text)
    message(FATAL_ERROR "Template main.cpp includes concrete platform/render/font dependencies:\n${_forbidden_include_text}")
endif()

set(_template_check_build_dir "${GREENFIELD_BINARY_DIR}/template-cpp-cmake-app-check")
file(REMOVE_RECURSE "${_template_check_build_dir}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${_template_dir}" -B "${_template_check_build_dir}"
    RESULT_VARIABLE _template_configure_result
    OUTPUT_VARIABLE _template_configure_output
    ERROR_VARIABLE _template_configure_error
)

if(_template_configure_result EQUAL 0)
    message(FATAL_ERROR "Template unexpectedly configured as a standalone project.")
endif()

set(_template_configure_text "${_template_configure_output}\n${_template_configure_error}")
string(FIND "${_template_configure_text}" "Greenfield SDK/runtime targets are not available" _missing_targets_position)
string(FIND "${_template_configure_text}" "not a standalone build" _standalone_guardrail_position)

if(_missing_targets_position EQUAL -1 OR _standalone_guardrail_position EQUAL -1)
    message(FATAL_ERROR
        "Template standalone configure failed for an unexpected reason:\n${_template_configure_text}"
    )
endif()
