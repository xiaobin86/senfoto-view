# SenFoTo008 俯仰角表暴露与 Laser Selection 对话框填充 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在雷达插件初始化/首次收到报文时把传感器俯仰角标定表暴露给 `vtkLidarPacketInterpreter`，并在 `lqLaserSelectionDialog` 打开时读取该表填充「Vertical Corr. (deg)」列。

**Architecture:** 在 `vtkLidarPacketInterpreter` 基类新增 `virtual vtkDoubleArray* GetVerticalCorrectionAngles()`，默认返回 `nullptr`。`vtkSenfoto008PacketInterpreter` 在 `Initialize()` 时默认填充 96 线角度表；首次 `ProcessPacket()` 若识别出 48 线型号，则替换为 48 线表。`lqLaserSelectionDialog::setLidarSource()` 从 interpreter 读取该数组并写入表格第 2 列。

**Tech Stack:** C++17, VTK (`vtkDoubleArray`), Qt (`QTableWidgetItem`), ParaView ServerManager, Senfoto008 packet format constants.

**Spec / 背景文档:**
- `docs/senfoto008-data-flow-and-features.md`（数据流与新功能插入点说明）
- `docs/superpowers/specs/2026-08-27-laser-selection-panel-design.md`（Laser Selection 面板设计 spec）

## Global Constraints

- 保持向后兼容：未实现该接口的 interpreter 返回 `nullptr`，对话框行为与现在一致（第 2 列保持 0.0）。
- 48 线 / 96 线判断：优先根据首次收到的 packet 头中的 `LIDAR_MODEL` 字段推断；未收到 packet 前默认按 96 线处理。
- 不修改现有激光通道选择过滤逻辑（`vtkLaserSelectionFilter`，其掩码由对话框经过滤器的 `LaserSelection` SM 属性推送），本计划仅补充 UI 显示数据（垂直角）。
- 激光通道选择已由独立的 `vtkLaserSelectionFilter` 实现（挂载在 source 下游、随 lidar 源自动附加）；本垂直角计划与之解耦，只读取并在对话框中展示垂直角，不改动过滤行为。
- 所有 C++ 修改需通过 `clang-format`（仓库根 `.clang-format`，Kitware 风格）。
- 新增/修改测试必须通过 `BUILD_TESTING=ON` 构建并运行。

---

## File Structure

| 文件 | 职责 |
|---|---|
| `LidarCore/IO/Lidar/vtkLidarPacketInterpreter.h` | 新增 `GetVerticalCorrectionAngles()` 虚接口与 `VerticalCorrectionAngles` 成员。 |
| `Plugins/Senfoto008Plugin/Senfoto008PacketInterpreters/vtkSenfoto008PacketInterpreter.h` | 声明覆盖（override）与型号跟踪成员。 |
| `Plugins/Senfoto008Plugin/Senfoto008PacketInterpreters/vtkSenfoto008PacketInterpreter.cxx` | 在 `Initialize()` / `ProcessPacket()` 中填充角度表。 |
| `Qt/Components/lqLaserSelectionDialog.cxx` | 在 `setLidarSource()` 中读取角度表并填充表格第 2 列。 |
| `Plugins/Senfoto008Plugin/Testing/TestSenfoto008PacketInterpreter.cxx` | 新增测试：默认 96 线、48 线 packet 处理后变为 48 线、数值正确。 |

---

## Task 1: 基类暴露俯仰角表接口

**Files:**
- Modify: `LidarCore/IO/Lidar/vtkLidarPacketInterpreter.h`

**Interfaces:**
- Produces: `virtual vtkDoubleArray* vtkLidarPacketInterpreter::GetVerticalCorrectionAngles()`
- Produces: `vtkNew<vtkDoubleArray> vtkLidarPacketInterpreter::VerticalCorrectionAngles`（protected 成员）

- [ ] **Step 1: 添加虚接口与成员**

在 `vtkLidarPacketInterpreter.h` 中，紧接 `GetNumberOfChannels()` 之后添加：

```cpp
  /**
   * Return the per-laser vertical correction angles (pitch) in degrees, if available.
   * The default implementation returns nullptr. Concrete interpreters may populate
   * VerticalCorrectionAngles during initialization or on first packet to expose
   * sensor-specific calibration values (e.g. Senfoto008 pitch angles).
   */
  virtual vtkDoubleArray* GetVerticalCorrectionAngles() { return nullptr; }
```

在 protected 成员区添加：

```cpp
  //! Per-laser vertical correction angles in degrees (optional, sensor-specific).
  vtkNew<vtkDoubleArray> VerticalCorrectionAngles;
```

- [ ] **Step 2: Commit**

```bash
git add LidarCore/IO/Lidar/vtkLidarPacketInterpreter.h
git commit -m "feat: add GetVerticalCorrectionAngles() hook to base interpreter"
```

---

## Task 2: Senfoto008 interpreter 填充角度表

**Files:**
- Modify: `Plugins/Senfoto008Plugin/Senfoto008PacketInterpreters/vtkSenfoto008PacketInterpreter.h`
- Modify: `Plugins/Senfoto008Plugin/Senfoto008PacketInterpreters/vtkSenfoto008PacketInterpreter.cxx`

**Interfaces:**
- Consumes: `senfoto008::GetVerticalAngles48Line()` / `GetVerticalAngles96Line()`
- Produces: `vtkDoubleArray* vtkSenfoto008PacketInterpreter::GetVerticalCorrectionAngles() override`
- Produces: helper `void PopulateVerticalCorrectionAngles(std::uint8_t lidarModel)`

- [ ] **Step 1: 在头文件添加 override 声明与型号成员**

在 `vtkSenfoto008PacketInterpreter.h` 的 `public:` 区（`Initialize()` 下方）添加：

```cpp
  /**
   * Expose the Senfoto008 vertical pitch angle table.
   * Populated during Initialize() (default 96-line) and updated on first packet
   * if the actual model is 48-line.
   */
  vtkDoubleArray* GetVerticalCorrectionAngles() override;
```

在 `private:` 区添加：

```cpp
  std::uint8_t LidarModel = senfoto008::LIDAR_MODEL_96_LINE;
  bool VerticalAnglesPopulated = false;
```

- [ ] **Step 2: 在 cxx 添加私有 helper 与实现**

在匿名命名空间或类内添加 helper（类内 private 亦可）：

```cpp
namespace
{
void FillVerticalAngles(vtkDoubleArray* arr, const std::array<double, 96>& angles)
{
  arr->SetName("vertical_correction");
  arr->SetNumberOfTuples(static_cast<vtkIdType>(angles.size()));
  for (std::size_t i = 0; i < angles.size(); ++i)
  {
    arr->SetValue(static_cast<vtkIdType>(i), angles[i]);
  }
}

void FillVerticalAngles(vtkDoubleArray* arr, const std::array<double, 48>& angles)
{
  arr->SetName("vertical_correction");
  arr->SetNumberOfTuples(static_cast<vtkIdType>(angles.size()));
  for (std::size_t i = 0; i < angles.size(); ++i)
  {
    arr->SetValue(static_cast<vtkIdType>(i), angles[i]);
  }
}
} // namespace
```

> 注：helper 放在 `vtkSenfoto008PacketInterpreter.cxx` 的匿名命名空间中，不暴露到头文件。

在 `vtkSenfoto008PacketInterpreter::Initialize()` 末尾（`Superclass::Initialize()` 之后）添加：

```cpp
  // Default to 96-line vertical correction table until the first packet tells us otherwise.
  this->LidarModel = senfoto008::LIDAR_MODEL_96_LINE;
  this->VerticalAnglesPopulated = false;
  FillVerticalAngles(this->VerticalCorrectionAngles.GetPointer(),
    senfoto008::GetVerticalAngles96Line());
  this->VerticalAnglesPopulated = true;
```

在 `vtkSenfoto008PacketInterpreter::ProcessPacket()` 中，解析出 `lidarModel` 后添加：

```cpp
  const std::uint8_t lidarModel = senfoto008::GetLidarModel(data);
  if (lidarModel != senfoto008::LIDAR_MODEL_48_LINE &&
    lidarModel != senfoto008::LIDAR_MODEL_96_LINE)
  {
    // existing unknown-model warning block...
    return;
  }

  // Update correction angle table when the model changes.
  if (lidarModel != this->LidarModel || !this->VerticalAnglesPopulated)
  {
    this->LidarModel = lidarModel;
    if (lidarModel == senfoto008::LIDAR_MODEL_48_LINE)
    {
      FillVerticalAngles(this->VerticalCorrectionAngles.GetPointer(),
        senfoto008::GetVerticalAngles48Line());
    }
    else
    {
      FillVerticalAngles(this->VerticalCorrectionAngles.GetPointer(),
        senfoto008::GetVerticalAngles96Line());
    }
    this->VerticalAnglesPopulated = true;
  }
```

> 注意：该逻辑应放在现有 `lidarModel` 校验之后、48/96 分支之前。

最后添加 override 实现：

```cpp
//-----------------------------------------------------------------------------
vtkDoubleArray* vtkSenfoto008PacketInterpreter::GetVerticalCorrectionAngles()
{
  return this->VerticalCorrectionAngles.GetPointer();
}
```

- [ ] **Step 3: Commit**

```bash
git add Plugins/Senfoto008Plugin/Senfoto008PacketInterpreters/vtkSenfoto008PacketInterpreter.h Plugins/Senfoto008Plugin/Senfoto008PacketInterpreters/vtkSenfoto008PacketInterpreter.cxx
git commit -m "feat(Senfoto008): populate vertical correction angle table on init/first packet"
```

---

## Task 3: Laser Selection 对话框读取角度表

**Files:**
- Modify: `Qt/Components/lqLaserSelectionDialog.cxx`

**Interfaces:**
- Consumes: `vtkLidarPacketInterpreter::GetVerticalCorrectionAngles()`
- Produces: 表格第 2 列（index 2）被填充为对应 `laser_id` 的俯仰角（度）。

- [ ] **Step 1: 在 `setLidarSource()` 中读取并填充**

在 `lqLaserSelectionDialog::setLidarSource()` 中，现有循环填充 checkbox 的代码块之后、函数结束之前添加：

```cpp
  // Populate the vertical-correction column from the interpreter's calibration table.
  if (vtkDoubleArray* angles = this->Interpreter->GetVerticalCorrectionAngles())
  {
    const vtkIdType availableTuples = angles->GetNumberOfTuples();
    for (int i = 0; i < numRows; ++i)
    {
      const int channel = this->Internal->Table->item(i, 1)->data(Qt::EditRole).toInt();
      double angle = 0.0;
      if (channel >= 0 && channel < availableTuples)
      {
        angle = angles->GetValue(channel);
      }
      this->Internal->Table->item(i, 2)->setData(Qt::EditRole, angle);
    }
  }
```

> 现有构造函数中已经把第 2 列初始化为 `0.0`，所以 `angles == nullptr` 或越界时无需额外处理。

- [ ] **Step 2: Commit**

```bash
git add Qt/Components/lqLaserSelectionDialog.cxx
git commit -m "feat(LaserSelectionDialog): populate vertical correction column from interpreter"
```

---

## Task 4: 测试 Senfoto008 角度表暴露

**Files:**
- Modify: `Plugins/Senfoto008Plugin/Testing/TestSenfoto008PacketInterpreter.cxx`

**Interfaces:**
- Consumes: `vtkSenfoto008PacketInterpreter::GetVerticalCorrectionAngles()`

- [ ] **Step 1: 在现有测试中添加断言**

在 `TestSenfoto008PacketInterpreter.cxx` 顶部已包含的 `<cmath>` 下追加：

```cpp
#include <limits>
```

在 `main()` 函数开头、处理任何 packet 之前，添加默认状态测试：

```cpp
  // Before any packet, the default table should be 96-line.
  vtkDoubleArray* defaultAngles = interpreter->GetVerticalCorrectionAngles();
  if (!defaultAngles || defaultAngles->GetNumberOfTuples() != 96)
  {
    std::cerr << "Expected default 96-line vertical correction table." << std::endl;
    return 1;
  }
  if (std::fabs(defaultAngles->GetValue(0) - 0.0) > 1e-9 ||
      std::fabs(defaultAngles->GetValue(1) - 0.95) > 1e-9 ||
      std::fabs(defaultAngles->GetValue(95) - 90.0) > 1e-9)
  {
    std::cerr << "Default 96-line vertical correction values mismatch." << std::endl;
    return 1;
  }
```

在现有测试末尾、函数返回 0 之前，添加 48 线 packet 处理后的测试：

```cpp
  // After processing a 48-line packet, the table should switch to 48 entries.
  vtkDoubleArray* angles = interpreter->GetVerticalCorrectionAngles();
  if (!angles || angles->GetNumberOfTuples() != 48)
  {
    std::cerr << "Expected 48-line vertical correction table after 48-line packet." << std::endl;
    return 1;
  }
  if (std::fabs(angles->GetValue(0) - 0.0) > 1e-9 ||
      std::fabs(angles->GetValue(1) - 0.95) > 1e-9 ||
      std::fabs(angles->GetValue(47) - 44.52) > 1e-9)
  {
    std::cerr << "48-line vertical correction values mismatch." << std::endl;
    return 1;
  }
```

- [ ] **Step 2: 构建并运行测试**

从 inner build 构建目标（路径/目标名可能因 CMake 命名而异，尝试以下之一）：

```bash
# 在 superbuild 根目录
cmake --build build --target TestSenfoto008PacketInterpreter -j

# 或在 inner build 目录
ninja -C build/superbuild/lidarview/build TestSenfoto008PacketInterpreter
```

运行：

```bash
ctest --test-dir build/superbuild/lidarview/build -R TestSenfoto008PacketInterpreter --output-on-failure
```

Expected: PASS。

- [ ] **Step 3: Commit**

```bash
git add Plugins/Senfoto008Plugin/Testing/TestSenfoto008PacketInterpreter.cxx
git commit -m "test(Senfoto008): verify vertical correction angle table"
```

---

## Task 5: 构建 SenFoToView 并验证 UI

**Files:**
- N/A（验证步骤）

- [ ] **Step 1: 构建客户端**

```bash
ninja -C build/superbuild/lidarview/build SenFoToView -j
```

- [ ] **Step 2: 手动验证**

1. 启动 SenFoToView。
2. 打开 `Senfoto-008-example.pcap` 或连接 Senfoto008 实时流。
3. 打开 `Laser Selection` 对话框（Tools 菜单或工具栏按钮）。
4. 观察第 2 列 `Vertical Corr. (deg)` 已按 laser_id 填充对应俯仰角。
   - 第 0 行 ≈ 0.0°
   - 第 1 行 ≈ 0.95°
   - 第 47 行 ≈ 44.52°
   - 48 线型号下第 48 行及以后应保持 0.0°

- [ ] **Step 3: 修复 clang-format 与编译警告**

```bash
clang-format -i LidarCore/IO/Lidar/vtkLidarPacketInterpreter.h \
  Plugins/Senfoto008Plugin/Senfoto008PacketInterpreters/vtkSenfoto008PacketInterpreter.h \
  Plugins/Senfoto008Plugin/Senfoto008PacketInterpreters/vtkSenfoto008PacketInterpreter.cxx \
  Qt/Components/lqLaserSelectionDialog.cxx \
  Plugins/Senfoto008Plugin/Testing/TestSenfoto008PacketInterpreter.cxx
```

重新构建确保无新增警告/错误。

- [ ] **Step 4: Commit 任何 format 修复**

```bash
git commit -am "style: apply clang-format to vertical-correction changes"
```

---

## Task 6: 更新架构文档

**Files:**
- Modify: `docs/senfoto008-data-flow-and-features.md`

- [ ] **Step 1: 在激光通道选择小节补充角度表说明**

在 `docs/senfoto008-data-flow-and-features.md` 的「4.1 激光通道选择」中，数据流生效点之后添加：

```markdown
#### 俯仰角表

`vtkSenfoto008PacketInterpreter` 在 `Initialize()` 时默认填充 96 线俯仰角表，并在首次 `ProcessPacket()` 识别到 48 线型号时切换为 48 线表。`lqLaserSelectionDialog::setLidarSource()` 通过 `vtkLidarPacketInterpreter::GetVerticalCorrectionAngles()` 读取该表，并填充到对话框第 2 列 `Vertical Corr. (deg)`。
```

- [ ] **Step 2: Commit**

```bash
git add docs/senfoto008-data-flow-and-features.md
git commit -m "docs: document vertical correction angle table exposure"
```

---

## Self-Review

### Spec coverage

- 基类接口暴露：Task 1 ✅
- Senfoto008 在初始化/首次 packet 时填充角度表：Task 2 ✅
- 48/96 线推断 + 默认 96 线：Task 2 ✅
- 对话框读取填充：Task 3 ✅
- 测试：Task 4 ✅
- 构建/UI 验证：Task 5 ✅
- 文档更新：Task 6 ✅

### Placeholder scan

- 无 `TBD` / `TODO` / `implement later`。
- 所有步骤包含实际代码/命令。
- 无模糊描述。

### Type consistency

- `GetVerticalCorrectionAngles()` 在基类与派生类中签名一致：`vtkDoubleArray* ()`。
- 辅助函数 `FillVerticalAngles` 重载分别接受 `std::array<double, 48>` 与 `std::array<double, 96>`。
- 对话框使用 `vtkIdType` 与 `int` 边界检查，避免越界。
