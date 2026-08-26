#!/usr/bin/env bash
#
# init.sh - Senfoto (LidarView) 一键初始化脚本
#
# 用法：
#   1) git clone -b develop --recursive https://github.com/xiaobin86/senfoto-view.git senfoto
#   2) cd senfoto
#   3) ./init.sh
#
# 脚本会自动完成「配置就绪」所需的一切：
#   - 在仓库的【同级目录】克隆 lidarview-superbuild（Kitware 公开仓库，无需鉴权）
#   - 初始化 lidarview 与 lidarview-superbuild 的子模块
#   - 在 Ubuntu/Debian 上安装构建依赖（需 sudo）
#   - 创建 build/ 并用 superbuild 配置 cmake，指向本仓库源码
#     （关键：-Dlidarview_SOURCE_SELECTION=source，否则会拉 Kitware 的 master，不含 Senfoto 插件）
#
# 完成后即可编译：
#   cmake --build ../build -j
# 产物位于：
#   ../build/install/bin/LidarView
#
set -u

# ---------- 路径（脚本所在目录 = LidarView 源码根） ----------
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PARENT_DIR="$(dirname "$REPO_DIR")"
SUPERBUILD_DIR="$PARENT_DIR/lidarview-superbuild"
BUILD_DIR="$PARENT_DIR/build"
SUPERBUILD_REPO="https://gitlab.kitware.com/LidarView/lidarview-superbuild.git"

echo "==> 仓库源码目录 : $REPO_DIR"
echo "==> 父目录(存放 superbuild 与 build) : $PARENT_DIR"

# ---------- 1. 克隆 superbuild ----------
if [ -d "$SUPERBUILD_DIR/.git" ]; then
  echo "==> lidarview-superbuild 已存在，跳过克隆"
else
  echo "==> 克隆 lidarview-superbuild ..."
  if git clone --recursive "$SUPERBUILD_REPO" "$SUPERBUILD_DIR"; then
    echo "==> superbuild 克隆完成"
  else
    echo "错误：superbuild 克隆失败，请检查网络后重试" >&2
    exit 1
  fi
fi

# ---------- 2. 初始化子模块 ----------
echo "==> 初始化 lidarview 子模块 ..."
git -C "$REPO_DIR" submodule update --init --recursive || true

echo "==> 初始化 superbuild 子模块 ..."
git -C "$SUPERBUILD_DIR" submodule update --init --recursive || true

# ---------- 3. 安装系统依赖 (仅 Ubuntu/Debian) ----------
if [ -f /etc/os-release ]; then
  # shellcheck disable=SC1091
  . /etc/os-release
fi
case "${ID:-} ${ID_LIKE:-}" in
  *ubuntu*|*debian*)
    echo "==> 检测到 Debian/Ubuntu，安装构建依赖 (需 sudo) ..."
    SUDO="$(command -v sudo || true)"
    if $SUDO apt-get update && $SUDO apt-get install -y \
      cmake ninja-build pkg-config python3 chrpath git git-lfs \
      build-essential byacc flex freeglut3-dev libbz2-dev libffi-dev \
      libfontconfig1-dev libfreetype6-dev libnl-genl-3-dev libopengl0 \
      libprotobuf-dev libx11-dev libx11-xcb-dev libxcb-glx0-dev libxcb-icccm4-dev \
      libxcb-image0-dev libxcb-keysyms1-dev libxcb-randr0-dev libxcb-render-util0-dev \
      libxcb-shape0-dev libxcb-shm0-dev libxcb-sync-dev libxcb-util-dev \
      libxcb-xfixes0-dev libxcb-xinerama0-dev libxcb-xkb-dev libxcb1-dev libxext-dev \
      libxfixes-dev libxi-dev libxkbcommon-dev libxkbcommon-x11-dev libxrender-dev \
      libxt-dev protobuf-compiler zlib1g-dev libglx-dev; then
      echo "==> 构建依赖安装完成"
    else
      echo "错误：apt 依赖安装失败，请检查网络/权限后重试" >&2
      exit 1
    fi
    ;;
  *)
    echo "==> 非 Debian/Ubuntu 系统，跳过 apt 依赖安装。"
    echo "    请参考 BUILD.md 第 2 节手动安装对应平台的构建依赖与 Qt6。"
    ;;
esac

# ---------- 前置检查：cmake / ninja ----------
command -v cmake >/dev/null 2>&1 || { echo "错误：未找到 cmake，请先安装（见 BUILD.md 第 2 节）" >&2; exit 1; }
command -v ninja >/dev/null 2>&1 || { echo "错误：未找到 ninja，请先安装（见 BUILD.md 第 2 节）" >&2; exit 1; }

# ---------- macOS 环境准备 ----------
IS_MACOS=0
case "$(uname -s)" in
  Darwin*) IS_MACOS=1 ;;
esac
if [ "$IS_MACOS" -eq 1 ]; then
  # 去掉 conda/miniforge/anaconda，避免与捆绑的 Python 3.12 冲突
  PATH="$(echo "$PATH" | tr ':' '\n' | grep -v -iE 'miniforge|conda|anaconda' | tr '\n' ':' | sed 's/:$//')"
  command -v brew >/dev/null 2>&1 && PATH="$(brew --prefix)/bin:$PATH"
  [ -d "$HOME/Library/Python/3.9/bin" ] && PATH="$HOME/Library/Python/3.9/bin:$PATH"
  QT6="${QT6:-$HOME/Qt/6.9.0/macos}"
  [ -d "$QT6" ] || QT6="/Users/acelan/Qt/6.9.0/macos"
  ICONV="$(brew --prefix libiconv 2>/dev/null)"; [ -z "$ICONV" ] && ICONV="/usr/local/opt/libiconv"
  SDK="$(xcrun --sdk macosx --show-sdk-path 2>/dev/null)"; [ -z "$SDK" ] && SDK="/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk"
  export PATH QT6 ICONV SDK
  echo "==> macOS 环境: Qt6=$QT6  ICONV=$ICONV  SDK=$SDK"
fi

# ---------- 4. 配置 cmake（superbuild 指向本地 lidarview 源码） ----------
if [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
  echo "==> build/ 已配置 (CMakeCache.txt 存在)，跳过 cmake 配置"
else
  echo "==> 创建并配置 build/ ..."
  mkdir -p "$BUILD_DIR"
  if [ "$IS_MACOS" -eq 1 ]; then
    cmake -S "$SUPERBUILD_DIR" -B "$BUILD_DIR" -GNinja \
      -DCMAKE_BUILD_TYPE=Release \
      -Dlidarview_SOURCE_SELECTION=source \
      -Dlidarview_SOURCE_DIR="$REPO_DIR" \
      -DCMAKE_PREFIX_PATH="$QT6;$ICONV" \
      -DCMAKE_LIBRARY_PATH="$ICONV/lib" \
      -DCMAKE_OSX_SYSROOT="$SDK"
    configure_rc=$?
  else
    cmake -S "$SUPERBUILD_DIR" -B "$BUILD_DIR" -GNinja \
      -DCMAKE_BUILD_TYPE=Release \
      -Dlidarview_SOURCE_SELECTION=source \
      -Dlidarview_SOURCE_DIR="$REPO_DIR"
    configure_rc=$?
  fi
  if [ "$configure_rc" -eq 0 ]; then
    echo "==> cmake 配置完成"
  else
    echo "错误：cmake 配置失败，请查看上方输出" >&2
    exit 1
  fi
fi

# macOS：插件靠 install/lib 下的 @rpath/Qt*.framework 解析，需把系统 Qt 框架软链进去
if [ "$IS_MACOS" -eq 1 ]; then
  echo "==> macOS: 软链 Qt 框架到 install/lib (插件解析 @rpath/Qt*.framework 用) ..."
  mkdir -p "$BUILD_DIR/install/lib"
  for f in "$QT6/lib/"*.framework; do
    [ -e "$f" ] && ln -sf "$f" "$BUILD_DIR/install/lib/" 2>/dev/null
  done
fi

echo ""
echo "=========================================="
echo " 初始化完成（已配置就绪，可开始编译）"
echo " 源码目录  : $REPO_DIR"
echo " Superbuild : $SUPERBUILD_DIR"
echo " 编译目录   : $BUILD_DIR"
echo ""
echo " 开始编译："
echo "   cmake --build $BUILD_DIR -j"
echo " 编译产物："
echo "   $BUILD_DIR/install/bin/LidarView"
echo "=========================================="
