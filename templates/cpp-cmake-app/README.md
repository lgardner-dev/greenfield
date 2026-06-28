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
    -DVCPKG_MANIFEST_DIR=/path/to/greenfield
cmake --build build/template-cpp-cmake-app --target greenfield_template_app
./build/template-cpp-cmake-app/bin/greenfield_template_app
```

The current top-level Greenfield project still requires Dawn/WebGPU and FreeType during configure because those dependencies are part of the current source-tree target topology. This template does not make those dependencies optional and does not use WebGPU at runtime.

## Current Limits

M7B does not implement install rules, package exports, `find_package(Greenfield)`, package config files, a public bootstrap API, CLI tooling, project generation, Windows packaging, browser-hosted WebAssembly support, or optional Dawn/WebGPU/FreeType behavior.

This template is a source-tree consumer example. Future install/export packaging may replace or supplement this contract, but it does not exist yet.
