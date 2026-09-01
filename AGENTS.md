# AGENTS.md

项目约定（长期记忆，所有会话必须遵守）。

## 提交后更新 Changelog（必做）

每次向 `develop` 提交并推送到远程后，**必须**把本批次（本次推送的所有提交）的修改
记录追加到项目根目录的 `CHANGELOG.md`：

- 每个批次一个小节，格式：`## YYYY-MM-DD 批次：<主题>`
- 按提交列出：`<commit-id> <一句话摘要>` + 必要的问题/方案说明
- 若批次含未提交但已验证的修复（随本批次一起提交的），一并说明
- 推送成功后再写 changelog，写完与下批改动一起提交（或单独 docs 提交）

## 新增文件头注释规范

凡是因为需求而**净新增（net-new）**的文件，必须在文件头部添加注释块，包含以下字段：

- **功能**：一句话描述本文件的功能 / 用途
- **作者**：acelan
- **新建时间**：YYYY-MM-DD（文件首次创建的日期）
- **修改时间**：YYYY-MM-DD（最近一次修改的日期；后续改动时同步更新）

### 适用范围

- 仅适用于本 fork **净新增**的文件（即上游 `Kitware/LidarView` 不存在的文件）。
- **不要**在仅被修改的上游文件上添加该头注释，例如：
  - `Qt/ApplicationComponents/lqOpenLidarReaction.cxx`（上游已有，只在其内新增了 `AutoAttachRadialDenoise` 等逻辑）
  - `lvComponents.qrc` 及各类 `CMakeLists.txt` 中对上游注册的改动
  - 任何标准 VTK/ParaView 上游文件
- 上游判定方法：用 `raw.githubusercontent.com/Kitware/LidarView/master/<path>` 取文件，返回 404 即净新增。

### 模板

C/C++（`.h` / `.cxx` / `.cpp` / `.cc`）：放在标准 VTK `/*===*/` 头之后；若文件无该头，则用 `//` 块置于文件最顶部。

```cpp
// ============================================================
// 功能：<一句话描述本文件功能>
// 作者：acelan
// 新建时间：YYYY-MM-DD
// 修改时间：YYYY-MM-DD
// ============================================================
```

XML（`.ui` / `.xml`）：置于文件最顶部。

```xml
<!--
  功能：<一句话描述本文件功能>
  作者：acelan
  新建时间：YYYY-MM-DD
  修改时间：YYYY-MM-DD
-->
```

CMake（`CMakeLists.txt`）：置于文件最顶部。

```cmake
# ============================================================
# 功能：<一句话描述本文件功能>
# 作者：acelan
# 新建时间：YYYY-MM-DD
# 修改时间：YYYY-MM-DD
# ============================================================
```

其它文本文件（如 `.md`、`.json`）若需标注，参照上述字段，使用对应注释语法；JSON 等不支持注释的格式不要强行插入。

## 界面布局持久化文件与验证前清理（必做）

Qt 界面默认布局（dock 位置/大小、工具栏、窗口尺寸）会被 **ParaView 的 `pqPersistentMainWindowStateBehavior`** 持久化（`LidarViewMainWindow.cxx` 中 `pqParaViewBehaviors` 默认启用）：

- **存储文件**：`~/.config/SenFoToView/SenFoToView.ini`（QSettings IniFormat，非 plist）
- **关键段**：`[MainWindow]`，含 `Layout=@ByteArray(...)`（dock/工具栏布局）和 `Size`（窗口尺寸）
- **行为**：app **退出时写入**、**启动时恢复**，会覆盖 `LidarViewMainWindow.ui`、`interface_modes_config.json` 里的新默认值

### 规则

凡修改了布局相关默认值（如 `LidarViewMainWindow.ui` 的 `dockWidgetArea`、`LidarViewMainWindow.cxx` 的 `tabifyDockWidget`/`resizeDocks`、`interface_modes_config.json`），**验证效果前必须先清理该 ini 的 `[MainWindow]` 段**，否则看到的是旧布局缓存，会误判改动无效。

### 清理方法（app 必须先退出）

```bash
python3 -c "
import configparser
p = '/Users/acelan/.config/SenFoToView/SenFoToView.ini'
c = configparser.RawConfigParser(strict=False, allow_unnamed_section=True)
c.optionxform = str
c.read(p, encoding='utf-8')
c.remove_section('MainWindow')
with open(p, 'w', encoding='utf-8') as f:
    c.write(f, space_around_delimiters=False)
print('ini 已清理')
"
```

### 验证前构建注意

本项目是 superbuild，改完源码需两步（只跑第一步会用旧副本）：

```bash
ninja -C ../build/superbuild/lidarview/build   # 内层编译 → bin/SenFoToView.app
ninja -C ../build superbuild/lidarview         # install 步骤 → build/install/Applications/SenFoToView.app（日常启动的副本）
```

### 编译缓存（ccache）

内层构建已配置 ccache launcher（`CMAKE_C/CXX_COMPILER_LAUNCHER=ccache`，缓存上限 10G）。
若 superbuild 重新 configure 导致缓存失效，重跑：

```bash
cmake -S . -B ../build/superbuild/lidarview/build \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
```
