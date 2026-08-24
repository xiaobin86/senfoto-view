# 自托管 Runner 落地清单（方案 C）

## 为什么用自托管（方案 C）

LidarView / ParaView superbuild 的 `build/` 编译树通常 20GB+，GitHub 托管 runner 有两个硬限制：
缓存默认 10GB 上限、本地磁盘只有约 14GB 可用——都装不下完整构建。

自托管 runner 把编译树留在**你自己的机器磁盘**上：
- 跨 run 天然复用，无需 GitHub 缓存、无 10GB 上限
- 不限分钟、不烧 GitHub 存储费（只付你自己的机器/云主机钱）
- 磁盘你随便给（建议 ≥200GB SSD）

## 仓库与 workflow

- 仓库：`github.com/xiaobin86/senfoto-view`，主分支 `develop`
- 两个 workflow：
  - `.github/workflows/build-windows.yml` → `runs-on: [self-hosted, windows]`
  - `.github/workflows/build-ubuntu.yml` → `runs-on: [self-hosted, linux]`
- 触发方式：GitHub Actions 页面**手动 Run workflow**（`workflow_dispatch`），不随 push 自动触发
- Windows workflow 已内置 `ilammy/msvc-dev-cmd` 步骤，自动加载 MSVC 环境（无需手动配 PATH）

## 注册 runner

1. 仓库 `Settings → Actions → Runners → New self-hosted runner`
2. 选操作系统 → 页面给出下载链接与 `--token`
3. 注册后该机器自动带 `self-hosted` + `windows` / `linux` 标签，正好匹配 workflow 的 `runs-on`

## Windows runner（以腾讯云 Windows Server 2022 为例）

- 规格建议：≥8 vCPU、≥32GB RAM、≥200GB SSD
- 预装：
  - **Visual Studio Build Tools 2022**（勾选 **MSVC v143 + Windows 10/11 SDK**）
  - **CMake**、**Ninja**（`choco install ninja -y`）、**Git**
- MSVC 环境：workflow 用 `ilammy/msvc-dev-cmd` 自动加载，机器端只管装 VS Build Tools 即可
- 安装 runner（管理员 PowerShell，解压后）：
  ```powershell
  .\config.cmd --url https://github.com/xiaobin86/senfoto-view --token <页面TOKEN>
  ```
  提示 "run as service?" 选 **Y**（装成 Windows 服务常驻）
- 可选：预装 Qt6 6.9.0（`win64_msvc2022_64`）并传 `-DQt6_DIR`，否则 superbuild 首次自行编译 Qt（更慢）
- 建议：把 runner 的 workspace / `build/` 目录加入 **Windows Defender 实时扫描排除**，否则几万个文件被实时扫描会严重拖慢编译
- 网络：需出网到 `github.com`、`gitlab.kitware.com`（克隆 superbuild）、`download.qt.io`（装 Qt）、`objects.githubusercontent.com`（上传 artifact），确认安全组未挡 443

## Ubuntu runner

- 预装：
  ```bash
  sudo apt-get install -y cmake ninja-build pkg-config python3 git git-lfs build-essential \
    byacc flex freeglut3-dev libbz2-dev libffi-dev libfontconfig1-dev libfreetype6-dev \
    libnl-genl-3-dev libopengl0 libprotobuf-dev libx11-dev libx11-xcb-dev libxcb-glx0-dev \
    libxcb-icccm4-dev libxcb-image0-dev libxcb-keysyms1-dev libxcb-randr0-dev \
    libxcb-render-util0-dev libxcb-shape0-dev libxcb-shm0-dev libxcb-sync-dev \
    libxcb-util-dev libxcb-xfixes0-dev libxcb-xinerama0-dev libxcb-xkb-dev libxcb1-dev \
    libxext-dev libxfixes-dev libxi-dev libxkbcommon-dev libxkbcommon-x11-dev \
    libxrender-dev libxt-dev protobuf-compiler zlib1g-dev libglx-dev
  ```
  （与 `BUILD.md` 列出的 X11/GL 依赖一致）
- 安装 runner（解压后）：
  ```bash
  ./config.sh --url https://github.com/xiaobin86/senfoto-view --token <页面TOKEN>
  sudo ./svc.sh install   # 装成 systemd 服务常驻
  ```
- 可选：预装 Qt6 6.9.0（`linux gcc_64`）
- 磁盘：≥200GB SSD

## 构建行为

- **首次运行**：全量编译 ParaView / VTK / LidarView，约数小时（烧你机器时间，不烧 GitHub 分钟）
- **之后**：`build/` 留在机器本地磁盘，增量编译，分钟级
- superbuild 克隆幂等：目录已存在则 `git pull`，否则 `git clone`
- 产物 `LidarView[.exe]` 作为 artifact 上传回 GitHub，可在 run 页面下载
- 想强制全量重建：删掉机器上的 `build/` 目录再触发
- 单 job 超时设为 330 分钟（留余量）；首次若超时可适当调大

## 安全

自托管 runner 会执行仓库里的 CI 脚本。私有仓库无虞；若仓库为公仓，需注意任何 PR 都能触发在你机器上跑代码。
