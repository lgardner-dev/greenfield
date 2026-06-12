# Greenfield

Greenfield is a UI-first C++20 engine skeleton for desktop applications.

## Current Scope

- SDL3 windowing and input
- A renderer-agnostic UI command layer
- A WebGPU renderer stub with clean ownership boundaries
- CMake with Ninja presets
- Optional vcpkg manifest-mode integration

## Build

```bash
cmake --preset dev
cmake --build --preset dev
```

If `VCPKG_ROOT` is set, the preset toolchain wrapper will load vcpkg automatically in manifest mode.
If vcpkg is not available yet, the current skeleton falls back to the system SDL3 package found via `pkg-config`.

## Run

```bash
./build/dev/bin/greenfield_sandbox
```

## Developer Commands

```bash
make bootstrap
make configure
make build
make run
make test
make clean
make format
```
