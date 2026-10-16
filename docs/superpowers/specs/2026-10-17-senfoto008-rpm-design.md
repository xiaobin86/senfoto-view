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

### 4.1 共享 `RPMCalculator`（提升为模块级）

把 `Plugins/VelodynePlugin/VelodynePacketInterpreters/RpmCalculator.{h,cxx}` 迁移到 `LidarView::IOLidar`（`LidarCore/IO/Lidar/`）：

- `LidarView::IOLidar` 已被 `VelodynePacketInterpreters` 与 `Senfoto008PacketInterpreters` 共同依赖（各自 `vtk.module` 的 `DEPENDS`），是天然共享点。
- 非 vtk 导出类用 `HEADERS` + `SOURCES` 登记（同 `LidarCore/IO/Network/CMakeLists.txt` 的 `InterpreterHelper.h` / `UDPPacketSender.h` 模式）。
- 导出宏 `VELODYNEPACKETINTERPRETERS_EXPORT` → `LVIOLIDAR_EXPORT`；包含头改为 `lvIOLidarModule.h`。
- Velodyne 侧：`VelodynePacketInterpreters/CMakeLists.txt` 从 `velodyne_sources` 移除 `RpmCalculator.cxx`（改由 IOLidar 提供）；两个 Velodyne 解释器的 `#include "RpmCalculator.h"` 不变（IOLidar 在依赖链上可解析）。
- **这是本方案唯一需改动上游 Velodyne 文件之处**（一个 CMakeLists）；按 `AGENTS.md` 不给被修改的上游文件加文件头注释。

接口保持不变（`RpmCalculator.h`）：

```cpp
using azimuth_t   = int;            // 百分之一度
using timestamp_t = unsigned int;   // 微秒
void   AddData(azimuth_t angle, timestamp_t rawtime);
double GetRPM();                    // 就绪前返回 0
void   ResetRPM();
double ComputeFrequency(timestamp_t frame_curr);
double GetFrequency();
```

### 4.2 Senfoto008 解释器计算 `Rpm`/`Frequency`

改 `Plugins/Senfoto008Plugin/Senfoto008PacketInterpreters/vtkSenfoto008PacketInterpreter.{h,cxx}`（既有文件）：

- 新增成员 `std::unique_ptr<RPMCalculator> RpmCalculator_;`（构造函数 `new`；`Initialize()` / `ResetInitializedState()` 中调用 `ResetRPM()`）。
- 先把 `packetTimestamp` 取到（现代码在 `SplitFrame()` 之后才取，需上移），换算成 `uint32` 微秒：
  ```cpp
  const double packetTimestamp = senfoto008::GetPacketTimestamp(data); // 秒(含小数)
  const timestamp_t tsUs = static_cast<timestamp_t>(packetTimestamp * 1e6); // uint32 微秒
  ```
- 在 `ProcessPacket` 的**块方位角预扫描循环**（现 `vtkSenfoto008PacketInterpreter.cxx:251-262`）中，逐块喂计算器：
  ```cpp
  RpmCalculator_->AddData(
    static_cast<int>(std::lround(blockAzimuths[b] * 100.0)), tsUs);
  ```
- 在检测到回绕（`wrapIdx >= 0`）且**调用 `Superclass::SplitFrame()` 之前**：
  ```cpp
  this->Rpm       = RpmCalculator_->GetRPM();
  this->Frequency = RpmCalculator_->ComputeFrequency(tsUs);
  RpmCalculator_->ResetRPM();
  Superclass::SplitFrame();   // 基类在此读取 Rpm/Frequency 写 field data
  ```
  ⚠️ **顺序关键**：必须在 `SplitFrame()` 之前赋值，否则写进 field data 的是旧值。
- 首帧：计算器未就绪 → `GetRPM()` / `ComputeFrequency()` 返回 0 → 不写数组 → `RPMText` 显示 `No RPM/FPS`（预期）。

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

**迁移（内容来自 Velodyne）**
- `LidarCore/IO/Lidar/RpmCalculator.h` / `.cxx`

**修改（上游/既有文件，不加文件头）**
- `LidarCore/IO/Lidar/CMakeLists.txt`（`SOURCES` 加 `RpmCalculator.cxx`，`HEADERS` 加 `RpmCalculator.h`）
- `Plugins/VelodynePlugin/VelodynePacketInterpreters/CMakeLists.txt`（`velodyne_sources` 移除 `RpmCalculator.cxx`）
- `Plugins/Senfoto008Plugin/Senfoto008PacketInterpreters/vtkSenfoto008PacketInterpreter.h` / `.cxx`
- `Qt/ApplicationComponents/lqOpenLidarReaction.cxx`
- `Qt/ApplicationComponents/CMakeLists.txt`（登记新 Reaction）
- `Application/Client/lvFilters.xml`
- `Application/Client/LidarViewMainWindow.cxx`
- `Application/Qt/ApplicationComponents/Resources/interface_modes_config.json`

## 6. 测试

- **脚本测试**：仿 `Testing/Python/TestRPMText.py`，用 `lvsmp.OpenPCAP(<Senfoto008 pcap>, "Senfoto008", "Senfoto")` + `smp.RPMInfo(Input=reader)`，`scene.GoToNext()` 若干帧后断言 `Text` 列匹配 `RPM: <数字>`，且数值落在合理区间（如 500–700）。
- **冒烟**：开 Senfoto008 实时流，Tools →「显示 RPM」勾选后视图出现 RPM 文本；开非 Senfoto008 源不出现。
- **回归**：开 Velodyne 源，确认仍能算出 RPM（验证 `RPMCalculator` 迁移未破坏 Velodyne）。
- **构建**（superbuild 两步，见 `AGENTS.md`）：`ninja -C ../build/superbuild/lidarview/build` → `ninja -C ../build superbuild/lidarview`。

## 7. 风险 / 注意

- **迁移副作用**：`RPMCalculator` 迁移需改上游 `VelodynePacketInterpreters/CMakeLists.txt`。若 VTK 模块系统对「非 vtk 导出类」在 IOLidar 中导出有障碍，降级为在 `Senfoto008Plugin` 内复制一份 `RpmCalculator`（~100 行），保持零跨插件耦合。
- **时间戳**：Senfoto008 为包级时间戳（帧内所有点同 ts）。RPM 用包时间戳跨度计算（600 RPM ≈ 10 Hz），精度足够；但需确认一圈内 `GetPacketTimestamp` 单调递增——数据手册 §10.5 曾记录「实测帧 ts 步长为 0」，实现时须先验证；若包时间戳不可用，回退用「每帧缠绕 + 帧计数/系统时间」估算。
- **重复挂载**：自动挂载与 Tools 开关并存时，务必 find-or-create，避免同源挂出两个 `RPMText`。
- **仅 Senfoto008**：自动挂载与菜单动作均以 `GetXMLName().startsWith("Senfoto008")` 判定，不影响其它雷达。
