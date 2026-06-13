# Greenfield

Greenfield is an open-source, C++20, SDK-first creative application engine.

The Greenfield SDK is the reusable runtime and library that developers use to build creative applications. This repository is a monorepo for SDK/runtime code, future Greenfield Studio work, examples and sandbox apps, tools, docs, and tests.

![Greenfield Control Room sandbox](docs/images/greenfield-control-room.png)

## Current Scope

- SDL3 windowing and input through a small platform layer
- Renderer-agnostic render commands that keep UI code independent from backend details
- A current Dawn/WebGPU accelerated backend with clean ownership boundaries
- A narrow Fast2D renderer backend foundation for renderer-neutral command consumption
- UI widget, layout, input, text, and render command basics
- Minimal SDK surface identity, root UI surface participation, and point-to-surface input routing
- CMake with Ninja presets
- vcpkg manifest-mode as the default dependency path

## Direction

- `greenfield_render_fast2d` exists as a sibling backend target. It consumes renderer-neutral `RenderCommandList` commands, prepares backend-local fill operations, rasterizes deterministic plain filled rectangles with clipping into a CPU raster target, and defers text rasterization.
- Fast2D preserves optional shape styling metadata such as corner radius, border color, and border thickness for later backend work, but the current CPU raster path draws only plain rectangle fills.
- Fast2D is not wired as the default sandbox renderer.
- `greenfield_render_webgpu` is the current real WebGPU backend target, and `greenfield_webgpu` remains a compatibility alias.
- Dawn/WebGPU is the current implemented accelerated backend and should remain backend-specific in the architecture direction.
- The current default build still requires Dawn/WebGPU and FreeType because the sandbox still uses the WebGPU renderer.
- Skia may be considered later as an optional renderer/backend, but it is not the initial foundation.
- Greenfield Studio is a future IDE/editor built on top of the SDK, not part of current M3 implementation work.
- Greenfield CLI is future tooling, not part of current M3 implementation work.
- Development can be Linux-first for v0.1 work, while preserving Linux, Windows, and browser-hosted WebAssembly as v0.1 release/export architecture considerations.
- Exported Greenfield apps should be C++/CMake-based first.
- Hot reload is not a core v0.1 requirement; fast incremental build UX matters more.

## Not In Scope Yet

The current M3 renderer foundation is intentionally minimal. Renderer selection/composition, full text/font sharing, richer 2D shape rasterization, rounded corners, borders, antialiasing, platform presentation, mixed-surface composition, Studio implementation, CLI implementation, Canvas2D, Scene3D, shader/editor surfaces, node graphs, a compositor, retained-mode UI, hot reload, Python bindings, and Skia integration are not in scope yet.

## Build

```bash
make bootstrap
make run
```

`make bootstrap` uses `VCPKG_ROOT` when it is already set. Otherwise it clones vcpkg into `.tools/vcpkg`, bootstraps it, and configures the `dev` preset.

System-installed dependencies are only used when `GREENFIELD_ALLOW_SYSTEM_DEPENDENCIES=ON` is passed explicitly.

Direct CMake usage is also supported:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

## Run

```bash
./build/dev/bin/greenfield_sandbox
```

## Developer Commands

```bash
make bootstrap
make build
make run
make test
make clean
make format
```
