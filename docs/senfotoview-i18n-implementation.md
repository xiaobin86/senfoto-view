<!--
  功能：SenFoToView 多语言（i18n）实现文档 —— 从 Qt 引擎底层到 ParaView 运行时再到本 fork 流水线，
        说明「多语言到底是怎么实现的」，并给出新增字符串的端到端操作。
  作者：acelan
  新建时间：2026-10-17
  修改时间：2026-10-17
-->

# SenFoToView 多语言（i18n）实现 —— 底层机制与端到端流程

> 本文是**实现层**文档，从最底层（Qt 翻译引擎）逐层往上讲清楚「翻译是怎么被找到并生效的」，
> 以及在本 fork 里**新增一个用户可见字符串时到底要动哪些文件、跑哪些命令**。
>
> 关联文档：
> - 架构 + 操作指南：`docs/senfotoview-i18n-zh-cn.md`
> - 方案 + 翻译规范 + 避坑：`docs/senfotoview-i18n-plan.md`
> - 工作区说明：`i18n/README.md`
> - 项目约定：`AGENTS.md`（§「汉化（i18n）翻译规范（必守）」）

---

## 0. 一句话总览

界面上一句 `tr("Open Pcap File")`，在运行时被替换成「打开 Pcap 文件」，要经过 **5 层**：

```
[1] Qt 引擎       tr() → QCoreApplication::translate(ctx, src) → 遍历已安装 QTranslator
[2] 目录/二进制   .ts(XML catalog)  --lupdate-->  源码    --lconvert/lrelease-->  .qm(二进制哈希表)
[3] 工具链        lupdate 提取 / lconvert·lrelease 编译（本 fork 用 lconvert 合并多目录）
[4] ParaView 运行时 生成的 main() 读 locale → installTranslator(paraview_<loc>.qm / qt_ / qtbase_)
[5] 本 fork 流水线  i18n/zh_CN/*.ts → build-zh-cn.sh → 投放 <install>/share/paraview-6.1/translations/
```

关键心法：**翻译的“键”是 `(context, source)` 二元组，不是字符串本身。** context 不同则同文本
也命中不了；`.qm` 里就是这张 `(context, source[, disambiguation]) → translation` 的哈希表。

---

## 1. 第一层：Qt 翻译引擎（最底层）

### 1.1 `tr()` 背后是什么

`tr()` 是 `QObject::tr()` 的宏/静态封装，最终都落到：

```cpp
QString QCoreApplication::translate(
    const char* context,      // 上下文（通常=类名）
    const char* sourceText,   // 源串（英文）
    const char* disambiguation = nullptr,  // 同 context 同 source 时的消歧
    int n = -1);              // 复数形式用
```

- 若没有任何已安装的 `QTranslator` 命中，则**原样返回 sourceText**（这就是“没翻译=英文”的根本原因）。
- context 从哪来：
  - 类内 `tr(...)`：context = 该类的**类名**（`Q_OBJECT`/`Q_DECLARE_TR_FUNCTIONS` 注入）。
  - `.ui` 生成的 `ui_xxx.h`：`QApplication::translate("类名", ...)`，context = **`.ui` 里的 `<class>` 名**。
  - 全局/静态：`QCoreApplication::translate("SomeContext", ...)` 手动指定。

### 1.2 查找键与优先级

- 查找键 = `(context, sourceText, disambiguation[, n])`。
- 若安装了多个 `QTranslator`，**后安装者优先**（LIFO）；先命中的生效。
  - 这正是 ParaView 装 `paraview_<loc>.qm` + `qt_<loc>.qm` + `qtbase_<loc>.qm` 三个互不冲突的原因。
- `disambiguation`：同一 context、同一 source 却有不同译法时用来区分（如 `tr("Open", "file")` vs `tr("Open", "port")`）。
- `%n` 复数：Qt 用 `numerus` 规则选 `<translation>` 里的一个；中文一般无复数变化，但仍需占位。

### 1.3 语言（locale）从哪来

`QTranslator::load("<prefix>_<locale>.qm")` 里的 `<locale>` 是显式给的文件名一部分；
应用侧把“当前语言”解析成 `locale`（见 §4）。`QLocale` 只影响格式化（数字/日期），不决定翻译。

---

## 2. 第二层：目录（`.ts`）与二进制（`.qm`）格式

### 2.1 `.ts` —— 人类可读的 XML catalog

```xml
<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="zh_CN">
<context>
    <name>LidarViewMainWindow</name>          <!-- context = 类名 -->
    <message>
        <location filename=".../LidarViewMainWindow.cxx" line="216"/>  <!-- 仅信息，运行时不用 -->
        <source>Sensor IP Config</source>      <!-- 键：源串（必须与 tr() 参数逐字符一致） -->
        <translation>雷达 IP 配置</translation> <!-- 值 -->
    </message>
    <message numerus="yes">                     <!-- 复数 -->
        <source>%n file(s)</source>
        <translation><numerusform>%n 个文件</numerusform></translation>
    </message>
</context>
</TS>
```

要点：
- `<name>` 即 context；`<source>` 必须与代码里 `tr()` 的实参**逐字符相同**（含空格、大小写、`&`）。
- `<translation type="unfinished">` 表示未翻译（`.qm` 里通常不收录或落回源串）。
- 同一个 `<source>` 可出现在**多个 context** 下（各自独立）。

### 2.2 `.qm` —— 编译后的二进制哈希表

- 由 `lrelease` 或 `lconvert` 生成；本质是 `(context, source, disambiguation[, numerus]) → translation` 的紧凑哈希表。
- 运行时 `QTranslator::load()` 把它读进内存，`translate()` 做 O(1) 查找。
- **`.qm` 不参与编译**应用：它是纯数据文件，改完投放、重启即可（本 fork 正是靠这点免重编）。

---

## 3. 第三层：工具链（提取 / 编译 / 合并）

| 工具 | 方向 | 作用 |
|---|---|---|
| `lupdate` | 源码 → `.ts` | 扫描源码里的 `tr()`/`.ui`，生成/刷新 catalog（context=类名） |
| `lrelease` | `.ts` → `.qm` | 单 catalog 编译 |
| `lconvert` | `.ts`(多个) → `.qm` | **合并多 catalog** 编译（本 fork 用这个） |

### 3.1 `lupdate` 的输入约定

`lupdate` 不直接吃目录，也不认独立 `.cxx`（除非经 `.pro` 或直接给 `.ts`）。本项目做法（见 §7.2）
是临时写一个 `.pro`：

```pro
SOURCES = /abs/path/Application/Client/LidarViewMainWindow.cxx ...
FORMS   = /abs/path/Application/Client/LidarViewMainWindow.ui ...
TRANSLATIONS = /abs/path/i18n/zh_CN/LidarView.ts
```

- C++ `.cxx/.h` 里的 `tr()` → context = 类名。
- `.ui` → context = `<class>` 名；同时会生成 `.ui` 里的所有可见文本条目。
- XML 的 `label`/`menu_label` 不归 `lupdate`（见 §3.3）。

### 3.2 合并

`lconvert a.ts b.ts ... -o paraview_<locale>.qm` 把多个 catalog 合进一个 `.qm`。
因为键含 context，多目录合并**不冲突**。ParaView 官方 `ParaViewTranslations.cmake` 的
`paraview_<locale>` target 内部就是 `lconvert`；本 fork 因官方无 `zh_CN`，自行脚本化。

### 3.3 XML `label` 的 special case

ServerManager XML（`<SourceProxy label=... />`、`<Category menu_label=... />`）由 ParaView 在运行时用
`QCoreApplication::translate("ServerManagerXML", label)` 翻译。**context 固定为 `ServerManagerXML`**。
ParaView 提供 `CMake/XML_translations_header_generator.py` 把 XML 里的 label 抽成头文件再交给 `lupdate`。

---

## 4. 第四层：ParaView 运行时装载

ParaView 作为基于 Qt 的应用，`main()` 由 `paraview_client_add` 用模板
`CMake/paraview_client_initializer.cxx.in` 生成，其中内联了翻译装载逻辑：

```cpp
QString locale = PVApp->getInterfaceLanguage();   // 见 §4.1
if (locale != "en") {
  translator = getQtTranslations("paraview", locale);  installTranslator(translator);  // paraview_<loc>.qm
  translator = getQtTranslations("qt",       locale);  installTranslator(translator);  // qt_<loc>.qm
  translator = getQtTranslations("qtbase",   locale);  installTranslator(translator);  // qtbase_<loc>.qm
}
```

### 4.1 locale 解析优先级

| 优先级 | 来源 |
|---|---|
| 1（最高） | 环境变量 `PV_TRANSLATIONS_LOCALE`（快速强制，无需改设置） |
| 2 | 设置项 `GeneralSettings.InterfaceLanguage`（UI：`Edit → Settings → General → Interface language`） |
| 3 | 默认 `"en"` |

### 4.2 `.qm` 搜索路径（`getQtTranslations`）

1. 先在 Qt 自身 `QLibraryInfo::TranslationsPath` 找 `<prefix>_<locale>.qm`（Qt 自带的 `qt_/qtbase_` 在此）；
2. 再在 ParaView 的 translations 目录找；
3. 都找不到 → 运行日志警告 `Could not load a <prefix> translation file with associated locale <locale>`。

ParaView translations 目录：
`vtkPVFileInformation::GetParaViewTranslationsDirectory()` = `GetParaViewSharedResourcesDirectory() + "/translations"`；
可用环境变量 `PV_TRANSLATIONS_DIR`（冒号分隔）覆盖。

### 4.3 「`doc` 目录陷阱」（本 fork 踩过）

`GetParaViewSharedResourcesDirectory()` 用 `vtkResourceFileLocator` **靠资源目录下存在 `doc/` 子目录**
来反推共享资源目录。本 fork 的 install 树 `build/install/share/paraview-6.1/` **只有 `xmls`、没有 `doc`**，
于是默认 translations 目录解析为空 → 不设 `PV_TRANSLATIONS_DIR` 时加载失败、语言下拉也列不出中文。
**解决**：投放脚本补建同级空目录 `<install>/share/paraview-6.1/doc/`。

### 4.4 语言下拉

`pqLanguageChooserWidget` 扫描上述 translations 目录里的所有 `paraview_*.qm`，自动列出语言；
选中后写入 `GeneralSettings.InterfaceLanguage`，**需重启**生效。

---

## 5. 第五层：本 fork 的落地（`i18n/` 工作区）

### 5.1 目录结构

```
i18n/
├── build-zh-cn.sh        # 合并 .ts → paraview_zh_CN.qm，收集 Qt qm，投放（补建 doc/）
├── translate_zh.py       # 机器翻译（Argos，专有名词译前遮蔽/译后还原）
├── qa_zh.py              # 守则检测（PROPER_NOUN / SHORTCUT / SUSPECT）
├── out/                  # 构建产物（.gitignore 忽略）
└── zh_CN/                # 9 个 catalog
    ├── Qt_Core.ts / Qt_Components.ts / Qt_ApplicationComponents.ts / Qt_Widgets.ts / Qt_Python.ts
    ├── Clients_ParaView.ts / Clients_ParaView-XMLs.ts / ServerManager-XMLs.ts   # ParaView 官方 8 目录
    └── LidarView.ts      # 本 fork 自有 UI（菜单/工具栏/主窗口/欢迎/关于）+ ServerManagerXML 菜单分类标签
```

### 5.2 `build-zh-cn.sh` 流程

```
1. /usr/lib/qt6/bin/lconvert  i18n/zh_CN/*.ts  →  out/paraview_zh_CN.qm    （合并 9 个目录）
2. 收集系统 Qt 的 qt_zh_CN.qm / qtbase_zh_CN.qm （默认 /usr/share/qt6/translations）
3. 投放 3 个 .qm → <INSTALL>/share/paraview-6.1/translations/
4. 补建同级 <INSTALL>/share/paraview-6.1/doc/ 空目录（§4.3）
```

可覆盖变量：`LCONVERT`、`QT_TRANSLATIONS_DIR`、`OUT_DIR`、`INSTALL_DIR`（或第一个位置参数）。

### 5.3 catalog ↔ 源码对照

| 用户可见字符串来源 | 进哪个 catalog | context |
|---|---|---|
| `Application/` 自有 `.cxx/.h/.ui` | `LidarView.ts` | 类名 / `.ui` 的 `<class>` |
| `Application/Client/lvFilters.xml`、`lvSources.xml` 的 `menu_label` | `LidarView.ts` | `ServerManagerXML` |
| `Qt/`、ParaView Qt 组件 | `Qt_*.ts` / `Clients_ParaView*.ts` | 各类名 |
| ServerManager 代理 `label` | `ServerManager-XMLs.ts` | `ServerManagerXML` |

---

## 6. context 到底按什么取（最容易错的地方）

| 字符串写在哪 | 运行时 context |
|---|---|
| 类 `Foo` 里的 `tr("X")` | `Foo` |
| `.ui` 文件（`<class>Foo</class>`） | `Foo` |
| `QCoreApplication::translate("Bar", "X")` | `Bar`（手动） |
| ServerManager XML 的 `label`/`menu_label` | `ServerManagerXML` |
| ParaView Qt 代码里的 `tr()` | 对应 ParaView 类名（`MainWindow`、`pqParaViewMenuBuilders`…） |

> **陷阱**：主菜单栏 `File` 曾翻译不出来——因为它由本 fork 的 `LidarViewMainWindow.ui` 生成
> （context=`LidarViewMainWindow`），而官方 catalog 里的同名串 context 是 `MainWindow`。**同文本不同 context 不命中。**

---

## 7. 端到端：新增一个用户可见字符串要做什么（Checklist）

> **本项目约定（`AGENTS.md`）：新增功能必须同步补中文翻译，不允许只留英文。**

### 7.1 代码侧

- C++ 用 `tr("English Text")`（**建议英文源串**，便于复用官方翻译/机器翻译）；不要硬编码中文再 `tr()`。
- `.ui` 里的文本会**自动**进 catalog（但本 fork 的对话框惯用硬编码中文——那部分**不走翻译**、直接显示中文；
  若希望可翻译，应改为英文并走 `tr`/`.ui` 文本条目）。
- XML `label`/`menu_label`：改 XML 后按 §3.3 抽成头文件再 `lupdate`（context=`ServerManagerXML`）。
- 动态拼接（如 `tr("Loaded %1").arg(n)`）：只把**模板**放进 `tr()`，参数在 `arg()`。

### 7.2 加入翻译（本项目最小流程）

1. 在正确 catalog（通常 `i18n/zh_CN/LidarView.ts`）的**正确 context** 下加：
   ```xml
   <message>
       <source>Sensor IP Config</source>
       <translation>雷达 IP 配置</translation>
   </message>
   ```
   > `<location>` 可选；`lupdate` 重跑会自动补。手加亦可（`lconvert` 不需要 location）。
2. 重建并投放：`./i18n/build-zh-cn.sh`
3. 重启应用：`PV_TRANSLATIONS_LOCALE=zh_CN .../bin/SenFoToView`（或 Settings 选中文后重启）。

> 若源码字符串较多，先用 §3.1 的 `.pro` + `lupdate` 批量刷新 `LidarView.ts`，再补译文。

### 7.3 校验

```bash
# 反编译 .qm，确认条目在
/usr/lib/qt6/bin/lconvert -i <install>/share/paraview-6.1/translations/paraview_zh_CN.qm -o /tmp/check.ts
grep -A1 '<source>Sensor IP Config</source>' /tmp/check.ts
```

---

## 8. 常见陷阱（实战）

| 现象 | 根因 | 处置 |
|---|---|---|
| 新 `tr()` 串显示英文 | 未进 `.ts` / `.qm` | 按 §7.2 补条目 + 重建 `.qm` |
| 条目在 `.ts` 里但不生效 | context 不匹配 | 确认运行时 context（§6），放到正确 `<context>` |
| 启动日志 `Could not load ... zh_CN` | 缺 `doc/` 或路径不对 | 跑 `build-zh-cn.sh`（补建 `doc/`）或设 `PV_TRANSLATIONS_DIR` |
| 语言下拉没有「中文」 | `paraview_zh_CN.qm` 不在搜索目录 | 投放 `.qm` 到 `<install>/share/paraview-6.1/translations/` |
| 改了 `.ts` 没变化 | 忘记 `build-zh-cn.sh` / 没重启 | 重建 + 重启 |
| `lupdate` 报 `no recognized extension` | 直接传了目录/`.cxx` | 经 `.pro`（§3.1） |
| 仓库根冒出 `.qmake.stash` | `lupdate` 临时文件 | 删除 |
| `.ui` 头部被 `uic` 报错 | 注释写在 `<?xml?>` 之前 | XML 声明必须在最前，注释放其后 |

---

## 9. 关键实现位置索引

| 内容 | 位置 |
|---|---|
| 运行时装载（生成的 main） | `build/superbuild/paraview/src/CMake/paraview_client_initializer.cxx.in` |
| locale / 路径解析 | `build/superbuild/paraview/src/Qt/Core/pqApplicationCore.cxx`（`getInterfaceLanguage` / `getQtTranslations` / `getTranslationsPathFromInterfaceLanguage`） |
| 语言选择控件 | `build/superbuild/paraview/src/Qt/ApplicationComponents/pqLanguageChooserWidget.cxx` |
| 共享资源目录定位 | `build/superbuild/paraview/src/Remoting/Core/vtkPVFileInformation.cxx` |
| 翻译 CMake 宏 | `build/superbuild/paraview/src/CMake/ParaViewTranslations.cmake` |
| XML→header 生成器 | `build/superbuild/paraview/src/CMake/XML_translations_header_generator.py` |
| 本 fork 流水线 | `i18n/build-zh-cn.sh`、`i18n/zh_CN/*.ts` |
| 官方本地化文档 | https://www.paraview.org/paraview-docs/latest/cxx/LocalizationHowto.html |
