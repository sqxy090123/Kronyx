# Kronyx Engine 文档索引

## 文档导航

- 📖 **系统架构**: [ARCHITECTURE.md](./ARCHITECTURE.md) - 完整的系统架构设计文档
- 🔌 **接口文档**: [INTERFACES.md](./INTERFACES.md) - 核心API接口参考
- 👨‍💻 **开发者指南**: [DEVELOPER_GUIDE.md](./DEVELOPER_GUIDE.md) - 开发、构建、测试指南
- 🔒 **安全审计**: [SECURITY_AUDIT.md](./SECURITY_AUDIT.md) - 脚本引擎安全审计报告
- 🛡️ **反篡改系统**: [anti_tamper.md](./anti_tamper.md) - 跨平台反篡改验证系统
- 📦 **资源管理**: [resource.md](./resource.md) - 资源加载与管理系统

## 核心概念

### 专有概念
- **实体组件系统 (ECS)**: Archetype-based存储的高性能实体组件系统
- **渲染硬件抽象 (RHI)**: 跨平台渲染抽象层，支持控制台和OpenGL后端
- **kyx脚本语言**: 引擎内置的轻量级脚本语言，支持寄存器VM和分代GC
- **反篡改系统**: HMAC-SHA256 + TOTP双组件验证机制

## 模块说明

### 核心模块
- **Core**: 数学、内存管理、容器、日志、时间、事件、输入、文件等基础工具
- **ECS**: Archetype布局的实体组件系统，O(1)实体查找
- **Scene**: 场景序列化与元数据管理（.ksn格式）
- **Resource**: 资源注册表与引用计数管理
- **Render**: RHI抽象 + 2D渲染管线 + 帧动画
- **Physics**: 刚体物理世界（SAP X轴broadphase + AABB narrowphase）
- **Script**: kyx词法/语法/编译/VM/GC/绑定系统
- **Pack**: 脚本与引擎打包工具（exe/npm/jar）

### 运行时模块
- **Runtime**: GLFW窗口、输入轮询与游戏循环
- **Anti-Tamper**: 反篡改验证系统
- **Editor**: Dear ImGui可视化编辑器（可选）

### 示例与工具
- **Demo**: 2D平台人垂直切片，串联所有子系统
- **Tests**: 完整的测试套件（20个测试目标）

## 快速开始

### 构建项目
```bash
cmake -B build && cmake --build build -j2
```

### 运行演示
```bash
cd build && ./ky_demo
```

### 运行测试
```bash
ctest --test-dir build --output-on-failure
```

## 版本信息

- **当前版本**: 0.1.0
- **标准**: C11
- **构建系统**: CMake ≥ 3.20
- **测试框架**: CTest + 自研断言宏

## 许可证

请参考 [LICENSE](../../LICENSE) 文件。