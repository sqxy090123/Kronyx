# G11 音频模块 任务清单

- [x] 规格：`requirements.md` + `design.md`（EARS + 架构 + 测试策略）
- [x] `include/kronyx/audio.h`：`kySfxParams` / `kyAudioClip` / `kySoundEmitter` + API
- [x] `src/audio/audio.c`：合成器（4 kind + LCG + 软限幅）、voice 表（32）、mix buffer（null sink）、`audio-update` 系统、模块自有 dead-clip 跟踪（防 UAF）
- [x] CMake：`audio.c` 入 `ky_engine`；`ky_test_audio` 挂 ctest（22→23）
- [x] demo 接线：跳跃 SINE_SWEEP + 落地 CLICK 脉冲，每 tick `ky_audio_mix(dt)`，teardown 释放
- [x] `tests/test_audio.c`（61 断言）+ CMake `ky_test_audio`
- [x] 全量 `ctest -j1` 23/23 + ASan 23/23 + `ky_demo` 无头冒烟 exit=0
- [x] 回写 `progress.md` G11 + `.monkeycode/docs/模块/Audio.md` 活契约
