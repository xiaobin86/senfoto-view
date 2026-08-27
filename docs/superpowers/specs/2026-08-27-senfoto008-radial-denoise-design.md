# Senfoto008 点云两级「径向距差」去噪 — 设计文档

- 日期：2026-08-27
- 分支：`feat/senfoto008-pointcloud-denoise`（基于 `develop`）
- 依据：用户提供的 `denoise_level1_level2_guide.md`（两级径向距差去噪参考实现）

## 1. 目标

为 Senfoto008 激光雷达点云增加两级基于**径向距离（`distance_m` / 斜距）**的去噪：

- **一级（跨帧 / 时间维度）**：同一 `(laser_id, 方位角分箱)` 的点，若与上一帧径向距差超过阈值 → 判为噪点。
- **二级（同帧 / 空间维度）**：同一 `laser_id` 内按方位角有序，取左-中-右三点，若中点偏离左右线性插值超过阈值 → 判为噪点。

两者默认开启，输出自动显示在主窗口渲染视图；均可在 filter 属性里单独关闭 / 调参。

## 2. 工程化调整（相对参考文档）

参考文档给出纯 C++ 参考实现，本设计在两处按讨论结论做调整：

- **一级对齐方式**：由「全局 index」改为「`(laser_id, 方位角分箱)` keyed 缓存」（用户拍板方案 b），以容忍丢包 / 相邻帧点数或顺序不一致导致的错位误杀。
- **噪点处理**：统一为 **Drop**（从输出 `vtkPolyData` 中移除该点，用户拍板），而非置 `distance_m=0`。原因：几何坐标保存在 `Points` 里，仅把 `distance_m` 置 0 不会让该点从视图消失。

## 3. 数据契约（来自 `vtkSenfoto008PacketInterpreter::CreateNewEmptyFrame`）

输入点云为 `vtkPolyData`，逐点数组（均由解释器恒久创建，无需高级数组开关）：

- `distance_m`：`vtkDoubleArray`，径向斜距 r —— 即文档所需的「径向距离」，直接复用，无需 `sqrt(x²+y²+z²)`。
- `laser_id`：`vtkUnsignedCharArray`，通道号 0..95。
- `azimuth`：`vtkDoubleArray`，方位角（度），用于一级分箱与二级排序。
- `intensity` / `timestamp` / `Points`（几何）：原样透传，去噪不改坐标。

> 几何在 `Points` 中；去噪算法**只决定点是否保留**，不修改任何坐标值。

## 4. 架构与落点（实现为 ParaView filter，而非改解释器）

**理由**：`vtkLidarStream` 的帧缓冲仅保留 2 帧且每次 `RequestData` 清空，解释器只暴露 `Frames.back()`，无持久「上一帧」接口。做成 filter 可以：独立开关、可与其他 filter 组合、属性自动进 UI、输出自动进主视图——契合「用管线 / 加个 filter」的诉求。

- 新增类：`vtkRadialDistanceDenoise : public vtkPolyDataAlgorithm`
  - `LidarCore/Filters/Processing/vtkRadialDistanceDenoise.cxx / .h`
- 注册：
  - `LidarCore/Plugin/Filters/RadialDistanceDenoise.xml`（`SetInputArrayToProcess` 绑定 `distance_m` 与 `laser_id`，属性见 §6）
  - `LidarCore/Plugin/CMakeLists.txt` 通过 `lidarcoreplugin_add_module_xml(MODULE_TARGET LidarView::FiltersProcessing XML_MODULE_FILES Filters/RadialDistanceDenoise.xml)` 注册（紧邻 `AdaptiveOutlierRemoval` 的注册块）
- 该 filter 编译进 `LidarCorePlugin`（已随 SenFoToView 打包），无需新建插件。

## 5. 算法

### 5.1 二级（同帧，先执行）

1. 取 `laser_id` 数组，按 `laser_id` 分桶；每桶内**按 `azimuth` 升序排序**（用户同意显式排序，保证左-中-右为相邻方位角）。
2. 对每个桶内序号 `k = 1 .. n-2`（内部点），取 `dMid = distance[r[k]]`、`dLeft = distance[r[k-1]]`、`dRight = distance[r[k+1]]`。
3. 判定（启用插值版，参考文档正式版）：`expected = (dLeft + dRight) / 2; if (|dMid - expected| > Level2Threshold) → drop`。
4. 端点（无左邻或无右邻）不参与判定。
- 阈值默认 `10.0`；仅当 `Level2Enabled` 时执行。

### 5.2 一级（跨帧，后执行）

- 内部状态：`std::unordered_map<Key, double> PrevRange`，`Key = (laser_id, azimuthBin)`，`azimuthBin = (int)floor(azimuth / AzimuthBinSize)`（`AzimuthBinSize` 默认与该雷达角分辨率相当，如 `0.1°`，作为属性可调）。
- 对当前帧每个点：算 `key`，若 `PrevRange` 含该 `key` 且 `|PrevRange[key] - cur| > Level1Threshold` → drop；处理完后**重建** `PrevRange`（先清空，再填入当前帧所有点的 `key -> distance`），即下一帧的「上一帧」基准。
- 阈值默认 `10.24`；仅当 `Level1Enabled` 时执行。
- 双回波注意：同一 `(laser_id, bin)` 可能落多个点，缓存取末次值；如需精确可改为按回波索引细分 key（本期不做，列入风险）。

### 5.3 输出

- 新建 `vtkPolyData`，拷贝**未被 drop** 的点（`Points` + 全部数组 + `verts`），保持数组名不变。
- 两级都开启时：先算二级 drop 集合，再叠加一级 drop 集合，最终输出为两者并集。

## 6. 属性（XML 暴露，UI 自动生成）

| 属性 | 类型 | 默认 | 说明 |
|---|---|---|---|
| `Level1Enabled` | bool | true | 一级（跨帧）开关 |
| `Level1Threshold` | double | 10.24 | 一级阈值（米） |
| `Level2Enabled` | bool | true | 二级（同帧）开关 |
| `Level2Threshold` | double | 10.0 | 二级阈值（米） |
| `NumberOfLasers` | int | 96 | 激光通道总数；运行时取 `max(laser_id)+1` 与默认值之大者 |
| `AzimuthBinSize` | double | 0.1 | 一级分箱粒度（度） |
| `DistanceArrayName` | string | `"distance_m"` | 径向距数组名（便于复用于其他解释器） |
| `LaserIdArrayName` | string | `"laser_id"` | 通道数组名 |

## 7. 自动挂载到 Senfoto008 流（用户拍板：默认挂上）

- 在 Senfoto008 流 / 读取源创建成功后，自动 `pqObjectBuilder::createFilter("filters", "RadialDistanceDenoise", sourceProxy)`，并将 filter 输出设为可见渲染输入。
- 接入点：`Qt/ApplicationComponents/lqOpenLidarReaction.cxx`（流 / 读取源创建成功处），判断为 Senfoto008 解释器时自动追加。
- 提供关闭开关（建议放 `LidarViewSettings` 或该 reaction 内 bool，**默认 true**），避免强制绑定。

## 8. 文件改动清单

- 新增：`LidarCore/Filters/Processing/vtkRadialDistanceDenoise.cxx / .h`
- 新增：`LidarCore/Plugin/Filters/RadialDistanceDenoise.xml`
- 改：`LidarCore/Plugin/CMakeLists.txt`（注册 XML 到 `LidarView::FiltersProcessing`）
- 改：`Qt/ApplicationComponents/lqOpenLidarReaction.cxx`（自动挂载）
- 确认：`LidarCore/Filters/Processing/vtk.module` 与该目录 `CMakeLists.txt` 已含 `LidarView::FiltersProcessing` 模块，新类经 `vtk_module_add_module` 纳入（参考同目录 `vtkAdaptiveOutlierRemoval`）。

## 9. 测试

- 二级单元验证：合成 polyData（参考文档 §6 最小用例）一条线 `[10,10,50,10,10]`、阈值 10 → 中点被标；斜坡 `[0,10,20]` → 中点保留。
- 一级单元验证：构造两帧、同 `(laser_id, bin)` 距差超阈 → 第二帧该点被 drop。
- 真实数据：打开 Senfoto008 流，确认 filter 自动挂载、主视图离群点明显减少、属性可调、可单独关闭某级。
- 性能：单帧点数规模（~10⁵–10⁶）下 `RequestData` 耗时可接受（分桶 + 排序为 `O(n log n)`）。

## 10. 风险 / 注意

- 一级分箱粒度（`AzimuthBinSize`）影响对错位 / 边界的敏感度；默认 `0.1°`，可按数据微调。
- 一级 keyed 缓存较全局 index 更稳，但仍假设同一 `(laser_id, bin)` 在两帧对应同一物理射线；若雷达转速抖动导致 bin 漂移，可改滑动窗口匹配（本期不做）。
- 真实遮挡边界（高曲率）在二级插值版下可能被误标；如需可切 OR / AND 判定（参考文档已给，留作后续开关）。
- 自动挂载限定 Senfoto008 源 + 可关闭，避免影响其他 lidar 场景。
