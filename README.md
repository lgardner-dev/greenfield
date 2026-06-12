# Greenfield

Greenfield is an open-source, general-purpose C++20 UI-first engine for polished cross-platform application UIs.

The engine is built around WebGPU-first rendering, an SDL3 platform layer, and renderer-agnostic UI commands so applications can keep UI code independent from backend details.

## Current Scope

- SDL3 windowing and input through a small platform layer
- A renderer-agnostic UI command layer
- WebGPU-first rendering with clean ownership boundaries
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
