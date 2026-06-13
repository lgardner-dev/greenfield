cmake_minimum_required(VERSION 3.24)

project(GreenfieldCppCmakeApp LANGUAGES CXX)

add_executable(greenfield_template_app
    src/main.cpp
)

target_compile_features(greenfield_template_app PRIVATE cxx_std_20)

# M5 does not install, package, export, or generate Greenfield SDK targets yet.
# A future exported workspace, vendored SDK checkout, or package/export mechanism
# should make these targets available before this template target is linked.
if(NOT TARGET greenfield_core OR NOT TARGET greenfield_render OR NOT TARGET greenfield_ui)
    message(FATAL_ERROR
        "Greenfield SDK/runtime targets are not available. "
        "This template is a scaffold for a future exported workspace, not a standalone build."
    )
endif()

target_link_libraries(greenfield_template_app
    PRIVATE
        greenfield_core
        greenfield_render
        greenfield_ui
)

# The app owns concrete composition-root policy. A future generated or hand-authored
# app may link platform and renderer targets such as these when the chosen target
# workspace provides them:
#
# target_link_libraries(greenfield_template_app
#     PRIVATE
#         greenfield_sdl_platform
#         greenfield_render_webgpu
# )
