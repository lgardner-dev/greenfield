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

## M3 Scope Guard

Current M3 work is limited to the narrow Fast2D renderer foundation unless the user explicitly requests a different milestone or slice.

Fast2D may consume renderer-neutral render commands, prepare backend-local fill operations, rasterize deterministic plain filled rectangles with clipping, preserve optional shape styling metadata for later backend work, and defer text. Do not make Fast2D the default renderer or add renderer selection/composition, full text/font sharing, richer shape rasterization, rounded corners, borders, antialiasing, platform presentation, mixed-surface composition, Studio, CLI, Canvas2D, Scene3D, shader tools, dashboards, node graphs, a compositor, retained-mode UI, hot reload, Python bindings, or Skia as part of M3 guardrail/doc work.
