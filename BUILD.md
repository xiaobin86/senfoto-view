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

## 0. 一键初始化（推荐）

不想手动敲命令？仓库根目录提供了 `init.sh`，clone 之后进入仓库目录执行即可自动完成
第 2~4 节的准备（克隆 superbuild、初始化子模块、安装 apt 依赖、cmake 配置）：

```bash
git clone -b develop --recursive https://github.com/xiaobin86/senfoto-view.git senfoto
cd senfoto
./init.sh
# 初始化完成后开始编译
cmake --build ../build -j
```

脚本会把 `lidarview-superbuild/` 和 `build/` 建在仓库的**同级目录**，
并把 superbuild 指向本仓库源码（`-Dlidarview_SOURCE_SELECTION=source`），
无需手动指定路径，也不依赖目录叫 `lidarview` 还是 `senfoto`。
非 Debian/Ubuntu 系统会跳过 apt 依赖并提示你手动安装。

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
senfoto-view/
├── lidarview/               # 本仓库：Senfoto 源码（来自 GitHub）
├── lidarview-superbuild/   # LidarView-Superbuild（来自 Kitware）
└── build/                  # 编译目录
```

```bash
# 1) Senfoto 源码：必须 develop 分支，并拉取子模块（Velodyne/Hesai 等官方插件）
git clone -b develop --recursive https://github.com/xiaobin86/senfoto-view.git lidarview

# 2) Superbuild：来自 Kitware 官方（本仓库未托管 superbuild，直接拉官方）
#    不写末尾的目录名时，git 默认用仓库名 lidarview-superbuild 作为目录名
git clone --recursive https://gitlab.kitware.com/LidarView/lidarview-superbuild.git
```

> **目录上下文**：以上两条 `git clone` 都在 `senfoto-view/` 工作根目录下执行
> （`git clone` 不会切换你的当前目录，执行后仍在 `senfoto-view/`）。
> 注意：两条命令之间**不要先 `cd lidarview`**，否则 superbuild 会被克隆进
> `lidarview/` 内部，破坏三者平级结构、导致后续 `cmake ../lidarview-superbuild` 找不到目录。
> 若已 `cd` 进去，先 `cd ..` 回到 `senfoto-view/` 再继续。

---

## 4. 配置（指向本地源码）

```bash
mkdir build && cd build

cmake ../lidarview-superbuild -GNinja -DCMAKE_BUILD_TYPE=Release \
  -Dlidarview_SOURCE_SELECTION=source \
  -Dlidarview_SOURCE_DIR=$(realpath ../lidarview)
```

可选功能开关（按需追加）：
- `-DENABLE_slam=True`：启用 SLAM 插件
- `-DENABLE_hesaisdk=True`：启用 Hesai 解释器
- `-DCMAKE_BUILD_TYPE=Debug` / `RelWithDebInfo`：默认 `Release`

> 也可用 `ccmake ../lidarview-superbuild` 或 `cmake-gui` 交互式配置。

---

## 5. 编译

> 以下命令在前面创建的 `build/` 目录下执行（第 4 步 `cd build` 之后的当前目录）。

```bash
cmake --build . -j
```

首次编译通常 20 分钟 ~ 2 小时（取决于核心数）。
产物位于 `build/install/`（即当前 `build/` 目录下的 `./install/`），
可执行程序为 `build/install/bin/SenFoToView`
（Senfoto008 / LakiBeam 插件已随 LidarView 一并编译进插件，由本仓库 `CMakeLists.txt` 注册）。

> 嫌手敲麻烦可用仓库根目录的 `./build.sh` 做增量编译（支持 Ubuntu / Windows-GitBash）：
> `./build.sh` 增量编译；`./build.sh --package` 额外打包；`./build.sh --clean` 重配全量构建。
> 前提仍是已用 `./init.sh` 完成初始化（build/ 已配置好 cmake）。

---

## 6. 运行

> 仍在 `build/` 目录下执行。

```bash
./install/bin/SenFoToView
# 等价于 build/install/bin/SenFoToView
```

若提示缺少共享库（Ubuntu 24.04），先执行第 2.3 节的运行期依赖安装。

---

## 7. 增量编译（只改了插件/源码时）

> 仍在 `build/` 目录下执行。`superbuild/lidarview/build` 也是相对于 `build/` 的路径。

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
3. 路径尽量短且靠近盘符根目录（如把 superbuild 源码放到 `C:\sb` 这样的短路径），避免 Windows 路径长度限制。
4. 也可在 Git Bash 下直接运行仓库根目录的 `./build.sh`（它会自动定位 VS 的 `vcvars64.bat` 初始化 MSVC 环境后编译）；`./build.sh --package` 会额外用 cpack 生成 Windows 安装包。
5. 不想用本地 Windows 也行：仓库内置 `.github/workflows/build-windows.yml`，push 到 GitHub 后会由 GitHub 的 Windows runner 自动编译并产出 `SenFoToView.exe`（在仓库 Actions 页面的 Artifacts 里下载）。
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
