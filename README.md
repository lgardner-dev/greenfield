# Greenfield

Greenfield is an open-source, C++20, SDK-first creative application engine.

The Greenfield SDK is the reusable runtime and library that developers use to build creative applications. This repository is a monorepo for SDK/runtime code, future Greenfield Studio work, examples and sandbox apps, tools, docs, and tests.

![Greenfield Control Room sandbox](docs/images/greenfield-control-room.png)

## Current Scope

- SDL3 windowing and input through a small platform layer
- Renderer-agnostic render commands that keep UI code independent from backend details
- A current Dawn/WebGPU accelerated backend with clean ownership boundaries
- Explicit sandbox renderer selection for `--renderer=webgpu` and `--renderer=fast2d`
- A narrow Fast2D renderer backend foundation plus opt-in visible SDL CPU raster sandbox presentation
- M6E Fast2D visual parity foundation: source-over filled rectangles, hard-edged rectangular and rounded borders/fills, intersected nested clips, and stable first visible raster presentation
- UI widget, layout, input, text, and render command basics, including Button, Checkbox, Toggle/Switch, Slider, and a narrow single-line TextInput
- M6A UI runtime groundwork: renderer-neutral `UiId` identity, clearer per-frame versus persistent `UiContext` state, minimal focus state, active-control capture, and per-frame mouse press/release consumption
- M6B first stateful controls: validated Checkbox and added Toggle/Switch as immediate-mode controls backed by `UiId`-keyed persistent boolean state
- M6C first continuous/numeric stateful control foundation: private `UiId`-keyed numeric state and a horizontal immediate-mode Slider with renderer-neutral track/fill/thumb/label commands
- M6F keyboard/focus groundwork: platform-neutral keyboard edge input, immediate-mode focus traversal for Button, Checkbox, Toggle/Switch, and Slider, Tab / Shift+Tab traversal, and Enter/Space activation for focused Button, Checkbox, and Toggle/Switch
- M6G visible focus styling: configurable per-control focus-style data that emits renderer-neutral focus-ring rectangle commands for focused Button, Checkbox, Toggle/Switch, and Slider controls
- M6H narrow Slider keyboard adjustment: platform-neutral Left/Right arrow edge input plus focused Slider keyboard adjustment with internal range-based stepping and safe clamping
- M6I text entry foundation: platform-neutral committed text and Backspace input, immediate-mode single-line TextInput, private `UiId`-keyed persistent text state, click-to-focus, focused append-only committed text, and focused Backspace-at-end behavior
- Minimal SDK surface identity, root UI surface participation, and point-to-surface input routing
- CMake with Ninja presets
- vcpkg manifest-mode as the default dependency path for the `developer` build profile
- M7E two-profile build contract: `developer` for the full dependency-complete sandbox path and `headless-fast2d` for dependency-light core/UI/Fast2D source-tree validation
- Source-tree consumer contract documentation, a working C++/CMake app template, and CTest validation for separate source-tree consumers, without install rules, packages, CLI behavior, or WASM implementation

## Direction

- `greenfield_render_fast2d` exists as a sibling backend target. It consumes renderer-neutral `RenderCommandList` commands, prepares backend-local fill operations, rasterizes deterministic filled rectangles with source-over alpha blending, hard-edged rectangular borders, hard-edged rounded fills, hard-edged rounded borders, and clipping into a CPU raster target, and defers text rasterization.
- Fast2D now handles intersected nested clips for its CPU raster path. Shape rasterization remains intentionally hard-edged and does not include antialiasing, vector paths, transforms, gradients, text, or full WebGPU visual parity.
- The single sandbox accepts `--renderer=webgpu` and `--renderer=fast2d` for side-by-side backend comparison without creating a second sandbox app.
- WebGPU remains the default interactive sandbox renderer.
- Fast2D is opt-in and visibly interactive in the sandbox through an SDL CPU raster presenter. The Fast2D SDL window is hidden until the first valid raster presentation so the first visible frame is stable. Text remains deferred in Fast2D, so the visible path shows stronger shape parity while labels and values remain visible only in the WebGPU path.
- The one-frame Fast2D diagnostic path remains available with `--renderer=fast2d --headless` or `--renderer=fast2d --diagnostic`.
- Renderer choice belongs in app/composition-root policy or narrow renderer-selection vocabulary, not in UI widgets, render commands, surface types, or platform abstractions.
- `greenfield_render_webgpu` is the current real WebGPU backend target, and `greenfield_webgpu` remains a compatibility alias.
- Dawn/WebGPU is the current implemented accelerated backend and should remain backend-specific in the architecture direction.
- The default `developer` build still requires SDL3, Dawn/WebGPU, and FreeType because the sandbox uses SDL and keeps WebGPU as the default interactive renderer.
- The `headless-fast2d` build profile avoids SDL3, Dawn/WebGPU, FreeType, the sandbox, and the vcpkg toolchain/manifest path for dependency-light core/UI/Fast2D validation.
- Skia may be considered later as an optional renderer/backend, but it is not the initial foundation.
- Greenfield Studio is a future IDE/editor built on top of the SDK, not part of current M4/M5 foundation work.
- Greenfield CLI is future tooling, not part of current M4/M5 foundation work.
- Development can be Linux-first for v0.1 work, while preserving Linux, Windows, and browser-hosted WebAssembly as v0.1 release/export architecture considerations.
- Greenfield app consumers should be C++/CMake-based first.
- App consumers are separate app projects, not the sandbox copied as a product template.
- `apps/sandbox` is the current demo and composition root. App consumers should consume SDK/runtime targets and provide their own composition-root policy.
- `templates/cpp-cmake-app` is a minimal working source-tree consumer template. It is not included in the root build and is not an install/export pipeline.
- `docs/consumer-contract.md` documents the supported source-tree consumer model, target categories, current broad header-surface posture, and deferred packaging work.
- A composition root may wire concrete host platform and renderer backend targets such as SDL, WebGPU, or Fast2D. Reusable SDK/UI/runtime/surface/export vocabulary should stay independent from SDL, Dawn/WebGPU, and FreeType.
- Hot reload is not a core v0.1 requirement; fast incremental build UX matters more.

## Not In Scope Yet

The current renderer-selection and Fast2D presentation work is intentionally narrow. It is not a compositor and does not implement mixed-surface composition. Fast2D text rasterization, rich text shaping, shared text/font architecture, antialiasing, vector paths, transforms, gradients, visual regression CI, full WebGPU visual parity, Studio implementation, CLI implementation, Canvas2D, Scene3D, shader/editor surfaces, node graphs, retained-mode UI, hot reload, Python bindings, and Skia integration are not in scope yet.

M7 consumer-contract work currently includes source-tree integration vocabulary, one minimal working C++/CMake app template, an external-style Fast2D consumer validation, and the M7E two-profile build contract. It does not add generated projects, CLI commands, install rules, package/export rules, Windows-specific workflows, or browser-hosted WebAssembly support.

M6F/M6G/M6H/M6I UI control work currently includes Button, Checkbox, Toggle/Switch, Slider, and a narrow single-line TextInput. Checkbox and Toggle/Switch preserve `UiId`-keyed persistent boolean state. Slider adds private `UiId`-keyed numeric state, returns `true` only when the current frame changes the value, emits renderer-neutral track/fill/thumb/label commands, supports click-to-set and drag-while-captured behavior, clamps values, and safely handles reversed or degenerate ranges. TextInput adds private `UiId`-keyed persistent text state, click-to-focus, focused append-only committed text, focused Backspace-at-end behavior, and returns `true` only when the final persisted text changes during the current frame. Existing immediate-mode controls participate in platform-neutral keyboard focus traversal: Tab and Shift+Tab move through the current frame's Button, Checkbox, Toggle/Switch, Slider, and TextInput encounter order. Keyboard focus is visible through configurable `FocusStyle` data on Button, Checkbox, Toggle/Switch, Slider, and TextInput styles rather than hardcoded renderer or platform rules. Focused controls emit renderer-neutral outer-ring rectangle commands around the button bounds, checkbox box bounds, toggle track bounds, slider track bounds, or text-input bounds. Focused Slider also responds to platform-neutral Left/Right arrow edge fields: Left decreases value, Right increases value, the step is a narrow internal increment based on the effective normalized range, values clamp to normalized min/max bounds, reversed ranges remain safe through normalization, degenerate ranges remain safe without false changes, and Slider returns `true` only when the value actually changes during the frame. TextInput does not add generic Enter/Space activation, cursor movement, arbitrary insertion, selection, clipboard, IME, multiline editing, validation, undo/redo, or grapheme-aware deletion. A small sandbox Slider example and one small TextInput example exist for manual visual verification. Screenshot capture has been proven as a local development workflow artifact, but screenshots are not committed project artifacts or required automated test outputs. WebGPU currently renders text through its backend-local FreeType path, while Fast2D still defers text and therefore does not yet display TextInput text in the visible Fast2D path. M6F/M6G/M6H/M6I do not add key repeat policy, a full shortcut/keybinding system, an input action system, character input editing beyond focused committed text append and Backspace-at-end, IME, clipboard, selection, accessibility, modal focus traps, retained UI trees, a full event dispatch system, spatial navigation, gamepad navigation, dropdowns, tabs, modals, toasts, tooltips, a compositor, mixed-surface composition, Canvas2D, Scene3D, shader tools, dashboards/editor systems, node graphs, Studio, CLI, project generation/export tooling, Fast2D text rasterization, a shared FreeType/text service, Skia, Python bindings, or hot reload.

## Current Build Shape

The top-level CMake build has one cache-string profile selector:

- `GREENFIELD_BUILD_PROFILE=developer` is the default. It preserves the existing dependency-complete developer behavior, discovers SDL3, Dawn/WebGPU, and FreeType, builds the SDL platform, WebGPU backend, sandbox, Fast2D backend, SDK/runtime targets, and full compatible test suite.
- `GREENFIELD_BUILD_PROFILE=headless-fast2d` builds only the dependency-light source-tree/core/UI/Fast2D path. It does not discover SDL3, Dawn/WebGPU, or FreeType; does not require the Greenfield vcpkg toolchain or root manifest; and excludes the SDL platform, SDL raster presenter implementation, WebGPU backend, WebGPU alias, and sandbox.

Invalid profile values fail during configure instead of falling back silently.

The current CMake project defines reusable SDK/runtime-style targets, concrete backend/platform targets, and one sandbox executable:

- `greenfield_core`: interface target for core value types.
- `greenfield_render`: interface target for renderer-neutral render commands and renderer interfaces.
- `greenfield_render_fast2d`: Fast2D renderer backend foundation with CPU filled-rectangle rasterization, hard-edged rounded fills/borders, source-over alpha blending, intersected nested clips, and deferred text.
- `greenfield_ui`: UI context, `UiId` identity, layout, style, focus/capture groundwork, keyboard focus traversal, and immediate widget basics including Button, Checkbox, Toggle/Switch, Slider, and narrow TextInput controls.
- `greenfield_platform`: interface target for platform abstractions.
- `greenfield_sdl_platform`: SDL platform, startup presenter, and CPU raster presenter implementation.
- `greenfield_render_webgpu`: Dawn/WebGPU renderer backend with backend-local FreeType text rendering.
- `greenfield_webgpu`: compatibility alias for `greenfield_render_webgpu`.
- `greenfield_sandbox`: demo executable in `apps/sandbox`.

`greenfield_sandbox` links `greenfield_core`, `greenfield_render`, `greenfield_ui`, `greenfield_sdl_platform`, `greenfield_render_fast2d`, and `greenfield_render_webgpu` in the `developer` profile. It is a demo composition root, so it may know about concrete SDL, WebGPU, and Fast2D targets while the reusable SDK layers remain independent of those concrete providers.

Supported SDK/runtime consumer-facing targets today are `greenfield_core`, `greenfield_render`, `greenfield_ui`, and `greenfield_platform`. Concrete composition-root dependency targets are `greenfield_render_fast2d`, `greenfield_render_webgpu`, `greenfield_webgpu`, and `greenfield_sdl_platform`. `greenfield_sandbox` is demo/internal-only and is not a supported consumer dependency.

The current Makefile exposes `bootstrap`, `configure`, `build`, `run`, `test`, `clean`, and `format`. The current CMake presets are `dev` and `release` for configure, build, and test flows. No generated export project, install/package/export workflow, Windows-specific workflow, or WASM-specific workflow exists yet in this repository.

The repository also contains `templates/cpp-cmake-app`, a small working source-tree consumer template. It is not automatically included by the root build.

CTest includes narrow source-tree consumer guardrails that configure, build, and run both `templates/cpp-cmake-app` and `consumers/source-tree-fast2d` in separate build trees. This validates the source-tree consumer contract without making either project part of the normal app build.

## Source-Tree Consumers

The supported consumer model is source-tree integration. A consumer project receives `GREENFIELD_SOURCE_DIR`, validates it, calls `add_subdirectory("${GREENFIELD_SOURCE_DIR}" greenfield-build EXCLUDE_FROM_ALL)`, and links supported Greenfield targets.

When using the default `developer` dependency path, configure consumers with the Greenfield toolchain and manifest:

```bash
cmake -S templates/cpp-cmake-app -B build/template-cpp-cmake-app \
    -DGREENFIELD_SOURCE_DIR=/path/to/greenfield \
    -DCMAKE_TOOLCHAIN_FILE=/path/to/greenfield/cmake/vcpkg-toolchain.cmake \
    -DVCPKG_MANIFEST_DIR=/path/to/greenfield \
    -DGREENFIELD_BUILD_PROFILE=developer
```

For dependency-light Fast2D consumers, configure source-tree consumers with the headless profile and without the Greenfield vcpkg toolchain or manifest:

```bash
cmake -S templates/cpp-cmake-app -B build/template-cpp-cmake-app-headless \
    -DGREENFIELD_SOURCE_DIR=/path/to/greenfield \
    -DGREENFIELD_BUILD_PROFILE=headless-fast2d
```

M7E does not implement install/export packaging or `find_package(Greenfield)`. See `docs/consumer-contract.md` for the full contract.

## Export Vocabulary

- Host platform: the environment and platform provider an app runs on, such as the current SDL desktop path or a future browser host.
- Renderer backend choice: the app/composition-root policy that selects a renderer implementation, currently `webgpu` or `fast2d` in the sandbox.
- App project: a future generated or hand-authored C++/CMake project that consumes Greenfield SDK/runtime targets.
- App target: the executable or equivalent target produced by an app project.
- Build/export target: the requested output platform or delivery direction for an app project, such as Linux desktop now and Windows or browser-hosted WebAssembly as future v0.1 considerations.
- Browser-hosted WebAssembly: a future build/export target direction, not a current implementation in this repo.

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

The dependency-light profile can be run without vcpkg:

```bash
cmake --preset headless-fast2d
cmake --build --preset headless-fast2d
ctest --preset headless-fast2d --output-on-failure
```

## Run

```bash
./build/dev/bin/greenfield_sandbox
./build/dev/bin/greenfield_sandbox --renderer=webgpu
./build/dev/bin/greenfield_sandbox --renderer=fast2d
./build/dev/bin/greenfield_sandbox --renderer=fast2d --headless
./build/dev/bin/greenfield_sandbox --renderer=fast2d --diagnostic
```

`--renderer=webgpu` is the default interactive path. `--renderer=fast2d` runs the visible interactive Fast2D path through SDL CPU raster presentation. `--renderer=fast2d --headless` and `--renderer=fast2d --diagnostic` both run the same one-frame diagnostic path and exit after reporting command/fill/text/raster diagnostics.

## Visual Verification

Run `make build` and `make test` before manual sandbox checks.

For local renderer verification, launch one or both interactive sandbox paths:

```bash
./build/dev/bin/greenfield_sandbox --renderer=webgpu --window-size=1280x720
./build/dev/bin/greenfield_sandbox --renderer=fast2d --window-size=1280x720
```

In automation or a non-interactive shell, timeout-based launch checks are reasonable:

```bash
timeout 3s ./build/dev/bin/greenfield_sandbox --renderer=webgpu --window-size=1280x720
timeout 3s ./build/dev/bin/greenfield_sandbox --renderer=fast2d --window-size=1280x720
```

Exit code `124` is expected when the interactive sandbox stays open until `timeout` stops it. Screenshot capture is optional and local-only. Screenshots are not committed artifacts and are not required CI artifacts.

## Developer Commands

```bash
make bootstrap
make build
make run
make test
make clean
make format
```
