<!--
  功能：Senfoto008 实时 RPM 显示 —— 实现文档（数据流、算法、代码落点、测试、限制）。
  作者：acelan
  新建时间：2026-10-17
  修改时间：2026-10-17
-->

# Senfoto008 实时 RPM 显示 —— 实现文档

> 设计/计划：`docs/superpowers/specs/2026-10-17-senfoto008-rpm-design.md`、
> `docs/superpowers/plans/2026-10-17-senfoto008-rpm.md`
> 分支：`feat/senfoto008-rpm`（已合入 `develop`）

---

## 1. 目标

Senfoto008 机械雷达在 SenFoToView 中显示**实时转速（RPM）**，仅对 Senfoto008 源生效。

---

## 2. 背景与选型（为什么不自己写显示层）

上游 LidarView 早已用 C++ 方案取代了旧的 Python `applogic.py` 飘字方案（commit `45debbed`
"Remove RPM behavior from applogic"）。该方案**已在本仓库**，我们直接复用，**不新建显示层**：

| 组件 | 位置 | 作用 |
|---|---|---|
| `Rpm` / `Frequency` 两个宏 | `LidarCore/IO/Lidar/vtkLidarPacketInterpreter.h` | 解释器对外暴露转速 |
| `SplitFrame()` 自动写字段 | `.../vtkLidarPacketInterpreter.cxx` | 把 `GetRpm()/GetFrequency()` 写成每帧 field data `"RPM"`/`"FPS"`（非 0 才写） |
| `vtkRPMText` 过滤器 | `LidarCore/Filters/General/vtkRPMText.{h,cxx}`，proxy `RPMText`（label `RPM Info`） | 读 `"RPM"/"FPS"` → 生成 2D 文本表 |

**为什么不用 Velodyne 的 `RPMCalculator`？**
`Plugins/VelodynePlugin` 是**第三方 git submodule**（`gitlab.kitware.com/LidarView/velodyneplugin.git`），
fork 只 bump 其指针、不修改其内容；其 `RPMCalculator` 是私有头且 `Senfoto008PacketInterpreters`
不依赖 `VelodynePlugin`。故**内联实现**等价逻辑（用户拍板方案 B），不触碰子模块。

---

## 3. 数据流

```
Senfoto008 MSOP 包
  └─(UDP/PCAP)→ vtkLidarStream / vtkLidarReader
        └─ vtkSenfoto008PacketInterpreter::ProcessPacket()
              ├─ 累计本帧 方位角极值 / 时间极值；检测回绕
              └─ 回绕时：Rpm/Frequency 赋值 → Superclass::SplitFrame()
                    └─ 基类 SplitFrame(): 把 "RPM"/"FPS" 写进 CurrentFrame 的 FieldData（非 0 才写）
  Frame(vtkPolyData, 带 FieldData "RPM"/"FPS")
        └─ [Filters→Sensor] RPMText (vtkRPMText) 读 "RPM"/"FPS" → vtkTable(列 Text)
              └─ 渲染视图 2D 文本： "RPM: 600" / "FPS: 10" / "No RPM/FPS"
```

---

## 4. 实现细节

### 4.1 解释器内联计算（`vtkSenfoto008PacketInterpreter`）

**运行时状态**（`vtkInternals`，每帧复位）：

```cpp
int AzMinCentiDeg = 0, AzMaxCentiDeg = 0; bool HasAz = false;  // 方位角，单位 0.01°
unsigned int TMinUs = 0, TMaxUs = 0; bool HasT = false;        // 时间，单位 µs
unsigned int LastFrameTsUs = 0; bool HasLastFrameTs = false;   // 上一帧时间戳（算 FPS）
```

`Initialize()` 复位 `HasAz/HasT/HasLastFrameTs`。

**`ProcessPacket()` 关键片段**：

```cpp
// 1) 包时间戳先取（现代码原本在 SplitFrame 之后取，这里上移），转 uint32 µs
const double packetTimestamp = senfoto008::GetPacketTimestamp(data);
const unsigned int tsUs = static_cast<unsigned int>(packetTimestamp * 1e6);
if (!internals->HasT) { internals->TMinUs = internals->TMaxUs = tsUs; internals->HasT = true; }
else { internals->TMinUs = std::min(internals->TMinUs, tsUs);
       internals->TMaxUs = std::max(internals->TMaxUs, tsUs); }

// 2) 预扫描 8 个块：累计方位角极值 + 检测回绕（沿用块级拆帧逻辑）
double blockAzimuths[DATA_BLOCKS] = {}; int wrapIdx = -1;
for (b...) {
  blockAzimuths[b] = senfoto008::GetBlockAzimuth(data, b);
  const int azCentiDeg = static_cast<int>(std::lround(blockAzimuths[b] * 100.0));
  if (!internals->HasAz) { internals->AzMinCentiDeg = internals->AzMaxCentiDeg = azCentiDeg; internals->HasAz = true; }
  else { internals->AzMinCentiDeg = std::min(...); internals->AzMaxCentiDeg = std::max(...); }
  if (wrapIdx < 0 && this->CheckBlockWrap(blockAzimuths[b])) wrapIdx = b;
}

// 3) 回绕 → 算 RPM/FPS，然后拆帧（必须在 Superclass::SplitFrame() 之前赋值！）
if (wrapIdx >= 0 && this->CurrentFrame && this->CurrentFrame->GetNumberOfPoints() > 0) {
  const double revolutionSpan = (internals->AzMaxCentiDeg - internals->AzMinCentiDeg) / 36000.0; // 圈数
  const double elapsedMinutes = (internals->TMaxUs - internals->TMinUs) / 60e6;                    // 分钟
  this->Rpm = (elapsedMinutes > 1e-12 && revolutionSpan > 0.0) ? (revolutionSpan / elapsedMinutes) : 0.0;
  if (internals->HasLastFrameTs && tsUs != internals->LastFrameTsUs)
    this->Frequency = 1.0 / ((tsUs - internals->LastFrameTsUs) * 1e-6);   // Hz
  internals->LastFrameTsUs = tsUs; internals->HasLastFrameTs = true;
  internals->HasAz = false; internals->HasT = false;   // 复位本帧累计
  Superclass::SplitFrame();                            // 基类在此读 Rpm/Frequency 写 field data
}
```

**公式**：
- `RPM = (方位角跨度 / 360°) / (时间跨度 / 60s) = 圈数 / 分钟`（方位角 0.01° → ÷36000；µs → ÷60e6）。
- `FPS = 1 / (相邻帧时间戳差)`（Hz）。
- **顺序关键**：必须在基类 `SplitFrame()` **之前**给 `Rpm`/`Frequency` 赋值，否则写进 field data 的是旧值。
- 首帧：无跨度 → 值为 0 → 不写字段 → 界面显示 `No RPM/FPS`（预期）。

### 4.2 基类字段写入（上游）

`vtkLidarPacketInterpreter::SplitFrame()`：`fsValues[0]=GetRpm()`、`fsValues[1]=GetFrequency()`，
非 0 时以 `vtkDoubleArray`（1 tuple）加入 `CurrentFrame->GetFieldData()`，数组名 `"RPM"`/`"FPS"`。

### 4.3 显示过滤器（上游，复用）

`vtkRPMText : vtkTableAlgorithm` 读输入的 `"RPM"`（优先）/`"FPS"`，输出单列 `Text` 的 `vtkTable`，
由 `RPMText.xml` 的 `<Hints><OutputPort type="text"/></Hints>` 渲染成视图内 2D 文本。

### 4.4 暴露与入口

| 文件 | 改动 |
|---|---|
| `Application/Client/lvFilters.xml` | 新增分类 `Sensor` 并加 `<Proxy group="filters" name="RPMText" />`（Filters 菜单可见） |
| `Qt/ApplicationComponents/lqOpenLidarReaction.cxx` | `AutoAttachRPMText(source)`：打开 Senfoto008 源时自动 `createFilter("filters","RPMText")` 并显示；开关 `QSettings LidarView/AutoAttachRPMText`（默认 true）。**仅** `prototype->GetXMLName().startsWith("Senfoto008")` |
| `Qt/ApplicationComponents/lqRPMTextReaction.{h,cxx}`（净新增） | Tools 菜单「显示 RPM」：查/建活动 Senfoto008 源的 `RPMText`，切换其可见性；状态存 `QSettings LidarView/ShowRPM`（对应旧 `actionShowRPM`） |
| `Application/Client/LidarViewMainWindow.cxx` | Tools 菜单加 checkable action `tr("Show RPM")` |
| `i18n/zh_CN/LidarView.ts` | `Show RPM → 显示 RPM` |

> 注：`lidarViewer` 模式默认隐藏 Tools 菜单（已在同期改动中放开）。自动挂载使得**无需**菜单即可看到 RPM。

---

## 5. 文件清单（相对 `lidarview/`）

- 改：`Plugins/Senfoto008Plugin/Senfoto008PacketInterpreters/vtkSenfoto008PacketInterpreter.{h,cxx}`
- 改：`Plugins/Senfoto008Plugin/Testing/TestSenfoto008PacketInterpreter.cxx`（新增 RPM 用例）
- 改：`Application/Client/lvFilters.xml`
- 改：`Qt/ApplicationComponents/lqOpenLidarReaction.cxx`
- 增：`Qt/ApplicationComponents/lqRPMTextReaction.{h,cxx}`
- 改：`Qt/ApplicationComponents/CMakeLists.txt`（登记新类）
- 改：`Application/Client/LidarViewMainWindow.cxx`
- 改：`i18n/zh_CN/LidarView.ts`

---

## 6. 测试

- 单元（`TestSenfoto008PacketInterpreter.cxx`）：合成 3 圈数据（每圈 10 包、az 0→359.9°、10ms/包），
  断言 `GetRpm()` 落在合理区间、`GetFrequency()` ≈ 帧率。
  ```bash
  ctest --test-dir /mnt/d/work/senfoto-view/build/superbuild/lidarview/build \
    -R TestSenfoto008PacketInterpreter --output-on-failure
  ```
- 冒烟：开 Senfoto008 源 → 视图出现 `RPM: <值>`；Tools→显示 RPM 可切换；非 Senfoto008 源不出现。

---

## 7. 限制与风险

- **包时间戳**：Senfoto008 为包级时间戳（帧内所有点同 ts），RPM 用包时间戳跨度计算，精度足够
  （600 RPM ≈ 10Hz）。但需确认一圈内 `GetPacketTimestamp` 单调递增——数据手册 §10.5 曾记录
  「实测帧 ts 步长为 0」，若不可用需回退用「帧计数/系统时间」估算。
- **不触碰子模块**：`VelodynePlugin` 及其 `RPMCalculator` 原样不动。
- **仅 Senfoto008**：全部门控以 `GetXMLName().startsWith("Senfoto008")`。
- 合成数据的 RPM 绝对值受“首包归上一帧”等边界影响，故单测用**宽区间**断言。

---

## 8. 使用

1. 打开 Senfoto008 源（PCAP 或实时流）→ 视图自动出现 `RPM: <值>`。
2. Tools →「显示 RPM」切换显隐。
3. 关闭自动挂载：`~/.config/SenFoToView/SenFoToView.ini` 中 `LidarView/AutoAttachRPMText=false`。
