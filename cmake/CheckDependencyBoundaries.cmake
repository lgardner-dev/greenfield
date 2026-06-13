if(NOT DEFINED GREENFIELD_SOURCE_DIR)
    message(FATAL_ERROR "GREENFIELD_SOURCE_DIR is required.")
endif()

set(_guarded_files)
set(_guarded_roots
    engine/core
    engine/input
    engine/render/fast2d
    engine/ui
)

foreach(_guarded_root IN LISTS _guarded_roots)
    file(GLOB_RECURSE _root_files
        "${GREENFIELD_SOURCE_DIR}/${_guarded_root}/*.cpp"
        "${GREENFIELD_SOURCE_DIR}/${_guarded_root}/*.h"
        "${GREENFIELD_SOURCE_DIR}/${_guarded_root}/*.hpp"
    )
    list(APPEND _guarded_files ${_root_files})
endforeach()

file(GLOB _render_abstraction_files
    "${GREENFIELD_SOURCE_DIR}/engine/render/*.cpp"
    "${GREENFIELD_SOURCE_DIR}/engine/render/*.h"
    "${GREENFIELD_SOURCE_DIR}/engine/render/*.hpp"
)
list(APPEND _guarded_files ${_render_abstraction_files})

list(APPEND _guarded_files
    "${GREENFIELD_SOURCE_DIR}/engine/platform/INativeSurfaceProvider.h"
    "${GREENFIELD_SOURCE_DIR}/engine/platform/IWindow.h"
)

set(_forbidden_include_pattern "^[ \t]*#[ \t]*include[ \t]*[<\"][^>\"]*(SDL|SDL3|SDL\\.h|dawn|webgpu|wgpu|ft2build\\.h|freetype|FT_)")
set(_violations)

foreach(_guarded_file IN LISTS _guarded_files)
    if(NOT EXISTS "${_guarded_file}")
        continue()
    endif()

    file(STRINGS "${_guarded_file}" _matching_lines REGEX "${_forbidden_include_pattern}")
    foreach(_matching_line IN LISTS _matching_lines)
        list(APPEND _violations "${_guarded_file}: ${_matching_line}")
    endforeach()
endforeach()

if(_violations)
    list(JOIN _violations "\n" _violation_text)
    message(FATAL_ERROR "Forbidden concrete dependency include found in SDK boundary files:\n${_violation_text}")
endif()
