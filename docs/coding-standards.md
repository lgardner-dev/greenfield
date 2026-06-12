# Greenfield Coding Standards

## Core Philosophy

Greenfield code should read like well-written pseudocode.

A reader should be able to understand what a class, function, variable, or file does without decoding abbreviations or jumping through layers of clever abstraction.

Prefer simple, explicit, readable code over clever, dense, or overly generic code.

## C++ Version

Use C++20.

## Design Principles

Follow SOLID principles where they improve clarity and maintainability:

- Single Responsibility Principle: each class or function should have one clear reason to change.
- Open/Closed Principle: prefer extension through clean interfaces rather than modifying unrelated code.
- Liskov Substitution Principle: derived implementations should honor the behavior promised by their interfaces.
- Interface Segregation Principle: prefer small focused interfaces over broad interfaces.
- Dependency Inversion Principle: high-level systems should depend on abstractions, not concrete platform/rendering details.

Do not use SOLID as an excuse to over-engineer. Simple code is preferred when a simple solution is sufficient.

## Resource Management

Use RAII for all owned resources.

Resource-owning classes should acquire resources in constructors or explicit factory functions and release them in destructors.

Avoid naked ownership of raw pointers. Prefer:

- stack values
- `std::unique_ptr`
- `std::shared_ptr` only when shared ownership is truly required
- `std::span` or references for non-owning views

Do not manually pair allocation and cleanup throughout business logic.

## Naming

Prefer descriptive names over abbreviations.

Names should make the code read like pseudocode.

Good examples:

- `RenderCommandList`
- `CreateWebGpuDevice`
- `HandleWindowResize`
- `SubmitRenderCommands`
- `MouseButtonState`
- `CurrentFrameWidth`

Avoid unclear abbreviations:

- `cmds`
- `ctx`
- `buf`
- `wnd`
- `mgr`
- `cfg`
- `tmp`
- `val`

Short names are acceptable only for very narrow scopes where the meaning is obvious, such as loop indices in small loops.

Prefer:

- Classes and structs: `PascalCase`
- Functions and methods: `PascalCase`
- Variables and parameters: `camelCase`
- Private member variables: `_camelCase`
- Constants: `PascalCase` or `kPascalCase`, but be consistent
- Files: match the primary type name where practical

## Function Design

Keep functions small, focused, and readable.

Prefer functions that do one thing clearly.

Avoid deeply nested control flow.

Use guard clauses to reduce nesting.

Aim for cyclomatic complexity below 10 for each function whenever practical.

If a function becomes difficult to read, split it into named helper functions that describe the steps.

Prefer this:

```cpp
void RenderFrame()
{
    BeginFrame();
    SubmitUserInterfaceCommands();
    PresentFrame();
}