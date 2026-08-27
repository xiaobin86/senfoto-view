# Senfoto008 点云两级「径向距差」去噪 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 Senfoto008 点云增加一个两级「径向距差」去噪 ParaView filter（一级跨帧、二级同帧），并默认自动挂载到 Senfoto008 流上，输出直接显示在主视图。

**Architecture:** 新增 `vtkRadialDistanceDenoise : vtkPolyDataAlgorithm`（位于 `LidarCore/Filters/Processing/`，注册进 `LidarView::FiltersProcessing` 插件）。算法只依赖逐点数组 `distance_m`（径向斜距）、`laser_id`、`azimuth`，不动几何坐标；噪点以 Drop 方式从输出 polyData 中剔除。一级的跨帧状态用 filter 内部 `PrevRange` 缓存（key = laser_id + 方位角分箱）持有，无需改动解释器/流。

**Tech Stack:** C++ / VTK (`vtkPolyDataAlgorithm`, `vtkRemovePolyData`, `vtkCleanPolyData`)、ParaView ServerManager XML、CMake (`vtk_module_add_module`, `lidarcoreplugin_add_module_xml`, `vtk_add_test_cxx`)、Qt (`pqObjectBuilder`, `QSettings`)。

**Spec:** `docs/superpowers/specs/2026-08-27-senfoto008-radial-denoise-design.md`

## Global Constraints

- 实现为 ParaView filter `vtkRadialDistanceDenoise`，落点 `LidarCore/Filters/Processing/`，XML 注册进 `LidarView::FiltersProcessing`。
- 一级：按 `(laser_id, 方位角分箱)` keyed 跨帧缓存，阈值默认 `10.24`。
- 二级：同 `laser_id` 内按 `azimuth` 排序后左-中-右插值判定，阈值默认 `10.0`。
- 噪点处理统一 **Drop**（从输出 polyData 中移除该点）。
- 默认**自动挂载**到 Senfoto008 流（`lqOpenLidarReaction`），带可关闭开关（默认开）。
- 输入数组固定为 `distance_m` + `laser_id` + `azimuth`（均由解释器恒久创建）。

---

## File Structure

| 文件 | 责任 |
|---|---|
| `LidarCore/Filters/Processing/vtkRadialDistanceDenoise.h` | filter 类声明 + 8 个属性读写宏 + `PrevRange` 跨帧缓存成员 |
| `LidarCore/Filters/Processing/vtkRadialDistanceDenoise.cxx` | `RequestData`：二级（分组+排序+插值）+ 一级（keyed 缓存）+ Drop 输出 |
| `LidarCore/Plugin/Filters/RadialDistanceDenoise.xml` | ServerManager 代理：3 个数组选择属性 + 阈值/开关/分箱属性 |
| `LidarCore/Filters/Processing/CMakeLists.txt` | 把类加入 `classes`（无 nanoflann 依赖，放基础列表） |
| `LidarCore/Plugin/CMakeLists.txt` | 把 XML 加入 `filters_processing_xml_files` |
| `Qt/ApplicationComponents/lqOpenLidarReaction.cxx` | Senfoto008 源创建后自动加 filter（含 `QSettings` 开关） |
| `LidarCore/Filters/Processing/Testing/Cxx/vtkRadialDistanceDenoiseTest.cxx` | 单元/集成测试（二级尖峰、斜坡保留、一级跨帧） |

---

### Task 1: 编写失败测试（TDD 红）

**Files:**
- Create: `LidarCore/Filters/Processing/Testing/Cxx/vtkRadialDistanceDenoiseTest.cxx`
- Modify: `LidarCore/Filters/Processing/Testing/Cxx/CMakeLists.txt`

**Interfaces:** 无（首次创建）；消费 `vtkRadialDistanceDenoise`（Task 2 实现）。

- [ ] **Step 1: 写测试文件**

```cpp
#include "vtkRadialDistanceDenoise.h"
#include <vtkNew.h>
#include <vtkPolyData.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkDoubleArray.h>
#include <vtkUnsignedCharArray.h>
#include <vtkPointData.h>
#include <iostream>

namespace
{
  vtkPolyData* BuildFrame(vtkDoubleArray* distance, vtkUnsignedCharArray* laser, vtkDoubleArray* azimuth)
  {
    vtkNew<vtkPolyData> pd;
    vtkNew<vtkPoints> pts;
    vtkIdType n = distance->GetNumberOfTuples();
    pts->SetNumberOfPoints(n);
    for (vtkIdType i = 0; i < n; ++i)
      pts->SetPoint(i, distance->GetValue(i), 0.0, 0.0); // 几何不参与去噪，占位即可
    pd->SetPoints(pts);
    pd->GetPointData()->AddArray(distance);
    pd->GetPointData()->AddArray(laser);
    pd->GetPointData()->AddArray(azimuth);
    vtkNew<vtkCellArray> verts;
    for (vtkIdType i = 0; i < n; ++i)
      verts->InsertNextCell(1, &i);
    pd->SetVerts(verts);
    return pd;
  }
}

int TestRadialDistanceDenoise(int, char*[])
{
  // ---- 二级：单激光线上的尖峰应被剔除 ----
  vtkNew<vtkDoubleArray> dist; dist->SetName("distance_m");
  vtkNew<vtkUnsignedCharArray> laser; laser->SetName("laser_id");
  vtkNew<vtkDoubleArray> azim; azim->SetName("azimuth");
  double d[] = { 10, 10, 50, 10, 10 };
  double a[] = { 0, 1, 2, 3, 4 };
  unsigned char l[] = { 0, 0, 0, 0, 0 };
  for (int i = 0; i < 5; ++i) { dist->InsertNextValue(d[i]); laser->InsertNextValue(l[i]); azim->InsertNextValue(a[i]); }
  vtkNew<vtkPolyData> pd; pd->ShallowCopy(BuildFrame(dist, laser, azim));
  vtkNew<vtkRadialDistanceDenoise> f;
  f->SetInputData(pd);
  f->SetLevel1Enabled(false);
  f->SetLevel2Threshold(10.0);
  f->Update();
  if (f->GetOutput()->GetNumberOfPoints() != 2) { std::cerr << "L2 spike FAIL: " << f->GetOutput()->GetNumberOfPoints() << "\n"; return 1; }

  // ---- 二级：线性斜坡中点应保留 ----
  vtkNew<vtkDoubleArray> dist2; dist2->SetName("distance_m");
  vtkNew<vtkUnsignedCharArray> laser2; laser2->SetName("laser_id");
  vtkNew<vtkDoubleArray> azim2; azim2->SetName("azimuth");
  double d2[] = { 0, 10, 20 }; double a2[] = { 0, 1, 2 }; unsigned char l2[] = { 0, 0, 0 };
  for (int i = 0; i < 3; ++i) { dist2->InsertNextValue(d2[i]); laser2->InsertNextValue(l2[i]); azim2->InsertNextValue(a2[i]); }
  vtkNew<vtkPolyData> pd2; pd2->ShallowCopy(BuildFrame(dist2, laser2, azim2));
  vtkNew<vtkRadialDistanceDenoise> f2;
  f2->SetInputData(pd2); f2->SetLevel1Enabled(false); f2->Update();
  if (f2->GetOutput()->GetNumberOfPoints() != 3) { std::cerr << "L2 ramp FAIL\n"; return 1; }

  // ---- 一级：跨帧同 (laser_id,bin) 距差超阈应被剔除 ----
  vtkNew<vtkDoubleArray> dA; dA->SetName("distance_m");
  vtkNew<vtkUnsignedCharArray> lA; lA->SetName("laser_id");
  vtkNew<vtkDoubleArray> aA; aA->SetName("azimuth");
  dA->InsertNextValue(100.0); lA->InsertNextValue(0); aA->InsertNextValue(10.0);
  vtkNew<vtkPolyData> fr1; fr1->ShallowCopy(BuildFrame(dA, lA, aA));
  vtkNew<vtkRadialDistanceDenoise> f1;
  f1->SetInputData(fr1); f1->SetLevel2Enabled(false); f1->SetLevel1Threshold(10.24);
  f1->Update(); // 第一帧：无上一帧，保留 1 点，并写入 PrevRange
  vtkNew<vtkDoubleArray> dB; dB->SetName("distance_m");
  vtkNew<vtkUnsignedCharArray> lB; lB->SetName("laser_id");
  vtkNew<vtkDoubleArray> aB; aB->SetName("azimuth");
  dB->InsertNextValue(200.0); lB->InsertNextValue(0); aB->InsertNextValue(10.0);
  vtkNew<vtkPolyData> fr2; fr2->ShallowCopy(BuildFrame(dB, lB, aB));
  f1->SetInputData(fr2); f1->Update(); // 第二帧：与上一帧距差 100 > 10.24 -> 剔除
  if (f1->GetOutput()->GetNumberOfPoints() != 0) { std::cerr << "L1 FAIL: " << f1->GetOutput()->GetNumberOfPoints() << "\n"; return 1; }

  std::cout << "PASS\n";
  return 0;
}
```

- [ ] **Step 2: 在测试 CMake 中注册**

把测试源文件追加到 `LidarCore/Filters/Processing/Testing/Cxx/CMakeLists.txt` 里既有的 `vtk_add_test_cxx(lvFiltersProcessingCxxTests tests` 调用中：

```cmake
  vtkRadialDistanceDenoiseTest.cxx
```

- [ ] **Step 3: 编译测试，确认因类未实现而失败**

```bash
cmake --build /Users/acelan/workspace/senfoto-view/build --target lvFiltersProcessingCxxTests
```

预期：编译错误 `vtkRadialDistanceDenoise.h: No such file or directory`（红）。

- [ ] **Step 4: 提交**

```bash
git add LidarCore/Filters/Processing/Testing/Cxx/vtkRadialDistanceDenoiseTest.cxx LidarCore/Filters/Processing/Testing/Cxx/CMakeLists.txt
git commit -m "test: add failing test for RadialDistanceDenoise filter"
```

---

### Task 2: 实现 filter（vtkRadialDistanceDenoise）

**Files:**
- Create: `LidarCore/Filters/Processing/vtkRadialDistanceDenoise.h`
- Create: `LidarCore/Filters/Processing/vtkRadialDistanceDenoise.cxx`

**Interfaces:**
- 消费者（Task 1 测试与 Task 3 XML）调用：
  - `SetLevel1Enabled(bool)` / `SetLevel1Threshold(double)` / `SetLevel2Enabled(bool)` / `SetLevel2Threshold(double)`
  - `SetNumberOfLasers(int)` / `SetAzimuthBinSize(double)`
  - `SetDistanceArrayName(const char*)` / `SetLaserIdArrayName(const char*)` / `SetAzimuthArrayName(const char*)`
  - `SetInputData(vtkPolyData*)`（VTK 基类）+ `Update()` / `GetOutput()`
- 输出：`vtkPolyData`，已剔除噪点（Points + 全部点数组 + verts 保留）。

- [ ] **Step 1: 写头文件**

```cpp
#ifndef vtkRadialDistanceDenoise_h
#define vtkRadialDistanceDenoise_h

#include <string>
#include <unordered_map>
#include <vtkPolyData.h>
#include <vtkPolyDataAlgorithm.h>
#include "lvFiltersProcessingModule.h"

class LVFILTERSPROCESSING_EXPORT vtkRadialDistanceDenoise : public vtkPolyDataAlgorithm
{
public:
  static vtkRadialDistanceDenoise* New();
  vtkTypeMacro(vtkRadialDistanceDenoise, vtkPolyDataAlgorithm)

  vtkSetMacro(Level1Enabled, bool);
  vtkGetMacro(Level1Enabled, bool);
  vtkSetMacro(Level1Threshold, double);
  vtkGetMacro(Level1Threshold, double);
  vtkSetMacro(Level2Enabled, bool);
  vtkGetMacro(Level2Enabled, bool);
  vtkSetMacro(Level2Threshold, double);
  vtkGetMacro(Level2Threshold, double);
  vtkSetMacro(NumberOfLasers, int);
  vtkGetMacro(NumberOfLasers, int);
  vtkSetMacro(AzimuthBinSize, double);
  vtkGetMacro(AzimuthBinSize, double);
  vtkSetStringMacro(DistanceArrayName);
  vtkGetStringMacro(DistanceArrayName);
  vtkSetStringMacro(LaserIdArrayName);
  vtkGetStringMacro(LaserIdArrayName);
  vtkSetStringMacro(AzimuthArrayName);
  vtkGetStringMacro(AzimuthArrayName);

protected:
  vtkRadialDistanceDenoise();
  ~vtkRadialDistanceDenoise() override = default;
  int FillInputPortInformation(int port, vtkInformation* info) override;
  int RequestData(vtkInformation*, vtkInformationVector**, vtkInformationVector*) override;

private:
  vtkRadialDistanceDenoise(const vtkRadialDistanceDenoise&) = delete;
  void operator=(const vtkRadialDistanceDenoise&) = delete;

  std::string DistanceArrayName = "distance_m";
  std::string LaserIdArrayName = "laser_id";
  std::string AzimuthArrayName = "azimuth";
  bool Level1Enabled = true;
  double Level1Threshold = 10.24;
  bool Level2Enabled = true;
  double Level2Threshold = 10.0;
  int NumberOfLasers = 96;
  double AzimuthBinSize = 0.1;
  std::unordered_map<long long, double> PrevRange;
};
#endif
```

- [ ] **Step 2: 写实现**

```cpp
#include "vtkRadialDistanceDenoise.h"
#include <vtkCellArray.h>
#include <vtkCleanPolyData.h>
#include <vtkDoubleArray.h>
#include <vtkIdTypeArray.h>
#include <vtkInformation.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkRemovePolyData.h>
#include <vtkUnsignedCharArray.h>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

constexpr int POINTS_INPUT_PORT = 0;
constexpr int OUTPUT_PORT = 0;

namespace
{
  long long MakeKey(int laserId, int bin) { return static_cast<long long>(laserId) * 100000LL + bin; }
}

vtkStandardNewMacro(vtkRadialDistanceDenoise)

vtkRadialDistanceDenoise::vtkRadialDistanceDenoise()
{
  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(1);
}

int vtkRadialDistanceDenoise::FillInputPortInformation(int, vtkInformation* info)
{
  info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkPolyData");
  return 1;
}

int vtkRadialDistanceDenoise::RequestData(
  vtkInformation*, vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  vtkPolyData* input = vtkPolyData::GetData(inputVector[POINTS_INPUT_PORT], 0);
  vtkPolyData* output = vtkPolyData::GetData(outputVector, OUTPUT_PORT);
  if (!input) return 0;

  vtkDataArray* distArr = input->GetPointData()->GetArray(this->DistanceArrayName.c_str());
  vtkDataArray* laserArr = input->GetPointData()->GetArray(this->LaserIdArrayName.c_str());
  vtkDataArray* azimArr = input->GetPointData()->GetArray(this->AzimuthArrayName.c_str());
  if (!distArr || !laserArr || !azimArr)
  {
    output->ShallowCopy(input); // 缺必需数组则原样透传
    return 1;
  }

  const vtkIdType n = input->GetNumberOfPoints();
  std::vector<char> drop(n, 0);

  // ===== 二级：同帧、同 laser_id、按方位角排序后左-中-右插值 =====
  if (this->Level2Enabled)
  {
    int maxLaser = this->NumberOfLasers - 1;
    for (vtkIdType i = 0; i < n; ++i)
      maxLaser = std::max(maxLaser, static_cast<int>(laserArr->GetTuple1(i)));
    std::vector<std::vector<vtkIdType>> byLaser(maxLaser + 1);
    for (vtkIdType i = 0; i < n; ++i)
      byLaser[static_cast<int>(laserArr->GetTuple1(i))].push_back(i);
    for (int lid = 0; lid <= maxLaser; ++lid)
    {
      auto& idx = byLaser[lid];
      std::sort(idx.begin(), idx.end(), [&](vtkIdType a, vtkIdType b) {
        return azimArr->GetTuple1(a) < azimArr->GetTuple1(b);
      });
      for (size_t k = 1; k + 1 < idx.size(); ++k)
      {
        double dMid = distArr->GetTuple1(idx[k]);
        double dLeft = distArr->GetTuple1(idx[k - 1]);
        double dRight = distArr->GetTuple1(idx[k + 1]);
        double expected = 0.5 * (dLeft + dRight);
        if (std::fabs(dMid - expected) > this->Level2Threshold)
          drop[idx[k]] = 1;
      }
    }
  }

  // ===== 一级：跨帧、keyed by (laser_id, 方位角分箱) =====
  std::unordered_map<long long, double> newPrev;
  if (this->Level1Enabled)
  {
    double binSize = this->AzimuthBinSize > 0.0 ? this->AzimuthBinSize : 0.1;
    for (vtkIdType i = 0; i < n; ++i)
    {
      int lid = static_cast<int>(laserArr->GetTuple1(i));
      int bin = static_cast<int>(std::floor(azimArr->GetTuple1(i) / binSize));
      long long key = MakeKey(lid, bin);
      double cur = distArr->GetTuple1(i);
      auto it = this->PrevRange.find(key);
      if (it != this->PrevRange.end() && std::fabs(it->second - cur) > this->Level1Threshold)
        drop[i] = 1;
      newPrev[key] = cur; // 双回波取末次；新帧覆盖旧基准
    }
    this->PrevRange.swap(newPrev);
  }
  else
  {
    this->PrevRange.clear();
  }

  // ===== 输出：剔除 Drop 点 =====
  vtkNew<vtkIdTypeArray> idsToRemove;
  for (vtkIdType i = 0; i < n; ++i)
    if (drop[i]) idsToRemove->InsertNextValue(i);

  if (idsToRemove->GetNumberOfTuples() == 0)
  {
    output->ShallowCopy(input);
    return 1;
  }
  vtkNew<vtkRemovePolyData> remove;
  remove->SetInputData(input);
  remove->SetPointIds(idsToRemove);
  vtkNew<vtkCleanPolyData> clean;
  clean->SetInputConnection(remove->GetOutputPort());
  clean->PointMergingOff();
  clean->Update();
  output->ShallowCopy(clean->GetOutput());
  return 1;
}
```

- [ ] **Step 3: 编译并运行测试，确认转绿**

```bash
cmake --build /Users/acelan/workspace/senfoto-view/build --target lvFiltersProcessingCxxTests
ctest --test-dir /Users/acelan/workspace/senfoto-view/build -R RadialDistanceDenoise --output-on-failure
```

预期：测试输出 `PASS`。

- [ ] **Step 4: 提交**

```bash
git add LidarCore/Filters/Processing/vtkRadialDistanceDenoise.h LidarCore/Filters/Processing/vtkRadialDistanceDenoise.cxx
git commit -m "feat: implement vtkRadialDistanceDenoise two-level filter"
```

---

### Task 3: 注册 ServerManager XML 与 CMake

**Files:**
- Create: `LidarCore/Plugin/Filters/RadialDistanceDenoise.xml`
- Modify: `LidarCore/Filters/Processing/CMakeLists.txt`（把类加入 `classes`）
- Modify: `LidarCore/Plugin/CMakeLists.txt`（把 XML 加入 `filters_processing_xml_files`）

**Interfaces:** XML 暴露的属性名须与 Task 2 的 `SetXxx` 命令一致（`Level1Enabled`/`Level1Threshold`/`Level2Enabled`/`Level2Threshold`/`NumberOfLasers`/`AzimuthBinSize`）。数组名固定为 `distance_m`/`laser_id`/`azimuth`（Task 2 中的默认值，对应 Senfoto008，不在 UI 暴露）。

- [ ] **Step 1: 写 XML**

```xml
<ServerManagerConfiguration>
  <ProxyGroup name="filters">
    <SourceProxy name="RadialDistanceDenoise" class="vtkRadialDistanceDenoise"
                 label="Senfoto008 Radial Distance Denoise">
      <Documentation
        short_help="Two-level radial-distance denoise for Senfoto008 point clouds"
        long_help="Level1: cross-frame radial distance diff keyed by (laser_id, azimuth bin). Level2: same-frame, same laser line, left-mid-right interpolation.">
        Two-level radial-distance denoise for Senfoto008. Uses the fixed per-point arrays
        distance_m (radial slant range), laser_id and azimuth (degrees). Level 1 compares each
        point's radial distance with the previous frame at the same (laser_id, azimuth bin);
        Level 2 compares each point on a laser line with the linear interpolation of its
        left/right neighbors.
      </Documentation>

      <InputProperty name="Input" port_index="0" command="SetInputConnection">
        <DataTypeDomain name="input_type"><DataType value="vtkPolyData"/></DataTypeDomain>
      </InputProperty>

      <OutputPort name="Output" index="0" />

      <IntVectorProperty name="Level 1 enabled" command="SetLevel1Enabled"
          number_of_elements="1" default_values="1">
        <BooleanDomain name="bool"/>
        <Documentation>Cross-frame (temporal) radial-distance denoise.</Documentation>
      </IntVectorProperty>

      <DoubleVectorProperty name="Level 1 threshold" command="SetLevel1Threshold"
          number_of_elements="1" default_values="10.24">
        <Documentation>Level 1 radial distance difference threshold (m).</Documentation>
      </DoubleVectorProperty>

      <IntVectorProperty name="Level 2 enabled" command="SetLevel2Enabled"
          number_of_elements="1" default_values="1">
        <BooleanDomain name="bool"/>
        <Documentation>Same-frame (spatial) radial-distance denoise.</Documentation>
      </IntVectorProperty>

      <DoubleVectorProperty name="Level 2 threshold" command="SetLevel2Threshold"
          number_of_elements="1" default_values="10.0">
        <Documentation>Level 2 left/right interpolation deviation threshold (m).</Documentation>
      </DoubleVectorProperty>

      <IntVectorProperty name="Number of lasers" command="SetNumberOfLasers"
          number_of_elements="1" default_values="96">
        <IntRangeDomain name="range" min="1" max="512"/>
        <Documentation>Number of laser lines; buckets are auto-extended to the max id seen.</Documentation>
      </IntVectorProperty>

      <DoubleVectorProperty name="Azimuth bin size" command="SetAzimuthBinSize"
          number_of_elements="1" default_values="0.1">
        <DoubleRangeDomain name="range" min="0.001" max="10"/>
        <Documentation>Level 1 azimuth bin size in degrees for the (laser_id, bin) cache key.</Documentation>
      </DoubleVectorProperty>

      <PropertyGroup label="Radial Distance Denoise">
        <Property name="Level 1 enabled"/>
        <Property name="Level 1 threshold"/>
        <Property name="Level 2 enabled"/>
        <Property name="Level 2 threshold"/>
        <Property name="Number of lasers"/>
        <Property name="Azimuth bin size"/>
      </PropertyGroup>
    </SourceProxy>
  </ProxyGroup>
</ServerManagerConfiguration>
```

- [ ] **Step 2: CMake — 把类加入模块（无 nanoflann 依赖，放基础 `classes`）**

`LidarCore/Filters/Processing/CMakeLists.txt` 中 `set(classes` 列表追加一行（不要放进 `if (LIDARVIEW_USE_NANOFLANN)` 块内）：

```cmake
  vtkRadialDistanceDenoise
```

- [ ] **Step 3: CMake — 把 XML 加入插件注册（`filters_processing_xml_files`，同样不放进 nanoflann 块）**

`LidarCore/Plugin/CMakeLists.txt` 中 `set(filters_processing_xml_files` 列表（约 82–92 行）追加：

```cmake
  Filters/RadialDistanceDenoise.xml
```

- [ ] **Step 4: 全量重新配置 + 编译插件，确认 filter 进入 UI**

```bash
cd /Users/acelan/workspace/senfoto-view
cmake -S lidarview-superbuild -B build >/dev/null 2>&1  # 仅重跑 configure 让 XML/类被拾取
cmake --build /Users/acelan/workspace/senfoto-view/build --target LidarCorePlugin
```

- [ ] **Step 5: 提交**

```bash
git add LidarCore/Plugin/Filters/RadialDistanceDenoise.xml LidarCore/Filters/Processing/CMakeLists.txt LidarCore/Plugin/CMakeLists.txt
git commit -m "feat: register RadialDistanceDenoise filter in LidarCorePlugin"
```

---

### Task 4: Senfoto008 流默认自动挂载 filter

**Files:**
- Modify: `Qt/ApplicationComponents/lqOpenLidarReaction.cxx`（两个创建源函数内追加自动挂载；anon 命名空间加两个 helper）

**Interfaces:**
- 消费 `vtkRadialDistanceDenoise` 的 proxy 名（Task 3 XML 的 `name="RadialDistanceDenoise"`，group `"filters"`）。
- 复用同文件已有的 `::InitAndDisplaySource(pqPipelineSource*, vtkSMProxy*, bool)` 显示 filter 输出。
- 提供 `QSettings().value("LidarView/AutoAttachRadialDenoise", true)` 开关（默认开）。

- [ ] **Step 1: 在 anon 命名空间加 helper**（位于 `InitAndDisplaySource` 之后）

```cpp
#include <QSettings>

bool IsRadialDenoiseAutoAttachEnabled()
{
  return QSettings().value("LidarView/AutoAttachRadialDenoise", true).toBool();
}

void AutoAttachRadialDenoise(pqPipelineSource* source)
{
  if (!source) return;
  pqObjectBuilder* builder = pqApplicationCore::instance()->getObjectBuilder();
  pqPipelineSource* filter = builder->createFilter("filters", "RadialDistanceDenoise", source);
  if (!filter) return;
  filter->getProxy()->UpdateVTKObjects();
  ::InitAndDisplaySource(filter, filter->getProxy(), true);
}
```

- [ ] **Step 2: 在 `openLidarStream()` 挂载**（当前 `::InitAndDisplaySource(source, prototype, true);` 之后，约 223 行）

```cpp
  ::InitAndDisplaySource(source, prototype, true);
  if (QString(prototype->GetXMLName()).startsWith("Senfoto008") && IsRadialDenoiseAutoAttachEnabled())
  {
    AutoAttachRadialDenoise(source);
  }
```

- [ ] **Step 3: 在 `openLidarPcap()` 挂载**（当前 `::InitAndDisplaySource(source, prototype, true);` 之后，约 181 行）

```cpp
    ::InitAndDisplaySource(source, prototype, true);
    if (QString(prototype->GetXMLName()).startsWith("Senfoto008") && IsRadialDenoiseAutoAttachEnabled())
    {
      AutoAttachRadialDenoise(source);
    }
```

- [ ] **Step 4: 编译客户端并冒烟验证**

```bash
cmake --build /Users/acelan/workspace/senfoto-view/build --target SenFoToView
```

验证：打开 Senfoto008 流/PCAP，管线浏览器应自动出现 `RadialDistanceDenoise` 滤镜，主视图点云离群点明显减少；在 `QSettings` 置 `LidarView/AutoAttachRadialDenoise=false` 后重开则不挂载。

- [ ] **Step 5: 提交**

```bash
git add Qt/ApplicationComponents/lqOpenLidarReaction.cxx
git commit -m "feat: auto-attach RadialDistanceDenoise to Senfoto008 sources"
```

---

## 注意 / 风险

- `vtkRemovePolyData` 来自 `VTK::FiltersGeneral`、`vtkCleanPolyData` 来自 `VTK::FiltersCore`，均已在 `LidarView::FiltersProcessing` 的 `PRIVATE_DEPENDS` 中，无需再加依赖。
- 一级 `PrevRange` 跨 `RequestData` 调用持续存在（filter 对象常驻）；同一帧被重复 `RequestData` 时 `PrevRange` 会被当前帧覆盖，比较差为 0，不会误杀（幂等）。
- `(laser_id,bin)` 键中 `bin = floor(azimuth / AzimuthBinSize)`；若雷达转速抖动导致 bin 漂移可能漏判，本期不做滑动窗口（见 spec §10）。
- 自动挂载后原始源表示可能仍可见；若需只显示去噪结果，可在 `AutoAttachRadialDenoise` 内隐藏上游源 representation（本期未做，留作可选优化）。
