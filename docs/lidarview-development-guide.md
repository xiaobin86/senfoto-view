# LidarView 二次开发、编译与启动指南

本文档记录 LidarView 在 `senfoto-view` 工作区中的完整开发流程，包括仓库结构、初始构建、插件二次开发、重新编译、安装与启动。

## 1. 仓库与目录结构

工作区根目录：`/mnt/d/work/senfoto-view`

```
senfoto-view/
├── lidarview/              # LidarView 主源码
│   ├── Application/        # 主程序入口
│   ├── LidarCore/          # 核心库（IO、Filter、Common 等）
│   ├── Plugins/            # 官方插件（Velodyne、LakiBeam、Senfoto008 等）
│   ├── Examples/Plugins/   # 示例插件
│   ├── Qt/                 # Qt UI 模块
│   ├── Testing/            # 测试框架
│   ├── CMakeLists.txt      # 顶层 CMake，注册默认插件
│   └── ...
├── lidarview-superbuild/   # 超级构建脚本（下载/编译依赖）
├── lidarview-release/      # 发布相关
├── build/                  # 构建输出（由 superbuild 生成）
│   ├── superbuild/         # 依赖库构建目录
│   │   ├── lidarview/build # LidarView 本体的 ninja 构建目录
│   │   └── paraview/       # ParaView/VTK 构建目录
│   └── install/            # 最终安装目录（可执行文件、插件、库）
└── docs/                   # 项目文档
```

## 2. 初始构建（首次）

首次构建已通过 superbuild 完成，产物位于：

- 构建目录：`/mnt/d/work/senfoto-view/build/superbuild/lidarview/build`
- 安装目录：`/mnt/d/work/senfoto-view/build/install`

若需重新完整构建，通常从 `build/` 目录执行：

```bash
cd /mnt/d/work/senfoto-view/build
ninja
```

superbuild 会负责 ParaView/VTK、Boost、PCAP++ 等依赖，并最终编译 LidarView。

## 3. 插件二次开发

### 3.1 插件目录规范

每个插件位于 `lidarview/Plugins/<PluginName>/`，典型结构如下（以 Senfoto008Plugin 为例）：

```
lidarview/Plugins/Senfoto008Plugin/
├── paraview.plugin                          # 插件元数据
├── CMakeLists.txt                           # 顶层插件 CMake，注册模块和测试
├── Senfoto008Proxies.xml                    # 暴露 Reader/Stream 名称
└── Senfoto008PacketInterpreters/
    ├── vtk.module                           # VTK 模块依赖声明
    ├── CMakeLists.txt                       # 模块构建 + ServerManager XML 注册
    ├── Senfoto008PacketFormat.h             # 协议常量与解析 helper
    ├── vtkSenfoto008PacketInterpreter.h     # 解释器类声明
    ├── vtkSenfoto008PacketInterpreter.cxx   # 解释器实现
    ├── Senfoto008PacketInterpreter.xml      # 内部解释器代理
    ├── Senfoto008LidarReader.xml            # Reader 源代理
    ├── Senfoto008LidarStream.xml            # Stream 源代理
    └── Testing/
        ├── CMakeLists.txt
        └── TestSenfoto008PacketInterpreter.cxx
```

### 3.2 注册新插件

在 `lidarview/CMakeLists.txt` 的 `lidarview_default_plugins` 列表中加入插件名：

```cmake
set(lidarview_default_plugins
  LidarCorePlugin
  DatasetIOPlugin
  VelodynePlugin
  LeishenPlugin
  LakiBeamPlugin
  Senfoto008Plugin    # <-- 新增
  LivoxPlugin
  RobosensePlugin
  HesaiPlugin
)
```

## 4. 重新编译

所有二次开发后的编译命令都在 LidarView 本体构建目录中执行：

```bash
cd /mnt/d/work/senfoto-view/build/superbuild/lidarview/build
```

### 4.1 全量编译

```bash
ninja
```

### 4.2 只编译某个插件

```bash
ninja Senfoto008Plugin
```

### 4.3 编译测试

```bash
ninja TestSenfoto008PacketInterpreter
```

### 4.4 运行测试

```bash
ctest -R TestSenfoto008PacketInterpreter -V
```

### 4.5 安装到运行目录

**关键步骤**：LidarView 实际运行读取的是 `build/install/` 下的插件，编译后必须安装：

```bash
ninja install
```

如果只 `ninja` 不 `ninja install`，UI 里看不到更新。

## 5. 启动 LidarView

安装完成后，从安装目录启动：

```bash
/mnt/d/work/senfoto-view/build/install/bin/LidarView
```

启动时会自动加载 `build/install/lib/lidarview/plugins/` 下的插件。

### 5.1 插件未出现时的排查

1. **是否执行了 `ninja install`**
   ```bash
   find /mnt/d/work/senfoto-view/build/install -name "*Senfoto008*"
   ```

2. **是否已注册到 `lidarview.plugins.xml`**
   ```bash
   grep Senfoto008 /mnt/d/work/senfoto-view/build/install/lib/lidarview/plugins/lidarview.plugins.xml
   ```

3. **是否完全重启了 LidarView**（修改插件后必须重启，不能热加载）

4. **查看启动日志**是否有加载失败或符号未找到的错误

## 6. 常用开发命令速查

| 目的 | 命令 |
|------|------|
| 完整编译 | `ninja` |
| 编译指定插件 | `ninja Senfoto008Plugin` |
| 编译指定测试 | `ninja TestSenfoto008PacketInterpreter` |
| 安装 | `ninja install` |
| 运行测试 | `ctest -R TestSenfoto008PacketInterpreter -V` |
| 启动程序 | `/mnt/d/work/senfoto-view/build/install/bin/LidarView` |
| 查看 ninja 目标 | `ninja -t targets \| grep Senfoto008` |

## 7. 开发注意事项

- **不要直接提交**：当前 hard constraint 禁止未经用户明确请求的 commit。
- **修改 CMake 后**：`ninja` 会自动重新跑 CMake，通常无需手动 `cmake .`。
- **BUILD_TESTING**：测试默认关闭。需要测试时可在 lidarview build 目录执行 `cmake -DBUILD_TESTING=ON .`，然后重新编译。
- **垂直角表/标定数据**：多线机械 LiDAR 的几何正确性依赖于俯仰角表，通常硬编码在 `*PacketFormat.h` 中；替换时必须重新编译并安装。
- **坐标约定**：LidarView 中点云坐标通常按 `x = dist·cos(el)·cos(az)`、`y = dist·cos(el)·sin(az)`、`z = dist·sin(el)` 计算，与具体雷达的 zero/forward 方向需对齐。

## 8. 参考文档

- 设计规格：`docs/superpowers/specs/2026-08-24-senfoto008-design.md`
- 实现计划：`docs/superpowers/plans/2026-08-24-senfoto008.md`
- 硬件规范：`docs/lidar-hardware-specifications.md`
