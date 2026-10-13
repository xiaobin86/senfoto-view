<!--
  功能：zh_CN 汉化质量复核报告（机器翻译问题清单与已修正项）。
  作者：acelan
  新建时间：2026-10-13
  修改时间：2026-10-13
-->

# zh_CN 汉化质量复核报告（2026-10-13）

## 1. 根因

| 来源 | 目录 | 质量 |
|---|---|---|
| SenFoToView 自有 UI（人工翻译） | `i18n/zh_CN/LidarView.ts` | 基本可用 |
| ParaView 官方 8 目录（**离线 Argos 机器翻译**） | `Qt_*.ts` / `Clients_*.ts` / `ServerManager-XMLs.ts` | **短串/菜单项/技术词大面积误译** |

典型：`E&xit`→“爱”、`&Connect...`→“欧内特...”、`F5`→“F5 导弹”、`Ctrl+Q`→“(简体中文)”、
`Clean`→“干净”、`Clip`→“剪辑”、`Crop`→“作物”、`Volume Of Revolution`→“革命数量”。

## 2. 本轮已修复（完成）

| 类别 | 数量 | 处理 |
|---|---|---|
| 快捷键/功能键被翻译（`Ctrl+/Alt/Shift/Meta+`、`F1-F12`） | 33 | 还原为源串 |
| 文件菜单 & 收藏夹（`E&xit`、`&Open...`、`&Connect...`、`Favorites` 等） | 23 | 改为正确中文 |
| 单字母/按键/轴标签被词表误命中（`x/X/P/N→页:1`、`C→C级`、`s→编号`、`-Z→说吧` 等） | 29 | 还原为源串 |
| **过滤器显示名（filters 组，插件提供）** | 409 | 还原为英文（保留英文） |
| **源/读卡器显示名（sources 组，插件提供）** | 317 | 还原为英文（保留英文） |

三类 proxy 名共还原 **633** 条译文为英文。插件名本身（`Senfoto008Plugin` 等，来自 `paraview.plugin`）
未进入 `.ts`，本就是英文。

复核结果：`.ts` 全部 XML 合法；快捷键类仅余 9 条（`Delete→删除`×6、`Left→左边` 为正确译文，
属误报；仅 `Home→页 次` 待确认）。

## 3. 待确认清单（尚未处理，机器翻译残留）

| 文件 | 内容 | 条数 |
|---|---|---|
| `qa/menu_review.csv` | 可见菜单/工具栏/动作标签 | 590 |
| `qa/suspect_review.csv` | 高置信度可疑项（词表误命中 + 过度压缩） | 656 |
| `qa/filter_names.txt` / `proxy_filters.csv` | 409 过滤器显示名（已还原英文） |
| `qa/source_names.txt` / `proxy_sources.csv` | 317 源/读卡器显示名（已还原英文） |

剩余问题集中在**属性面板标签**（ServerManager-XMLs 545 条），例如
`Volume Of Revolution`、`Spatio Temporal Harmonics`、`Multicorrelative Statistics`、
`Point Plane Interpolator`、`Brownian Points` 等。建议整体重译（LLM + 术语表）而非逐条手工。

## 4. 术语表（建议）

点云=Point Cloud；激光雷达=LiDAR；帧=Frame；滤波器=Filter；表示=Representation；
收藏夹=Favorites；加载=Load；渲染视图=Render View；管线浏览器=Pipeline Browser；抓包文件=Pcap File。

**专有名词（一律保留英文）**：`ParaView`、`LidarView`、`SenFoToView`、`Kitware`、`VTK`、`Qt`、`Python`。
本批已修正 `ParaView→帕拉维尤`、`ParaView→参数`、`VTK→维塔克`、`SenFoToView→SenFotoView 图像` 等；
`translate_zh.py` 已加入译前遮蔽/译后还原。

## 5. 机制

`.ts` 改动后执行 `i18n/build-zh-cn.sh` 重投 `.qm` 即可，**无需重新编译**（翻译为数据层，
运行时从 `build/install/share/paraview-6.1/translations/` 读取）。
