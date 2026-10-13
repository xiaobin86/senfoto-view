<!--
  功能：SenFoToView 中文本地化（汉化）实现方案、翻译规范与避坑清单（长期维护入口）。
  作者：acelan
  新建时间：2026-10-13
  修改时间：2026-10-13
-->

# SenFoToView 汉化实现方案 · 翻译规范 · 避坑清单

> 本文是汉化工作的**总入口**：实现方案 + 必守翻译规范 + 历史错误案例（避免重犯）。
> 架构原理详见 `senfotoview-i18n-zh-cn.md`；工作目录 `lidarview/i18n/`；项目约定见 `AGENTS.md`。

---

## 1. 目标

让 SenFoToView（基于 ParaView 6.1 的 LidarView fork）的界面显示中文，且：
- 复用 ParaView 官方 i18n 机制，**不改造应用代码**；
- 翻译改动**无需重新编译**（数据层 `.ts → .qm → 投放`）；
- **专有名词与标识符保持英文**（见 §4）。

## 2. 总体架构（一句话）

```
i18n/zh_CN/*.ts ──lconvert合并──▶ paraview_zh_CN.qm ──投放──▶ build/install/share/paraview-6.1/translations/
                                                        （运行时 QTranslator 自动加载，locale≠en 时）
```

- 语言来源优先级：环境变量 `PV_TRANSLATIONS_LOCALE` > 设置项 `GeneralSettings.InterfaceLanguage` > `en`。
- `ServerManagerXML` context 承载 XML 的 `label`/`menu_label`；C++/`.ui` 字符串用**类名**作 context。
- 资源目录反推依赖 `<install>/share/paraview-6.1/doc/`（脚本补建，见 §5）。

## 3. 实现步骤

1. **骨架**：从官方 `paraview-translations` 的 `en/*.ts` 复制 8 个目录到 `i18n/zh_CN/`，改
   `language="zh_CN"`；自有 UI 用 `lupdate` 从 `Application/` 提取为 `LidarView.ts`。
2. **翻译**：`i18n/translate_zh.py`（多后端 MT + 术语表 + 专有名词遮蔽，见 §4）；人工校订关键项。
3. **构建投放**：`./i18n/build-zh-cn.sh`（合并 9 个 `.ts` → `paraview_zh_CN.qm`，收集 Qt 的
   `qt_zh_CN.qm`/`qtbase_zh_CN.qm`，投放并补建 `doc/`）。
4. **校验**：`python3 i18n/qa_zh.py 'i18n/zh_CN/*.ts'`（§7）。
5. **验证**：`PV_TRANSLATIONS_LOCALE=zh_CN build/install/bin/SenFoToView`。

## 4. 翻译规范（必守）

| # | 规则 | 说明 |
|---|---|---|
| R1 | **专有名词保留英文** | `ParaView`、`LidarView`、`SenFoToView`、`Kitware`、`VTK`、`Qt`、`Python` 一律原样，不翻译/不音译 |
| R2 | **proxy/插件显示名保留英文** | 所有 `<SourceProxy>` 的 `label`（`filters`/`sources` 组）与插件名（`Senfoto008Plugin` 等）保持英文 |
| R3 | **快捷键/功能键不翻译** | `Ctrl+/Alt+/Shift+/Meta+*`、`F1-F12` 的译文必须**等于源串** |
| R4 | **占位符/HTML/加速键保护** | 保留 `%1 %2 %L1 {…}` 的数量与顺序；HTML 只译文本节点、标签原样；`&X` 加速键需保留 |
| R5 | **context 必须匹配** | XML 标签 → `ServerManagerXML`；C++/`.ui` → 类名（同名串不同 context 不命中） |
| R6 | **术语统一** | 见 §6 术语表 |
| R7 | **单字母/轴/按键标签保持原样** | 源为 `x/X/Y/Z/P/N`、`-Z` 等按键/轴标签时，译文=源串（勿经词表翻译） |
| R8 | **判断型保留项** | `Delete→删除`、`Left→左边` 等**正确**译文不要误当快捷键回退（`qa_zh.py` 的 SHORTCUT 需人工确认） |

**机器翻译管线中的落实**：`translate_zh.py` 的 `PROPER_NOUNS`（R1）在译前用私有区占位符遮蔽、
译后还原；`TERM_FIX` 修正已知误译；`GLOSSARY` 提供术语命中。

## 5. 投放机制与 `doc` 陷阱

- `.qm` 直接投放，**无需重编译**；重启应用生效（不支持热加载）。
- 默认搜索路径 `<install>/share/paraview-6.1/translations` 依赖 `vtkResourceFileLocator` 反推
  共享资源目录（以同级 `doc/` 为锚）。本项目 install 树默认无 `doc/`，`build-zh-cn.sh` 会补建；
  否则设 `PV_TRANSLATIONS_DIR` 绕过。

## 6. 术语表

点云=Point Cloud；激光雷达=LiDAR；帧=Frame；滤波器=Filter；表示=Representation；
收藏夹=Favorites；加载=Load；渲染视图=Render View；管线浏览器=Pipeline Browser；抓包文件=Pcap File。

## 7. 校验工具

| 工具 | 作用 |
|---|---|
| `i18n/qa_zh.py` | 自动筛查：`SAME_AS_SOURCE` / `PLACEHOLDER` / `HTML_TAG` / `ACCEL` / `NO_CJK` / `PROPER_NOUN`（R1）/ `SHORTCUT`（R3）/ `SUSPECT`（词表误命中） |
| `i18n/qa-report-zh.md` | 阶段性复核报告与待确认清单 |
| `i18n/qa/*.csv` | 过滤器/读卡器名、可疑译文等清单（`proxy_filters`/`proxy_sources`/`menu_review`/`suspect_review`） |

> 注：`SAME_AS_SOURCE` 对 R1/R2/R3/R7 的刻意保留项属**正常**，其余需人工判定。

## 8. 避坑清单（已发生，务必避免重犯）

| 现象（错误） | 根因 | 规避 |
|---|---|---|
| `E&xit`→“爱”、`&Connect...`→“欧内特”、`&Load State...`→“胶州” | 离线 Argos MT 对短串/生僻词失效 | 短标签人工校订；扩充 `GLOSSARY`/`TERM_FIX` |
| `Ctrl+Q`→“(简体中文)”、`Ctrl+P`→“缩写+P”、`F5`→“F5 导弹” | MT 把快捷键当词翻译 | R3；`qa_zh.py` SHORTCUT 检测 |
| `x/X/P`→“页:1”、`C`→“C级”、`s`→“编号”、`-Z`→“-Z -Z -说吧” | MT 把单字母当单位/词 | R7；SUSPECT `页:1`/`缩写` |
| `ParaView`→“帕拉维尤”“参数”、`VTK`→“维塔克”、`SenFoToView`→“SenFotoView 图像” | MT 音译/改写专有名词 | R1；`_mask_proper`；`qa_zh.py` PROPER_NOUN |
| `Clean`→“干净”、`Clip`→“剪辑”、`Crop`→“作物”、`Volume Of Revolution`→“革命数量” | 过滤器名被翻译 | R2；已还原 409 过滤器 + 317 源为英文 |
| `Intersect With`→“与”、`Volume Representation`→“人数”、`Bounding Planes`→“撞机” | MT 过度压缩长标签 | 属性标签整体重译（LLM+术语），勿逐条小修 |
| 主菜单 `File` 翻不了 | context 不匹配（自有类名 vs `MainWindow`） | R5 |
| 设了 `.qm` 仍英文 | 缺同级 `doc/` 或未设 locale | §5；`PV_TRANSLATIONS_LOCALE=zh_CN` |
| 语言设置里没有 Settings | `actionEditSettings` 未加入当前界面模式白名单 | 见 `interface_modes_config.json`（lidarViewer 已补） |

## 9. 日常操作流程

**改/补翻译**：编辑 `i18n/zh_CN/<catalog>.ts` → `./i18n/build-zh-cn.sh` → 重启应用（无需重编译）。
**新增自有源串**：`lupdate`（见架构文档 §4.2）→ 翻译 → 投放。
**整体重译 ParaView 目录**：`translate_zh.py`（受 R1 遮蔽保护）→ `qa_zh.py` 复核 → 投放。

### 9.1 人工修改某条错误翻译（步骤）

**第 1 步 · 定位目录文件**（按你看到该串的位置）：

| 看到的位置 | 改哪个文件 |
|---|---|
| 主菜单栏/工具栏/面板标题/欢迎·关于（SenFoToView 自有） | `i18n/zh_CN/LidarView.ts` |
| 过滤器名/属性名/设置项/ParaView 代理与菜单 XML 标签 | `i18n/zh_CN/ServerManager-XMLs.ts`、`Clients_ParaView-XMLs.ts`（context = `ServerManagerXML`） |
| ParaView 对话框/控件/按钮（组件） | `Qt_Components.ts`、`Qt_ApplicationComponents.ts` |
| 核心/基础控件 | `Qt_Core.ts`、`Qt_Widgets.ts`；`Qt_Python.ts`（Python 相关）；`Clients_ParaView.ts` |

**第 2 步 · 找到那一条**（用英文源串检索；同名串可能出现在多个文件/context，改对 context 的那条）：
```bash
grep -n -A3 '<source>Save &amp;Animation...</source>' i18n/zh_CN/*.ts
```

**第 3 步 · 改 `<translation>`**，并把 `<translation>` 上的 `type="unfinished"` 去掉：
```xml
<message>
    <location filename=".../pqFileMenuBuilder.ui" line="58"/>
    <source>Save &amp;Animation...</source>
    <translation>保存动画...(&amp;A)</translation>   <!-- 改这里 -->
</message>
```
> XML 转义：`&` 写 `&amp;`、`<` 写 `&lt;`、`>` 写 `&gt;`；`&amp;X` 是加速键，须保留。
> 遵守 §4 规范：专有名词/快捷键/代理名按规则处理。

**第 4 步 · 重新生成并投放**（无需重编译）：
```bash
cd lidarview && ./i18n/build-zh-cn.sh
```

**第 5 步 · 生效**：**完全退出并重启** SenFoToView（不支持热加载）。

**第 6 步 · 验证**（可选，无需开 GUI）：
```bash
/usr/lib/qt6/bin/lconvert -i build/install/share/paraview-6.1/translations/paraview_zh_CN.qm -o /tmp/c.ts
grep -A1 '<source>Save &amp;Animation...</source>' /tmp/c.ts
# 或跑守则检测：python3 i18n/qa_zh.py 'i18n/zh_CN/*.ts'
```

> 注意：仅当翻译**完全缺失**（源头新增了串）时才需要 `lupdate` 重新提取（§ 架构文档 4.2）。

## 10. 参考

- 架构与操作：`docs/senfotoview-i18n-zh-cn.md`
- 工作目录说明：`i18n/README.md`
- 复核报告：`i18n/qa-report-zh.md`
- 项目约定：`AGENTS.md`
- 官方：https://www.paraview.org/paraview-docs/latest/cxx/LocalizationHowto.html
