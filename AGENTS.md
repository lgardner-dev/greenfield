# Greenfield Agent Instructions

This repository is the Greenfield UI engine, a C++20 UI-first engine intended to power the Omnivity router management UI and future UI-heavy applications.

When modifying this repository, follow the coding standards in `docs/coding-standards.md`.

Important priorities:
1. Prefer readability over cleverness.
2. Use RAII for resource ownership.
3. Follow SOLID principles where they improve clarity and maintainability.
4. Keep functions small and focused.
5. Keep cyclomatic complexity below 10 whenever practical.
6. Prefer descriptive names over abbreviations.
7. Avoid premature abstractions, but keep boundaries clean.
8. Do not add large frameworks, scripting, ECS, editor systems, mobile support, or WASM support until explicitly requested.
9. Keep the UI layer independent from WebGPU-specific details.
10. Do not let widgets directly call WebGPU APIs.