# Greenfield

Greenfield is a UI-first C++20 engine skeleton for desktop applications.

## Current Scope

- SDL3 windowing and input
- A renderer-agnostic UI command layer
- A WebGPU renderer stub with clean ownership boundaries
- CMake with Ninja presets
- vcpkg manifest-mode as the default dependency path

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
