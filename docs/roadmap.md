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

## M5

M5 is the export and target foundation.

Current M5 work is vocabulary, boundary alignment, and one minimal illustrative C++/CMake app-template scaffold. It defines how Greenfield talks about host platforms, renderer backend choice, app projects, app targets, build/export targets, and browser-hosted WebAssembly as a future target direction.

For this slice, exported apps are future C++/CMake-based app projects that consume Greenfield SDK/runtime targets. Exported apps are not the sandbox. `apps/sandbox` remains a demo and composition root that wires current SDK/UI/platform/renderer targets together.

Future generated or exported apps should provide their own composition-root policy. That policy may wire concrete SDL, WebGPU, or Fast2D targets, but reusable SDK/UI/runtime/surface/export vocabulary should not directly depend on SDL, Dawn/WebGPU, or FreeType.

`templates/cpp-cmake-app` is a narrow scaffold for the intended exported-app shape. It is not included in the root build and does not make `apps/sandbox` a product template.

A small CTest guardrail validates the scaffold structure and expected standalone CMake stop. It protects the M5 contract without adding a generator, package, install flow, or export pipeline.

M5 does not yet add generated projects, CLI behavior, install rules, package/export logic, Windows-specific export workflows, browser-hosted WebAssembly support, or changes to sandbox runtime behavior.

## M6A

M6A is the product-quality UI runtime foundation before stateful controls.

Current M6A work includes renderer-neutral and platform-neutral `UiId` control identity, internal normalization of existing immediate UI string names into `UiId` for runtime state, clearer `UiContext` persistent/per-frame state boundaries, minimal persistent focus state, generalized active-control capture state, and per-frame mouse press/release consumption so overlapping immediate buttons cannot claim the same gesture.

The immediate UI programming model remains intact. Existing button behavior, scroll panels, layout, clipping, text command emission, renderer selection, Fast2D diagnostic behavior, and WebGPU sandbox behavior are preserved.

M6A does not add keyboard navigation, text input, IME, clipboard, selection, accessibility, retained UI trees, a full event dispatch system, modal focus traps, stateful controls, compositor work, mixed-surface composition, Studio, CLI, project generation/export tooling, visible Fast2D presentation, Fast2D text rasterization, Skia, Python bindings, or hot reload.

## M6B

M6B is the first stateful controls milestone.

Current M6B work validates the existing Checkbox as complete enough for this milestone and adds Toggle/Switch as a new immediate-mode stateful control. Checkbox and Toggle both use `UiId`-keyed persistent boolean state in `UiContext`, return `true` only when the current frame changes that state, use active-control capture and per-frame mouse press/release consumption, and emit renderer-neutral render commands.

Checkbox and Toggle state is independent for different `UiId`s. Matching names intentionally share the same `UiId`-keyed boolean state, including across Checkbox and Toggle. Existing Button behavior, scroll panels, layout, clipping, render command behavior, renderer selection, Fast2D diagnostic behavior, and WebGPU sandbox behavior are preserved. M6B does not modify sandbox or runtime renderer behavior.

M6B does not add sliders, tabs, dropdowns, modals, toasts, tooltips, keyboard navigation, text entry, IME, clipboard, selection, accessibility, modal focus traps, a retained UI tree, a full event dispatch system, compositor work, mixed-surface composition, Canvas2D, Scene3D, shader tools, dashboards/editor systems, node graphs, Studio, CLI, project generation/export tooling, visible Fast2D presentation, Fast2D text rasterization, a shared FreeType/text service, Skia, Python bindings, or hot reload.

## M6C

M6C is the first continuous/numeric stateful control foundation.

Current M6C work adds private `UiId`-keyed numeric state in `UiContext` and a horizontal immediate-mode Slider control. Slider values persist across frames by `UiId`, return `true` only when the current frame changes the value, use active-control capture and per-frame mouse press/release consumption, support click-to-set and drag-while-captured behavior, clamp values, and safely handle reversed or degenerate ranges.

Slider emits renderer-neutral track, fill, thumb, and label commands through `RenderCommandList`. It does not call SDL, Dawn/WebGPU, FreeType, or renderer APIs. Existing Button, Checkbox, Toggle/Switch, scroll panel, layout, clipping, render command, renderer selection, Fast2D diagnostic, and WebGPU sandbox behavior are preserved.

The existing Control Room sandbox includes one small Slider example for manual visual verification. A local screenshot capture workflow has been proven during development, but screenshots are not committed project artifacts and are not part of normal automated test validation.

M6C does not add keyboard navigation, text entry, dropdowns, tabs, modals, toasts, tooltips, accessibility, a retained UI tree, a full event dispatch system, a broad controls library, compositor work, mixed-surface composition, Canvas2D, Scene3D, shader tools, Studio, CLI, project generation/export tooling, visible Fast2D presentation, Fast2D text rasterization, a shared text/font service, Skia, Python bindings, or hot reload.

## v0.1 Direction

- C++20-first authoring.
- Linux-first development is acceptable.
- v0.1 release/export awareness includes Linux, Windows, and browser-hosted WebAssembly.
- Exported apps should be C++/CMake-based first.
- Exported apps should consume SDK/runtime targets and define their own app targets rather than treating `apps/sandbox` as a product template.
- The current C++/CMake app-template scaffold is illustrative only; future tooling may generate, copy, or replace it.
- Fast incremental build UX matters more than hot reload.
- Browser-hosted WASM is a future target direction for exported apps, but it should not be implemented in current M4/M5 foundation work unless explicitly requested.

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
- App template generation beyond the current illustrative scaffold
- Install, package, or export rule implementation
- Windows-specific export workflow implementation
- Keyboard navigation, text input, IME, clipboard, selection, and accessibility
- Retained UI tree or full UI event dispatch system
- Broad product-quality UI control set beyond the current Checkbox, Toggle/Switch, and Slider foundation
