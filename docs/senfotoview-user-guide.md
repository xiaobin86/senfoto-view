<!--
  功能：SenFoToView 用户使用说明，涵盖启动、加载雷达实时流/PCAP/点云文件、
        界面操作、染色（含按 X/Y/Z 坐标染色）、SenFoToView 特色功能
        （激光通道选择、径向距离去噪、距离/方位角范围过滤）、视图与测量工具
        （SpreadSheet、Measurement Grid、Ruler、Grid Source）、常用工具与导出。
  作者：acelan
  新建时间：2026-08-29
  修改时间：2026-08-29
-->

# SenFoToView 用户使用说明

SenFoToView 是 LidarView 的 Senfoto 派生版，用于实时接收、回放、可视化与处理三维激光雷达点云数据。本文档面向最终用户，介绍最常用的操作流程。

## 1. 启动程序

编译完成后（详见 [BUILD.md](../BUILD.md)）：

- **Linux**：运行 `../build/install/bin/SenFoToView`
- **macOS**：执行 `open ../build/install/Applications/SenFoToView.app`
  - **注意**：不要在 conda 已激活的终端直接启动，避免与捆绑的 Python 3.12 冲突。

启动后会进入主界面：左侧为 **Pipeline Browser**（管线浏览器），中间为 3D 渲染视图，上方为工具栏。

## 2. 加载雷达数据

### 2.1 实时流（Live Stream）

连接雷达前，需要先把电脑网口配置为静态 IP，并关闭该网口的防火墙。

常见雷达参考配置：

| 雷达型号 | IP 地址示例 | 网关 |
|---------|------------|------|
| Velodyne HDL-32E | 192.168.1.70 | 255.255.255.0 |
| Velodyne HDL-64E | 192.168.3.70 | 192.168.3.255 |

操作步骤：

1. 点击工具栏 **Open Lidar Stream** 图标（或菜单 **File → Open Lidar Stream**）。
2. 在弹出的对话框中选择对应的雷达厂商/型号（如 Velodyne、Hesai、Livox、Robosense、Senfoto008 等）。
3. 选择校准文件（calibration file）。程序通常已内置默认校准，也可点击 **Custom** 加载自定义文件。
4. 点击 **OK**，点云会实时显示在 3D 视图中。

### 2.2 回放 PCAP 文件

1. 点击工具栏 **Open Lidar File** 图标（或菜单 **File → Open Lidar File**）。
2. 选择 `.pcap` 文件。
3. 选择对应的雷达型号与校准文件，点击 **OK**。
4. 时间轴控件会出现在界面下方，可播放、暂停、拖动进度。

### 2.3 打开点云文件

SenFoToView 也支持直接打开通用点云文件格式：

1. 点击菜单 **File → Open**。
2. 选择文件类型，例如 `.pcd`、`.ply`、`.las`、`.vtk`/`.vtp` 等。
3. 点击 **OK** 加载。
4. 如果文件本身没有 `X`、`Y`、`Z` 标量数组，想按坐标染色时，请参见 [4.2 按 X/Y/Z 坐标染色](#42-按-xyz-坐标染色)。

> 注意：通过 **File → Open** 打开的通用点云文件，不会自动携带 `laser_id`、`azimuth`、`distance_m` 等雷达专属数组，因此部分雷达专用功能（如激光通道选择、径向距离去噪）不可用。

## 3. 界面概览

| 区域 | 说明 |
|------|------|
| **Pipeline Browser** | 左侧，显示当前 source/filter/representation 的层级。 |
| **Properties Panel** | 右侧/下方，选中某个对象后显示可配置属性。 |
| **Color Toolbar** | 顶部，切换着色数组、调整颜色映射、显示/隐藏色标。 |
| **Time Toolbar** | 底部，播放/暂停/跳转时间帧（时序数据）。 |
| **3D View** | 中间，点云渲染区域，支持旋转、平移、缩放。 |

## 4. 染色（按属性给点云上色）

### 4.1 切换着色数组

1. 在 **Pipeline Browser** 中选中点云的 representation（通常是 source 下方的条目）。
2. 在顶部 **Color Toolbar** 的下拉框中选择数组，例如：
   - `intensity`（回波强度）
   - `distance_m`（径向距离）
   - `laser_id`（激光通道编号）
   - `timestamp`（时间戳）
   - `X`、`Y`、`Z`（按坐标染色，见下文）
3. 点击 **Reset Range** 可自动适配颜色范围。

### 4.2 按 X/Y/Z 坐标染色

部分雷达数据源会直接输出 `X`、`Y`、`Z` 数组；如果下拉框里没有，可使用 **Point Coordinates To Scalars** filter：

1. 在 **Pipeline Browser** 中选中点云 source。
2. 点击菜单 **Filters → Common → Point Coordinates To Scalars**。
3. 在属性面板中勾选需要生成的坐标数组（默认 `Generate X`、`Generate Y`、`Generate Z` 全开）。
4. 点击 **Apply**。
5. 回到 **Color Toolbar**，此时下拉框中会出现 `X`、`Y`、`Z`，选择即可按对应坐标染色。

## 5. SenFoToView 特色功能

### 5.1 激光通道选择（Laser Selection）

部分雷达（如 Velodyne、Hesai、Senfoto008 等）输出 `laser_id` 数组。SenFoToView 提供了激光通道选择功能，可让用户只显示/隐藏指定的激光线。

**打开方式：**

- 菜单 **Tools → Laser Selection**
- 或工具栏 **Laser Selection** 按钮

**使用方法：**

1. 打开对话框后，会列出当前雷达的所有激光通道。
2. 勾选需要显示的通道，取消勾选需要隐藏的通道。
3. 点击 **Apply** 应用。
4. 勾选 "Apply in future sessions" 可将当前选择保存到下次启动。

> 该功能通过自动挂载在雷达 source 下游的 **LaserSelection** filter 实现。对非雷达数据（无 `laser_id`）不可用。

### 5.2 径向距离去噪（Radial Distance Denoise）

针对 Senfoto008 雷达点云，SenFoToView 内置了两级径向距离去噪 filter，用于抑制飞点、孤立噪点：

- **一级（跨帧）**：同一 `(laser_id, 方位角分箱)` 的点，若与上一帧径向距离差超过阈值 → 判为噪点。
- **二级（同帧）**：同一 `laser_id` 内按方位角排序，若某点偏离左右邻居线性插值超过阈值 → 判为噪点。

**使用方式：**

1. 打开 Senfoto008 实时流或 PCAP 时，程序会自动在管线中挂载 **RadialDistanceDenoise** filter。
2. 在 **Pipeline Browser** 中选中该 filter。
3. 在属性面板中可调整：
   - `Level 1 enabled` / `Level 1 threshold`：一级开关与阈值（默认 10.24 m）
   - `Level 2 enabled` / `Level 2 threshold`：二级开关与阈值（默认 10.0 m）
   - `Azimuth bin size`：一级分箱粒度（默认 0.1°）
4. 点击 **Apply**，视图会自动刷新。

> 对其他雷达型号，可手动添加：**Filters → Common → Senfoto008 Radial Distance Denoise**（需输入含 `distance_m`、`laser_id`、`azimuth` 数组的点云）。

### 5.3 距离与方位角范围过滤（Senfoto008）

Senfoto008 解释器支持在数据解析阶段按距离和方位角过滤：

1. 打开 Senfoto008 数据后，在 **Pipeline Browser** 中选中 source。
2. 在属性面板中找到：
   - `Min Distance` / `Max Distance`：只保留径向距离在此范围内的点。
   - `Start Angle` / `End Angle`：只保留方位角在此范围内的点（单位：度，0–360）。
3. 修改数值后点击 **Apply**。

> 该过滤在解释器内完成，可减轻下游 filter 与渲染的压力。

## 6. 视图与测量工具

### 6.1 SpreadSheet 视图（数据表格）

SpreadSheet 视图以表格形式展示当前点云每个点的属性数据，便于查看 `intensity`、`distance_m`、`laser_id`、`timestamp`、`X`、`Y`、`Z` 等数组值。

**打开方式：**

- 点击主工具栏 **SpreadSheet** 图标（表格图标）。
- 或菜单 **View → SpreadSheet View**（如可用）。

**使用方法：**

1. 点击按钮后，右侧会弹出一个 dock 窗口，显示当前选中 representation 的逐点数据。
2. 在 **Pipeline Browser** 中切换选中的 source/filter，表格会同步更新。
3. 使用表格列头的右键菜单可隐藏/显示特定列。
4. 与 3D 视图联动：在 3D 视图中框选点时，表格会高亮对应行；反之在表格中选择行时，3D 视图中也会高亮对应点。

> 提示：SpreadSheet 视图对大数据集可能较占资源，查看完成后可再次点击按钮关闭 dock。

### 6.2 测量网格（Measurement Grid）

测量网格是铺在地面（XY 平面）上的参考网格，用于直观判断点云尺度和位置。

**显示/隐藏方式：**

- 点击工具栏或 **Edit** 菜单中的 **Show Measurement Grid** 按钮。
- 该按钮为切换状态：按一次显示网格，再按一次隐藏。

> 注意：测量网格仅在 **LidarGridView** 视图模式下可用。如果按钮为灰色，请确认当前视图是否为 LidarGridView。

### 6.3 标尺（Ruler）

标尺工具用于在 3D 视图中测量两点之间的距离。

**打开方式：**

- 点击工具栏中的 **Ruler** 图标。
- 通常提供两种模式：
  - **2D Ruler**：在正交投影下测量屏幕上两点的距离。
  - **3D Ruler**：测量场景中两个 3D 表面点之间的距离。

**使用方法：**

1. 点击 **Ruler** 按钮，使其处于激活/勾选状态。
2. 按住 **Ctrl** 键，在 3D 视图中点击鼠标左键确定第一个点。
3. 再次按住 **Ctrl** 键点击鼠标左键确定第二个点。
4. 视图中会显示一条标尺线段及距离标注。
5. 再次点击 **Ruler** 按钮可退出测量模式。

> 2D Ruler 仅在正交投影（Camera Parallel Projection）下可用；3D Ruler 在透视/正交模式下均可用。

### 6.4 自定义网格源（Grid Source）

如果需要更灵活地生成参考网格平面（例如调整网格大小、原点、法向），可使用 **Grid Source**：

1. 点击菜单 **Sources → Grid Source**。
2. 在属性面板中调整：
   - `GridNbTicks`：网格刻度数
   - `Scale`：每个刻度的长度
   - `Origin`：网格原点
   - `Normal`：网格法向
3. 点击 **Apply**，网格会作为独立 source 显示在视图中。

## 7. 导出数据

1. 在 **Pipeline Browser** 中选中要导出的 source 或 filter。
2. 点击菜单 **File → Export Scene** 或 **File → Save Data**。
3. 选择格式：CSV、PLY、PCD、LAS、VTK 等。
4. 设置输出路径并保存。

## 8. 常见问题

**Q：启动后崩溃或提示 Python 错误**
> 确保不在 conda/miniforge 已激活的终端中启动 SenFoToView。

**Q：实时流没有数据**
> 检查网口静态 IP、网关、防火墙；确认雷达型号与校准文件匹配。

**Q：染色下拉框里没有 X/Y/Z**
> 对没有原生坐标数组的数据源，使用 **Filters → Common → Point Coordinates To Scalars** filter 生成。

**Q：Laser Selection 对话框打开后没有通道列表**
> 确认当前数据源是雷达数据且包含 `laser_id` 数组。通用点云文件（如 PCD/PLY）不支持此功能。

**Q：Senfoto008 去噪后点云明显变少**
> 去噪 filter 默认阈值较激进，可在属性面板中增大 `Level 1 threshold` / `Level 2 threshold`，或临时关闭某一级。

## 9. 相关文档

- 开发者快速开始与构建说明：[README.md](../README.md)
- 架构与功能扩展指南：[senfoto008-data-flow-and-features.md](./senfoto008-data-flow-and-features.md)
- 新增雷达插件模板：[adding-a-lidar-sensor-plugin.md](./adding-a-lidar-sensor-plugin.md)
