# Greenfield Architecture

Greenfield is an open-source, C++20, SDK-first creative application engine. The current codebase is intentionally small, with narrow interfaces between core types, input, platform code, renderer-neutral commands, the current Dawn/WebGPU backend, and UI widgets.

This document describes the current layer boundaries. It should help contributors add features without accidentally coupling UI, platform, and renderer code.

## SDK-First Architecture

Greenfield is organized around the Greenfield SDK: the reusable runtime and library developers build applications with.

- Greenfield Studio is a future application built on top of the SDK.
- Greenfield CLI is future tooling around the SDK.
- Shared engine layers should stay product-neutral and reusable.
- Composition roots, sample apps, and sandbox applications may wire concrete implementations together, but reusable SDK layers should not depend on those applications.

## Layer Overview

### `engine/core`

Core contains small value types shared across the engine:

- `Vec2`
- `Rect`
- `Color`

These types are dependency-light and should stay independent from platform, renderer, font, and application code.

### `engine/input`

Input contains platform-neutral interaction state. `InputState` currently exposes mouse position, left mouse button transitions, and vertical scroll delta.

Input state should describe what happened, not where it came from. It must not depend on SDL, WebGPU, Dawn, FreeType, or any concrete windowing backend.

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
- The current Dawn/WebGPU backend is an implemented accelerated backend.
- Greenfield should not be described as WebGPU-first.
- Fast2D is the intended future default baseline renderer.
- WebGPU/Dawn should remain backend-specific and optional in direction.
- Skia may be considered later as an optional renderer/backend, but it is not the initial foundation.
- Normal UI should not require WebGPU-specific concepts.

### `engine/render/webgpu`

The WebGPU backend lives under `engine/render/webgpu`:

- `WebGpuContext` creates and owns the WebGPU instance, surface, adapter, device, queue, and surface configuration.
- `WebGpuRenderer` implements the renderer flow for submitted render commands.

WebGPU and Dawn includes should stay in this layer. FreeType usage for the current WebGPU text path is also contained here. UI code should never call WebGPU APIs directly.

`WebGpuContext` depends on `INativeSurfaceProvider`, not `SdlWindow`. This is an important boundary: SDL is only one possible provider of native surface handles.

### `engine/ui`

UI contains the immediate-mode UI context, layout helpers, styles, buttons, panels, text helpers, and scroll panels.

The UI layer depends on:

- `engine/core` for geometry and color
- `engine/input` for user interaction state
- `engine/render` for render commands

The UI layer must not include SDL, Dawn, WebGPU, or FreeType. Widgets should emit renderer-neutral commands and let the active renderer decide how to draw them.

### `apps/sandbox`

The sandbox is the current demo application. It wires the layers together:

- creates the SDL window
- draws startup feedback
- creates the WebGPU context and renderer
- builds UI each frame
- submits UI render commands to the renderer

Application code may know about concrete implementations because it is the composition root. Shared engine layers should not take dependencies back on the sandbox.

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

platform/sdl
  -> platform interfaces
  -> SDL3

core/input/render commands
  -> no SDL, Dawn, WebGPU, or FreeType dependencies
```

The core rule is that platform and renderer details do not leak upward into UI or renderer-neutral command types.

## Important Boundaries

- UI code must not include or depend on SDL, Dawn, WebGPU, or FreeType.
- Render commands must remain renderer-agnostic.
- Renderer backends must be swappable.
- WebGPU code must stay under `engine/render/webgpu`.
- SDL code must stay in the SDL platform implementation and startup presenter area.
- `WebGpuContext` depends on `INativeSurfaceProvider`, not `SdlWindow`.
- Future targets should add new providers or backends rather than coupling directly to existing concrete implementations.
- Widgets should emit commands through `RenderCommandList`; they should not call renderer APIs directly.
- Platform implementations should translate native events into `InputState`; UI should consume `InputState` without knowing the source.
- Composition roots may wire concrete implementations together; reusable SDK layers should not depend on sandbox or demo applications.

## Platform Strategy

v0.1 development can be Linux-first, but release and export awareness should preserve Linux, Windows, and browser-hosted WebAssembly targets.

Future platform targets should add providers or backends rather than leaking platform APIs into UI or renderer-neutral layers. The boundary should stay on abstractions, not on concrete platform SDKs.

## Surface Direction

UI surfaces exist now. Canvas2D, Scene3D, shader/editor surfaces, editor panels, dashboards, and other custom interactive surfaces are future directions only.

Those systems are not part of M0. When they arrive, the architectural intent is that they participate in one cohesive application experience without violating renderer or platform boundaries.

## Renderer Flow

The current frame rendering path is:

1. Application code begins a UI frame with the current layout and `InputState`.
2. UI widgets and drawing helpers emit a `RenderCommandList`.
3. The application submits the command list through `IRenderer`.
4. The renderer batches and interprets rectangle, text, and clip commands.
5. The WebGPU backend draws the frame and presents the surface.

`RenderCommandList` is the contract between UI and rendering. Adding a new renderer should not require UI widgets to know which backend is active.

## Input Flow

The current input path is:

1. SDL collects native events in `SdlWindow::PollEvents`.
2. `SdlWindow` translates those events into platform-neutral `InputState`.
3. The application reads `InputState` through `IWindow`.
4. `UiContext` consumes `InputState` while building widgets.
5. UI interaction changes widget state and emits render commands for the current frame.

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
