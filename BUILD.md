# Senfoto (LidarView) 编译指南

本仓库是 LidarView 的派生版本，集成了 Senfoto008 / LakiBeam 等自定义激光雷达插件。
编译采用 **LidarView-Superbuild** 体系：superbuild 负责拉取并构建所有第三方依赖
（ParaView、VTK、Qt6、boost 等），再编译本仓库的 LidarView 源码（含 Senfoto 插件）。

> ⚠️ **两个最容易踩的坑**
> 1. 必须克隆 **`develop`** 分支——Senfoto 插件只提交在这个分支，`master` 上没有。
> 2. 配置 superbuild 时必须用 `-Dlidarview_SOURCE_SELECTION=source -Dlidarview_SOURCE_DIR=...`
>    指向本仓库本地源码；否则 superbuild 默认从 Kitware 拉取 LidarView 的 `master`，
>    编译出的程序**不包含你的 Senfoto 插件**。

---

## 1. 环境要求

| 项目 | 要求 |
|------|------|
| 操作系统 | Ubuntu 22.04 / 24.04（最稳，CI 同此）；或 Windows + MSVC 2019+ |
| 磁盘空间 | ≥ 50 GB 空闲（依赖下载 + 编译产物很大） |
| 网络 | 需联网，superbuild 会下载 ParaView / Qt / boost 等数 GB 源码 |
| 工具 | CMake ≥ 3.20.3、Ninja ≥ 1.8.2、GCC 11+（Ubuntu 22.04 自带即可）、pkg-config、Python 3、git、git-lfs（可选，仅跑测试需要） |

---

## 2. 安装系统依赖（Ubuntu）

### 2.1 构建期依赖

```bash
sudo apt-get update
sudo apt-get install -y \
  cmake ninja-build pkg-config python3 chrpath git git-lfs \
  build-essential byacc flex freeglut3-dev libbz2-dev libffi-dev \
  libfontconfig1-dev libfreetype6-dev libnl-genl-3-dev libopengl0 \
  libprotobuf-dev libx11-dev libx11-xcb-dev libxcb-glx0-dev libxcb-icccm4-dev \
  libxcb-image0-dev libxcb-keysyms1-dev libxcb-randr0-dev libxcb-render-util0-dev \
  libxcb-shape0-dev libxcb-shm0-dev libxcb-sync-dev libxcb-util-dev \
  libxcb-xfixes0-dev libxcb-xinerama0-dev libxcb-xkb-dev libxcb1-dev libxext-dev \
  libxfixes-dev libxi-dev libxkbcommon-dev libxkbcommon-x11-dev libxrender-dev \
  libxt-dev protobuf-compiler zlib1g-dev libglx-dev
```

### 2.2 Qt6

最简单：让 superbuild **自己编译 Qt6**（零手动配置，但耗时较长，约几十分钟）。
想加速可单独安装 Qt 6.9，再在配置时加 `-DQt6_DIR=<Qt6 安装路径>/lib/cmake/Qt6`。
（Windows 下 Qt6 **必须**预装并传 `-DQt6_DIR`。）

### 2.3 运行期依赖（Ubuntu 24.04）

```bash
sudo apt-get install -y libgomp1 libxcb-cursor0 libxcb-xinerama0 libxcb-xinput0 libquadmath0
```

---

## 3. 克隆仓库

目录结构建议（三者平级）：

```
work/
├── lidarview/      # 本仓库：Senfoto 源码（来自 GitHub）
├── lvsb/           # LidarView-Superbuild（来自 Kitware）
└── lvsb-build/     # 编译目录
```

```bash
# 1) Senfoto 源码：必须 develop 分支，并拉取子模块（Velodyne/Hesai 等官方插件）
git clone -b develop --recursive https://github.com/xiaobin86/senfoto-view.git lidarview

# 2) Superbuild：来自 Kitware 官方（本仓库未托管 superbuild，直接拉官方）
git clone --recursive https://gitlab.kitware.com/LidarView/lidarview-superbuild.git lvsb
```

---

## 4. 配置（指向本地源码）

```bash
mkdir lvsb-build && cd lvsb-build

cmake ../lvsb -GNinja -DCMAKE_BUILD_TYPE=Release \
  -Dlidarview_SOURCE_SELECTION=source \
  -Dlidarview_SOURCE_DIR=$(realpath ../lidarview)
```

可选功能开关（按需追加）：
- `-DENABLE_slam=True`：启用 SLAM 插件
- `-DENABLE_hesaisdk=True`：启用 Hesai 解释器
- `-DCMAKE_BUILD_TYPE=Debug` / `RelWithDebInfo`：默认 `Release`

> 也可用 `ccmake ../lvsb` 或 `cmake-gui` 交互式配置。

---

## 5. 编译

```bash
cmake --build . -j
```

首次编译通常 20 分钟 ~ 2 小时（取决于核心数）。
产物位于 `./install/`，可执行程序为 `./install/bin/LidarView`
（Senfoto008 / LakiBeam 插件已随 LidarView 一并编译进插件，由本仓库 `CMakeLists.txt` 注册）。

---

## 6. 运行

```bash
./install/bin/LidarView
```

若提示缺少共享库（Ubuntu 24.04），先执行第 2.3 节的运行期依赖安装。

---

## 7. 增量编译（只改了插件/源码时）

完整 `cmake --build . -j` 很慢。只改了 LidarView 源码（如 Senfoto 插件）时，
直接重编 lidarview 子项目更快：

```bash
cmake --build superbuild/lidarview/build -j --target install
# 或
ninja -C superbuild/lidarview/build -j8 install
```

---

## 8. Windows 简述

1. 打开 **"VS20XX x64 Native Tools Command Prompt"**（MSVC 2019+）。
2. 安装 CMake、Ninja、**Qt6（必须预装）**，配置时传 `-DQt6_DIR=<...>/lib/cmake/Qt6`。
3. 路径尽量短且靠近盘符根目录（如 `C:\lvsb`），避免 Windows 路径长度限制。
4. 克隆、配置、编译步骤与上面一致，配置加上：
   ```
   -Dlidarview_SOURCE_SELECTION=source -Dlidarview_SOURCE_DIR=<lidarview 路径>
   ```

---

## 9. 常见问题

- **编译出的程序没有 Senfoto 设备？** 检查是否忘了 `-Dlidarview_SOURCE_SELECTION=source`，
  或克隆的不是 `develop` 分支。
- **Qt6 相关配置报错？** 确认 `-DQt6_DIR` 指向正确的 `lib/cmake/Qt6`，且路径用正斜杠。
- **子模块拉取失败？** 确认网络可访问 `gitlab.kitware.com`，或手动
  `cd lidarview && git submodule update --init --recursive`。
- **更多排错**：见 superbuild 仓库 `Documentation/faq.md`、`Documentation/install_qt.md`。
