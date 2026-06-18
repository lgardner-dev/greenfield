# Greenfield Architecture

Greenfield is an open-source, C++20, SDK-first creative application engine. The current codebase is intentionally small, with narrow interfaces between core types, input, platform code, renderer-neutral commands, the current Dawn/WebGPU backend, the narrow Fast2D backend foundation, and UI widgets.

This document describes the current layer boundaries. It should help contributors add features without accidentally coupling UI, platform, and renderer code.

## SDK-First Architecture

Greenfield is organized around the Greenfield SDK: the reusable runtime and library developers build applications with.

- Greenfield Studio is a future application built on top of the SDK.
- Greenfield CLI is future tooling around the SDK.
- Shared engine layers should stay product-neutral and reusable.
- Composition roots, sample apps, and sandbox applications may wire concrete implementations together, but reusable SDK layers should not depend on those applications.
- Exported apps are future C++/CMake-based app projects that consume SDK/runtime targets and provide their own composition-root policy.
- Exported apps are not the current sandbox. `apps/sandbox` is a demo/composition root that proves how the current targets can be wired together.

## M5 Export And Target Vocabulary

The current repository has vocabulary for export and target planning plus one minimal illustrative C++/CMake app-template scaffold. It does not yet implement generated app projects, install rules, package rules, CLI export behavior, Windows-specific export workflows, or browser-hosted WebAssembly builds.

- Host platform: the operating environment and platform provider an app runs on. The current interactive host path is SDL desktop through `greenfield_sdl_platform`.
- Renderer backend choice: composition-root policy that selects a renderer implementation. The sandbox currently exposes `--renderer=webgpu` and `--renderer=fast2d`.
- App project: a future generated or hand-authored C++/CMake project outside the sandbox that consumes Greenfield SDK/runtime targets.
- App target: the executable or equivalent CMake target produced by an app project.
- Build/export target: the requested output platform or delivery direction for an app project, such as Linux desktop today and Windows or browser-hosted WebAssembly as future v0.1 considerations.
- Browser-hosted WebAssembly: a future target direction that should preserve SDK/UI/runtime boundaries. It is not implemented by the current build.
- App template scaffold: `templates/cpp-cmake-app` documents the intended future exported-app shape without being included in the root build or implementing export tooling.

Reusable SDK, UI, runtime, surface, and export vocabulary must not directly depend on SDL, Dawn/WebGPU, or FreeType. Concrete composition roots may wire platform and renderer backend targets together when they produce an app target.

## Layer Overview

### `engine/core`

Core contains small value types shared across the engine:

- `Vec2`
- `Rect`
- `Color`
- `SurfaceId`
- `SurfaceBounds`
- `Surface`

These types are dependency-light and should stay independent from platform, renderer, font, and application code.

### `engine/input`

Input contains platform-neutral interaction state and minimal routing helpers. `InputState` currently exposes mouse position, left mouse button transitions, and vertical scroll delta. The interaction routing vocabulary can hit-test an input point against surface bounds and return the target `SurfaceId` when one is found.

Input state and routing should describe what happened and which renderer-neutral surface bounds were targeted, not where the input came from. They must not depend on SDL, WebGPU, Dawn, FreeType, or any concrete windowing backend.

### `engine/platform`

Platform contains abstract interfaces for windows and native drawing surfaces:

- `IWindow` owns the platform-neutral window loop surface for the rest of the engine.
- `INativeSurfaceProvider` exposes the native surface handles needed by rendering backends.

`INativeSurfaceProvider` is the bridge between a concrete platform window and a renderer backend that needs native handles. Renderers should depend on this abstraction instead of depending on concrete window classes.

### `engine/platform` SDL Implementation

The current SDL implementation lives in `engine/platform`:

- `SdlWindow` implements `IWindow` and `INativeSurfaceProvider`.
- `SdlWindow` owns the SDL window, polls SDL events, updates `InputState`, and exposes native Wayland or X11 surface information.
- `SdlStartupPresenter` uses SDL window surfaces to draw an immediate startup frame before WebGPU takes over.
- `SdlRasterPresenter` uses SDL window surfaces to present CPU raster pixels for the opt-in Fast2D sandbox path.

SDL code should stay in the SDL platform implementation and startup presenter area. Engine UI and renderer-neutral code should not include SDL headers.

### `engine/render`

Render contains renderer-facing abstractions that are not tied to a graphics API:

- `RenderCommand`
- `RenderCommandList`
- `IRenderer`

The UI emits `RenderCommandList` objects made of simple commands such as filled rectangles, text, and clip pushes/pops. These commands must remain renderer-agnostic. They should not contain SDL, Dawn, WebGPU, FreeType, or backend-specific handles.

## Renderer Strategy

The renderer direction is split between current implementation and future baseline intent:

- Render commands remain renderer-agnostic.
- The sandbox has explicit renderer backend selection through `--renderer=webgpu` and `--renderer=fast2d`.
- `greenfield_render_webgpu` is the current real WebGPU backend target.
- `greenfield_webgpu` remains a compatibility alias for `greenfield_render_webgpu`.
- The current Dawn/WebGPU backend is an implemented accelerated backend.
- WebGPU remains the default interactive sandbox renderer.
- The current default build requires Dawn/WebGPU and FreeType because the sandbox still uses the WebGPU renderer by default.
- Greenfield should not be described as WebGPU-first.
- `greenfield_render_fast2d` is an implemented sibling backend foundation, and the sandbox path is currently opt-in visibly interactive through SDL CPU raster presentation.
- Fast2D now rasterizes deterministic filled rectangles with source-over alpha blending, hard-edged rectangular borders, hard-edged rounded fills, hard-edged rounded borders, and intersected nested clips. Its visible path has stronger Control Room shape parity than the original plain-rectangle foundation, but it still defers text and does not claim full WebGPU visual parity.
- The one-frame Fast2D diagnostic/headless path remains available through `--renderer=fast2d --headless` or `--renderer=fast2d --diagnostic`.
- Renderer choice belongs in app/composition-root policy or narrow renderer-selection vocabulary, not UI widgets, render commands, surface types, or platform abstractions.
- WebGPU/Dawn should remain backend-specific in direction.
- Skia may be considered later as an optional renderer/backend, but it is not the initial foundation.
- Normal UI should not require WebGPU-specific concepts.

### `engine/render/webgpu`

The WebGPU backend lives under `engine/render/webgpu`:

- `WebGpuContext` creates and owns the WebGPU instance, surface, adapter, device, queue, and surface configuration.
- `WebGpuRenderer` implements the renderer flow for submitted render commands.

WebGPU and Dawn includes should stay in this layer. FreeType usage for the current WebGPU text path is also contained here. UI code should never call WebGPU APIs directly.

This backend-local FreeType ownership is intentional for the current WebGPU renderer. `greenfield_ui` and `greenfield_render` must stay free of SDL, Dawn/WebGPU, and FreeType dependencies while emitting renderer-agnostic text commands. Future renderer backends, such as `greenfield_render_fast2d`, may own their own text/font path or share a later renderer-neutral text service without changing UI or application code.

M6E intentionally defers Fast2D text rasterization. WebGPU continues to own the current backend-local FreeType text path. Fast2D remains guarded from FreeType includes, so adding backend-local FreeType to Fast2D would be an explicit future boundary-policy decision. A temporary built-in debug bitmap font was considered but deferred because it would create misleading low-quality visual parity.

`WebGpuContext` depends on `INativeSurfaceProvider`, not `SdlWindow`. This is an important boundary: SDL is only one possible provider of native surface handles.

### `engine/render/fast2d`

The Fast2D backend foundation lives under `engine/render/fast2d`:

- `Fast2DRenderer` implements `IRenderer`.
- It consumes renderer-neutral `RenderCommandList` commands.
- It prepares backend-local filled-rectangle operations.
- It handles clip pushes and pops, including observable clip underflow tracking.
- It defers text rasterization for later backend or shared text/font work.
- It can rasterize deterministic filled rectangles, source-over alpha blending, hard-edged rectangular borders, hard-edged rounded fills, hard-edged rounded borders, and intersected nested clips into a backend-owned CPU raster target.

Fast2D shape and blend work remains backend-local under `engine/render/fast2d`; the reusable UI/runtime/render-neutral command code does not branch on renderer backend. Text is still deferred/count-only. Rich text shaping, shared text/font architecture, antialiasing, vector paths, transforms, gradients, full WebGPU visual parity, visual regression CI, and renderer composition remain future work.

Fast2D must stay free of SDL, Dawn/WebGPU, and FreeType includes. Dependency boundary tests cover `engine/render/fast2d` with the same concrete-dependency guard as core, input, render-neutral command types, and UI.

### `engine/ui`

UI contains the immediate-mode UI context, renderer-neutral UI identity, layout helpers, styles, buttons, panels, text helpers, scroll panels, and the current narrow stateful controls.

The UI layer depends on:

- `engine/core` for geometry and color
- `engine/input` for user interaction state
- `engine/render` for render commands

The UI layer must not include SDL, Dawn, WebGPU, or FreeType. Widgets should emit renderer-neutral commands and let the active renderer decide how to draw them.

The current immediate UI root is also describable as a `UiSurface`: a lightweight UI value that carries the root surface identity and the same frame bounds passed to `UiContext::BeginFrame`. This lets the UI root participate in the SDK surface vocabulary without adding a compositor, retained UI tree, or future Canvas2D/Scene3D surface system.

M6A added product-quality runtime groundwork inside this immediate UI model:

- `UiId` is the renderer-neutral and platform-neutral control identity type. Existing immediate UI string names are normalized internally into `UiId` where runtime state needs stable identity.
- `UiContext` separates persistent runtime state from per-frame state. Style, active control identity, focus identity, and vertical scroll offsets persist across frames. Render commands, layout stack, scroll panel stack, input snapshot, and mouse press/release consumption flags are rebuilt for each frame.
- Focus is intentionally minimal and persistent. `UiContext` can request, clear, query, and report the focused control identity, but this does not add keyboard navigation, text entry, focus traversal, accessibility semantics, or modal focus traps.
- Active-control state is generalized as capture state instead of button-specific state. Buttons use it to keep a press/release gesture tied to the initiating control across frames.
- Per-frame mouse press/release consumption prevents later overlapping buttons from claiming the same gesture while preserving existing immediate button calls.

M6B establishes the first boolean stateful control foundation inside the same immediate UI model:

- Checkbox already existed and was audited as complete enough for M6B.
- Toggle/Switch was added as a new immediate-mode stateful control.
- Checkbox and Toggle both use `UiId`-keyed persistent boolean state in `UiContext`.
- Both controls return `true` only when the current frame changes the control state.
- State is independent for different `UiId`s. Matching names intentionally share the same `UiId`-keyed boolean state, even across Checkbox and Toggle.
- Both controls use the existing active-control capture and per-frame mouse press/release consumption behavior.
- Both controls emit renderer-neutral fill and text commands through `RenderCommandList`; they do not call SDL, Dawn/WebGPU, FreeType, or renderer APIs.

M6C establishes the first continuous/numeric stateful control foundation:

- `UiContext` now has private `UiId`-keyed numeric state alongside its boolean state. This numeric state remains internal to the UI runtime and is not exposed as a public app-facing get/set API.
- Slider was added as a horizontal immediate-mode control. It uses the same `UiId` identity model as the other stateful controls, so values persist across frames by control identity and remain independent for different names.
- Slider returns `true` only when the current frame changes its value.
- Slider uses the existing active-control capture and per-frame mouse press/release consumption behavior.
- Slider supports click-to-set and drag-while-captured behavior using the current platform-neutral `InputState`.
- Slider clamps values into its effective range and safely handles reversed or degenerate ranges.
- Slider emits renderer-neutral track, fill, thumb, and label commands through `RenderCommandList`; it does not call SDL, Dawn/WebGPU, FreeType, or renderer APIs.

The sandbox includes one small Slider example in the existing Control Room UI for manual visual verification. A local screenshot capture workflow has been proven useful during development, but screenshots are not committed project artifacts and are not required automated test outputs.

This foundation is UI runtime and first numeric-control groundwork, not a retained-mode system or broad controls milestone. It does not add tabs, dropdowns, modals, toasts, tooltips, keyboard input, text input, IME, clipboard, selection, accessibility, modal focus traps, a retained UI tree, full event dispatch system, compositor, mixed-surface composition, Canvas2D, Scene3D, shader tools, dashboards/editor systems, node graphs, Studio, CLI, project generation/export tooling, visible Fast2D presentation, Fast2D text rasterization, a shared FreeType/text service, Skia, Python bindings, or hot reload.

### `apps/sandbox`

The sandbox is the current demo application. It wires the layers together:

- creates the SDL window
- draws startup feedback
- creates the WebGPU context and renderer
- builds UI each frame
- submits UI render commands to the renderer

The sandbox accepts `--renderer=webgpu` and `--renderer=fast2d`. The default `webgpu` path is the interactive path and continues to create the SDL window, startup presenter, WebGPU context, and WebGPU renderer. The opt-in `fast2d` path creates its own SDL window, builds the same Control Room UI each frame, submits the renderer-neutral `RenderCommandList` to Fast2D, and presents the CPU raster through `SdlRasterPresenter`. Fast2D keeps its SDL window hidden until the first valid raster presentation, which keeps first visible raster presentation in platform/composition-root code rather than reusable UI or render-neutral code. The one-frame diagnostic/headless path remains available with `--renderer=fast2d --headless` or `--renderer=fast2d --diagnostic`.

Application code may know about concrete implementations because it is the composition root. Shared engine layers should not take dependencies back on the sandbox.

`apps/sandbox` is not an exported app template. Future generated or exported apps should consume SDK/runtime targets, define their own app target, and choose their own composition-root policy for host platform and renderer backend wiring.

`templates/cpp-cmake-app` is a minimal scaffold for that future shape. It is separate from `apps/sandbox`, expects SDK/runtime targets to be supplied by a containing workspace or future package/export mechanism, and leaves concrete SDL/WebGPU/Fast2D wiring to the app composition root.

## Current CMake Target Shape

The top-level build currently defines these targets:

- `greenfield_core`: interface target for core value types.
- `greenfield_render`: interface target for renderer-neutral command and renderer interfaces.
- `greenfield_render_fast2d`: Fast2D renderer backend foundation with CPU filled-rectangle rasterization, source-over alpha blending, hard-edged rounded fills/borders, intersected nested clips, and deferred text.
- `greenfield_ui`: UI runtime target.
- `greenfield_platform`: interface target for platform abstractions.
- `greenfield_sdl_platform`: SDL platform, startup presenter, and CPU raster presenter target.
- `greenfield_render_webgpu`: Dawn/WebGPU renderer backend target with backend-local FreeType usage.
- `greenfield_webgpu`: compatibility alias for `greenfield_render_webgpu`.
- `greenfield_sandbox`: demo executable.

The sandbox app target links `greenfield_core`, `greenfield_render`, `greenfield_ui`, `greenfield_sdl_platform`, `greenfield_render_fast2d`, and `greenfield_render_webgpu`. This is acceptable because the sandbox is the composition root. That wiring should not move into renderer-neutral commands, UI widgets, platform interfaces, SDK surface vocabulary, or future export vocabulary.

Tests currently cover core, render command, renderer backend kind, layout, UI, Fast2D renderer behavior, and dependency boundaries. The dependency boundary test guards reusable SDK-facing files from direct SDL, Dawn/WebGPU, and FreeType includes.

Tests also include a narrow `templates/cpp-cmake-app` guardrail. It checks scaffold structure, truthful M5 limit language, absence of concrete platform/render/font includes in the template app source, and the intended standalone CMake failure when SDK/runtime targets are not available. It does not build the template as an app target or implement export behavior.

## Dependency Direction

Dependencies should point from higher-level composition and features toward narrow abstractions and value types.

```text
apps
  -> ui
  -> render interfaces
  -> platform interfaces

ui
  -> core
  -> input
  -> render commands

render/webgpu
  -> render commands
  -> platform native surface abstraction

render/fast2d
  -> render commands
  -> core value types

platform/sdl
  -> platform interfaces
  -> SDL3

core/input/render commands
  -> no SDL, Dawn, WebGPU, or FreeType dependencies
```

The core rule is that platform and renderer details do not leak upward into UI or renderer-neutral command types.

Renderer backend policy follows the same rule. A composition root such as `apps/sandbox` may select and wire concrete backends, but that choice should not leak into widgets, render commands, SDK surface types, or platform abstractions.

## Important Boundaries

- UI code must not include or depend on SDL, Dawn, WebGPU, or FreeType.
- Render commands must remain renderer-agnostic.
- Renderer backends must be swappable.
- Renderer selection vocabulary must stay narrow and renderer-neutral.
- WebGPU code must stay under `engine/render/webgpu`.
- Fast2D code must stay under `engine/render/fast2d` and remain free of SDL, Dawn/WebGPU, and FreeType.
- SDL code must stay in the SDL platform implementation and startup presenter area.
- `WebGpuContext` depends on `INativeSurfaceProvider`, not `SdlWindow`.
- Future targets should add new providers or backends rather than coupling directly to existing concrete implementations.
- Widgets should emit commands through `RenderCommandList`; they should not call renderer APIs directly.
- Platform implementations should translate native events into `InputState`; UI should consume `InputState` without knowing the source.
- Composition roots may wire concrete implementations together; reusable SDK layers should not depend on sandbox or demo applications.

## Platform Strategy

v0.1 development can be Linux-first, but release and export awareness should preserve Linux, Windows, and browser-hosted WebAssembly targets.

Future platform targets should add providers or backends rather than leaking platform APIs into UI or renderer-neutral layers. The boundary should stay on abstractions, not on concrete platform SDKs.

Windows and browser-hosted WebAssembly are export considerations for v0.1, not completed export implementations in the current repository. Browser-hosted WebAssembly should be approached as a future host platform and build/export target direction, with browser-specific providers or backends added behind the same boundaries.

## Surface Direction

Minimal SDK-level surface identity and bounds value types exist now for future interaction tree work. The current foundation also has a small hit-test/routing vocabulary for routing a point to a surface identity. Canvas2D, Scene3D, shader/editor surfaces, editor panels, dashboards, and other custom interactive surfaces are future directions only.

Those systems are not part of the current foundation. The current routing foundation is not a compositor, retained UI tree, event system, or full interaction tree. When richer systems arrive, the architectural intent is that they participate in one cohesive application experience without violating renderer or platform boundaries.

## Renderer Flow

The current frame rendering path is:

1. Application code begins a UI frame with the current layout and `InputState`.
2. UI widgets and drawing helpers emit a `RenderCommandList`.
3. The application submits the command list through `IRenderer`.
4. The renderer batches and interprets rectangle, text, and clip commands.
5. The WebGPU backend draws the frame and presents the surface.

`RenderCommandList` is the contract between UI and rendering. Adding a new renderer should not require UI widgets to know which backend is active.

The current Fast2D backend also consumes `RenderCommandList`, but it is limited to backend-local command preparation, clipped CPU rasterization for filled rectangles, hard-edged borders/rounded rectangles, source-over alpha blending, and deferred text tracking. The sandbox composition root can present that CPU raster through the SDL raster presenter without making Fast2D own SDL details.

The current renderer-selection work is not a compositor and does not implement mixed-surface composition. It lets the sandbox choose between the default interactive WebGPU path, the opt-in interactive Fast2D SDL raster path, and the opt-in one-frame Fast2D diagnostic/headless path.

## Input Flow

The current input path is:

1. SDL collects native events in `SdlWindow::PollEvents`.
2. `SdlWindow` translates those events into platform-neutral `InputState`.
3. The application reads `InputState` through `IWindow`.
4. `UiContext` snapshots `InputState` for the current immediate UI frame.
5. UI interaction updates persistent runtime state where needed, consumes per-frame mouse press/release gestures, and emits render commands for the current frame.

This keeps native event details out of UI code and leaves room for other platform providers to produce the same `InputState`.

## Startup Flow

The current startup path is:

1. The SDL window is created and appears on screen.
2. `SdlStartupPresenter` draws an immediate non-transparent frame using an SDL window surface.
3. The startup presenter releases the SDL window surface before the 3D API surface takes over.
4. `WebGpuContext` initializes WebGPU using `INativeSurfaceProvider`.
5. `WebGpuRenderer` becomes the normal frame presenter.

The startup presenter is intentionally SDL-specific. It should not become a general rendering dependency for UI or renderer-neutral code.

## Extension Points

These are expected places to extend Greenfield while preserving boundaries:

- CPU renderer: implement `IRenderer` against `RenderCommandList` without changing UI widgets.
- Alternate platform provider: implement `IWindow` and `INativeSurfaceProvider` for another desktop windowing stack.
- Browser/WASM target: add a browser platform provider and renderer/backend path rather than making UI depend on browser APIs.
- Mobile target: add mobile platform providers and native surface descriptors without coupling widgets to mobile SDKs.
- Richer layout system: evolve `engine/ui` layout types while continuing to emit renderer-neutral commands.
- Text shaping: add shaping behind renderer or text services without putting FreeType or backend-specific font code in UI widgets.
- Accessibility: add platform-neutral semantics and platform adapters without mixing accessibility APIs into widget drawing code.

Some of these targets are future architecture directions, not current feature commitments. Follow the repository guidance before adding large new systems such as mobile, WASM, scripting, editor functionality, or additional surface types.

## Contributor Rules of Thumb

- Put concrete platform code behind platform interfaces.
- Put concrete graphics API code under its renderer backend.
- Keep `engine/core`, `engine/input`, and render command types small and dependency-free.
- Let `apps/sandbox` compose concrete implementations; do not move that coupling into engine layers.
- Add abstractions only when they protect a real boundary or remove real duplication.
- When adding a feature, ask whether it belongs to UI behavior, platform event translation, renderer-neutral commands, or a concrete backend.
- If a header inclusion would pull SDL, Dawn, WebGPU, or FreeType into UI or command types, stop and add a boundary instead.
