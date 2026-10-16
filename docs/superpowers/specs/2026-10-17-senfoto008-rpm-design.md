# Senfoto008 实时 RPM 显示 — 设计文档

- 日期：2026-10-17
- 分支：`feat/senfoto008-rpm`（基于 `develop`）
- 依据：本仓库既有上游能力（`vtkRPMText` + `Rpm`/`Frequency` field data）；`02.Code` 中已废弃的 Python `applogic.py` 飘字方案（仅作对照，不移植）

## 1. 目标

为 Senfoto008 机械雷达在 SenFoToView 中提供**实时转速（RPM）显示**，且仅在 Senfoto008 源上生效（非 Senfoto008 源不显示）。

## 2. 研究结论（关键：不重复造轮子）

上游 LidarView 早在 2023 年就**移除**了旧的 Python applogic「展示RPM」飘字方案（commit `45debbed [refact] Remove RPM behavior from applogic`），改为纯 C++ 方案，且该方案**已在本 fork 中**：

- 解释器基类 `vtkLidarPacketInterpreter` 暴露两个宏（`LidarCore/IO/Lidar/vtkLidarPacketInterpreter.h:169-170`）：
  - `Frequency`（Hz；旋转雷达即每秒圈数）
  - `Rpm`（转/分）
- `SplitFrame()`（`.../vtkLidarPacketInterpreter.cxx:64-79`）在值**非零**时，把它们写成每帧的 field-data 数组 `"RPM"` / `"FPS"`（`vtkDoubleArray`，1 tuple）。
- 过滤器 `vtkRPMText`（`LidarCore/Filters/General/vtkRPMText.{h,cxx}`，proxy 名 `RPMText`、label `RPM Info`，`LidarCore/Plugin/Filters/RPMText.xml`）读取这两个数组，渲染成渲染视图内的 2D 文本：优先 `RPM: <v>`，否则 `FPS: <v>`，都为零则 `No RPM/FPS`（`.cxx:82-96`）。已注册于 `LidarCore/Plugin/CMakeLists.txt:68`。

**唯一缺口**：`vtkSenfoto008PacketInterpreter` 从不设置 `Rpm`/`Frequency`（恒为 0）→ `RPMText` 只会显示 `No RPM/FPS`；且 `RPMText` 未列入 `Application/Client/lvFilters.xml` 白名单。

**结论**：本功能 = 给 Senfoto008 解释器补上 `Rpm`/`Frequency` + 让 `RPMText` 可达（Filters 菜单 + 自动挂载 + Tools 开关）。**不新建显示层。**

## 3. 数据契约

| 字段 | 类型 | 单位 | 来源 |
|---|---|---|---|
| `vtkLidarPacketInterpreter::Frequency` | double | Hz（圈/秒） | 解释器写入 |
| `vtkLidarPacketInterpreter::Rpm` | double | 转/分 | 解释器写入 |
| field data `"RPM"` / `"FPS"` | vtkDoubleArray(1) | 同上报 | 基类 `SplitFrame()` 自动写（非 0 才写） |
| `vtkRPMText` 输出 | vtkTable(列 `Text`) | — | 读 `"RPM"`/`"FPS"` 生成文本 |

## 4. 架构与落点

### 4.1 内联 RPM 计算（不依赖第三方子模块）

> **约束（执行时确认）**：`Plugins/VelodynePlugin` 是第三方 git submodule（上游 `gitlab.kitware.com/LidarView/velodyneplugin.git`），fork 只 bump 其指针、**不修改其内容**。其 `RPMCalculator` 是私有头，且 `Senfoto008PacketInterpreters` 不依赖 `VelodynePlugin`。因此**不迁移、不复用**子模块里的 `RPMCalculator`，改为在 Senfoto008 解释器内**内联实现**等价的 RPM/FPS 计算（~30 行）。

计算逻辑（等价于子模块 `RpmCalculator` 的 min/max 版）：

- **RPM**：对一帧内累计的「方位角极值」与「时间极值」：
  `dAngle = (AzMax - AzMin) / 36000.0`（圈数；方位角为 0.01° 单位，`/100/360`）；
  `dTimeMin = (TMax - TMin) / 60e6`（分钟；时间为 µs）；
  `RPM = dAngle / dTimeMin`。
- **Frequency**：`dT = 本帧时间 − 上帧时间`；`FPS = 1/dT(s)`（子模块用 α=0.05 一阶平滑，此处可省略或保留）。
- 单位：方位角 0.01°（hundredth deg）、时间 µs（uint32）。

### 4.2 Senfoto008 解释器计算 `Rpm`/`Frequency`

改 `Plugins/Senfoto008Plugin/Senfoto008PacketInterpreters/vtkSenfoto008PacketInterpreter.{h,cxx}`（既有文件）：

- 在 `vtkInternals` 新增运行时状态（每帧复位）：
  ```cpp
  int AzMin = 0, AzMax = 0; bool HasAz = false;          // 0.01 度
  unsigned int TMin = 0, TMax = 0; bool HasT = false;    // 微秒
  unsigned int LastFrameTsUs = 0; bool HasLastFrameTs = false;
  ```
- `Initialize()` 里复位上述状态。
- 先把 `packetTimestamp` 取到（现代码在 `SplitFrame()` 之后才取，需上移），换算成 `uint32` 微秒：
  ```cpp
  const double packetTimestamp = senfoto008::GetPacketTimestamp(data); // 秒(含小数)
  const unsigned int tsUs = static_cast<unsigned int>(packetTimestamp * 1e6);
  ```
- 在**块方位角预扫描循环**中逐块累计方位角极值：
  ```cpp
  const int azHundredth = static_cast<int>(std::lround(blockAzimuths[b] * 100.0));
  if (!internals->HasAz) { internals->AzMin = internals->AzMax = azHundredth; internals->HasAz = true; }
  else { internals->AzMin = std::min(internals->AzMin, azHundredth);
         internals->AzMax = std::max(internals->AzMax, azHundredth); }
  ```
  同时累计时间极值（首次置 `HasT`）。
- 在回绕（`wrapIdx >= 0`）且**调用 `Superclass::SplitFrame()` 之前**计算并赋值：
  ```cpp
  const double dAngle  = (internals->AzMax - internals->AzMin) / 36000.0;   // 圈数
  const double dTimeMin = (internals->TMax - internals->TMin) / 60e6;       // 分钟
  this->Rpm = (dTimeMin > 1e-12 && dAngle > 0.0) ? (dAngle / dTimeMin) : 0.0;
  if (internals->HasLastFrameTs && tsUs != internals->LastFrameTsUs)
    this->Frequency = 1.0 / ((tsUs - internals->LastFrameTsUs) * 1e-6);
  internals->LastFrameTsUs = tsUs; internals->HasLastFrameTs = true;
  internals->HasAz = false; internals->HasT = false;   // 复位本帧累计
  Superclass::SplitFrame();   // 基类在此读取 Rpm/Frequency 写 field data
  ```
  ⚠️ **顺序关键**：必须在 `SplitFrame()` 之前赋值，否则写进 field data 的是旧值。
- 首帧：无方位角/时间极值 → `Rpm`/`Frequency` 为 0 → 不写数组 → `RPMText` 显示 `No RPM/FPS`（预期）。

### 4.3 暴露 + 自动挂载 + Tools 开关

- **Filters 菜单**：`Application/Client/lvFilters.xml` 增加分类（建议新分类 `Sensor`，或加入现有 `Common`）：
  ```xml
  <Proxy group="filters" name="RPMText" />
  ```
- **自动挂载**：`Qt/ApplicationComponents/lqOpenLidarReaction.cxx` 新增 helper `AutoAttachRPMText(pqPipelineSource*)`（仿 `AutoAttachRadialDenoise`，用同文件 `::InitAndDisplaySource(filter, filter->getProxy(), true)`）；在 `openLidarPcap()`（`:262` 附近）与 `openLidarStream()`（`:309` 附近）现有 `InitAndDisplaySource` 之后：
  ```cpp
  if (QString(prototype->GetXMLName()).startsWith("Senfoto008") && IsRPMTextAutoAttachEnabled())
    AutoAttachRPMText(source);
  ```
  开关：`QSettings().value("LidarView/AutoAttachRPMText", true).toBool()`。
- **Tools 菜单开关**（对应旧 `actionShowRPM`）：**净新增** `Qt/ApplicationComponents/lqRPMTextReaction.{h,cxx}`；在 `Application/Client/LidarViewMainWindow.cxx` 的 Tools 菜单构造处（现 `:211-214`，与 `lqLaserSelectionReaction` 并列）加一个 checkable action「显示 RPM」，行为：
  - 找活动 Senfoto008 源的 `RPMText` consumer（找不到则在用户首次开启时创建）；
  - 切换其可见性并 `render()`；勾选状态持久化到 `QSettings`。
- **界面模式登记**：`Application/Qt/ApplicationComponents/Resources/interface_modes_config.json` 三模式（`lidarViewer`/`pointCloudTool`/`advancedMode`）的 `menus` 中登记该 action。

## 5. 文件改动清单

**净新增（加 acelan 文件头，见 `AGENTS.md`）**
- `Qt/ApplicationComponents/lqRPMTextReaction.h` / `.cxx`

**修改（上游/既有文件，不加文件头）**
- `Plugins/Senfoto008Plugin/Senfoto008PacketInterpreters/vtkSenfoto008PacketInterpreter.h` / `.cxx`（内联 RPM/FPS 计算）
- `Plugins/Senfoto008Plugin/Testing/TestSenfoto008PacketInterpreter.cxx`（新增 RPM 用例）
- `Qt/ApplicationComponents/lqOpenLidarReaction.cxx`
- `Qt/ApplicationComponents/CMakeLists.txt`（登记新 Reaction）
- `Application/Client/lvFilters.xml`
- `Application/Client/LidarViewMainWindow.cxx`
- `Application/Qt/ApplicationComponents/Resources/interface_modes_config.json`

> 不触碰 `Plugins/VelodynePlugin`（第三方 git submodule）。

## 6. 测试

- **脚本测试**：仿 `Testing/Python/TestRPMText.py`，用 `lvsmp.OpenPCAP(<Senfoto008 pcap>, "Senfoto008", "Senfoto")` + `smp.RPMInfo(Input=reader)`，`scene.GoToNext()` 若干帧后断言 `Text` 列匹配 `RPM: <数字>`，且数值落在合理区间（如 500–700）。
- **冒烟**：开 Senfoto008 实时流，Tools →「显示 RPM」勾选后视图出现 RPM 文本；开非 Senfoto008 源不出现。
- **回归**：开 Velodyne 源，确认其自带 RPM 仍正常（本次未改子模块，属安全回归检查）。
- **构建**（superbuild 两步，见 `AGENTS.md`）：`ninja -C ../build/superbuild/lidarview/build` → `ninja -C ../build superbuild/lidarview`。

## 7. 风险 / 注意

- **子模块约束**：`RPMCalculator` 属第三方子模块 `Plugins/VelodynePlugin`，不可修改/复用，故采用内联实现（§4.1）；与子模块逻辑等价但独立维护。
- **时间戳**：Senfoto008 为包级时间戳（帧内所有点同 ts）。RPM 用包时间戳跨度计算（600 RPM ≈ 10 Hz），精度足够；但需确认一圈内 `GetPacketTimestamp` 单调递增——数据手册 §10.5 曾记录「实测帧 ts 步长为 0」，实现时须先验证；若包时间戳不可用，回退用「每帧缠绕 + 帧计数/系统时间」估算。
- **重复挂载**：自动挂载与 Tools 开关并存时，务必 find-or-create，避免同源挂出两个 `RPMText`。
- **仅 Senfoto008**：自动挂载与菜单动作均以 `GetXMLName().startsWith("Senfoto008")` 判定，不影响其它雷达。
