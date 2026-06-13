# Greenfield C++/CMake App Template

This is a minimal scaffold for the intended shape of a future exported Greenfield app project.

It is not a project generator, CLI export output, install package, or standalone release workflow. The root Greenfield build does not include this template automatically.

## Intent

- C++/CMake-first app project.
- Separate from `apps/sandbox`.
- Owns its app target and composition-root policy.
- Consumes Greenfield SDK/runtime targets from a generated workspace, vendored SDK checkout, or future package/export mechanism.
- Wires concrete platform and renderer targets in the app composition root.
- Treats Linux as the current primary development path.
- Preserves Windows and browser-hosted WebAssembly as future target/export considerations.

## Current Limits

M5 does not implement a CLI, project generator, install rules, package/export logic, Windows packaging, browser-hosted WebAssembly support, or optional Dawn/WebGPU/FreeType behavior.

The CMake file is intentionally illustrative. It expects Greenfield SDK/runtime targets to already exist in the containing build or future exported workspace.
