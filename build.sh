#!/usr/bin/env bash
#
# build.sh - Senfoto (LidarView) 一键构建脚本（支持 Ubuntu / Windows / macOS）
#
# 前提：已通过 ./init.sh（或手动）完成初始化，即 build/ 已配置好 cmake。
#       本脚本只负责“编译最终产物”，不负责首次环境搭建。
#
# 用法：
#   ./build.sh            # 增量编译（每次改完代码跑这个）
#   ./build.sh --package  # 编译并打包（cpack 生成安装包 / Windows 安装器）
#   ./build.sh --clean    # 删除 CMakeCache 后重新配置并全量构建
#
# 平台：
#   - Ubuntu / Linux：直接用 ninja 增量编译。
#   - Windows：在 Git Bash 下运行；脚本自动定位 VS 的 vcvars64.bat 初始化
#              MSVC x64 环境后再编译（需已安装 Visual Studio + Qt6）。
#              Windows 分支为尽力实现，请在实际 Windows + Git Bash 环境验证。
#   - macOS：用系统 Qt6（superbuild 的 MUST_USE_SYSTEM）+ 本机 SDK 编译；
#            构建后自动把 Qt 框架软链进 install/lib，供插件按 @rpath 解析；
#            并清理 conda 环境以避免与捆绑 Python 3.12 冲突。
#
set -u

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PARENT_DIR="$(dirname "$REPO_DIR")"
BUILD_DIR="$PARENT_DIR/build"
SUPERBUILD_DIR="$PARENT_DIR/lidarview-superbuild"

PACKAGE=0
CLEAN=0
for arg in "$@"; do
  case "$arg" in
    --package) PACKAGE=1 ;;
    --clean)   CLEAN=1 ;;
    *) echo "未知参数: $arg" >&2; echo "可用: --package / --clean" >&2; exit 1 ;;
  esac
done

# 平台检测
IS_WINDOWS=0
IS_MACOS=0
case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*|Windows_NT) IS_WINDOWS=1 ;;
  Darwin*) IS_MACOS=1 ;;
esac

# macOS 环境准备：清理 conda、定位 CMake 3.31 / homebrew / Qt6 / SDK
setup_macos() {
  # 去掉 conda/miniforge/anaconda，避免与捆绑的 Python 3.12 冲突
  PATH="$(echo "$PATH" | tr ':' '\n' | grep -v -iE 'miniforge|conda|anaconda' | tr '\n' ':' | sed 's/:$//')"
  # pip 安装的 CMake 3.31（须优先于 homebrew 的 cmake 4.x，后者会破坏子工程）与 homebrew
  command -v brew >/dev/null 2>&1 && PATH="$(brew --prefix)/bin:$PATH"
  [ -d "$HOME/Library/Python/3.9/bin" ] && PATH="$HOME/Library/Python/3.9/bin:$PATH"
  # Qt6 系统路径（可用环境变量 QT6 覆盖）
  QT6="${QT6:-$HOME/Qt/6.9.0/macos}"
  [ -d "$QT6" ] || QT6="/Users/acelan/Qt/6.9.0/macos"
  # libiconv（homebrew）
  ICONV="$(brew --prefix libiconv 2>/dev/null)"; [ -z "$ICONV" ] && ICONV="/usr/local/opt/libiconv"
  # macOS SDK
  SDK="$(xcrun --sdk macosx --show-sdk-path 2>/dev/null)"; [ -z "$SDK" ] && SDK="/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk"
  export PATH QT6 ICONV SDK
  echo "==> macOS 环境: Qt6=$QT6  ICONV=$ICONV  SDK=$SDK"
}
[ "$IS_MACOS" -eq 1 ] && setup_macos

VCVARS_W=""

# 定位 VS 的 vcvars64.bat（仅 Windows）
vswhere_vcvars() {
  local vswhere="/c/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe"
  [ -x "$vswhere" ] || { echo "错误：未找到 vswhere.exe，请确认已安装 Visual Studio（含“C++ 桌面开发”负载）。" >&2; exit 1; }
  local vs_path
  vs_path="$("$vswhere" -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | tr -d '\r')"
  echo "$vs_path/VC/Auxiliary/Build/vcvars64.bat"
}

# 路径（Windows 下转换为原生路径供 cmd 使用）
if [ "$IS_WINDOWS" -eq 1 ]; then
  BUILD_DIR_W="$(cygpath -w "$BUILD_DIR")"
  REPO_DIR_W="$(cygpath -w "$REPO_DIR")"
  SUPERBUILD_DIR_W="$(cygpath -w "$SUPERBUILD_DIR")"
  VCVARS_W="$(vswhere_vcvars)"
else
  BUILD_DIR_W="$BUILD_DIR"
  REPO_DIR_W="$REPO_DIR"
  SUPERBUILD_DIR_W="$SUPERBUILD_DIR"
fi

if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
  echo "错误：未找到 $BUILD_DIR/CMakeCache.txt" >&2
  echo "请先运行 ./init.sh 完成初始化（或手动执行 superbuild 的 cmake 配置）。" >&2
  exit 1
fi

# 通过临时 .bat 在 MSVC 环境下执行命令（避免 cmd 引号嵌套问题）
run_in_vs_env() {
  local bat
  bat="$(mktemp -t senfoto_build_XXXX.bat)"
  {
    echo "call \"$VCVARS_W\""
    echo "$1"
  } > "$bat"
  cmd.exe /c "$(cygpath -w "$bat")"
  local rc=$?
  rm -f "$bat"
  return $rc
}

# ---------- --clean：重新配置 ----------
if [ "$CLEAN" -eq 1 ]; then
  echo "==> --clean：删除 CMakeCache 并重新配置 ..."
  rm -f "$BUILD_DIR/CMakeCache.txt"
  if [ "$IS_WINDOWS" -eq 1 ]; then
    run_in_vs_env "cmake -S \"$SUPERBUILD_DIR_W\" -B \"$BUILD_DIR_W\" -GNinja -DCMAKE_BUILD_TYPE=Release -Dlidarview_SOURCE_SELECTION=source -Dlidarview_SOURCE_DIR=\"$REPO_DIR_W\""
  elif [ "$IS_MACOS" -eq 1 ]; then
    cmake -S "$SUPERBUILD_DIR" -B "$BUILD_DIR" -GNinja \
      -DCMAKE_BUILD_TYPE=Release \
      -Dlidarview_SOURCE_SELECTION=source \
      -Dlidarview_SOURCE_DIR="$REPO_DIR" \
      -DCMAKE_PREFIX_PATH="$QT6;$ICONV" \
      -DCMAKE_LIBRARY_PATH="$ICONV/lib" \
      -DCMAKE_OSX_SYSROOT="$SDK"
  else
    cmake -S "$SUPERBUILD_DIR" -B "$BUILD_DIR" -GNinja \
      -DCMAKE_BUILD_TYPE=Release \
      -Dlidarview_SOURCE_SELECTION=source \
      -Dlidarview_SOURCE_DIR="$REPO_DIR"
  fi
fi

# ---------- 编译 ----------
echo "==> 增量编译 ..."
if [ "$IS_WINDOWS" -eq 1 ]; then
  run_in_vs_env "cmake --build \"$BUILD_DIR_W\" --config Release"
else
  cmake --build "$BUILD_DIR" -j
fi

# macOS：插件靠 install/lib 下的 @rpath/Qt*.framework 解析，需把系统 Qt 框架软链进去
if [ "$IS_MACOS" -eq 1 ]; then
  echo "==> macOS: 软链 Qt 框架到 install/lib (插件解析 @rpath/Qt*.framework 用) ..."
  mkdir -p "$BUILD_DIR/install/lib"
  for f in "$QT6/lib/"*.framework; do
    [ -e "$f" ] && ln -sf "$f" "$BUILD_DIR/install/lib/" 2>/dev/null
  done
fi

# ---------- 打包 ----------
if [ "$PACKAGE" -eq 1 ]; then
  echo "==> 打包 (cpack) ..."
  if [ "$IS_WINDOWS" -eq 1 ]; then
    run_in_vs_env "cd /d \"$BUILD_DIR_W\" && ctest -C Release -R cpack"
  else
    (cd "$BUILD_DIR" && ctest -R cpack)
  fi
fi

echo ""
echo "=========================================="
echo " 构建完成"
if [ "$IS_MACOS" -eq 1 ]; then
  echo " 产物: $BUILD_DIR/install/Applications/SenFoToView.app"
  echo " 启动: open $BUILD_DIR/install/Applications/SenFoToView.app"
  echo "       (请勿在 conda 激活的终端直接运行二进制，会撞捆绑的 Python 3.12)"
else
  echo " 产物: $BUILD_DIR/install/bin/SenFoToView"
fi
echo "=========================================="
