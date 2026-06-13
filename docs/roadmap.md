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

M2 includes SDK-level surface identity and bounds, root immediate UI surface participation, and minimal point-to-surface input routing. It does not include a compositor, retained-mode UI tree, Canvas2D, Scene3D, shader/editor tools, node graphs, Studio, or CLI.

## M3

M3 is the narrow Fast2D renderer foundation.

Current M3 work includes the `greenfield_render_fast2d` backend target, renderer-neutral `RenderCommandList` consumption, backend-local fill preparation, clip stack handling, clip underflow tracking, deferred text tracking, and minimal CPU-side filled-rectangle rasterization with clipping. Fast2D preserves optional shape styling metadata for later backend work, but the current CPU raster path draws only deterministic plain rectangle fills.

Fast2D is not production-ready and is not wired as the default sandbox renderer. Full renderer composition, full text/font sharing, richer 2D shape rasterization, rounded corners, borders, antialiasing, visible platform presentation, mixed-surface composition, Canvas2D, Scene3D, shader tooling, Studio, and CLI remain future work.

## M4

M4 is the narrow renderer-selection reality check.

Current M4 work includes renderer-neutral `RendererBackendKind` vocabulary, sandbox `--renderer=webgpu` and `--renderer=fast2d` parsing, and documentation of the current Fast2D presentation boundary. WebGPU remains the default interactive sandbox renderer. Fast2D is selectable only as an opt-in diagnostic/headless path: it runs one Control Room frame, consumes the UI `RenderCommandList`, rasterizes currently supported CPU filled-rectangle behavior, defers/counts text, reports diagnostics, and exits without visible presentation.

M4 does not implement a compositor or mixed-surface composition. Visible Fast2D presentation requires a future CPU raster presenter, SDL upload seam, or equivalent platform presentation decision.

## v0.1 Direction

- C++20-first authoring.
- Linux-first development is acceptable.
- v0.1 release/export awareness includes Linux, Windows, and browser-hosted WebAssembly.
- Exported apps should be C++/CMake-based first.
- Fast incremental build UX matters more than hot reload.
- Browser-hosted WASM is required for exported apps eventually, but it should not be implemented in the current M4 renderer-selection work unless explicitly requested.

## Renderer Roadmap Posture

- Current default interactive sandbox renderer: Dawn/WebGPU through `greenfield_render_webgpu`.
- Sandbox renderer selection: `--renderer=webgpu` or `--renderer=fast2d`.
- Compatibility alias: `greenfield_webgpu` points to `greenfield_render_webgpu`.
- Current default build requirement: Dawn/WebGPU and FreeType remain required because the sandbox still uses the WebGPU renderer.
- Implemented Fast2D foundation: `greenfield_render_fast2d`, limited to renderer-neutral command consumption, backend-local preparation, clipped CPU filled-rectangle rasterization, and deferred text.
- Current Fast2D sandbox status: opt-in diagnostic/headless, not visibly presentable.
- Future default baseline direction: Greenfield-owned Fast2D after later composition and presentation work.
- WebGPU: backend-specific accelerated renderer direction.
- Skia: possible later optional backend, not the current foundation.

## Not In Scope Yet

- Full renderer composition
- Full text/font sharing
- Richer 2D shape rasterization
- Rounded corners, borders, and antialiasing in Fast2D
- Visible platform presentation for Fast2D
- CPU raster presenter or SDL upload seam for Fast2D
- Mixed-surface composition
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
