# Kronyx Engine - 许可证说明

## 主要许可证
**Kronyx Engine 采用 MIT 许可证**

Copyright (c) 2024 Kronyx Engine

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.

## 第三方依赖

### 标准库
- **C Standard Library (libc)** - 无特定许可证，系统自带
- **libm (数学库)** - BSD/GPL 许可，系统自带
- **Windows SDK** (time.c) - Microsoft 许可，仅 Windows 平台

### 算法
以下算法为公开算法实现，非特定库代码：
- **FNV-1a 哈希算法** - 用于 hashmap，由 Glenn Fowler 等人设计
- **SAP (Sweep and Prune)** - 碰撞检测算法，经典游戏开发技术
- **Quicksort** - 标准排序算法，C 库 qsort 实现

### 无其他第三方库
本项目未链接或包含以下类型的第三方代码：
- 无 GPL/LGPL 代码
- 无 AGPL 代码
- 无复制粘贴的开源库代码
- 无动态链接的第三方 DLL/SO

## 测试状态
```
ky_test_core:     33 assertions, 0 failures
ky_test_math:     33 assertions, 0 failures
ky_test_ecs:      41 assertions, 0 failures
ky_test_render:    9 tests ran, 0 failures
ky_test_physics:  30 tests ran, 0 failures
------------------------------------
总计:            146 tests, 0 failures
```

## 代码原创性
所有源代码均为原创实现，遵循以下原则：
1. 仅使用公开算法和数据结构
2. 不复制任何开源项目的具体代码
3. 所有实现均从头编写
4. 符合 MIT 许可证要求
