# Changelog

记录向 `develop` 推送的每个批次的主要修改。批次格式：`## YYYY-MM-DD 批次：<主题>`。
由 AGENTS.md「提交后更新 Changelog」约定维护；本文首条为历史批次回填（2026-08-31 及以前）。

---

## 2026-09-01 批次：SF008 解码对齐 Airy + Airy 标定参考 + ccache

- `7f914a5` fix(senfoto008): 方位角解码对齐 Airy 参考实现
  - 盲区防护：相邻块方位角跳变 >1°（每圈 ~10° 接缝）钳位到标称间隔，避免污染逐发光插值
  - 包尾块对插值缺失修复：48-95 线 az 步长 0.19°/0.61° 交替（应 0.4°）→ 包尾用对间标称差（对齐 rs_driver `TwoInOneBlockIterator`）
  - 水平角修正框架：`AddPoint` 中 `az += horiz[laserId]`；表来自打墙自标定（test.csv/plan.csv，1-23、55-70 线已填，其余待补）
- `591c189` docs: Airy 标定参考（§10：包参数、DIFOP 标定表偏移与提取方法、96 通道 vert/horiz 实测表、Airy vs SF008 差异清单 §10.5）；用户手册新增 §7.1 选区导出 CSV
- `91b1d9d` docs(agents): ccache launcher 配置与 superbuild 重新 configure 后的恢复方法

## 2026-08-31 批次：界面布局 + PLY 自动显示 + 协议实测记录

- `f92da4f` feat(pointcloud): 无 cell 的 PLY 点云自动追加 Vertex Glyph filter 并切 Points 表示
- `ab25628` feat(ui): lidarViewer 模式工具栏加 OpenData/SaveData；pointCloudTool 加 displayPropertiesDock
- `67b6622` feat(ui): pipeline/properties/display/view 四个 dock 面板从右侧移到左侧（含 tabify 依赖修正）
- `9b74229` docs(agents): 界面布局持久化文件（~/.config/SenFoToView/SenFoToView.ini）与验证前清理规则
- `5b814e5` + `977bbf5` 去噪 L1 调试打印的加入与移除（净效果为移除）
- `8020971` docs(protocol): §9 拆帧机制与帧起始角实测（包计数器为大端、丢包 0.002%、传感器 ~10° az 跳变属固件问题）；§4 垂直角表非严格等差警告
- `f82c617` docs(denoise): Level-1 去噪 azimuth bin 定义、目的与使用注意

## 2026-08-29 批次：PointCoordinatesToScalars + 用户手册

- `44e86ee` feat(filter): 新增 PointCoordinatesToScalars（为无原生坐标数组的数据源生成 X/Y/Z 标量数组，支持按坐标染色）
- `f168dcc` + `3b6bd10` docs: SenFoToView 用户手册（实时流/PCAP/文件打开/染色/SpreadSheet/测量网格/标尺/网格源）

## 2026-08-28 批次：工程配置与文档

- `6116c74` style: 激光选择图标与工具栏顺序
- `2e943f8` / `26da2d5` / `7bec732` / `d9a1483` docs+build: 架构文档案例、可移植 VSCode/IntelliSense 配置、README 快速上手
- `3da07e9` chore: xml 格式化
- `af935f1` docs: AGENTS.md 新增净新增文件头注释规范，并为 Senfoto008/去噪/激光选择文件补充功能/作者/时间注释

## 2026-08-27 批次：径向距离去噪 filter + 激光选择面板

- `e6e9cd7` / `91fbbb8` docs: 去噪设计 spec 与实施计划
- `a987ecb` test: RadialDistanceDenoise 单元测试
- `9d44d5a` feat: vtkRadialDistanceDenoise 两级去噪 filter（二级：同帧同线插值尖峰检测；一级：跨帧 (laser_id, 方位角分箱) 缓存比对）
- `880b020` / `7c3bce5` 注册到 LidarCorePlugin + 自动挂载到 Senfoto008 源
- `360bd95` / `b8a95af` fix: 析构释放数组名 char*；空名防护/配对键/顺序前提加固
- `936c4d8` feat: base interpreter 增加逐线选择掩码 + 帧过滤
- `6b606f5` ~ `117dd30`（含 `6212134` 合并）: 激光选择对话框 UI 及系列修复（表格逻辑、MarkModified 参数、SM 属性推送触发重新拆帧、128 项默认值等）
- `5553420` feat: model-aware 激光选择 + Spreadsheet 工具栏按钮
- `caf1092` / `5cc39d2` docs: 激光选择面板设计文档、数据流与功能插入点架构文档
- `7030da0` feat: 激光选择工具栏图标
- `970225e` chore: ignore 本地测试 pcap

## 2026-08-26 批次：LidarViewer 菜单与 macOS 构建

- `f14d0e8` → `872ac95`（revert）→ `d06c66f` Edit 菜单面板开关的尝试与回退，最终以 View 菜单暴露面板可见性
- `74576ef` fix(macOS): 修复品牌化 SenFoToView.app 的 rpath 与插件加载
- `5212fb1` build: build.sh / init.sh 增加 macOS 支持

## 2026-08-25 批次：Senfoto008 范围过滤 UI

- `e50ce2b` feat: Senfoto008 距离/方位角范围过滤暴露到 reader 属性面板
