# 音频模块 (Audio)

## 概述

`audio` 是引擎首个音频子系统，G11 切片交付。纯 C 的 **PCM SFX 合成器 +
混音器**，null-sink 输出到进程内 `float` mix buffer，**不依赖任何系统音频
库**（无 OpenAL / SDL / miniaudio），与渲染层 console 后端、粒子池同策略：
无头可测、宿主零配置、确定性可复现。真实设备输出（ALSA / CoreAudio /
WASAPI）留待后续，C API 缝（`ky_audio_play/mix`）已就位，设备后端只替换
sink 实现即可，宿主代码零改动。

## 头文件

- `include/kronyx/audio.h`

## API

| 函数 | 说明 |
|------|------|
| `ky_sfx_params_new(kind, dur, f0, f1, amp)` | 返回默认 `kySfxParams`（seed=0） |
| `kyAudioClip *ky_audio_synthesize(p)` | 参数 -> 波形；`p==NULL` / `duration<=0` / 非法 kind 返回 NULL；采样全 clamped 到 [-1,1] |
| `void ky_audio_clip_free(c)` | 释放波形；NULL 空操作；置模块自有 dead 跟踪 |
| `int ky_audio_play(c, vol)` | 入队 voice（上限 32）；返回新 voice 数，满/非法返回 -1 |
| `int ky_audio_mix(dt)` | 推进全部 voice，累加进 mix buffer；返回写入采样数；`dt<=0` 返回 0 |
| `float *ky_audio_mix_buffer()` | mix buffer 指针 |
| `size_t ky_audio_mix_buffer_samples()` | 最近一次 mix 写入的采样数 |
| `int ky_audio_active_count()` | 当前 voice 数 |
| `void ky_audio_shutdown()` | 回收 voice + 清 buffer + 清 dead 跟踪；幂等 |
| `int ky_audio_register_components(w)` | 注册 `"sound"` 组件 + `"audio-update"` 系统，幂等；NULL 返回 -1 |

## 常量

| 常量 | 值 | 含义 |
|------|----|----|
| `KY_AUDIO_SAMPLE_RATE` | 44100 | 默认采样率 Hz |
| `KY_AUDIO_MAX_VOICES` | 32 | 并发 voice 上限（超出静默丢弃） |
| `KY_AUDIO_MIX_MAX` | 44100*2 | 单帧 mix buffer 上限采样数 |

## 合成种类

| kind | 说明 |
|------|------|
| `KY_SFX_SINE_SWEEP` | 正弦 `freq0` -> `freq1` 扫频，含 5% 淡入淡出 |
| `KY_SFX_SQUARE` | 方波 `freq0`，幅度 x0.5 |
| `KY_SFX_NOISE` | xorshift32 白噪声 burst，`seed` 可设复现，线性衰减包络 |
| `KY_SFX_CLICK` | 短促二次衰减脉冲（首采样 +1，后续 -1 交替） |

## 使用范例

```c
ky_audio_register_components(w);  /* 可选：ECS 自动触发 */

kyAudioClip *jump = ky_audio_synthesize(
    &ky_sfx_params_new(KY_SFX_SINE_SWEEP, 0.08f, 300.0f, 900.0f, 0.7f));
ky_audio_play(jump, 0.6f);

for (int i = 0; i < 60; i++) {
    ky_world_step(w, 1.0f/60.0f);   /* 含 audio-update 系统 */
    ky_audio_mix(1.0f/60.0f);       /* 音频与物理时间轴对齐 */
}
ky_audio_clip_free(jump);
ky_audio_shutdown();
```

## 行为契约

- **确定性**：同 `kySfxParams`（含 seed）逐采样相等。
- **voice 上限**：第 33 个 `ky_audio_play` 静默丢弃，`active_count` 保持 32。
- **clip 提前释放**：voice 经**模块自有 dead-clip 集合**检测已释放指针，
  下一次 mix 安全跳过，**不解引用已释放结构体**（ASan 干净）。
- **mix 帧数**：`min(ceil(dt*SR), KY_AUDIO_MIX_MAX)`。
- **mix 越界截断**：voice 索引越过 clip 末尾即回收该 voice。

## 测试

`tests/test_audio.c`（61 断言），ctest 目标 `audio`。覆盖：合成合法/非法/
确定性/软限幅、播放参数校验、voice 满 32 截断、mix 帧数/回收/dt<=0、ECS
trigger 自动播放、提前 free clip 安全跳过、shutdown 幂等。

## 不做（本切片）

系统音频设备输出（ALSA / CoreAudio / WASAPI）、WAV/OGG 解码、多声道/立体声/
空间音频、kyx 脚本 `std.play` 绑定、编辑器音频面板。
见 `.monkeycode/specs/audio-system/`。
