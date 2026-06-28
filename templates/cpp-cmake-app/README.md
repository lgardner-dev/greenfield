# Greenfield C++/CMake App Template

This is a minimal working source-tree consumer template for a Greenfield app project.

It is not a project generator, CLI export output, install package, package config, or standalone release workflow. The root Greenfield build does not include this template automatically.

## Intent

- C++/CMake-first app project.
- Separate from `apps/sandbox`.
- Owns its app target and composition-root policy.
- Uses `GREENFIELD_SOURCE_DIR` to locate a local Greenfield source checkout.
- Consumes Greenfield through source-tree `add_subdirectory`.
- Links supported SDK/runtime targets: `greenfield_core`, `greenfield_render`, and `greenfield_ui`.
- Links `greenfield_render_fast2d` as the concrete headless renderer backend for this tiny template app.
- Treats Linux as the current primary development path.
- Preserves Windows and browser-hosted WebAssembly as future target/export considerations.

## Source-Tree Contract

Configure this template by passing the Greenfield source checkout explicitly:

```bash
cmake -S templates/cpp-cmake-app -B build/template-cpp-cmake-app \
    -DGREENFIELD_SOURCE_DIR=/path/to/greenfield \
    -DCMAKE_TOOLCHAIN_FILE=/path/to/greenfield/cmake/vcpkg-toolchain.cmake \
    -DVCPKG_MANIFEST_DIR=/path/to/greenfield \
    -DGREENFIELD_BUILD_PROFILE=developer
cmake --build build/template-cpp-cmake-app --target greenfield_template_app
./build/template-cpp-cmake-app/bin/greenfield_template_app
```

For dependency-light Fast2D validation, configure with the headless profile and without the Greenfield vcpkg toolchain or manifest:

```bash
cmake -S templates/cpp-cmake-app -B build/template-cpp-cmake-app-headless \
    -DGREENFIELD_SOURCE_DIR=/path/to/greenfield \
    -DGREENFIELD_BUILD_PROFILE=headless-fast2d
cmake --build build/template-cpp-cmake-app-headless --target greenfield_template_app
./build/template-cpp-cmake-app-headless/bin/greenfield_template_app
```

`developer` is the default dependency-complete profile and keeps SDL3, Dawn/WebGPU, FreeType, the sandbox, and vcpkg-backed developer flow available. `headless-fast2d` is the dependency-light profile for this template's core/UI/Fast2D path.

## Current Limits

M7E does not implement install rules, package exports, `find_package(Greenfield)`, package config files, a public bootstrap API, CLI tooling, project generation, Windows packaging, browser-hosted WebAssembly support, or package-consumer dependency discovery.

This template is a source-tree consumer example. Future install/export packaging may replace or supplement this contract, but it does not exist yet.
