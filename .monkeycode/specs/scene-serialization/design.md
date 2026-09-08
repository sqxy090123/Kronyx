# G6 · 场景序列化 · 设计

## 概览

`kyScene` 是 ECS world 的可序列化容器。序列化格式为纯文本 `.ksn`（Kronyx Scene）。读写只依赖 `file.h` 和 `string.h`，不依赖 renderer 或 input。

```
ky_scene_save(scene, path)
  → 遍历 world 中所有有 Transform 的实体
  → 按 entity/component 块结构写入文本
  → 返回 0 或负错误码

ky_scene_load(scene, path)
  → 清空 world（destroy + recreate）
  → 注册 transform/sprite/camera2d 组件类型
  → 逐行解析文件，创建实体并填充组件
  → 返回 0 或负错误码
```

## 文件格式

```
# 注释行（忽略）

scene "name" v1
meta key "value"         # 可选，多行

entity "name_or_empty"   # 实体定义开始
  transform x="0" y="0" rotation_z="0" scale_x="1" scale_y="1" z="0"
  sprite layer="0" flip="0" texture="assets/foo.png"
  camera2d x="0" y="0" rotation_z="0" zoom="1"
           viewport_w="800" viewport_h="600"
           clear_r="0.1" clear_g="0.1" clear_b="0.1" clear_a="1" active="1"
end                       # 实体定义结束
```

**字段规则**：
- `transform`：必填；x/y/rotation_z/scale_x/scale_y/z（float）
- `sprite`：可选；layer(int)/flip(int)/texture(path)
- `camera2d`：可选；x/y/rotation_z/zoom/(viewport_w/viewport_h)/clear_r/g/b/a/active

## 文件

| 文件 | 职责 |
|------|------|
| `include/kronyx/scene.h` | 扩展 `kyScene` 结构体 + save/load API 声明 |
| `src/scene/scene.c` | 实现 save/load，行解析器，组件写入 |
| `tests/test_scene_serialize.c` | round-trip + 错误路径测试 |
| `CMakeLists.txt` | `ky_test_scene_serialize` 目标 |

## 实现要点

### 行解析器

使用 `kyString` 累积当前行，按空格分割 key="value" 对：
1. 跳过空行和 `#` 注释行
2. 遇到 `scene` 行：读取 name 和 version
3. 遇到 `entity` 行：开始新实体块，记录缩进级别
4. 遇到 `end` 行：结束当前实体块
5. 遇到 `key="value"`：解析后存入当前组件的属性 map

### 属性存储

每个组件用 `kyHashMap` 存 key→string，由行解析器填充。save 时遍历 map 输出；load 时根据 key 映射到对应 struct 字段。

### 场景保存

遍历 world 中所有 alive 实体（通过 `ky_world_alive_count` + `ky_world_get_alive_entity`），对每个有 `transform` 组件的实体：
- 写入 `entity "unnamed"` 或 entity id 字符串
- 写入 transform 字段
- 若有 sprite 组件，写入 sprite 字段（texture 路径存 NULL 则省略）
- 若有 camera2d 组件，写入 camera2d 字段
- 写入 `end`

### 场景加载

1. `ky_file_read` 读整个文件到内存
2. 逐行解析，维护状态机（outside/inside_entity/inside_component）
3. 碰到 `entity` 行：spawn 实体，进入 entity 块
4. 碰到组件行（`transform`/`sprite`/`camera2d`）：parse 属性，spawn 组件并填值
5. 碰到 `end` 行：退出 entity 块

### 纹理处理

纹理路径存为字符串属性；加载时不创建纹理（无 renderer），由调用方按需通过 `ky2d_make_texture` + `ky_world_add_component` 补充。save 时若 sprite.texture == NULL 则省略 texture 属性（表示使用默认白色）。

## 错误码

| 值 | 含义 |
|----|------|
| 0 | 成功 |
| -1 | 文件不存在 / IO 错误 |
| -2 | OOM |
| -3 | 解析错误（格式非法） |
| -4 | 缺少必填字段 |

## 依赖

- `kronyx` core（ecs、memory、string、file）
- `ky_2d`（kyTransform、kySprite、kyCamera2D 定义）
- 无需 renderer（纹理路径只是字符串）

## 退出标准

- `ky_test_scene_serialize` 绿（15+ 断言）。
- ASan 全量 ctest 17/17 绿。
- `.ksn` 文件可手工编辑。
