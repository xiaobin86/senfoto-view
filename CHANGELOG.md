# Changelog

记录向 `develop` 推送的每个批次的主要修改。批次格式：`## YYYY-MM-DD 批次：<主题>`。
由 AGENTS.md「提交后更新 Changelog」约定维护；本文首条为历史批次回填（2026-08-31 及以前）。

---

## 2026-10-22 批次：记录 Airy/lwIP HTTP 坑位

- `e0f43294` docs: 新增 `docs/airy-http-lwip-notes.md`；并在 `AGENTS.md` 增加"雷达 Web API HTTP 坑位（必守）"
  - 核心：lwIP 对 header 名**大小写敏感**；Qt 在 **Windows** 会把自定义头名规范化为小写 → `QNetworkAccessManager::post()` 被设备丢弃（Ubuntu 正常）
  - 约定：**带 body 的请求必须用 `QTcpSocket` 裸发**（首字母大写头、发后保持 ~1s 再优雅 FIN）；保存端点/字段集/无响应判定/NoProxy 等一并写入长期记忆

---

## 2026-10-22 批次：时间同步选项上移 + 验证前快速连通探测

- `0323f7c9` feat(airy): 时间同步选项放到通用页；验证前 2s 连通探测
  - 把 `SynSrc / PDomNum / RTPdelay / NoLeap / SyTOut / UToL / LToU` 从隐藏表单移到对话框**可见的「时间同步」分区**（这些字段本来就在通用页 POST 报文中）
  - 改 IP 后回读会打到新地址；新地址未起来时会卡在系统 TCP 连接超时（Windows ~20s）。现在验证前先做 **2s 裸 TCP 探测**，不通立即返回「已下发，请刷新确认」
  - 结果：改 IP 场景总耗时从 ~20s+ 降到约 4s

---

## 2026-10-22 批次：Airy 保存请求改用裸 TCP 发送

- `47853b90` fix(airy): 保存 POST 改用裸 TCP socket，头名按浏览器大小写发送
  - 依据：`/home/acelan/test11111.pcap` 抓到的 app 实际 POST —— Qt 在 Windows 上把自定义头名**全部小写**（`host`/`content-type`/`content-length`…），而设备 httpd 是 **lwIP**（响应 `HTTP/1.0`），疑似**大小写敏感** → 丢弃请求；Ubuntu 上 Qt 保留大小写所以能成功
  - 处理：`apply()` 不再用 `QNetworkAccessManager`，改为 `QTcpSocket` 直接写原始报文：`Host/Content-Type/Content-Length/User-Agent/Accept/Accept-Language/Accept-Encoding/Origin/Referer/Upgrade-Insecure-Requests/Cache-Control/Connection` 严格首字母大写、顺序与浏览器一致，body 不变（41 字段 / 433B）
  - 发送后保持 1s 再 `disconnectFromHost()`（优雅 FIN，避免 RST）
  - 新增日志：`raw POST bytes <n> head: ...`、`POST sent/closed`、`[AiryApply] ReMode=<值>`
  - GET 仍使用 QNAM（一直正常）

---

## 2026-10-22 批次：Airy 保存后保持连接

- `6707671b` fix(airy): POST 之后保持 500ms 再销毁连接
  - 现象：同一份代码 **Ubuntu 可改、Windows 不能**（body 已与浏览器抓包一致）
  - 推断：Windows 上立即销毁 `QNetworkAccessManager` 可能以 **RST** 关闭，lwIP 会丢弃尚未处理完的请求
  - 处理：POST 返回后 `msleep(500)`，让设备在连接仍打开时完成写入

---

## 2026-10-22 批次：Airy 请求过程日志细化

- `66b554a6` chore(airy): 细化 HTTP 步骤日志；去掉客户端超时
  - 每个请求打印：`GET/POST <url>` + 字段数 → `OK(<bytes>)` / `FAILED(<原因>)` / `no reply(<原因>)`
  - Qt 网络错误映射为明确原因（连接被拒 / 主机不可达 / 找不到主机 / 连接被关闭 / 代理错误）
  - 移除自建 3s abort 超时，交由系统/Qt 报告真实错误
  - 说明：设备对 `POST /Parameter_Setting.html` **从不回包**（浏览器抓包亦然），故 "Connection closed" 属正常，以 1s 后回读比对为准

---

## 2026-10-22 批次：Airy 应用后校验改为 1s 单次

- `755786fe` fix(airy): 应用后等 1s 只回读一次（不重试）；IP 输入维持 IPv4-only
  - 回读比对仍保留：不一致报「未生效：<字段>」
  - 撤回临时加入的 `host:port` 输入支持（改由 root 在 80 端口跑 mock 调试）

---

## 2026-10-22 批次：Airy 保存请求按浏览器抓包精确复刻

- `1eabbaf4` fix(airy): 复刻浏览器 Save 请求（请求头 + 表单体）
  - **依据**：`261021.pcap`（Windows）与 `200to202.pcap`（Ubuntu）中浏览器对 `POST /Parameter_Setting.html` 的请求 —— 两者**完全一致**，证明不存在平台差异；此前"Ubuntu 能改/Windows 不能"是**我们报文错误 + 假成功提示**造成的错觉
  - **请求体**：改为浏览器的 **41 字段固定顺序**，补上 `tabs-two=on`、`save_param=Save`、`mode2=Mode2`；去掉设备不接受的额外字段（`MVEn/DVEn/PVEn`、`RotaSpe`、`MotorR`、`MSideEn`、`msop_vlan` 等）
  - **请求头**：严格按抓包设置 `User-Agent / Accept / Accept-Language / Accept-Encoding / Origin / Referer / Upgrade-Insecure-Requests / Cache-Control / Connection`（`Host`、`Content-Length` 由 Qt 自动生成）
  - **结果校验**：应用后回读 `setting_data.json` 并逐项比对提交值，不一致直接报「未生效：<字段>」，不再有假成功
  - 备注：`/cgi-bin/param_setting.cgi` 仅出现在 HTML 的"saved from"注释中，实测 404，非真实端点

---

## 2026-10-20 批次：Airy 请求绕过系统代理 + 调试日志

- `a7f9639c` fix(airy): Airy 局域网请求绕过系统代理；HTTP 调试日志增强
  - 三处 `QNetworkAccessManager` 均设置 `QNetworkProxy::NoProxy` —— 修复 Windows 上系统代理/梯子把发往 `192.168.x` 的请求劫持（浏览器正常但程序超时的典型现象）
  - 日志增加：系统代理解析结果（type/host/port）、HTTP 状态码、`errorCode`/`errorString`、失败时响应片段
  - 日志增加：刷新目标 IP、`/device_data.json` 设备信息读取结果
  - 行为补充：改 IP 后回读验证失败视为"已下发"并提示（设备切换地址会断开连接）

---

## 2026-10-20 批次：SF-Q96A 雷达硬件参数对话框 + 菜单/布局调整

- `b5674f0c` feat(ui): Airy/SF-Q96A 硬件参数对话框；菜单与布局调整
  - 新增 `lqAiryHardwareConfig`：Airy Web API 客户端 —— `GET /setting_data.json`（全部参数）、`GET /device_data.json`（编号/MAC/固件/型号）、`POST /Parameter_Setting.html`（`save_param=Save`）；带 8s 超时
  - 新增 `lqAiryHardwareConfigDialog`：设备信息（编号/线束/MAC/主板·底板·APP·电机固件，禁用输入）；常用参数两列（主机IP|雷达IP、掩码|网关、MSOP|DIFOP）；回波模式 4 单选（Strongest/First/Last/Dual）；IP 旁刷新图标；⚙ 内联展开其余参数（下拉/数字框/文本框，字段名可读）；应用/取消/确定；IPv4 与端口校验
  - 提交语义：**用刷新时的旧 IP** 下发；改 IP 后设备断连导致超时属正常，**用新 IP 回读验证**判定成功；IP 保存为「上次成功使用」，仅成功后更新
  - 新增 `lqAiryHardwareConfigReaction`：编辑菜单「雷达硬件参数设置 / LiDAR Hardware Parameters」（SF-Q96A 启用；Senfoto008 仍走原对话框）
  - 菜单：显示 RPM / 雷达 IP 配置移入**编辑**菜单；**隐藏 Tools 与 Filters**；管线浏览器默认显示；Enable multi sensors 默认不勾选；LaserSelection 存在时隐藏原始 frame 源；RPM Info 不再成为活动源
  - i18n：新增中文字符串（`Qt_Components.ts`、`LidarView.ts`、`ServerManager-XMLs.ts`）
  - docs：新增 `docs/senfotoview-packaging-implementation.md`（三平台打包方案）
  - 同批包含工作区中并行的小改动：`LidarGridView.xml` 网格默认 scale 10→1、`i18n/qa_zh.py`/`translate_zh.py` 与两处 `.ts` 译文修正

---

## 2026-10-19 批次：帮助菜单精简 + 关于对话框文案

- `09a5b3ac` feat(ui): Help 菜单只保留 About；About 对话框去掉 kitware.com 链接并替换为未感科技中英文介绍
  - `lqHelpMenuBuilder.ui`：帮助菜单精简为仅 `About...`
  - `lqLidarViewMenuBuilders::buildHelpMenu`：移除 How To SLAM / Professional Support / Website / Release Notes / Community / ParaView Guide·Tutorial·Website / Bug Report 等项
  - `LidarViewMainWindow`：对菜单栏实际显示的 `menuHelp` 兜底，仅保留 About
  - `lqAboutDialog.{ui,cxx}`：删除 `www.kitware.com` 与 `lidarview.kitware.com` 链接；Tab 改为「关于未感科技 / About Senfoto」，内容替换为未感科技中英文两段介绍

---

## 2026-10-19 批次：SF-Q96 / SF-Q96A 传感器 + Senfoto008 RPM/时间戳修正

- `f4bb526f` feat(senfotoview): 新增 SF-Q96/SF-Q96A 传感器；修正 Senfoto008 RPM 与时间戳
  - Senfoto008 解释器显示名 `Senfoto008` → `SF-Q96`（`Senfoto008Proxies.xml` 的 `lidar_interpreters` Proxy name）
  - 新增 `Plugins/SFQ96APlugin`（由 RoboSense/RS96 整份复制；锁定 Airy、无型号下拉），注册为选择项 `SF-Q96A`
  - `lqLidarConfigurationDialog`：解释器选择列表白名单，仅显示 `SF-Q96`/`SF-Q96A`。因其它雷达插件为上游 submodule，按约定不改其 `*Proxies.xml`
  - Senfoto008 RPM：改为直读包头**偏移 4–5（大端）**；删除原"方位角/时间极值"推算
  - Senfoto008 时间戳：改为 **10 字节大端、200 MHz（5 ns/tick）**计数器解码；修复 FPS 异常（旧代码按 6B 秒 LE + 4B 微秒 LE 解码得 ~2.77e14 s，×1e6 溢出 32 位）
  - SF-Q96A RPM：从 **DIFOP（UDP 7788）偏移 373（2B 大端）**读取；自动挂载 `RPMText` 并把 Tools「显示 RPM」扩展到 SF-Q96A
  - `docs`: `senfoto008-protocol.md` 更新 RPM 偏移 4–5 + 新增「§12 包时间戳」

---

## 2026-10-17 批次：实现文档（多语言 / RPM / 雷达网络配置）

- `docs`: 新增 `docs/senfotoview-i18n-implementation.md`（从 Qt 翻译引擎 → catalog/`.qm` → lupdate/lconvert → ParaView 运行时装载 → 本 fork 流水线的底层实现 + 新增字符串端到端 checklist）
- `docs`: 新增 `docs/senfoto008-rpm-implementation.md`（实时 RPM：解释器内联计算 + 复用上游 `RPMText` + 自动挂载/Tools 开关）
- `docs`: 新增 `docs/senfoto008-network-config-implementation.md`（雷达 IP/端口：私有协议 `0xfc60/0xfc62` + 密码框 + 配置对话框 + Tools 入口）

---

## 2026-10-17 批次：Senfoto008 实时 RPM + 雷达 IP/端口配置

- `15e98594` Merge `feat/senfoto008-rpm`：Senfoto008 解释器**内联**计算 RPM/FPS（方位角/时间极值），基类 `SplitFrame()` 自动写 field data；复用上游 `RPMText` 过滤器显示，打开 Senfoto008 源时自动挂载；Tools「显示 RPM」开关
  - `b775a073` feat(senfoto008): 内联 RPM/FPS 计算 + 单元测试（约 600 RPM 用例）
  - `760f3f75` feat(ui): `RPMText` 加入 Filters→Sensor 菜单
  - `da745e31` feat(ui): 自动挂载 `RPMText` + `lqRPMTextReaction` Tools 开关
- `bce35d62` Merge `feat/senfoto008-network-config`：新增雷达网络配置（雷达IP / PC IP / MAC / 端口）
  - `9da71a53` feat(senfoto008): `lqSenfotoNetworkConfig` 协议封装（UDP `0xfc60` 写 / `0xfc62` 读，8091）+ 单测
  - `f2e39605` / `f69b3536` feat(senfoto008): 密码框 + 配置对话框
  - `77b87968` feat(ui): Tools「雷达 IP 配置」入口（仅 Senfoto008 源可用）
- `b8cfdcb1` feat(ui): lidarViewer 模式显示 Tools 菜单
- `4b9f240d` i18n(zh_CN): 新增菜单项中文翻译（Show RPM / Sensor IP Config）
- `4085c184` docs(agents): 新增功能必须同步补 zh_CN 翻译（长期约定）

---

## 2026-10-17 批次：忽略 agent 会话状态目录

- `c8967f0f` chore(git): `.gitignore` 忽略 `.omo/`（OhMyOpenCode 会话状态目录）

---

## 2026-10-16 批次：雷达点云默认 1 像素 + 去噪上游图层默认隐藏

- `909056c2` feat(ui): 雷达点云默认点大小改为 1（属性框默认值同步）
  - `lqPointCloudAutoRepresentationBehavior` 新增 `representationAdded` 槽 `onRepresentationAdded`，对每个新建表示判定其来源
  - 所有 `<Sensor>LidarReader` / `<Sensor>LidarStream` 源（含派生图层 `LaserSelection` / `RadialDistanceDenoise`）的表示 `PointSize` 设为 1；沿 `pqPipelineFilter` 上游回溯判定雷达来源
  - 效果：雷达点云默认 1 像素，Display 属性框 `Point Size` 默认显示 1；PLY 与非雷达数据不受影响
  - 备注：不改动上游 superbuild/ParaView（ParaView 表示层 `PointSize` 默认仍为 2.0），仅由 fork 内行为覆盖
- `758f5b61` feat(ui): 去噪后默认隐藏上游雷达/框选图层，仅显示去噪结果
  - `AutoAttachRadialDenoise` 追加 `RadialDistanceDenoise` 后，隐藏上游 reader/stream 与 `LaserSelection` 图层（新增辅助 `SetPipelineSourceVisible`）
  - 效果：默认只渲染去噪结果，避免上游点云叠加显示

---

## 2026-10-13 批次：汉化修正（专有名词/术语/错误机器翻译）+ 规范文档 + TrailingFrame 输入端口修复

- fix(i18n): 修正 zh_CN 大规模机器翻译错误（约 1700 条）
  - 专有名词一律保留英文：`ParaView`/`VTK`/`LidarView`/`SenFoToView`/`Kitware`/`Qt`/`Python`（曾音译为帕拉维尤/维塔克等）
  - 过滤器/读卡器/插件显示名保留英文：还原 409 个 filters + 317 个 sources（Clip/Clean/Contour 等）
  - 术语系统性修正：representation→表示、normal→法向、property→属性、scalar→标量、origin→原点、
    index→索引、clip→裁剪、bond→键、face→面、streamline→流线、assembly→装配、host→主机、mesh→网格 等
  - 快捷键/功能键/单字母函数名（Ctrl+*/F1-12、sin/cos/sqrt/mag…、环境变量）不翻译；修正
    `annulus→废墟/圆环`、`Skybox→天盘`、`Stride→弦乐`、`Number of Bins→宾数`、`End User→终止用户`、
    `Catalyst→催化`、`%1 TB→肺结核`、`messages→信件`、HTML/CSS 与 URL 占位残留 等
  - `i18n/translate_zh.py`：新增专有名词译前遮蔽/译后还原 + `TERM_FIX`；`i18n/qa_zh.py`：新增
    `PROPER_NOUN`/`SHORTCUT`/`SUSPECT` 守则检测
- docs(i18n): 新增 `docs/senfotoview-i18n-plan.md`（实现方案 + 翻译规范 + 避坑清单）；
  更新 `i18n/README.md`、`i18n/qa-report-zh.md`、`docs/senfotoview-i18n-zh-cn.md`；
  `AGENTS.md` 增「汉化（i18n）翻译规范（必守）」小节
- fix(ui): `lqTrailingFrameReaction` 按代理定义解析 `TrailingFrame` 的输入端口名（`LIDAR Frame`），
  修复直接用 `createFilter(group,name,source)` 时输入端口 0 未连接、`vtkTrailingFrame` 报错的问题
- feat(ui): `interface_modes_config.json` 的 `lidarViewer` 模式 Edit 菜单加入 `actionEditSettings`（Settings…）

---

## 2026-10-09 批次：全量中文汉化 + 隐藏模式切换按钮

- `f9d2cc0a` feat(i18n): 全量 zh_CN 机器翻译（约 1.1 万条）+ 翻译/校对脚本
  - 8 个 ParaView 目录 + LidarView 自有，共约 10,607 条去重译文；`paraview_zh_CN.qm` 由 150 条增至 11,395 条（约 1.7 MB）
  - 背景：免费在线 MT（Google/有道）被限流/封禁后，改用**离线 Argos Translate**（en→zh）批量翻译（无限流、约 31 条/秒）
  - 新增 `i18n/translate_zh.py`（多后端、占位符/HTML/加速键保护、术语表、磁盘缓存可续跑）；`i18n/qa_zh.py`（校对筛查）
  - 校对第一轮：修复 HTML 标签被翻译损坏（改为「只译文本节点、标签原样」）、术语（protractor→量角器、sin→正弦、camera→相机）等；问题由 40 降至 11（剩余为无害的符号/专有名词）
- `32064e75` feat(ui): 隐藏 Point Cloud/Advanced 模式切换按钮
  - `interface_modes_config.json` 的 `interfaceControlsToolbar` 白名单三个模式均只保留 `actionLidarViewerSetup`
  - 注意：此改动会禁用工具栏上的模式切换（无法再切到 Point Cloud/Advanced）

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
