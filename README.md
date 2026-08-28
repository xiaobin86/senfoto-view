![LidarView](Application/Client/Resources/Images/About.png)

# Introduction

LidarView performs real-time reception, recording, visualization and processing of 3D LiDAR data.

LidarView currently supports a variety of LiDAR models from multiple manufacturers, including:
- `Velodyne`: VLP-16, VLP-32, HDL-32, HDL-64, Puck LITE, Puck Hi-Res and Alpha Prime (VLS-128).
- `Ouster`: OS0, OS1, OS2 and OSDome
- `Hesai`: Pandar40P, Pandar40M, Pandar64, Pandar20A, Pandar20B, PandarQT, PandarXT-16, PandarXT-32, PandarXTM, Pandar128, OT128 and QT128.
- `Robosense`: RS16, RS32, BPerl, Airy, Helios (16 & 32), Ruby (48, 80 & 128), Ruby Plus (48, 80 & 128), M1, M2, M3, MX and E1.
- `Livox`: Mid-40, Tele-15, Horizon, Mid-70, Avia, Mid-360 and HAP.
- `Leishen`: C16, C32 and MS_C16.

Additional sensor models may be supported upon request, provided that the drivers or specifications are publicly available.

This open-source codebase, developed by Kitware, is widely adopted by many LiDAR vendors, often rebranded under different names, to display their live LiDAR data.

Many of these LiDAR sensors sweep an array of lasers (often 8 to 128) 360&deg;
with a vertical field of view of tens of degrees at a 5-20Hz spinning frequency,
capturing about a million points per second.

LidarView can display live sensors' streams or playback pre-recorded data stored in `.pcap` files.

LidarView displays the distance measurements from the LiDAR as point cloud
data and custom color maps for multiple variables such as
intensity-of-return, time, distance, azimuth, and laser id. 

The processed data can be exported in multiple file formats (CSV, PLY, LAS, ...),
and screenshots of the currently displayed point cloud can be easily exported with the help of a button.

As a [Paraview](https://www.paraview.org/) based application, LidarView can effortlessly offer Paraview's features and plugins.

![LidarView](Application/Client/Resources/Images/LidarViewExample.png)
    Lidar data processed by [Kitware's SLAM](#slam) within LidarView

# Senfoto 叉子 · 开发者快速开始

本仓库是 [LidarView](https://www.paraview.org/) 的 **Senfoto 派生版**，在 LidarView 基础上集成了
Senfoto008 / LakiBeam 等自研激光雷达插件，编译产物名为 **SenFoToView**。

> ⚠️ 必须克隆 **`develop`** 分支并拉取子模块：Senfoto 插件只提交在 `develop`，`master` 上没有。

## 一键初始化（推荐）

```bash
git clone -b develop --recursive https://github.com/xiaobin86/senfoto-view.git senfoto
cd senfoto
./init.sh        # 克隆 lidarview-superbuild、初始化子模块、装依赖、配置 cmake
./build.sh        # 增量编译
```

`init.sh` 会把 `lidarview-superbuild/` 与 `build/` 建在仓库**同级目录**，并把 superbuild 指向本仓库源码
（`-Dlidarview_SOURCE_SELECTION=source`，否则会拉 Kitware 的 master，不含 Senfoto 插件）。

## 前置条件

| 项 | 要求 |
|----|------|
| 操作系统 | Ubuntu 22.04 / 24.04；Windows + MSVC 2019+；macOS 15+（Apple Silicon，**需系统级 Qt6**） |
| 分支 | 必须 `develop` + `--recursive` 子模块 |
| 工具 | CMake ≥ 3.20.3（**macOS 请用 3.31，不要用 Homebrew 的 4.x**）、Ninja、Qt6（macOS 必须系统级） |

> 完整环境与排错见 **[BUILD.md](./BUILD.md)**（含 macOS / Windows 专节、以及第 11 节 VSCode 配置）。

## 运行

- Linux：`../build/install/bin/SenFoToView`
- macOS：`open ../build/install/Applications/SenFoToView.app`
  （**不要在 conda 激活的终端直接运行**，会撞捆绑的 Python 3.12 导致崩溃）

## 代码导航 / 开发环境（VSCode）

仓库已内置 `.vscode/c_cpp_properties.json`，配合构建生成的 `compile_commands.json` 即可实现
“转到定义 / 查看引用 / 悬浮文档” 等跳转（Qt / ParaView / VTK 头文件均可跳转）。
新机器只需按 **[BUILD.md 第 11 节](./BUILD.md)** 跑一次：

```bash
cd build
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ./superbuild/lidarview/build
```

然后 VSCode `Ctrl+Shift+P` → `Developer: Reload Window`。

## 架构与「如何加功能」

- **项目架构 + 数据/UI 两条链路 + 4 个加功能案例（新增插件 / filter / 面板 / 工具栏）**：
  [`docs/senfoto008-data-flow-and-features.md`](./docs/senfoto008-data-flow-and-features.md)
- 通用开发指南：[`docs/lidarview-development-guide.md`](./docs/lidarview-development-guide.md)
- 新增雷达插件模板：[`docs/adding-a-lidar-sensor-plugin.md`](./docs/adding-a-lidar-sensor-plugin.md)
- LidarView 原始架构：[`docs/lidarview-architecture.md`](./docs/lidarview-architecture.md)

# Features

- Input from live sensor stream or recorded `.pcap` file
- Visualization of timestamped LiDAR returns in 3D
- Spreadsheet inspector for LiDAR attributes (timestamp, azimuth, laser id, etc)
- Record to `.pcap` from sensor
- Export to CSV, PLY, PCD, LAS or VTK formats
- Grid and Ruler tools
- Show or hide lasers subsets
- Show multiple frames of data simultaneously and aggregate them
- Apply 3D transforms to pointclouds
- Run SLAM to estimate the trajectory of the LiDAR in the scene and build a 3D map of the environment

Many other features can be added using Plugins, some can be found on [this page](https://gitlab.kitware.com/LidarView/plugins).

Feel free to reach out at kitware@kitware.com for support or new features development.

# How to install

See associated download links in the [Release](https://gitlab.kitware.com/LidarView/lidarview/-/releases) page of this repository.

Nightly (master) packages are available [here](https://gitlab.kitware.com/LidarView/lidarview-superbuild/-/pipelines?scope=all&source=schedule&ref=master). (click on `Download artifacts`)

More detailed installation instructions are available on the [`LVCore/Documentation/INSTALLATION.md`](https://gitlab.kitware.com/LidarView/lidarview-core/-/blob/master/Documentation/INSTALLATION.md) page.

## Build from source

**本叉子（Senfoto）的构建步骤见本地 [BUILD.md](./BUILD.md)**（涵盖 Ubuntu / Windows / macOS，
以及 VSCode IntelliSense 配置）。上游 LidarView 的通用构建与打包说明见
[LidarView-superbuild README](https://gitlab.kitware.com/LidarView/lidarview-superbuild/-/blob/master/README.md)。

# How to use

## Sensor streaming

Specific network configuration is required for sensor livestream.
The ethernet adapter connected to the sensor has to be switched from dynamic IP address assignment to static IP address selection and correct IP adress and gateway must be specified.

For example:

* Velodyne HDL-32E
  * IP address: 192.168.1.70 (70 as example, any number except 201 works)
  * Gateway: 255.255.255.0
* Velodyne HDL-64E
  * IP address: 192.168.3.70 (70 as example, any number except 43 works)
  * Gateway: 192.168.3.255

In order for sensor streaming to work properly, it is important to
disable firewall restrictions for the chosen ethernet port and allow inbound traffic.
Alternatively, completely disable the firewall for the ethernet device connected to the sensor (including both public and private networks).

When opening pre-recorded data or live sensor streaming data,
one is prompted to choose a calibration file.
This calibration can either be directly embedded in LidarView,
or may be loaded from a custom location.

## SLAM documentation <a name="slam"></a>

More [instructions](https://gitlab.kitware.com/keu-computervision/slam/-/blob/master/paraview_wrapping/doc/How_to_SLAM_with_LidarView.md) can be found on the [LidarSlam repository](https://gitlab.kitware.com/keu-computervision/slam).

Have a look also at the [How to SLAM with LidarView](https://vimeo.com/524848891) webinar.

# Sample data

LiDAR data samples for LidarView can be obtained from:

* [MIDAS](http://www.midasplatform.org/) in the [Velodyne LiDAR collection](http://midas3.kitware.com/midas/community/29)
* Kitware's demos shared [drive](https://drive.google.com/drive/folders/1yrNUelUsjKcXdC8FH8DpXeOPTyiB_pLS?usp=sharing)

# Questions / Reporting Bugs

You found a bug:
 1. If you have a patch, please read the [CONTRIBUTING.md](./CONTRIBUTING.md) document.
 2. Otherwise you can also open an entry in the [LidarView Issue Tracker](https://gitlab.kitware.com/LidarView/lidarview/-/issues).

Note that we also have a [LidarView / SLAM section](https://discourse.paraview.org/c/lidar/15) in ParaView Discourse forum where you ask the community about a functionality / issue.
 
# For Github users

[Github](https://github.com/Kitware/LidarView) is a mirror of the
[official repository](https://gitlab.kitware.com/LidarView/LidarView).
We do not actively monitor issues or pull request on Github. Please use the
[official repository](https://gitlab.kitware.com/LidarView/LidarView) to report issues or contributes fixes.

# License

The source code for LidarView is made available under the Apache 2.0 license.

See [LICENSE](LICENSE).
