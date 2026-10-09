#!/usr/bin/env bash
#
# build-zh-cn.sh — 为 SenFoToView（LidarView fork）构建中文（zh_CN）翻译二进制。
#
# 机制（见 i18n/README.md）：
#   ParaView 运行时在 locale != "en" 时加载 paraview_<locale>.qm、qt_<locale>.qm、
#   qtbase_<locale>.qm。本脚本把 i18n/zh_CN/*.ts 合并为 paraview_zh_CN.qm，
#   并收集 Qt 自带的 qt_zh_CN.qm / qtbase_zh_CN.qm，一并投放进应用的
#   translations/ 目录（默认 build/install/share/paraview-6.1/translations）。
#
# 用法：
#   i18n/build-zh-cn.sh [安装目录]
#   环境变量：
#     LCONVERT              指定 lconvert（默认 /usr/lib/qt6/bin/lconvert）
#     QT_TRANSLATIONS_DIR   指定 Qt 翻译目录（默认 /usr/share/qt6/translations）
#     OUT_DIR               中间产物目录（默认 i18n/out）
#     INSTALL_DIR           覆盖安装目录（等价于第一个位置参数）
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$SCRIPT_DIR/zh_CN"
OUT_DIR="${OUT_DIR:-$SCRIPT_DIR/out}"

LCONVERT="${LCONVERT:-/usr/lib/qt6/bin/lconvert}"
QT_TRANSLATIONS_DIR="${QT_TRANSLATIONS_DIR:-/usr/share/qt6/translations}"

# 默认安装到 superbuild 的 install 树；亦可传参/用环境变量覆盖。
DEFAULT_INSTALL="/mnt/d/work/senfoto-view/build/install/share/paraview-6.1/translations"
INSTALL_DIR="${1:-${INSTALL_DIR:-$DEFAULT_INSTALL}}"

if [ ! -x "$LCONVERT" ]; then
  echo "ERROR: lconvert not found/executable: $LCONVERT" >&2
  echo "       set LCONVERT=/path/to/lconvert" >&2
  exit 1
fi

mkdir -p "$OUT_DIR"

mapfile -t ts_files < <(ls "$SRC_DIR"/*.ts 2>/dev/null)
if [ "${#ts_files[@]}" -eq 0 ]; then
  echo "ERROR: no .ts files in $SRC_DIR" >&2
  exit 1
fi

echo "==> merging ${#ts_files[@]} catalogs -> paraview_zh_CN.qm"
"$LCONVERT" "${ts_files[@]}" -o "$OUT_DIR/paraview_zh_CN.qm"

for qm in qt_zh_CN.qm qtbase_zh_CN.qm; do
  if [ -f "$QT_TRANSLATIONS_DIR/$qm" ]; then
    cp "$QT_TRANSLATIONS_DIR/$qm" "$OUT_DIR/$qm"
    echo "==> bundled Qt translation: $qm"
  else
    echo "WARN: $qm not found in $QT_TRANSLATIONS_DIR (skipped)" >&2
  fi
done

mkdir -p "$INSTALL_DIR"
cp "$OUT_DIR"/*.qm "$INSTALL_DIR"/

# ParaView 通过 vtkResourceFileLocator 定位资源目录下的 `doc` 子目录来反推
# “共享资源目录”（GetParaViewSharedResourcesDirectory），进而得到默认的
# translations 目录。本项目的 install 树未打包 doc，故这里补建一个空的同级
# doc 目录，使不设置 PV_TRANSLATIONS_DIR 时也能自动加载 .qm（并让设置里的
# Interface language 下拉能列出中文）。
mkdir -p "$(dirname "$INSTALL_DIR")/doc"

echo "==> installed into: $INSTALL_DIR"
ls -l "$INSTALL_DIR"/*.qm
echo
echo "启动验证："
echo "  PV_TRANSLATIONS_LOCALE=zh_CN /mnt/d/work/senfoto-view/build/install/bin/SenFoToView"
echo "或在 UI 里：Edit -> Settings -> Interface language -> 选择「中文」后重启。"
