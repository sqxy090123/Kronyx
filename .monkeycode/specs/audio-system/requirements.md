# G11 音频模块 需求文档

Feature: audio-system
Updated: 2026-09-29

## Introduction

G11 交付引擎首个音频模块 `audio`：纯 C 的 **PCM 波形合成器 + 混音器**，
以 ECS `"emitter"` 风格的 `kySoundEmitter` 组件接入，宿主 / demo 可随时
`ky_audio_play()` 触发一段 SFX（正弦扫频、噪声 burst、方波 tick）。
模块自带**无设备输出后端（null sink）**——进程内把波形混入一块 `float`
mix buffer，**不依赖任何系统音频库**（无 OpenAL / SDL_mixer / miniaudio），
与渲染层"console 后端零依赖"策略对齐；真实设备输出（ALSA / CoreAudio /
WASAPI）留待 G11+。

本模块的价值：
- 给 demo（跳跃 / 落地脉冲）与未来的 kyx 脚本绑定（`std.play`）提供一条
  可测、可确定性复现的音频通路；
- 为 G11 之后的设备后端预留清晰的 C API 缝（`kyAudioDevice` 抽象），
  后续实现 ALSA 时只加一个 sink 实现，宿主代码零改动。

## Glossary

- **PCM 波形**: 单声道 32-bit float 线性采样序列，模块默认采样率 44100 Hz。
- **SFX 片段 (clip)**: 一段合成波形，由合成器按参数生成（非文件解码；
  V1 不做 WAV 解码，只做参数化合成）。
- **mix buffer**: 进程内 `float` 环形/定长缓冲，每次 `ky_audio_mix(dt)`
  清零累加所有活跃 clip 的后续采样，宿主可读取用于验证或转发给设备。
- **kySoundEmitter**: ECS 组件，描述一个 SFX 源（参数 + 当前播放状态）。
- **null sink**: 无系统设备的输出后端；把混音结果写入模块 mix buffer，
  供测试断言与后续设备后端消费。

## Requirements

### Requirement 1: 合成器 — 参数化 SFX 生成

**User Story:** AS 宿主开发者，I want 按参数合成一段 SFX 波形，so that
跳跃/落地/命中等音效无需外部资产文件即可使用。

#### Acceptance Criteria

1. WHEN 宿主调用 `ky_audio_synthesize(params)` 且 `params.kind` 为
   `KY_SFX_SINE_SWEEP` / `KY_SFX_SQUARE` / `KY_SFX_NOISE` / `KY_SFX_CLICK`
   之一，音频模块 SHALL 返回一段非 NULL 波形，长度 =
   `ceil(params.duration * sample_rate)` 个采样，且 `sample_rate` 缺省 44100。
2. IF `params.duration` <= 0 或 `params.kind` 非法，`ky_audio_synthesize`
   SHALL 返回 NULL（不写栈上出参）。
3. WHEN 两个 `kySfxParams` 逐字段相等，音频模块 SHALL 生成逐采样相等的
   波形（确定性：噪声类使用可设 seed 的 LCG，同 seed 复现）。
4. WHILE 波形生成中，所有采样值 SHALL 落在 [-1.0, 1.0] 闭区间内（内置
   软限幅，防后续混音削波）。

### Requirement 2: 播放与混音

**User Story:** AS 宿主 / demo，I want 在某一时刻播放一个 clip 并让多个
clip 叠加，so that 跳跃与落地音效可以重叠而不串音。

#### Acceptance Criteria

1. WHEN 宿主调用 `ky_audio_play(clip, volume)` 且 clip 非 NULL、volume 在
   [0,1] 内，音频模块 SHALL 使该 clip 在后续 N 次 `ky_audio_mix` 中
   按采样步进推进，N = 波形采样数 / 每帧采样数；推进期间 `ky_audio_active_count()`
   计数 +1。
2. WHILE 同一 clip 有多个并发播放请求，音频模块 SHALL 允许至多
   `KY_AUDIO_MAX_VOICES=32` 个并发 voice；超出时静默丢弃新请求（不报错、
   不崩溃），`ky_audio_active_count()` 不超过 32。
3. WHEN 宿主调用 `ky_audio_mix(dt)`，音频模块 SHALL 将 dt 秒内应产生的
   所有采样累加进 mix buffer（长度 = `dt * sample_rate`，向上取整，
   上限 `KY_AUDIO_MIX_MAX=44100*2` 采样），越界采样静默截断，并返回
   实际写入 mix buffer 的采样数。
4. IF `ky_audio_mix` 的 `dt` <= 0，音频模块 SHALL 不推进任何 voice、
   不清空 mix buffer，返回 0。

### Requirement 3: 与 ECS / demo 集成

**User Story:** AS demo 垂直切片，I want 角色跳跃 / 落地时自动播一声，
so that 音频成为 demo 垂直切片的第四个可感知子系统（粒子已是第三个）。

#### Acceptance Criteria

1. WHEN demo 的 role 触发跳跃（vy 被置为 JUMP_VY），demo SHALL 调用一次
   `ky_audio_play(jump_clip, 0.6)`；当 role 触地（grounded 由 0 变 1），
   demo SHALL 调用一次 `ky_audio_play(land_clip, 0.4)`。
2. WHEN demo 每 tick 推进一次 `ky_world_step(dt)`，demo SHALL 调用
   `ky_audio_mix(dt)` 以把音频时间轴与物理 / 粒子时间轴对齐。
3. IF 宿主调用了 `ky_audio_register_components(w)`，音频模块 SHALL 注册
   `"sound"` 组件与 `"audio-update"` 系统（幂等），使挂有 `kySoundEmitter`
   的实体在 `ky_world_step` 时按自身 `trigger` 字段自动播放（trigger 在
   本帧置 1 时播放一次并清零）。

### Requirement 4: 无设备后端与可测试性

**User Story:** AS CI 套件，I want 在无系统音频设备的无头环境验证音频
行为，so that 音频模块像粒子 / console 渲染一样可被 ctest 覆盖。

#### Acceptance Criteria

1. 音频模块 SHALL 提供一个 null 输出 sink（默认且唯一 V1 sink），把混音
   结果写入进程内 mix buffer；宿主 SHALL 能通过 `ky_audio_mix_buffer()`
   拿到该 buffer 指针与长度用于断言。
2. WHEN 测试进程从空状态开始播放一个 `KY_SFX_CLICK`（duration 0.05s）
   并 mix 满 0.05s，mix buffer SHALL 含非零采样（证明通路打通），且
   后续再 mix 0.05s 后该 voice 计数归零（clip 播完自动回收）。
3. 模块的 mix buffer / voice 表为进程内静态（与 `particle2d` / `2d.c`
   的 `g_items` 同模式），测试 SHALL 用相对计数（基线差值）而非绝对值断言。

### Requirement 5: 资源管理与生命周期

**User Story:** AS 宿主，I want 显式释放合成的波形而不泄漏，so that
长驻 demo 不会累积波形内存。

#### Acceptance Criteria

1. WHEN 宿主拿到 `ky_audio_synthesize` 返回的 `kyAudioClip*`，宿主 SHALL
   在用完时调用 `ky_audio_clip_free(clip)` 释放；`ky_audio_clip_free(NULL)`
   为空操作。
2. WHILE 一个 clip 正被 voice 引用，宿主提前 `ky_audio_clip_free` 后，
   相关 voice SHALL 在下一次 `ky_audio_mix` 时被安全跳过（模块通过
   `clip->alive` 标志守卫），不读写已释放内存（ASan 干净）。
3. WHEN 宿主调用 `ky_audio_shutdown()`，音频模块 SHALL 回收全部 voice
   与 mix buffer，使 `ky_audio_active_count()` 归零；调用幂等。

## Out of Scope (V1 不做)

- 系统音频设备输出（ALSA / CoreAudio / WASAPI）；设备后端仅预留
  `kyAudioDevice` 缝。
- WAV / OGG 文件解码；仅参数化合成。
- 多声道 / 立体声 / 空间音频。
- kyx 脚本 `std.play` 绑定（G11+ 绑定层扩展，非本模块）。
- 编辑器音频面板。

## INCOSE 自检

- 每条 AC 单一可测概念、主动语态、无模糊词 / 兜底条款。
- 计数类断言均给出上限与缺省值（44100 / 32 / 44100*2 / 0x12345678）。
- "静默丢弃 / 截断" 替代 "绝不" 类绝对词，给出可观测后果。
