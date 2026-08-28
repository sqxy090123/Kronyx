# Kronyx Engine

Cross-platform, data-oriented game engine written in C11 with a built-in scripting language.

## Architecture

```
App / Tools (Editor, Demos)
    |
Kronyx Layer (Game Scripts, kyx VM)
    |
Engine Layer (ECS, Scene, Resource, Physics, Audio)
    |
RHI + Render Core (GL / Vulkan Backend)
    |
Platform Layer (Window, Input, FS, Time, Thread)
```

## Features

- **ECS**: Archetype-based entity-component system with O(1) component lookup
- **Rendering**: RHI abstraction layer, OpenGL 3.3 Core backend (Vulkan 1.2 deferred)
- **Physics**: Self-authored rigid body physics with SAP broadphase, GJK/EPA narrowphase
- **Scripting**: Custom kyx language with forced-comment mechanism, register-based VM
- **Editor**: Dear ImGui-based editor with viewport, hierarchy, property inspector
- **Cross-platform**: Windows, Linux, macOS via CMake

## Build

```bash
cmake -B build -DKYR_BUILD_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Modules

| Module | Path | Description |
|--------|------|-------------|
| core | src/core/ | Math, memory, arrays, hashmap, strings, logging, timing |
| ecs | src/ecs/ | Archetype-based entity-component system |
| scene | src/scene/ | Scene graph with metadata |
| resource | src/resource/ | Resource manager with reference counting |
| render | src/render/ | RHI abstraction + OpenGL stub backend |
| physics | src/physics/ | Rigid body physics (SAP, GJK/EPA, PGS) |
| script | src/script/ | kyx lexer, parser, VM (in progress) |
| editor | tools/editor/ | ImGui editor panels (pending) |

## Roadmap

- [x] P0: Core layer (math, memory, containers, log, time)
- [x] P1: ECS + Scene + Resource Manager
- [x] P2: Render RHI + OpenGL stub
- [ ] P3: Physics engine (SAP, GJK/EPA, PGS solver)
- [ ] P4: kyx scripting language full stack
- [ ] P5: ImGui editor + kyx debugger
- [ ] P6: Example games + Vulkan backend

## License

Proprietary.
