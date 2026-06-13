# Greenfield Roadmap

Greenfield is an open-source, C++20, SDK-first creative application engine.

## Project Identity

- Greenfield SDK: reusable runtime and library developers build applications with.
- Greenfield Studio: future IDE/editor built on top of the SDK.
- Greenfield CLI: future tooling for creating, building, running, and exporting projects.

## M0

M0 is documentation, doctrine, public positioning, and roadmap alignment.

M0 should avoid major implementation work.

## M2

M2 is the minimal Surface and Interaction Tree foundation.

Current M2 work includes SDK-level surface identity and bounds, root immediate UI surface participation, and minimal point-to-surface input routing. It does not include a compositor, retained-mode UI tree, Canvas2D, Scene3D, shader/editor tools, node graphs, Studio, CLI, or new renderer backends.

## v0.1 Direction

- C++20-first authoring.
- Linux-first development is acceptable.
- v0.1 release/export awareness includes Linux, Windows, and browser-hosted WebAssembly.
- Exported apps should be C++/CMake-based first.
- Fast incremental build UX matters more than hot reload.
- Browser-hosted WASM is required for exported apps eventually, but it should not be implemented in the current M2 foundation unless explicitly requested.

## Renderer Roadmap Posture

- Current implemented backend: Dawn/WebGPU.
- Current default build requirement: Dawn/WebGPU and FreeType remain required because WebGPU is the only implemented real renderer backend.
- Future default baseline direction: Greenfield-owned Fast2D.
- WebGPU: backend-specific accelerated renderer direction.
- Skia: possible later optional backend, not the current foundation.

## Not In Scope Yet

- Fast2D implementation
- Greenfield Studio implementation
- Greenfield CLI implementation
- Canvas2D
- Scene3D
- shader/editor tools
- node graphs
- hot reload
- Python bindings
- Skia integration
- WASM implementation work
