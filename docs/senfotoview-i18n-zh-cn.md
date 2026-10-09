<!--
  功能：SenFoToView 中文（zh_CN）汉化 —— 架构说明与翻译修改指南（存档）。
  作者：acelan
  新建时间：2026-10-09
  修改时间：2026-10-09
-->

# SenFoToView 中文（zh_CN）汉化 —— 架构与操作指南

> 本文档记录本 fork 界面的**中文本地化（汉化）架构**与**修改/补充翻译的操作流程**，供后续维护者直接上手。
>
> 关联文件：
> - 工作目录与脚本：`lidarview/i18n/`（`README.md`、`build-zh-cn.sh`、`zh_CN/*.ts`）
> - 项目约定：`lidarview/AGENTS.md`（提交后更新 Changelog、净新增文件头注释规范）
> - 上游机制参考：ParaView 官方 *Localization Howto*

---

## 1. 概述

SenFoToView 是基于 **ParaView 6.1** 的客户端，其界面翻译**完全复用 ParaView 官方 i18n 机制**，无需改造应用代码。

- 官方翻译仓库 `paraview-translations` **不含 `zh_CN`**，本项目自行维护中文目录。
- 中文翻译以 Qt `.ts` 源文件维护，编译为 `.qm` 二进制，运行时由 ParaView 自动加载。
- 本项目的 `.qm` **直接投放**到安装树的 `translations/` 目录，**无需重新编译**。

已完成的汉化范围（见 §6）：ParaView 少量冒烟 + SenFoToView 自有菜单/工具栏/主窗口/欢迎/关于对话框。

---

## 2. 运行时架构

### 2.1 语言来源（优先级从高到低）

1. 环境变量 `PV_TRANSLATIONS_LOCALE`（快速强制，无需改设置）
2. 设置项 `GeneralSettings.InterfaceLanguage`（UI：`Edit → Settings → General → Interface language`）
3. 默认 `"en"`

### 2.2 加载流程

启动器模板 `CMake/paraview_client_initializer.cxx.in` 生成 `main()` 时内联以下逻辑：

```
locale = PVApp->getInterfaceLanguage()            // 见 §2.1
if locale != "en":
    installTranslator( getQtTranslations("paraview", locale) )  // paraview_<locale>.qm
    installTranslator( getQtTranslations("qt",       locale) )  // qt_<locale>.qm
    installTranslator( getQtTranslations("qtbase",   locale) )  // qtbase_<locale>.qm
```

`getQtTranslations(prefix, locale)`：
1. 先在 Qt 自身的 `QLibraryInfo::TranslationsPath` 找 `<prefix>_<locale>.qm`；
2. 再在“翻译目录”（见 §2.3）找 `<prefix>_<locale>.qm`；
3. 都找不到则输出警告 `Could not load a <prefix> translation file with associated locale <locale>`。

> **要点**：`paraview_<locale>.qm` 是一个**合并文件**，内含全部 8 个 ParaView 目录 + 本项目自有目录的所有 context。QTranslator 按 `(context, source)` 匹配，因此合并无冲突。

### 2.3 `.qm` 命名与搜索路径

| 项 | 规则 |
|---|---|
| 搜索路径 | 环境变量 `PV_TRANSLATIONS_DIR`（冒号分隔，优先）→ 默认 `<install>/share/paraview-6.1/translations` |
| 文件名 | `<prefix>_<locale>.qm`，如 `paraview_zh_CN.qm`、`qt_zh_CN.qm`、`qtbase_zh_CN.qm` |
| 语言下拉 | `pqLanguageChooserWidget` 扫描上述目录里所有 `*.qm` 自动列出语言；**只有 `paraview_<locale>.qm` 等有效文件存在，才会出现该语言** |

### 2.4 翻译 context（关键）

Qt 按 (context, source) 匹配翻译。本项目涉及两类 context：

| 字符串来源 | context |
|---|---|
| C++ 代码 / `.ui` 文件 | 所在**类名**（如 `LidarViewMainWindow`、`lqFileMenuBuilder`、`LidarViewMainWindow`） |
| ParaView 代理/菜单 XML 的 `label` / `menu_label` | 固定为 `ServerManagerXML` |

> **常见坑**：即使 source 文本相同，**context 不同则不会命中**。例如主菜单栏 `File` 早期无法翻译，是因为它由 SenFoToView 自己的 `LidarViewMainWindow`/菜单 `.ui` 生成（context 为自有类名），而 ParaView 目录里的同名串 context 是 `MainWindow`/`pqParaViewMenuBuilders`。

### 2.5 资源目录定位与 `doc` 依赖（重要陷阱）

`GetParaViewTranslationsDirectory()` = `GetParaViewSharedResourcesDirectory() + "/translations"`。
而 `GetParaViewSharedResourcesDirectory()` 用 `vtkResourceFileLocator` **定位资源目录下的 `doc` 子目录**来反推共享资源目录。

本项目 install 树 `build/install/share/paraview-6.1/` **只有 `xmls`、没有 `doc`**，导致：
- 默认 translations 目录解析为空 → 不设 `PV_TRANSLATIONS_DIR` 时 `.qm` 加载失败；
- 设置里的语言下拉也列不出中文。

**解决**：`i18n/build-zh-cn.sh` 在投放时补建一个空的同级 `doc/` 目录（`mkdir -p "$(dirname "$INSTALL_DIR")/doc"`）。
临时替代：`PV_TRANSLATIONS_DIR=<你的 translations 目录>`。

---

## 3. 构建架构（`i18n/`）

### 3.1 目录结构

```
i18n/
├── README.md            # 简版说明（指向本文档）
├── build-zh-cn.sh       # 合并 .ts → paraview_zh_CN.qm，收集 Qt qm，投放
├── .gitignore           # 忽略 out/
├── out/                 # 构建产物（不入库）
└── zh_CN/               # 9 个目录文件
    ├── Qt_Core.ts                     # ┐
    ├── Qt_Components.ts               # │
    ├── Qt_ApplicationComponents.ts    # │ ParaView 官方 8 目录（骨架，
    ├── Qt_Widgets.ts                  # │ 绝大多数 type="unfinished"）
    ├── Qt_Python.ts                   # │
    ├── Clients_ParaView.ts            # │
    ├── Clients_ParaView-XMLs.ts       # │
    ├── ServerManager-XMLs.ts          # ┘
    └── LidarView.ts      # SenFoToView 自有 UI（菜单/工具栏/主窗口/欢迎/关于）
```

### 3.2 ParaView 官方目录（8 个）

由官方 `paraview-translations` 仓库的 `en/*.ts` 模板复制而来，仅把
`<TS version="2.1">` 改为 `<TS version="2.1" language="zh_CN">`。官方源仓库：
`https://gitlab.kitware.com/paraview/paraview-translations`（现有语言：`de_DE es_ES fr_FR it_IT ja_JP ko_KR nl_NL pt_BR tr_TR`）。
全部约 **11,503** 条（`ServerManager-XMLs.ts` 独占 8,524）。

### 3.3 SenFoToView 自有目录（`LidarView.ts`）

- 由 `lupdate` 从 `Application/` 的 `.cxx / .h / .ui` 提取（**136 条**），context 为类名
  （`LidarViewMainWindow`、`lqFileMenuBuilder`、`lqEditMenuBuilder`、`lqHelpMenuBuilder`、
  `lqMainControlsToolbar`、`lqInterfaceControlsToolbar`、`lqWelcomeDialog`、`lqAboutDialog`、`pqmacrosToolbar`）。
- 另附加 `ServerManagerXML` context 的客户端**菜单分类标签**（`lvFilters.xml`/`lvSources.xml`
  的 `menu_label`，共 7 条）。
- 已翻译 **124 条**自有 UI + 7 条分类标签。

### 3.4 `build-zh-cn.sh` 流程

```
1. /usr/lib/qt6/bin/lconvert  zh_CN/*.ts  →  out/paraview_zh_CN.qm   （合并全部目录）
2. 收集 Qt 自带       qt_zh_CN.qm / qtbase_zh_CN.qm   （默认 /usr/share/qt6/translations）
3. 投放 3 个 .qm  →  <INSTALL>/share/paraview-6.1/translations/
4. 补建同级 <INSTALL>/share/paraview-6.1/doc/ 空目录（见 §2.5）
```

可用环境变量：`LCONVERT`（默认 `/usr/lib/qt6/bin/lconvert`）、`QT_TRANSLATIONS_DIR`、
`OUT_DIR`、`INSTALL_DIR`；也可用第一个位置参数覆盖安装目录。

---

## 4. 修改 / 补充翻译的操作流程

### 4.1 修改已有翻译

1. 用 Qt Linguist 或任意 `.ts` 编辑器打开 `i18n/zh_CN/<目录>.ts`；
2. 找到目标 `<message>`，把 `<translation type="unfinished"></translation>`
   填上中文并去掉 `type="unfinished"`：
   ```xml
   <message>
       <source>Open Pcap File</source>
       <translation>打开 Pcap 文件</translation>
   </message>
   ```
3. 保存后执行 §4.3 的重建+投放+验证。

> **目录对照**：UI 控件/对话框 → `Qt_*`；ParaView 代理/菜单 XML → `ServerManager-XMLs.ts` / `Clients_ParaView-XMLs.ts`；SenFoToView 自有 → `LidarView.ts`。

### 4.2 新增 / 刷新源字符串

**SenFoToView 自有字符串**（源码改动了 `tr(...)` 或 `.ui` 后）：用 `lupdate` 重新提取。
`lupdate` 不直接吃目录、也不认 `.cxx`（需经 `.pro`）：

```bash
cd lidarview
root=$(pwd)
srcs=$(find Application \( -name '*.cxx' -o -name '*.h' \) -printf "$root/%p " 2>/dev/null)
forms=$(find Application -name '*.ui' -printf "$root/%p " 2>/dev/null)
{ printf 'SOURCES = %s\n' "$srcs"; printf 'FORMS = %s\n' "$forms";
  printf 'TRANSLATIONS = %s\n' "$root/i18n/zh_CN/LidarView.ts"; } > /tmp/lv.pro
/usr/lib/qt6/bin/lupdate -no-obsolete /tmp/lv.pro
rm -f .qmake.stash        # lupdate 会在仓库根留下该临时文件
```

**ParaView 官方目录新增源串**：从官方仓库拉取新 `en/*.ts`，把新增 `<message>` 合并进
`i18n/zh_CN/` 对应文件后补翻译。

**客户端 XML 菜单标签**（新增 `Category menu_label` 等）：在 `LidarView.ts` 末尾的
`ServerManagerXML` context 里补 `<message>`（source 为**解码后**的文本，`&` 在 `.ts` 中写作 `&amp;`）。
批量提取可用 ParaView 的 `CMake/XML_translations_header_generator.py`：

```bash
python3 <paraview>/CMake/XML_translations_header_generator.py \
  -o /tmp/xmls.h -s "$(pwd)/" \
  Application/Client/lvFilters.xml Application/Client/lvSources.xml
/usr/lib/qt6/bin/lupdate /tmp/xmls.h -ts /tmp/xmls.ts   # context 均为 ServerManagerXML
```

### 4.3 重建 + 投放 + 验证

```bash
cd lidarview
./i18n/build-zh-cn.sh                                   # 合并 + 投放（默认投到 build/install/...）
PV_TRANSLATIONS_LOCALE=zh_CN /mnt/d/work/senfoto-view/build/install/bin/SenFoToView
```

**无需重新编译**：ParaView 运行时直接从 `translations/` 读取 `.qm`；重启应用即可。

### 4.4 新增一种语言（示例：某 `xx_YY`）

1. `mkdir i18n/xx_YY`，从官方 `en/*.ts` 复制 8 个目录，改 `language="xx_YY"`；
2. 复制 `LidarView.ts` 并改 `language`；
3. 复制 `build-zh-cn.sh` 或在脚本中参数化语言，产出 `paraview_xx_YY.qm`；
4. 投放后用 `PV_TRANSLATIONS_LOCALE=xx_YY` 验证。

---

## 5. 验证

### 5.1 启动与断言

```bash
PV_TRANSLATIONS_LOCALE=zh_CN /mnt/d/work/senfoto-view/build/install/bin/SenFoToView
```

- 语言下拉出现「中文」← 只要存在有效 `paraview_zh_CN.qm` 即成立；
- 主菜单栏显示 文件(F)/编辑(E)/视图(V)/帮助(H)；
- 工具栏/面板标题菜单（如 管线浏览器、属性、查找数据…）显示中文。

查看翻译告警：启动日志中若出现
`Could not load a paraview translation file with associated locale zh_CN`，则是 §2.5 的路径问题。

### 5.2 校验 `.qm` 内容（无需启动 GUI）

```bash
/usr/lib/qt6/bin/lconvert -i build/install/share/paraview-6.1/translations/paraview_zh_CN.qm -o /tmp/check.ts
grep -c '<translation>[^<]' /tmp/check.ts     # 已翻译条数
```

### 5.3 GUI 截图（WSLg 环境）

本工作区 `DISPLAY=:1` 为 WSLg 集成桌面，`ffmpeg x11grab`/`import` 抓根窗口**只能拿到 Windows 桌面**（Linux 窗口是独立 HWND）。可靠做法是对目标 X 窗口直接 `XGetImage`：

- 用 `xwininfo -root -tree | grep '("SenFoToView" "SenFoToView")'` 找到窗口（主窗口尺寸 2560×1453）；
- 用 `XGetImage` 把该窗口像素导出为 PPM，再用 PIL 转 PNG。
  （本次验证即用此法的临时 C 程序；`xwd` 对 ARGB 窗口会报 `BadColor`，`PIL`/`ffmpeg` 均不认 XWD。）

---

## 6. 当前覆盖范围与待办

| 范围 | 状态 |
|---|---|
| ParaView 内核 UI（8 目录，~11.5k 条） | ⏳ 仅 150 条冒烟，其余英文 |
| SenFoToView 自有 UI（菜单/工具栏/主窗口/欢迎/关于） | ✅ 124 条 + 7 条分类标签 |
| 各插件过滤器/属性 `label`（`<SourceProxy label>` / `<Property>`） | ❌ 未覆盖（海量） |
| superbuild 持久化（随构建自动产出 `.qm`） | ❌ 未做 |

**待办建议**：
1. 插件 label：用 §4.2 的 `XML_translations_header_generator.py` 批量提取后翻译；
2. ParaView 全量：脚本化机器翻译 + 术语校对（术语建议见下）；
3. 持久化：将 `zh_CN` 纳入翻译项目并把 `zh_CN` 加入
   `lidarview-superbuild/pvsb/projects/paraview.bundle.common.cmake` 的 `paraview_languages`。

**术语建议**（待评审）：点云=point cloud；激光雷达=LiDAR；帧=frame；渲染视图=Render View；
管线浏览器=Pipeline Browser；属性=Properties；滤波器=Filter；数据流=Stream；抓包文件=Pcap File。

---

## 7. 故障排查

| 现象 | 原因 / 解决 |
|---|---|
| 启动日志 `Could not load ... zh_CN` | §2.5：缺 `doc` 目录或 `translations/` 路径不对 → 运行脚本补建 `doc/`，或设 `PV_TRANSLATIONS_DIR` |
| 语言下拉没有「中文」 | `paraview_zh_CN.qm` 不在搜索目录，或 `doc` 缺失 |
| 某些串已翻译但仍显示英文 | context 不匹配（§2.4）；确认该串在运行时所属的类/`ServerManagerXML` |
| `.ts` 改了没生效 | 忘了跑 `build-zh-cn.sh` 重新生成 `.qm`；或没重启应用 |
| `lupdate` 报 `has no recognized extension` | 直接传了目录/`.cxx`；应经 `.pro`（§4.2） |
| 仓库出现 `.qmake.stash` | `lupdate` 临时文件，删除即可 |

---

## 8. 关键实现位置与参考

| 内容 | 位置 |
|---|---|
| 运行时装载 | `build/superbuild/paraview/src/CMake/paraview_client_initializer.cxx.in` |
| 语言/路径解析 | `.../Qt/Core/pqApplicationCore.cxx`（`getInterfaceLanguage` / `getTranslationsPathFromInterfaceLanguage` / `getQtTranslations`） |
| 语言选择控件 | `.../Qt/ApplicationComponents/pqLanguageChooserWidget.cxx` |
| 资源目录定位 | `.../Remoting/Core/vtkPVFileInformation.cxx`（`GetParaViewSharedResourcesDirectory`） |
| 翻译 CMake 宏 | `.../CMake/ParaViewTranslations.cmake` |
| XML→header 生成器 | `.../CMake/XML_translations_header_generator.py` |
| 官方本地化文档 | https://www.paraview.org/paraview-docs/latest/cxx/LocalizationHowto.html |
| 官方翻译仓库 | https://gitlab.kitware.com/paraview/paraview-translations |
