# Greenfield Agent Instructions

This repository is Greenfield, an open-source, C++20, SDK-first creative application engine.

The Greenfield SDK is the reusable runtime/library developers build apps with. The monorepo contains SDK/runtime code, future Studio work, examples and sandbox apps, tools, docs, and tests.

When modifying this repository, follow the coding standards in `docs/coding-standards.md`.

Important priorities:
1. Prefer readability over cleverness.
2. Use RAII for resource ownership.
3. Follow SOLID principles where they improve clarity and maintainability.
4. Keep functions small and focused.
5. Keep cyclomatic complexity below 10 whenever practical.
6. Prefer descriptive names over abbreviations.
7. Avoid premature abstractions, but keep boundaries clean.
8. Design SDK-first: reusable runtime and library boundaries come before app-specific tooling.
9. Keep UI code independent from SDL, Dawn, WebGPU, FreeType, and other concrete platform/rendering/font providers.
10. Keep render commands renderer-agnostic.
11. Keep platform implementations behind platform abstractions.
12. Keep WebGPU-specific code isolated under its backend/module.
13. Keep SDL-specific code isolated under its platform/startup presenter area.
14. Future platform targets should add providers/backends rather than coupling SDK or UI code directly to concrete implementations.
15. Do not add large frameworks, scripting, ECS, editor systems, or mobile support until explicitly requested.
16. Do not implement WASM support unless explicitly requested for a later milestone, but preserve browser-hosted WebAssembly as a v0.1 target and architecture consideration in docs and boundaries.
17. Do not let widgets directly call WebGPU APIs.

## M4-M6B Scope Guard

M4 work is limited to narrow renderer backend selection and Fast2D diagnostic/headless guardrail documentation unless the user explicitly requests a different milestone or slice.

The sandbox exposes `--renderer=webgpu` and `--renderer=fast2d`. WebGPU remains the default interactive path. Fast2D may consume renderer-neutral render commands, prepare backend-local fill operations, rasterize deterministic plain filled rectangles with clipping, preserve optional shape styling metadata for later backend work, defer/count text, and report diagnostics from one headless Control Room frame. Do not make Fast2D visibly presentable or the default renderer, and do not add full renderer composition, full text/font sharing, richer shape rasterization, rounded corners, borders, antialiasing, platform presentation, mixed-surface composition, Studio, CLI, Canvas2D, Scene3D, shader tools, dashboards, node graphs, a compositor, retained-mode UI, hot reload, Python bindings, or Skia as part of M4 guardrail/doc work. Visible Fast2D presentation requires a future CPU raster presenter, SDL upload seam, or equivalent platform presentation decision.

Current M5 work is limited to export and target foundation vocabulary plus the narrow `templates/cpp-cmake-app` scaffold and its CTest guardrail unless the user explicitly requests more implementation. M5 docs may define host platform, renderer backend choice, app project, app target, build/export target, and browser-hosted WebAssembly as a future target direction. Do not add generated projects, CLI behavior, install/package/export logic, Windows-specific workflows, WASM support, optional Dawn/WebGPU or FreeType behavior, or sandbox runtime changes as part of M5 vocabulary/template work.

Current M6A work is limited to product-quality UI runtime groundwork unless the user explicitly requests a different milestone or slice. The implemented foundation preserves the immediate UI model while adding renderer-neutral `UiId` identity, clearer `UiContext` persistent/per-frame state boundaries, minimal persistent focus state, generalized active-control capture, and per-frame mouse press/release consumption.

Current M6B work is limited to the first stateful controls unless the user explicitly requests a different milestone or slice. Checkbox already existed and was audited as complete enough for M6B. Toggle/Switch was added as a new immediate-mode control. Both controls use `UiId`-keyed persistent boolean state, return `true` only when the current frame changes that state, emit renderer-neutral render commands, and use active-control capture plus per-frame mouse press/release consumption. Different `UiId`s keep independent state; matching names intentionally share the same `UiId`-keyed boolean state. Do not turn M6B work into sliders, tabs, dropdowns, modals, toasts, tooltips, keyboard navigation, text entry, IME, clipboard, selection, accessibility, modal focus traps, a retained UI tree, full event dispatch system, broad control inventory, compositor, mixed-surface composition, Studio, CLI, project generation/export tooling, visible Fast2D presentation, Fast2D text rasterization, shared FreeType/text service, Skia, Python bindings, hot reload, or sandbox/runtime renderer behavior changes unless explicitly requested.
