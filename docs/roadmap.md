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

M3 was the narrow Fast2D renderer foundation.

M3 introduced the `greenfield_render_fast2d` backend target, renderer-neutral `RenderCommandList` consumption, backend-local fill preparation, clip stack handling, clip underflow tracking, deferred text tracking, and minimal CPU-side filled-rectangle rasterization with clipping. Later M6E work expanded the current CPU raster path beyond the original plain-rectangle foundation.

Fast2D is not production-ready and is not wired as the default sandbox renderer. The sandbox now has opt-in visible Fast2D presentation through SDL CPU raster upload, and M6E adds hard-edged rounded fills/borders, source-over alpha blending, and intersected nested clips. Full renderer composition, full text/font sharing, antialiasing, full visual parity, mixed-surface composition, Canvas2D, Scene3D, shader tooling, Studio, and CLI remain future work.

## M4

M4 is the narrow renderer-selection reality check.

M4 work included renderer-neutral `RendererBackendKind` vocabulary, sandbox `--renderer=webgpu` and `--renderer=fast2d` parsing, and documentation of the original Fast2D presentation boundary. WebGPU remains the default interactive sandbox renderer. The one-frame Fast2D diagnostic/headless path is preserved through `--renderer=fast2d --headless` or `--renderer=fast2d --diagnostic`.

M6D adds the narrow SDL CPU raster presenter and opt-in visible Fast2D sandbox path. M6D did not implement a compositor, mixed-surface composition, Fast2D text rasterization, richer shape rasterization, or full visual parity.

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

## M6F

M6F is the narrow keyboard/focus navigation groundwork slice for the existing immediate UI runtime.

Current M6F work adds platform-neutral per-frame keyboard edge fields to `InputState` for Tab, Shift+Tab, Enter, and Space. SDL key-down events populate those fields in platform code, keeping UI code independent from SDL.

`UiContext` now rebuilds focusable registration each frame from immediate-mode control encounter order. Button, Checkbox, Toggle/Switch, and Slider register as focusable. Tab moves focus forward through that current-frame order, and Shift+Tab moves backward. When no control is focused, traversal starts from the first or last registered control. When persisted focus points to a control missing from the current frame, traversal restarts from the corresponding frame edge.

Focused Button activates on Enter/Space. Focused Checkbox and Toggle/Switch toggle on Enter/Space. Existing Button, Checkbox, Toggle/Switch, Slider mouse behavior, scroll panels, layout, clipping, renderer-neutral render command behavior, renderer selection, Fast2D diagnostic and visible paths, and WebGPU sandbox behavior are preserved.

M6F does not add text entry, character input, IME, clipboard, selection, accessibility, screen reader semantics, modal focus traps, dropdowns, tabs, modals, tooltips, a retained UI tree, a full event dispatch system, a shortcut/keybinding system, spatial navigation, gamepad navigation, focus visuals beyond any existing control drawing, Studio, CLI, Canvas2D, Scene3D, Fast2D text, Skia, Python bindings, or hot reload.

## M6G

M6G is the narrow visible focus styling slice for the existing immediate UI runtime and M6F keyboard/focus groundwork.

Current M6G work adds shared configurable focus-style vocabulary through `FocusVisualKind { None, OuterRing }` and `FocusStyle { kind, color, thickness, outset, cornerRadiusOffset }`. Button, Checkbox, Toggle/Switch, and Slider now each carry per-control `FocusStyle` data. The approved default is a high-contrast outer ring, but the style remains configurable rather than hardcoded in renderer or platform code.

Focused Button, Checkbox, Toggle/Switch, and Slider emit renderer-neutral rectangle commands for an outer focus ring around button bounds, checkbox box bounds, toggle track bounds, or slider track bounds. Focus state remains in `UiContext`, and focus visuals remain part of UI control/style drawing. Renderers do not know focus semantics, and SDL/platform code does not know focus styling.

Manual visual verification was completed in both WebGPU and Fast2D through the existing sandbox renderer-selection path. WebGPU remains the default interactive sandbox renderer, and Fast2D remains the opt-in visible UI iteration path with deferred text. Slider continues to participate in focus traversal.

M6G does not add a full theme engine, accessibility support, text entry, IME, clipboard, selection, shortcut/keybinding systems, retained UI, modal focus traps, spatial or gamepad navigation, Studio, CLI, Canvas2D, Scene3D, Fast2D text, Skia, Python bindings, or hot reload.

## M6H

M6H is the narrow focused Slider keyboard adjustment slice for the existing immediate UI runtime.

Current M6H work adds platform-neutral per-frame Left/Right arrow edge fields to `InputState`. SDL translates non-repeat Left/Right key-down events into those fields and clears them during per-frame input reset, keeping UI code independent from SDL and native key event details.

Focused Slider now responds to that platform-neutral arrow vocabulary. Left decreases value and Right increases value. Slider uses a narrow internal keyboard step based on the effective normalized range, clamps to normalized min/max bounds after keyboard adjustment, remains safe for reversed ranges through normalized bounds, remains safe for degenerate ranges without false changes, and returns `true` only when the current frame actually changes the value.

Existing Button, Checkbox, Toggle/Switch, and Slider mouse behavior, focus traversal, focus visuals, keyboard activation, scroll panels, layout, clipping, renderer-neutral render command behavior, renderer selection, Fast2D diagnostic and visible sandbox paths, and WebGPU sandbox behavior are preserved. WebGPU remains the default interactive sandbox renderer. Fast2D remains opt-in and visible with deferred text rendering.

M6H does not add key repeat policy, a full shortcut/keybinding system, an input action system, text input, character input, IME, clipboard, selection, accessibility, screen reader semantics, spatial navigation, gamepad navigation, a retained UI tree, a full event dispatch system, dropdowns, tabs, modals, toasts, tooltips, Greenfield Studio, Greenfield CLI, Canvas2D, Scene3D, Fast2D text rendering, Skia, Python bindings, or hot reload.

## M6E

M6E is the Fast2D visual parity foundation slice for the existing one-sandbox renderer-selection workflow.

Current M6E work keeps WebGPU as the default interactive sandbox renderer and keeps Fast2D opt-in through `--renderer=fast2d`. The same sandbox continues to support `--renderer=webgpu` and `--renderer=fast2d` without creating a second sandbox app.

M6E improves Fast2D visible UI parity with backend-local support for source-over alpha blending for filled rectangles, hard-edged rectangular borders, hard-edged rounded rectangle fills, hard-edged rounded rectangle borders, and intersected nested clip handling. It also keeps the Fast2D SDL window hidden until the first valid raster presentation so the first visible Fast2D frame is stable.

M6E intentionally defers Fast2D text rasterization. WebGPU currently owns backend-local FreeType text rendering. Fast2D remains guarded from FreeType includes, so adding backend-local FreeType to Fast2D would be an explicit future boundary-policy decision. A temporary built-in debug bitmap font was considered but deferred because it would create misleading low-quality visual parity.

M6E does not finish Fast2D and does not implement rich text shaping, shared text/font architecture, antialiasing, vector paths, transforms, gradients, compositor work, mixed-surface composition, visual regression CI, full WebGPU visual parity, Studio, CLI, Canvas2D, Scene3D, Skia, Python bindings, hot reload, accessibility, IME, or shared text/font services.

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
- Implemented Fast2D foundation: `greenfield_render_fast2d`, limited to renderer-neutral command consumption, backend-local preparation, clipped CPU filled-rectangle rasterization, source-over alpha blending, hard-edged rectangular and rounded borders/fills, intersected nested clips, stable first visible SDL raster presentation, and deferred text.
- Current Fast2D sandbox status: opt-in visibly interactive through SDL CPU raster presentation, with the one-frame diagnostic/headless path still available through `--renderer=fast2d --headless` or `--renderer=fast2d --diagnostic`.
- Future default baseline direction: Greenfield-owned Fast2D after later composition and presentation work.
- WebGPU: backend-specific accelerated renderer direction.
- Skia: possible later optional backend, not the current foundation.

## Not In Scope Yet

- Full renderer composition
- Full text/font sharing
- Richer 2D shape rasterization beyond the current hard-edged Fast2D rectangle and rounded-rectangle path
- Antialiasing in Fast2D
- Fast2D text rasterization
- Full visual parity for Fast2D presentation
- Visual regression CI
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
- Text entry, character input, IME, clipboard, selection, accessibility, and screen reader semantics
- Shortcut/keybinding system, spatial navigation, gamepad navigation, and Slider arrow-key/repeat behavior
- Retained UI tree or full UI event dispatch system
- Broad product-quality UI control set beyond the current Checkbox, Toggle/Switch, and Slider foundation
