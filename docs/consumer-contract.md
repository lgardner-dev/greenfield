# Greenfield Source-Tree Consumer Contract

M7B supports source-tree consumption as the current app-consumer contract. This is not install/export packaging.

## Supported Source-Tree Model

A consumer project receives an explicit `GREENFIELD_SOURCE_DIR` CMake cache variable that points at a local Greenfield checkout. The consumer validates that path and integrates Greenfield with:

```cmake
add_subdirectory("${GREENFIELD_SOURCE_DIR}" greenfield-build EXCLUDE_FROM_ALL)
```

The consumer then links Greenfield CMake targets directly. This source-tree model is validated by `templates/cpp-cmake-app` and `consumers/source-tree-fast2d`.

Current consumer configuration should also use the Greenfield toolchain and manifest when using the default vcpkg dependency path:

```bash
cmake -S <consumer-source> -B <consumer-build> \
    -DGREENFIELD_SOURCE_DIR=/path/to/greenfield \
    -DCMAKE_TOOLCHAIN_FILE=/path/to/greenfield/cmake/vcpkg-toolchain.cmake \
    -DVCPKG_MANIFEST_DIR=/path/to/greenfield
```

The current top-level Greenfield project still requires SDL3, Dawn/WebGPU, and FreeType during configure because those dependencies are part of the current target topology. M7B does not make those dependencies optional.

## Supported Target Categories

SDK/runtime consumer-facing targets:

- `greenfield_core`: core value types.
- `greenfield_render`: renderer-neutral interfaces and render commands.
- `greenfield_ui`: immediate-mode UI runtime.
- `greenfield_platform`: platform abstractions for consumers that own a platform provider or renderer needing native surface handles.

Concrete composition-root dependency targets:

- `greenfield_render_fast2d`: concrete CPU renderer backend for deterministic/headless or explicit Fast2D composition roots.
- `greenfield_render_webgpu`: concrete Dawn/WebGPU renderer backend.
- `greenfield_webgpu`: compatibility alias for `greenfield_render_webgpu`.
- `greenfield_sdl_platform`: concrete SDL platform and presentation implementation.

Demo/internal-only targets:

- `greenfield_sandbox`: demo composition root under `apps/sandbox`; consumers must not link it or depend on sandbox code.

## Header-Surface Posture

The repository currently exposes broad project-root include paths through its CMake targets. M7B documents intended target and layer categories, but it does not reorganize headers, add include-prefix migration, or create a curated public include tree.

Consumer-facing headers today are the headers reached through the supported SDK/runtime targets and, when intentionally linked by a composition root, the selected concrete backend/platform target. Headers under `apps/sandbox` are not consumer-facing.

## Template And Consumer Validation

- `templates/cpp-cmake-app` is now a working source-tree consumer template. It links SDK/runtime targets plus `greenfield_render_fast2d` as its concrete headless renderer choice.
- `consumers/source-tree-fast2d` is an in-tree external-style validation consumer. It remains separate from `apps/sandbox` and performs deterministic Fast2D frame validation.
- CTest configures, builds, and runs both projects in separate build trees.

## Deferred Packaging Work

M7B intentionally defers:

- install/export rules;
- CMake export sets;
- `find_package(Greenfield)`;
- package config files;
- package publishing;
- public app-bootstrap APIs;
- optional Dawn/WebGPU/FreeType behavior;
- visible SDL/WebGPU consumer examples;
- CLI tooling and project generation.
