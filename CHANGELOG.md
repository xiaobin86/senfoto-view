# Changelog

记录向 `develop` 推送的每个批次的主要修改。批次格式：`## YYYY-MM-DD 批次：<主题>`。
由 AGENTS.md「提交后更新 Changelog」约定维护；本文首条为历史批次回填（2026-08-31 及以前）。

---

## 2026-10-09 批次：中文汉化 + 主工具栏/播放器行 UI 增强

- `09dbec9f` docs: 将 MEMORY.md 项目扩展到合并进 AGENTS.md
  - 把工作区根 `MEMORY.md` 的架构/扩展方式记忆并入 `AGENTS.md` 统一管理
- `307ca71c` feat(i18n): 添加 zh_CN 汉化骨架、lconvert 构建脚本与 .qm 投放流程
  - 从 ParaView 官方 en 模板生成 8 个 `zh_CN` 目录（含少量冒烟翻译）；`i18n/build-zh-cn.sh` 用 `lconvert` 合并为 `paraview_zh_CN.qm` 并投放，无需重编译
  - 关键点：ParaView 靠资源目录下的 `doc/` 反推 translations 目录，脚本补建空 `doc/`
- `d10c6b98` feat(i18n): 汉化自有菜单/工具栏/主窗口（LidarView.ts 136 条 + 菜单分类）
  - 用 `lupdate` 提取 `Application/` 自有字符串并翻译；主菜单栏/工具栏/面板标题/欢迎/关于框中文化
- `e74bd1e1` docs(i18n): 存档汉化架构与翻译修改指南（`docs/senfotoview-i18n-zh-cn.md`）
- `244fdf67` feat(ui): 主工具栏新增框选点按钮（Select Points Through）
  - 接入 `pqRenderViewSelectionReaction::SELECT_FRUSTUM_POINTS`，三个界面模式白名单均可见
- `856748a4` feat(ui): lidarViewer 模式显示相机复位/缩放与中心轴动作
  - `interface_modes_config.json` 的 lidarViewer `cameraToolbar`/`axesToolbar` 白名单补充相应动作
- `90416751` feat(ui): 播放器行新增 Trailing Frame(TF) 数值控件
  - `lqLivePlayerWidget` 增加 "TF" + QSpinBox；新增 `lqTrailingFrameReaction` 对活动雷达源应用 `TrailingFrame` 过滤器，0=关闭
- `ed7d88c0` feat(ui): 点云默认点大小改为 1
  - `lqPointCloudAutoRepresentationBehavior` 的 PLY Points 表示 `PointSize` 由 2.0 改为 1.0
- `42c06071` Merge branch 'feat/i18n-zh-cn' into feat/box-select-points-toolbar（合并后一并并入 develop）

---

## 2026-09-01 批次：流录制崩溃修复 + 块级拆帧 + pcap 导出加固

- `f440134` fix(stream): 流录制崩溃修复
  - 问题：点击"录制帧"选完路径确定后整个进程 crash（SIGABRT）
  - 根因 1：`lqStreamPCAPRecorder::startWriterProxy` 中 `toUtf8().constData()` 悬垂指针，
    SM 属性拷贝到垃圾路径 → `Tins::PacketWriter::init(乱码)` 抛异常
  - 根因 2：录制写线程无 try/catch → 未捕获异常 → `std::terminate` → abort
  - 方案：QByteArray 保活；RecordingLoop 整体 try/catch，失败走 `vtkGenericWarningMacro`
  - 附带：用户提供的崩溃报告（Thread 15 tins init → cxa_throw → terminate）直接定位
- `3444dc4` fix(senfoto008): 块级拆帧（对齐 Airy `SplitStrategyByAngle`）
  - 问题：帧边界滞后 1 包（1.6°），帧尾混入下一圈越界点
  - 方案：回绕检测细化到块粒度（0.2°），跨 0° 包的越界块丢弃（~0.06%/圈），
    帧索引与回放使用同一块级检测
- `e94bc26` fix(ui): pcap 导出加固
  - 同名自覆盖防护：导出默认名=源文件名时写出会截断源文件（曾导致 127MB 源 pcap 丢失）
  - CURRENT_FRAME 修复：`-1` 哨兵传入 unsigned 参数必然失败边界检查 → 解析当前显示帧索引
  - 对话框防呆：帧范围输入框仅 Frame Range 模式可用（此前 ALL_FRAMES 下填值被静默忽略，
    曾导致"填了 10 帧却全量导出"）
  - 增加导出诊断日志（mode/start/stop/目标路径）
  - 注：曾实现"直接调用客户端 VTK 对象"绕过 CS 流层静默吞调用，经验证回退保留原流路径；
    若 CURRENT_FRAME 再现静默失败可恢复该改动
- `e48cd4f` docs: changelog 政策（AGENTS.md）+ 历史批次回填

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
