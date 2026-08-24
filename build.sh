#!/usr/bin/env bash
#
# build.sh - Senfoto (LidarView) 一键构建脚本（支持 Ubuntu / Windows）
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
case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*|Windows_NT) IS_WINDOWS=1 ;;
esac

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
echo " 产物: $BUILD_DIR/install/bin/LidarView"
echo "=========================================="
