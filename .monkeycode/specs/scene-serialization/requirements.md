# G6 · 场景序列化 · 需求

## 目标

`kyScene` 能把 ECS 世界中的实体 + Transform / Sprite / Camera2D 写入文本文件（`.ksn`，Kronyx Scene），并能从文件读回重建完全相同的组件状态。格式须可读、无二进制 blob（纹理路径存字符串，纹理在加载时由渲染层按路径重建）。

## 角色

- **Scene 序列化器**：读/写 `.ksn` 文件，零外部依赖（只用 `file.h` + `string.h`）。
- **测试**：无头逻辑测试验证 round-trip。
- **编辑器**（G5 后续可用）：通过 API 保存/打开场景文件。

## 需求

### R1 文件格式

- R1.1 格式为 UTF-8 文本，以 `scene` 行开头，后跟可选元数据块，再跟若干 `entity` 块。
- R1.2 注释行以 `#` 开头。空行忽略。
- R1.3 基本骨架：

```
scene "name" v1
meta author "dev"

entity "camera"
  transform x="0" y="0" rotation_z="0" scale_x="1" scale_y="1" z="0"
  camera2d x="0" y="0" rotation_z="0" zoom="1" viewport_w="800" viewport_h="600" clear_r="0.1" clear_g="0.1" clear_b="0.1" clear_a="1" active="1"
end

entity "player"
  transform x="1" y="2" rotation_z="0" scale_x="2" scale_y="3" z="0"
  sprite layer="5" flip="0"
end
```

- R1.4 字段名小写，snake_case；数值用 float（允许整数字面量）。
- R1.5 `sprite` 可带 `texture` 属性（文件路径），省略则用默认白色 1x1 纹理。
- R1.6 entity 的 `transform` 是必填块；`sprite` 和 `camera2d` 可选。

### R2 写入 API

- R2.1 `ky_scene_save(s, path)`：把 world 中所有有 Transform 组件的实体序列化到文件。
- R2.2 输出目录不存在时返回错误而非崩溃。
- R2.3 文件已存在时被覆盖。
- R2.4 保存失败（IO 错误、OOM）返回负码。

### R3 读取 API

- R3.1 `ky_scene_load(s, path)`：清空 s->world，重新注册 Transform/Sprite/Camera2D，然后从文件重建实体。
- R3.2 只识别本格式支持的组件块；未知块/字段忽略并打 warn。
- R3.3 缺少必填字段（transform 缺失）返回错误。
- R3.4 读取失败（文件不存在、解析错乱）返回负码。

### R4 场景生命周期

- R4.1 `ky_scene_save` 不修改 s->world（只读）。
- R4.2 `ky_scene_load` 后 world 中实体与文件一致（id 可能不同，但组件状态一致）。
- R4.3 支持空场景（0 实体）读写。

### R5 测试

- R5.1 写一个场景，读回，断言每个实体在两个 world 中组件状态完全一致。
- R5.2 验证空场景读写。
- R5.3 验证缺失文件、格式错误等错误路径返回负码。

## 验收

- `tests/test_scene_serialize.c` 绿（15+ 断言）。
- 全量 `ctest`（ASan, `detect_leaks=0`）17/17 绿。
- `.ksn` 文件人类可读、可手动编辑。

## 非目标

- 通用反射、二进制版本迁移
- 场景内资源依赖解析（纹理路径只是字符串，不自动拷贝资源）
- 脚本绑定场景操作
