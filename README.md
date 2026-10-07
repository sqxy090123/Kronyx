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
- **Rendering**: RHI abstraction layer, OpenGL 3.3 Core + Vulkan 1.2 (lavapipe) backend
- **Physics**: Self-authored rigid body physics with SAP broadphase, AABB narrowphase, joints, particle + audio subsystems
- **Scripting**: Custom kyx language with forced-comment mechanism, register-based VM + generational GC
- **Packaging**: exe / npm / jar / APK skeletons (`pack.c`); APK emits a deployable Android project (manifest + Activity + JNI bridge + NDK CMake + `assets/game.kyx`)
- **Editor**: Dear ImGui-based editor with viewport, hierarchy, property inspector
- **Cross-platform**: Windows, Linux, macOS, Android via CMake

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
| render | src/render/ | RHI abstraction + OpenGL 3.3 Core + Vulkan 1.2 (lavapipe) backends |
| physics | src/physics/ | Rigid body physics (SAP broadphase, AABB narrowphase, joints) |
| particle | src/particle2d/ | Emitter component + particle-update system + render pass |
| audio | src/audio/ | Parametric SFX synthesizer + 32-voice mixer (null sink) |
| script | src/script/ | kyx lexer, parser, register-based VM + generational GC |
| pack | src/pack/ | exe / npm / jar / APK project-skeleton generators |
| editor | tools/editor/ | ImGui editor panels |

## Roadmap

- [x] P0: Core layer (math, memory, containers, log, time)
- [x] P1: ECS + Scene + Resource Manager
- [x] P2: Render RHI + OpenGL stub
- [x] P3: Physics engine (SAP, GJK/EPA, PGS solver)
- [x] P4: kyx scripting language (lexer/parser/VM + generational GC + stdlib bindings)
- [ ] P5: ImGui editor + kyx debugger
- [x] P6: Example games + Vulkan (lavapipe) backend + APK packaging skeleton

## License

Proprietary.
