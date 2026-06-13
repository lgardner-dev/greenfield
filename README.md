# Greenfield

Greenfield is an open-source, C++20, SDK-first creative application engine.

The Greenfield SDK is the reusable runtime and library that developers use to build creative applications. This repository is a monorepo for SDK/runtime code, future Greenfield Studio work, examples and sandbox apps, tools, docs, and tests.

![Greenfield Control Room sandbox](docs/images/greenfield-control-room.png)

## Current Scope

- SDL3 windowing and input through a small platform layer
- Renderer-agnostic render commands that keep UI code independent from backend details
- A current Dawn/WebGPU accelerated backend with clean ownership boundaries
- UI widget, layout, input, text, and render command basics
- CMake with Ninja presets
- vcpkg manifest-mode as the default dependency path

## Direction

- Fast2D is the intended future default baseline renderer, but it is not implemented in M0.
- Dawn/WebGPU is the current implemented accelerated backend and should remain backend-specific and optional in the architecture direction.
- Skia may be considered later as an optional renderer/backend, but it is not the initial foundation.
- Greenfield Studio is a future IDE/editor built on top of the SDK, not part of M0 implementation work.
- Greenfield CLI is future tooling, not part of M0 implementation work.
- Development can be Linux-first for v0.1 work, while preserving Linux, Windows, and browser-hosted WebAssembly as v0.1 release/export architecture considerations.
- Exported Greenfield apps should be C++/CMake-based first.
- Hot reload is not a core v0.1 requirement; fast incremental build UX matters more.

## Not In Scope Yet

M0 is documentation and public positioning only. Studio implementation, CLI implementation, Canvas2D, Scene3D, shader/editor surfaces, hot reload, Python bindings, Skia integration, and Fast2D implementation are not in scope yet.

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
