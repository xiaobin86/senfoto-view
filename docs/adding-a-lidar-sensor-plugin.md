# 为 LidarView 新增一个雷达/lidar Sensor 插件所需的文件

本文说明：要给 LidarView 增加一种新传感器（雷达/lidar 数据源），需要创建哪些文件、每个文件的作用，以及它们如何通过"名字引用链"串起来让 UI 能识别并加载该传感器。

> 约定：以下以 `MySensor` 作为占位名。仓库内已有两个完全合规的实战样例：
> - 官方案例 `Examples/Plugins/TimeBasedLidarInterpreter/`（基于时间的帧切分）
> - 真实插件 `Plugins/Senfoto008Plugin/`（森佛 008 机械雷达，已注册进默认插件）
> 本文描述与这两者逐一对齐。

---

## 1. 一个 sensor 插件 = 一组 ParaView 插件文件

LidarView 通过 **ParaView 插件机制** 接入新传感器。一个新 sensor 本质上提供：

1. 一个**包解释器（Packet Interpreter）**——把该传感器网络包（MSOP）解码成点云；
2. 一个 **Reader 代理**（读 `.pcap`）和一个 **Stream 代理**（实时流）；
3. 一组 **ServerManager XML** 把上述对象暴露给 ParaView/LidarView 的 UI；
4. 插件清单与构建脚本。

这些文件分成两层目录：**插件根目录**（构建入口 + 插件级 XML）和**模块子目录**（真正的 C++ 解释器 + 模块级 XML）。

---

## 2. 完整文件树（模板）

```
MySensorPlugin/                                   # 插件根目录（目录名任意）
├── paraview.plugin                                # 【插件清单】名字 + 依赖模块
├── CMakeLists.txt                                # 【构建入口】paraview_add_plugin(...)
├── MySensorProxies.xml                           # 【插件级 XML】"lidar_interpreters" 代理：对话框下拉项 → Reader/Stream
└── MySensorPacketInterpreters/                   # 【模块子目录】名字 = 模块名去掉命名空间
    ├── CMakeLists.txt                            # 构建 VTK 模块 + 注册 3 个模块级 XML
    ├── vtk.module                                # 模块名、依赖（必须依赖 LidarView::IOLidar）
    ├── vtkMySensorPacketInterpreter.h            # 解释器类声明（继承 vtkLidarPacketInterpreter）
    ├── vtkMySensorPacketInterpreter.cxx          # 解释器类实现（5 个钩子）
    ├── MySensorPacketFormat.h                    # 包格式：常量、字节偏移、方位角/标志 helper、垂直角表
    ├── MySensorPacketInterpreter.xml             # 【模块级 XML】interpreter 代理（internal_interpreter 组）
    ├── MySensorLidarReader.xml                   # 【模块级 XML】pcap 读取代理（sources 组）
    └── MySensorLidarStream.xml                   # 【模块级 XML】实时流代理（sources 组）
```

> 可选：`Testing/` 子目录（参考 `Senfoto008Plugin/Testing/`，配合顶CMake `option(BUILD_TESTING ...)`）。

---

## 3. 每个文件的作用（核心）

| 文件 | 作用 | 关键点 |
|------|------|--------|
| **`paraview.plugin`** | 插件清单 | `NAME` 即插件名；`REQUIRES_MODULES` 至少含 `VTK::CommonCore` 和 `LidarView::IOLidar`。 |
| **`MySensorPlugin/CMakeLists.txt`** | 构建入口 | 调用 `paraview_add_plugin(MySensorPlugin REQUIRED_ON_CLIENT REQUIRED_ON_SERVER VERSION "1.0" MODULES <ModuleNS>::<Module> MODULE_FILES .../vtk.module SERVER_MANAGER_XML .../MySensorProxies.xml)`。 |
| **`MySensorProxies.xml`** | 插件级 XML（1 个） | 在 `lidar_interpreters` 代理组里定义一个代理，**名字就是 UI 下拉框里显示的传感器名**；通过 `LidarReader`/`LidarStream` 两个 `StringVectorProperty` 指向 Reader/Stream 代理名。 |
| **`MySensorPacketInterpreters/CMakeLists.txt`** | 模块构建 | `vtk_module_add_module(<ModuleNS>::<Module> FORCE_STATIC CLASSES vtkMySensorPacketInterpreter HEADERS MySensorPacketFormat.h)`；随后 `paraview_add_server_manager_xmls(MODULE ... XMLS MySensorPacketInterpreter.xml MySensorLidarReader.xml MySensorLidarStream.xml)`。 |
| **`vtk.module`** | 模块元信息 | `NAME` 必须等于顶 CMake `MODULES` 里写的命名空间全名；`DEPENDS` 至少 `LidarView::IOLidar` + `VTK::CommonCore`。 |
| **`vtkMySensorPacketInterpreter.h/.cxx`** | 解释器实现 | 继承 `vtkLidarPacketInterpreter`，实现 5 个钩子（见第 5 节）。`New()` + `vtkTypeMacro(...)` 必须有。 |
| **`MySensorPacketFormat.h`** | 包格式定义 | 包大小、块大小、各字段字节偏移、方位角/标志提取 helper、各线垂直角表。纯头文件，供 `.cxx` 使用。 |
| **`MySensorPacketInterpreter.xml`** | 模块级 XML（解释器） | 在 `internal_interpreter` 组里定义解释器代理，`class="vtkMySensorPacketInterpreter"`，继承 `CommonLidarPacketInterpreter`；可在 `<IntVectorProperty>` 等里暴露自定义属性（如 `PublishInterval`）。 |
| **`MySensorLidarReader.xml`** | 模块级 XML（读 pcap） | `LidarReaderProxy`，继承 `CommonLidarReader`，`class="vtkLidarReader"`；用 `<SubProxy command="SetLidarInterpreter">` 挂上解释器代理；用 `<Hints><ReaderFactory extensions="pcap" .../></Hints>` 让它出现在"打开 PCAP"对话框。 |
| **`MySensorLidarStream.xml`** | 模块级 XML（实时流） | `LidarStreamProxy`，继承 `CommonLidarStream`，`class="vtkLidarStream"`；同样用 SubProxy 挂解释器；`<Hints><LiveSource/></Hints>` 让它用于实时流。 |

---

## 4. 文件之间的"名字引用链"（如何串起来）

这是最容易出错的地方：**多个 XML 通过代理名互相引用，名字必须一致**。以官方案例实际命名为准：

```
paraview.plugin
  NAME = TimeBasedLidarInterpreter
        │
        ▼  top CMakeLists.txt
paraview_add_plugin(TimeBasedLidarInterpreter
   MODULES TimeBasedLidarInterpreter::TimeBasedLidarPacketInterpreter   ← 对应 vtk.module 的 NAME
   MODULE_FILES .../TimeBasedLidarPacketInterpreter/vtk.module
   SERVER_MANAGER_XML .../TimeBasedLidarExampleProxies.xml)            ← 插件级 XML
        │
        ▼  TimeBasedLidarPacketInterpreter.xml
<Proxy name="TimeBasedLidarPacketInterpreter"                          ← 解释器代理名
       class="vtkTimeBasedLidarPacketInterpreter"
       base_proxyname="CommonLidarPacketInterpreter">
        │
        ▼  TimeBasedLidarReader.xml / TimeBasedLidarStream.xml
<LidarReaderProxy name="TimeBasedLidarReader" ...>                     ← Reader 代理名
   <SubProxy command="SetLidarInterpreter">
     <Proxy proxyname="TimeBasedLidarPacketInterpreter"/>              ← 必须 = 上面的解释器代理名
   </SubProxy>
   <Hints><ReaderFactory extensions="pcap" .../></Hints>
        │
        ▼  TimeBasedLidarExampleProxies.xml（插件级）
<ProxyGroup name="lidar_interpreters">
  <Proxy name="TimeBasedLidarExample"                                  ← UI 下拉框显示的传感器名
         base_proxyname="BaseInterpreter">
    <StringVectorProperty name="LidarReader" default_values="TimeBasedLidarReader"/>   ← 必须 = Reader 代理名
    <StringVectorProperty name="LidarStream" default_values="TimeBasedLidarStream"/>   ← 必须 = Stream 代理名
  </Proxy>
</ProxyGroup>
```

**核对清单（名字必须一致）：**
1. 顶 CMake `MODULES` 的值 == `vtk.module` 的 `NAME`。
2. Reader/Stream XML 里 SubProxy 的 `proxyname` == 解释器 XML 的 `<Proxy name=...>`。
3. 插件级 `*Proxies.xml` 里 `LidarReader`/`LidarStream` 的 `default_values` == Reader/Stream XML 的 `<LidarReaderProxy name=...>`/`<LidarStreamProxy name=...>`。
4. 插件级代理的 `name`（如 `TimeBasedLidarExample`）就是 Open Stream / Open PCAP 对话框里"Interpreter Selection"下拉框显示的文字。

> 基类代理（`CommonLidarPacketInterpreter`、`CommonLidarReader`、`CommonLidarStream`、`BaseInterpreter`）由 LidarView 核心（`LidarCorePlugin`）提供，无需自己写。

---

## 5. 解释器类必须实现的钩子

`vtkMySensorPacketInterpreter` 继承 `vtkLidarPacketInterpreter`，需覆盖以下虚函数（以 `Senfoto008` 真实声明为例）：

```cpp
static vtkMySensorPacketInterpreter* New();
vtkTypeMacro(vtkMySensorPacketInterpreter, vtkLidarPacketInterpreter);

void Initialize() override;                                   // 初始化（务必先调 Superclass::Initialize()）
bool IsLidarPacket(const unsigned char* data,
                  unsigned int dataLength) override;          // 判断该包是否为本传感器合法包
bool PreProcessPacket(const unsigned char* data,
                     unsigned int dataLength,
                     double& outLidarDataTime) override;     // 建 pcap 帧索引；返回是否检测到新帧
void ProcessPacket(const unsigned char* data,
                  unsigned int dataLength) override;          // 解码包、填点；方位回卷时调 SplitFrame()
vtkSmartPointer<vtkPolyData> CreateNewEmptyFrame(
                  vtkIdType nbrOfPoints,
                  vtkIdType prereservedNbrOfPoints = 60000) override;  // 创建空帧，预分配数组
```

实现要点（已与 `RotativeLidarInterpreter` 参考实现逐项核对，规范写法）：
- 构造函数里 `SetSensorVendor("...")` / `SetSensorModelName("...")`，并 `ResetCurrentFrame()`。
- `CreateNewEmptyFrame` 用 `InterpreterHelper.h` 的 `InitArrayForPolyData` 创建 X/Y/Z（`isAdvanced=true`）、Intensity/LaserId/Distance/Azimuth/Timestamp 等数组；X/Y/Z 传 `this->EnableAdvancedArrays`，其余传 `false`；最后 `pd->GetPointData()->SetActiveScalars("intensity")`。**此函数与参考实现逐行一致即可**。
- `ProcessPacket` 用 `this->Points->InsertNextPoint(x,y,z)` + `InsertNextValueIfNotNull(PointsX, ...)` 落点；方位回卷（`currentAzimuth < LastAzimuth`）时调 `SplitFrame()`（且在本包落点之前）。
- 坐标约定：`x = dist·cos(el)·cos(az)`、`y = dist·cos(el)·sin(az)`、`z = dist·sin(el)`（见 `lidarview/docs/lidarview-development-guide.md`）。
- 常把单点逻辑抽成私有 `AddPoint(azimuthDeg, elevationDeg, distanceM, laserId, intensity, timestamp)` 辅助函数。

---

## 6. `Format.h` 的约定

`MySensorPacketFormat.h` 是纯头文件，集中描述该传感器的二进制包结构，便于维护与单测：
- 包/块尺寸常量（`PACKET_SIZE`、`BLOCK_SIZE`、`CHANNELS_PER_BLOCK` 等）；
- 各字段字节偏移；
- 方位角、数据标志的提取 helper（如 `GetBlockAzimuth(data, blockIdx)`、`IsValidDataBlock(data, blockIdx)`）；
- 各激光线的垂直角/仰角表（`std::array<double, N>`）。

> 这些协议细节必须与该传感器的**真实数据手册**对齐（96 线分块配对、各 block 的 laser ID 映射、是否有俯仰等），代码逻辑正确不代表点云几何正确。

---

## 7. 注册插件（让 LidarView 自动加载）

两种方式：

**(A) 注册为默认插件（推荐用于正式传感器）**
在 `lidarview/CMakeLists.txt` 的 `lidarview_default_plugins` 列表中加入插件名（如 `Senfoto008Plugin`、`LakiBeamPlugin` 已在其中）。注意：`LidarCorePlugin` 必须排在第一位。

**(B) 手动加载（用于调试/示例）**
- 示例插件随 `-DBUILD_EXAMPLES=ON` 构建；在 LidarView 里 `Tools > Manage Plugins > <插件名> > Load Selected` 加载；
- 打开流时在 Interpreter Selection 里选对应下拉项。

---

## 8. 编译与验证

```bash
# 构建并安装单个插件（注册进默认插件时）
ninja MySensorPlugin && ninja install

# 示例插件
cmake -DBUILD_EXAMPLES=ON <src>  # 然后 ninja TimeBasedLidarInterpreter
```

验证建议：
- 用 `vtkLidarTestTools::TestPacketInterpreter` 做解释器 sanity check（示例 README 已说明）；`Senfoto008Plugin` 自带 `Testing/`（可用 `BUILD_TESTING=ON` 开启）。
- 用真实 `.pcap` 或 `point_sender.py`（示例 `Utilities/` 提供）跑一遍，确认点云坐标/帧切分正确。

---

## 9. 现成参考：仓库内的 `Senfoto008Plugin`

完整可用的真实插件，结构与本文模板一一对应：

| 模板文件 | Senfoto008 实际文件 |
|----------|----------------------|
| `paraview.plugin` | `Plugins/Senfoto008Plugin/paraview.plugin`（NAME=Senfoto008Plugin） |
| `CMakeLists.txt` | `Plugins/Senfoto008Plugin/CMakeLists.txt`（含 `BUILD_TESTING` + `Testing`） |
| `MySensorProxies.xml` | `Plugins/Senfoto008Plugin/Senfoto008Proxies.xml`（`lidar_interpreters` 组） |
| 模块子目录 | `Plugins/Senfoto008Plugin/Senfoto008PacketInterpreters/` |
| `vtk.module` | `Senfoto008Plugin::Senfoto008PacketInterpreters` |
| 解释器 `.h/.cxx` | `vtkSenfoto008PacketInterpreter.{h,cxx}` |
| `Format.h` | `Senfoto008PacketFormat.h` |
| 3 个模块级 XML | `Senfoto008PacketInterpreter.xml` / `Senfoto008LidarReader.xml` / `Senfoto008LidarStream.xml` |

该插件已注册进 `lidarview_default_plugins`，是"照着写"的最佳范本。

---

## 10. 快速清单（照抄不漏）

- [ ] `paraview.plugin`：`NAME` + `REQUIRES_MODULES` 含 `LidarView::IOLidar`
- [ ] 插件根 `CMakeLists.txt`：`paraview_add_plugin(... MODULES <NS>::<Mod> MODULE_FILES .../vtk.module SERVER_MANAGER_XML .../MySensorProxies.xml)`
- [ ] `vtk.module`：`NAME` 与上面 `MODULES` 一致；`DEPENDS` 含 `LidarView::IOLidar`、`VTK::CommonCore`
- [ ] 模块 `CMakeLists.txt`：`vtk_module_add_module(... FORCE_STATIC ...)` + `paraview_add_server_manager_xmls(... 3 个 XML)`
- [ ] 解释器类：5 个钩子 + `New()`/`vtkTypeMacro`；`CreateNewEmptyFrame` 与参考实现一致
- [ ] `Format.h`：包布局与垂直角表对齐硬件手册
- [ ] 3 个模块级 XML：`*PacketInterpreter.xml`（interpreter 代理）、`*LidarReader.xml`（带 `ReaderFactory` Hints）、`*LidarStream.xml`（带 `LiveSource` Hints）；SubProxy 指向解释器代理名
- [ ] 插件级 `MySensorProxies.xml`：`lidar_interpreters` 代理，`LidarReader`/`LidarStream` 指向对应代理名
- [ ] 在 `lidarview/CMakeLists.txt` 的 `lidarview_default_plugins` 注册（LidarCorePlugin 必须第一）
- [ ] `ninja MySensorPlugin && ninja install`，用 pcap/模拟脚本验证点云
