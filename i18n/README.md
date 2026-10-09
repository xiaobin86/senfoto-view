# i18n — SenFoToView 中文（zh_CN）本地化

本目录为 LidarView fork（SenFoToView）的界面汉化工作区，基于 **ParaView 6.1 官方 i18n 机制**。

> 现状：**骨架 + 小规模验证**。已完成 zh_CN 目录骨架与构建/投放管道，仅注入少量冒烟翻译用于验证。
> 全量翻译、LidarView 自有字符串、superbuild 持久化见文末「后续阶段」。

---

## 1. 运行时机制（已在 ParaView 6.1 源码确认）

启动器模板 `CMake/paraview_client_initializer.cxx.in`：

```
locale = 环境变量 PV_TRANSLATIONS_LOCALE
         → 设置项 GeneralSettings.InterfaceLanguage
         → "en"
若 locale != "en"：
    installTranslator( getQtTranslations("paraview", locale) )   # 加载 paraview_<locale>.qm
    installTranslator( getQtTranslations("qt",       locale) )   # 加载 qt_<locale>.qm
    installTranslator( getQtTranslations("qtbase",   locale) )   # 加载 qtbase_<locale>.qm
```

- `.qm` 搜索路径优先级：环境变量 `PV_TRANSLATIONS_DIR`（冒号分隔多个目录）→ 默认
  `<install>/share/paraview-6.1/translations`（由 `vtkPVFileInformation::GetParaViewTranslationsDirectory()` 决定）。
- 语言下拉（`pqLanguageChooserWidget`）会扫描上述目录里所有 `*.qm`，自动列出其语言；选择后写入
  `GeneralSettings.InterfaceLanguage`，**需重启**生效。
- ServerManager XML 的 `label` / `menu_label` 也走 `QCoreApplication::translate("ServerManagerXML", …)`，
  因此过滤器/属性/菜单标签都能被 `.qm` 翻译。
- 快捷强制：`PV_TRANSLATIONS_LOCALE=zh_CN`（无需改设置）。

## 2. 官方翻译仓库

`gitlab.kitware.com/paraview/paraview-translations`（superbuild 项目 `paraviewtranslations`）。

- 单语言 = 8 个 `.ts`（本目录 `zh_CN/` 与其一一对应）：
  `Qt_Core` `Qt_Components` `Qt_ApplicationComponents` `Qt_Widgets` `Qt_Python`
  `Clients_ParaView` `Clients_ParaView-XMLs` `ServerManager-XMLs`
- 官方语言：`de_DE es_ES fr_FR it_IT ja_JP ko_KR nl_NL pt_BR tr_TR`（+ `en` 模板）。
  **官方没有 `zh_CN`**，需自行维护。
- 规模：全部约 **11,503** 条字符串（`ServerManager-XMLs.ts` 独占 8,524）。
- 编译：`cmake --build . --target <locale>` 会把该语言的 8 个 `.ts` 合并为
  `paraview_<locale>.qm`（内部即 `lconvert *.ts -o paraview_<locale>.qm`）。

## 3. 目录结构

```
i18n/
├── README.md            # 本文件
├── build-zh-cn.sh       # 合并 .ts → paraview_zh_CN.qm，并收集 Qt qm、投放
├── .gitignore           # 忽略 out/
├── out/                 # 构建产物（不入库）
└── zh_CN/               # 8 个 zh_CN 目录文件（骨架，大部分 type="unfinished"）
    ├── Qt_Core.ts
    ├── Qt_Components.ts
    ├── Qt_ApplicationComponents.ts
    ├── Qt_Widgets.ts
    ├── Qt_Python.ts
    ├── Clients_ParaView.ts
    ├── Clients_ParaView-XMLs.ts
    └── ServerManager-XMLs.ts
```

`zh_CN/*.ts` 由官方 `en/*.ts` 模板复制而来，仅把 `<TS version="2.1">` 改为
`<TS version="2.1" language="zh_CN">`，并注入少量冒烟翻译（见第 5 节）。

## 4. 构建与投放

```bash
# 合并 .ts → paraview_zh_CN.qm，并安装到应用的 translations/ 目录
./i18n/build-zh-cn.sh
# 或指定安装目录
./i18n/build-zh-cn.sh /path/to/share/paraview-6.1/translations
```

脚本会产出/投放 3 个文件：

| 文件 | 来源 | 作用 |
|---|---|---|
| `paraview_zh_CN.qm` | 合并 `i18n/zh_CN/*.ts` | ParaView/LidarView UI 与 XML 标签 |
| `qt_zh_CN.qm` | 系统 Qt6 | Qt 通用控件/对话框 |
| `qtbase_zh_CN.qm` | 系统 Qt6 | Qt 基础模块 |

> 无需重新编译：ParaView 运行时直接从 `translations/` 目录读取 `.qm`。

### 注意：默认目录需要一个同级 `doc` 目录

`GetParaViewTranslationsDirectory()` = `GetParaViewSharedResourcesDirectory() + "/translations"`，
后者用 `vtkResourceFileLocator` **定位资源目录下的 `doc` 子目录**来反推共享资源目录。
本项目的 install 树（`build/install/share/paraview-6.1/`）只有 `xmls`、没有 `doc`，会导致
默认 translations 目录解析为空 → 不设 `PV_TRANSLATIONS_DIR` 时加载失败、语言下拉也列不出中文。
`build-zh-cn.sh` 会补建一个空的同级 `doc/` 目录解决此问题。
（临时替代：`PV_TRANSLATIONS_DIR=<你的 translations 目录>` 可绕过。）

## 5. 验证

```bash
PV_TRANSLATIONS_LOCALE=zh_CN /mnt/d/work/senfoto-view/build/install/bin/SenFoToView
```

- 或 UI：`Edit → Settings → Interface language → 选择「中文」`，点 Apply/OK 后重启。
- 冒烟断言：
  1. 语言下拉出现「中文」（只要存在有效 `paraview_zh_CN.qm` 即成立）；
  2. 主菜单/若干面板出现中文（来自 `Clients_ParaView.ts` 的 29 条冒烟翻译等）。

## 6. 补翻译流程（后续）

1. 用 Qt Linguist / 任意 `.ts` 编辑器打开 `i18n/zh_CN/<catalog>.ts`，把
   `<translation type="unfinished"></translation>` 填上中文（去掉 `type="unfinished"`）。
2. 重新运行 `./i18n/build-zh-cn.sh` 并重启应用。
3. 如需把官方新增源串同步进骨架：
   ```bash
   git clone https://gitlab.kitware.com/paraview/paraview-translations.git
   # 用官方 en/<catalog>.ts 覆盖 i18n/zh_CN/ 对应文件的 source/location，
   # 或借助该仓库的 files_update 目标后迁回已翻译内容。
   ```

术语建议（待评审）：点云 = point cloud，激光雷达 = LiDAR，帧 = frame，
渲染视图 = Render View，管线浏览器 = Pipeline Browser，属性 = Properties。

## 7. 后续阶段（未包含在本骨架中）

- **P1 全量翻译**：对 8 个目录的 11.5k 条字符串做机器翻译 + 术语校对（可用脚本批量调用翻译服务/LLM）。
- **P2 LidarView 自有字符串**：用 `lupdate` 提取 `Application/` + `lvFilters.xml`/`lvSources.xml`
  及自有 `Plugins/*.xml`，生成 `Clients_SenFoToView*.ts` 并翻译，合并进 `paraview_zh_CN.qm`。
- **P3 superbuild 持久化**：将 `zh_CN` 纳入翻译项目、把 `zh_CN` 加入 `paraview_languages`
  （见 `lidarview-superbuild/pvsb/projects/paraview.bundle.common.cmake`），随构建自动产出 `.qm`。

## 8. 关键实现参考

| 内容 | 位置 |
|---|---|
| 运行时装载 | `build/superbuild/paraview/src/CMake/paraview_client_initializer.cxx.in` |
| 语言/路径解析 | `build/superbuild/paraview/src/Qt/Core/pqApplicationCore.cxx`（`getInterfaceLanguage` / `getTranslationsPathFromInterfaceLanguage` / `getQtTranslations`） |
| 语言选择控件 | `build/superbuild/paraview/src/Qt/ApplicationComponents/pqLanguageChooserWidget.cxx` |
| XML 标签翻译 | `QCoreApplication::translate("ServerManagerXML", …)`（ParaView Qt 代码） |
| 翻译 CMake 宏 | `build/superbuild/paraview/src/CMake/ParaViewTranslations.cmake` |
| 官方翻译仓库 | https://gitlab.kitware.com/paraview/paraview-translations |
| 官方本地化文档 | https://www.paraview.org/paraview-docs/latest/cxx/LocalizationHowto.html |
