# SenFoToView 数据流与功能扩展架构说明

> 本文面向想在 SenFoToView（LidarView 的 Senfoto008 定制版本）中新增功能的开发者。
> 即使你是第一次接触这个项目，只要照着本文的「目录架构 → 核心概念 → 两条链路 → 4 个案例」读下来，就能理解代码是怎么组织的、为什么要这么改，并照着案例一步步写出可运行的代码。
>
> 文中所有路径都相对源码根 `senfoto-view/`。

---

## 1. 仓库目录架构：每个文件夹放什么、起什么作用

先建立一张「地图」，后面提到文件时你才知道它在哪、为什么在那。

| 目录 | 作用 | 你会在哪里遇到它 |
|---|---|---|
| `Application/` | **应用程序外壳**：主窗口、可执行入口、面向最终用户的 GUI 组装 | 改主窗口、加菜单/工具栏入口、注册 app 级资源 |
| `Application/Client/` | 主窗口 `LidarViewMainWindow` 及其 `.ui`，lidar 专用的 source/filter XML | 在 Tools 菜单加 action、改主窗口布局 |
| `Application/Qt/` | app 专属的 Qt 组件（工具栏、菜单构建器、界面模式管理） | 改主工具栏、改界面模式切换逻辑 |
| `LidarCore/` | **核心数据与分析能力**：VTK 算法、IO、LidarCorePlugin | 写 filter、写 reader/stream 基础类、注册 ServerManager XML |
| `LidarCore/IO/Lidar/` | lidar 数据 IO：reader、stream、解释器基类 | 理解数据怎么进来（`vtkLidarReader` 等） |
| `LidarCore/Filters/Processing/` | 所有点云处理 filter（去噪、通道选择等） | 写新的点云处理 filter |
| `LidarCore/Plugin/` | LidarCorePlugin 的 ServerManager XML 与资源 | 注册 filter/reader 的 XML 到 ParaView |
| `LidarCore/Remoting/ServerManager/` | source/reader 的服务器端代理（`vtkSMLidarReaderProxy` 等） | 一般不需要改，理解管线代理即可 |
| `Qt/` | **通用 Qt 组件**：与 ParaView 紧密耦合的 UI 模块 | 写对话框、Reaction、属性控件 |
| `Qt/ApplicationComponents/` | 通用 Reactions 与工具栏（如 `lqOpenLidarReaction`、`lqDockableSpreadSheetReaction`） | 加自动挂载逻辑、加 Reaction |
| `Qt/Components/` | 通用对话框与控件（如 `lqLaserSelectionDialog`） | 写功能面板/对话框 |
| `Plugins/` | **设备/厂商插件**：每种雷达一个独立插件 | 新增雷达类型 |
| `Plugins/Senfoto008Plugin/` | Senfoto008 雷达插件（本文案例 4.1） | 参考模板 |
| `Examples/` | 官方参考样例（必读） | 照抄正确写法 |
| `Examples/Plugins/` | 各种插件示例：新解释器、Filter、时间切片等 | 写新功能前先找同类型样板 |
| `CMake/` | 构建辅助脚本 | 一般不用改 |
| `Testing/` | 顶层测试 | 加测试时参考 |
| `Utilities/` | Doxygen / Sphinx 等文档工具 | 不用管 |
| `docs/` | 项目文档（本文所在处） | — |

**一句话记忆**：`LidarCore` 管「数据和算法」，`Qt` 管「界面组件」，`Plugins` 管「设备」，`Application` 管「把前面三者拼成一个产品」。

---

## 2. 核心概念：读代码前必须理解的词

### 2.1 LidarView 与 ParaView 的关系

LidarView 是 ParaView 的一个**定制分支**。ParaView 负责「管线的运行时框架、渲染、UI 框架」，LidarView 在其上叠加「lidar 数据源、点云算法、lidar 专用界面」。所以你写的功能最终都是「注册给 ParaView 框架的插件/模块」。

### 2.2 管线（Pipeline）

管线是一串**数据生产者 → 处理者 → 消费者**的链条：

```
Source（reader / stream） → [Filter → Filter → …] → Representation → View（屏幕）
```

- 每个节点都是一个 ParaView proxy。
- 数据以 `vtkPolyData` 形式从上游往下流。
- 你在 UI 的「Pipeline Browser」里看到的每一个方块，就是管线里的一个节点。

### 2.3 Source / Filter / Representation / View

| 概念 | 是什么 | 举例 |
|---|---|---|
| **Source** | 数据的起点，没有输入、有输出 | `vtkLidarReader`、`vtkLidarStream` |
| **Filter** | 有输入、有输出，对 `vtkPolyData` 做变换 | `RadialDistanceDenoise`、`LaserSelection` |
| **Representation** | 决定「点云怎么画」（颜色、点大小、是否显示） | ParaView 自动创建的 `vtkSMPVRepresentationProxy` |
| **View** | 真正的渲染窗口 | `pqRenderView`（3D 视图） |

### 2.4 ServerManager Proxy（代理）

ParaView 把 C++ 对象分成「客户端」和「服务端（数据端）」。你在 Qt 代码里操作的是 **proxy**（客户端代理），它通过 ServerManager 把属性同步到服务端真正的 VTK 对象。

- 配置文件：`*.xml`（ServerManager XML）声明 proxy 的名字、类、属性。
- 运行时：你在 Qt 里拿到 `vtkSMProxy*`，用 `vtkSMPropertyHelper` 读写属性，`UpdateVTKObjects()` 把改动推到服务端。

### 2.5 vtkPolyData

一帧点云在 VTK 里的数据结构。包含：

- `Points`：每个点的三维坐标。
- 点数据数组（PointData）：`intensity`、`laser_id`、`distance_m`、`azimuth`、`timestamp` 等逐点属性。

所有 filter 的输入/输出都是 `vtkPolyData`。

### 2.6 Reaction

LidarView 把「用户点了一个菜单项 / 工具栏按钮后要做什么」封装成 `pqReaction` 的子类，叫 **Reaction**。

- UI 里的一个 `QAction`（按钮）绑定一个 Reaction。
- 点击 → Reaction 的 `onTriggered()` 执行具体逻辑。
- 好处：菜单项和工具栏按钮可以复用同一个 Reaction。

### 2.7 界面模式（Interface Mode）

LidarView 有 `lidarViewer` / `pointCloudTool` / `advancedMode` 三种界面模式。每种模式显示不同的菜单、工具栏、dock。配置写在 `interface_modes_config.json`。

- 改了工具栏/菜单，记得在这个 JSON 里注册，否则切换模式时会被隐藏。

### 2.8 构建系统三件套

| 文件 | 作用 |
|---|---|
| `vtk.module` | 声明一个 VTK 模块（名字、依赖、导出符号） |
| `paraview.plugin` | 声明一个 ParaView 插件（名字、依赖模块） |
| `*.xml`（ServerManager XML） | 声明 proxy 及其属性，让 ParaView 认识你的类 |

记住：**新增一个 VTK 类，要在 `vtk.module` / `CMakeLists.txt` 里登记；新增一个可被 UI 使用的算法/源，要写 ServerManager XML；新增一个独立功能包，要写 `paraview.plugin`。**

---

## 3. 总体架构：两条链路

新增功能先判断它属于哪条链路：

- **数据流转链路**：改「数据从哪来、怎么变成点云、怎么被处理」。
- **UI 展示链路**：改「用户从哪里点、界面怎么显示」。

### 3.1 数据流转链路（报文 → 屏幕）

```
Stage 1：解析 / 过滤
  PCAP / 网络报文
    → vtkLidarReader / vtkLidarStream
    → vtkLidarPacketInterpreter（ProcessPacket / SplitFrame）
    → vtkPolyData（Frame）

Stage 2：渲染
  vtkPolyData
    → ParaView Source/Filter Proxy
    → Representation
    → pqRenderView
    → 屏幕
```

**Stage 1 关键文件**：

| 角色 | 文件 | 关键方法 |
|---|---|---|
| 离线 PCAP | `LidarCore/IO/Lidar/vtkLidarReader.cxx` | `RequestInformation()` / `RequestData()` |
| 实时流 | `LidarCore/IO/Lidar/vtkLidarStream.cxx` | `Start()` / `ConsumePacket()` |
| 解释器基类 | `LidarCore/IO/Lidar/vtkLidarPacketInterpreter.cxx` | `PreProcessPacketWrapped()` / `ProcessPacketWrapped()` / `SplitFrame()` |
| 具体解释器 | `Plugins/Senfoto008Plugin/.../vtkSenfoto008PacketInterpreter.cxx` | `IsLidarPacket()` / `ProcessPacket()` |

**Stage 2 关键文件**：

| 角色 | 文件 |
|---|---|
| Reader Proxy | `LidarCore/Remoting/ServerManager/vtkSMLidarReaderProxy.cxx` |
| Stream Proxy | `LidarCore/Remoting/ServerManager/vtkSMLidarStreamProxy.cxx` |
| 客户端打开源 | `Qt/ApplicationComponents/lqOpenLidarReaction.cxx` |

**核心方法：客户端如何把数据送进视图**

```cpp
// lqOpenLidarReaction.cxx 中的内部辅助函数
void InitAndDisplaySource(pqPipelineSource* source, vtkSMProxy* prototype, bool updateAnimation)
{
  vtkSMProxy* proxy = source->getSourceProxy();
  proxy->Copy(prototype);           // 把配置对话框选的属性拷贝到真实 proxy
  proxy->UpdateVTKObjects();        // 同步到服务端

  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  source->updatePipeline();
  pqView* view = pqActiveObjects::instance().activeView();
  controller->Show(source->getSourceProxy(), 0, view->getViewProxy());  // 端口 0 = Frame
  pqActiveObjects::instance().setActiveSource(source);
}
```

**在数据链路中创建并挂载一个 filter**

```cpp
pqObjectBuilder* builder = pqApplicationCore::instance()->getObjectBuilder();
// 创建一个 filter，输入自动接在 inputSource 的输出端口 0
pqPipelineSource* filter = builder->createFilter("filters", "RadialDistanceDenoise", inputSource);
filter->getProxy()->UpdateVTKObjects();
::InitAndDisplaySource(filter, filter->getProxy(), true);
```

**把客户端数据推到服务端 filter（触发重算）**

```cpp
vtkSMPropertyHelper propHelper(filterProxy, "LaserSelection");
propHelper.Set(mask.data(), mask.size());   // 写 SM 属性
filterProxy->UpdateVTKObjects();            // 同步到服务端
filterProxy->MarkModified(filterProxy);     // 标记已改，服务端重新执行 RequestData
```

> **为什么 filter 都挂在 source 下游、而不是改解释器？** 因为解释器只负责「把包变成点云」，没有「上一帧」持久状态、也不方便独立开关；而 filter 可独立开关、可组合、属性自动进 UI、输出自动进视图。任何「渲染前改点云」的需求都应做成 filter。

### 3.2 UI 展示链路（菜单 → 工具栏 → dock → 界面模式）

```
Application/Client/LidarViewMainWindow.cxx
  │  setupUi() 加载 .ui（menubar、dock、central widget）
  │
  ├─ lqLidarViewMenuBuilders::buildFileMenu / buildEditMenu / buildToolbars(...)
  │     └─ lqMainControlsToolbar 加载 lqMainControlsToolbar.ui，绑定 Reaction
  │
  ├─ pqParaViewMenuBuilders::buildViewMenu()
  │
  └─ lqLidarViewManager::restoreSavedInterfaceLayout()
        └─ 读 interface_modes_config.json
        └─ updateMenusLayout / updateToolBarsLayout / updateDockWidgetsLayout
```

**一个工具栏按钮从定义到可用，要经过这几步**：

1. 在 `lqMainControlsToolbar.ui` 声明一个 `QAction`。
2. 在 `<addaction>` 里把它放进工具栏。
3. 在 `lqMainControlsToolbar.cxx` 里 `new lqXxxReaction(ui.actionXxx)` 绑定业务。
4. （图标）SVG 放进 `Application/Qt/Components/Resources/Icons/`，并在 `lvComponents.qrc` 注册。
5. （显隐）在 `interface_modes_config.json` 的 `mainControlsToolbar` 列表里加上 action 名。

> **为什么要在 JSON 注册？** `lqLidarViewManager::updateToolBarsLayout()` 在切换界面模式时会按 JSON 只保留列表里的 action，没注册的会被隐藏。

---

## 4. 基于数据流转链路的功能扩展案例

### 4.1 案例一：新增一个雷达插件（Senfoto008）

#### 目标

让 Senfoto008 机械雷达（48 线 / 96 线、MSOP 报文、UDP 8089）能以 PCAP 或实时流方式接入管线。

#### 你会学到

- 怎么写一个 ParaView 插件
- 怎么写一个 packet interpreter 并继承基类
- 怎么用 SubProxy 把解释器挂到 reader/stream
- 怎么让 LidarView 在打开时识别这个新雷达

#### 前置条件

- 已知雷达报文格式（magic、端口、每块结构、通道布局）。
- 参考：`Plugins/Senfoto008Plugin/`、`Examples/Plugins/`（官方样板）、`docs/adding-a-lidar-sensor-plugin.md`。

#### 完整步骤

**步骤 1：新建插件目录与 `paraview.plugin`**

```
Plugins/MySensorPlugin/
├── paraview.plugin
├── CMakeLists.txt
├── MySensorProxies.xml
└── MySensorPacketInterpreters/
    ├── vtk.module
    ├── CMakeLists.txt
    ├── MySensorPacketFormat.h
    ├── vtkMySensorPacketInterpreter.h
    ├── vtkMySensorPacketInterpreter.cxx
    ├── MySensorPacketInterpreter.xml
    ├── MySensorLidarReader.xml
    └── MySensorLidarStream.xml
```

`paraview.plugin` 内容：

```text
NAME
  MySensorPlugin
DESCRIPTION
  Reader and stream for My Sensor
REQUIRES_MODULES
  VTK::CommonCore
  LidarView::IOLidar
```

> **为什么需要 `paraview.plugin`？** ParaView 的插件扫描机制靠它识别插件名和依赖模块。没有它，构建系统不会把你的模块编译成插件。

**步骤 2：写 `MySensorPacketInterpreters/vtk.module`**

```text
NAME
  MySensorPlugin::MySensorPacketInterpreters
LIBRARY_NAME
  MySensorPacketInterpreters
DEPENDS
  LidarView::IOLidar
  VTK::CommonCore
PRIVATE_DEPENDS
  VTK::CommonDataModel
```

> **为什么需要 `vtk.module`？** VTK 用模块化方式管理依赖和导出符号。模块名 `MySensorPlugin::MySensorPacketInterpreters` 会被后面 CMake 和 XML 引用。

**步骤 3：写插件 `CMakeLists.txt`**

```cmake
paraview_add_plugin(MySensorPlugin
  REQUIRED_ON_CLIENT
  REQUIRED_ON_SERVER
  VERSION "1.0"
  MODULES
    MySensorPlugin::MySensorPacketInterpreters
  MODULE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/MySensorPacketInterpreters/vtk.module"
  SERVER_MANAGER_XML
    "${CMAKE_CURRENT_SOURCE_DIR}/MySensorProxies.xml"
)
```

> **为什么用 `paraview_add_plugin`？** 它把模块、XML、qrc 等资源打包成一个 ParaView 插件，并生成自动加载所需的信息。

**步骤 4：写 `MySensorPacketInterpreters/CMakeLists.txt`**

```cmake
set(classes
  vtkMySensorPacketInterpreter
)

vtk_module_add_module(MySensorPlugin::MySensorPacketInterpreters
  FORCE_STATIC
  CLASSES ${classes}
)

paraview_add_server_manager_xmls(
  MODULE MySensorPlugin::MySensorPacketInterpreters
  XMLS
    MySensorPacketInterpreter.xml
    MySensorLidarReader.xml
    MySensorLidarStream.xml
)
```

> **为什么要把 XML 注册在这里？** `paraview_add_server_manager_xmls` 让 ParaView 在插件加载时读取这些 XML，从而认识你的 proxy 和属性。

**步骤 5：实现解释器，继承 `vtkLidarPacketInterpreter`**

```cpp
#include <vtkLidarPacketInterpreter.h>
#include "MySensorPacketInterpretersModule.h"

class MYSENSORPACKETINTERPRETERS_EXPORT vtkMySensorPacketInterpreter
  : public vtkLidarPacketInterpreter
{
public:
  static vtkMySensorPacketInterpreter* New();
  vtkTypeMacro(vtkMySensorPacketInterpreter, vtkLidarPacketInterpreter);

  void Initialize() override;
  bool IsLidarPacket(unsigned char const* data, unsigned int dataLength) override;
  bool PreProcessPacket(unsigned char const* data, unsigned int dataLength,
                        double& outLidarDataTime) override;
  void ProcessPacket(unsigned char const* data, unsigned int dataLength) override;

protected:
  vtkSmartPointer<vtkPolyData> CreateNewEmptyFrame(vtkIdType nbrOfPoints,
    vtkIdType prereservedNbrOfPoints = 60000) override;
  vtkMySensorPacketInterpreter();
  ~vtkMySensorPacketInterpreter();
};
```

- `IsLidarPacket()`：判断一个 UDP 包是不是你的雷达包（查 magic / 头）。
- `PreProcessPacket()`：建帧索引时用，检测帧边界（方位角回绕）。
- `ProcessPacket()`：真正的解析，把每点填进 `CurrentFrame`。
- `CreateNewEmptyFrame()`：创建一帧 `vtkPolyData`，定义 `Points`、`intensity`、`laser_id` 等数组。

> **为什么继承 `vtkLidarPacketInterpreter`？** 基类已经实现了帧管理、坐标变换、reader/stream 对接。你只管「怎么把包变成点」，其余交给框架。

**步骤 6：写解释器 XML（继承 `CommonLidarPacketInterpreter`）**

```xml
<ServerManagerConfiguration>
  <ProxyGroup name="internal_interpreter">
    <Proxy name="MySensorPacketInterpreter"
           class="vtkMySensorPacketInterpreter"
           label="My Sensor Interpreter"
           base_proxygroup="internal_interpreter"
           base_proxyname="CommonLidarPacketInterpreter">
      <!-- 可暴露距离过滤、FOV 等自定义属性 -->
    </Proxy>
  </ProxyGroup>
</ServerManagerConfiguration>
```

**步骤 7：写 Reader XML，用 SubProxy 挂解释器**

```xml
<LidarReaderProxy name="MySensorLidarReader"
                  class="vtkLidarReader"
                  label="My Sensor LiDAR Reader"
                  base_proxygroup="internal_interpreter"
                  base_proxyname="CommonLidarReader">
  <SubProxy command="SetLidarInterpreter">
    <Proxy name="PacketInterpreter"
           proxygroup="internal_interpreter"
           proxyname="MySensorPacketInterpreter" />
    <ExposedProperties>
      <PropertyGroup label="LiDAR Interpreter Options">
        <Property name="TimeOffset" />
        <Property name="IgnoreZeroDistances" panel_visibility="advanced" />
      </PropertyGroup>
    </ExposedProperties>
  </SubProxy>
  <Hints>
    <ReaderFactory extensions="pcap" file_description="My Sensor LiDAR files" />
  </Hints>
</LidarReaderProxy>
```

> **为什么用 SubProxy？** reader 是通用的 `vtkLidarReader`，真正「懂协议」的是解释器。SubProxy 把解释器注入 reader，reader 在运行时调用解释器的 `ProcessPacket()`。这是 LidarView 的标准做法，避免每种雷达都重写一遍 reader。

Stream XML 同理，把 `LidarReaderProxy` 换成 `LidarStreamProxy`、`CommonLidarReader` 换成 `CommonLidarStream`。

**步骤 8：写 `MySensorProxies.xml`，让传感器选择对话框识别**

```xml
<ServerManagerConfiguration>
  <ProxyGroup name="lidar_interpreters">
    <Proxy name="MySensor"
           base_proxygroup="internal_interpreter"
           base_proxyname="BaseInterpreter">
      <StringVectorProperty name="LidarReader"
                            number_of_elements="1" override="1"
                            default_values="MySensorLidarReader" information_only="1" />
      <StringVectorProperty name="LidarStream"
                            number_of_elements="1" override="1"
                            default_values="MySensorLidarStream" information_only="1" />
    </Proxy>
  </ProxyGroup>
</ServerManagerConfiguration>
```

> **为什么要这一步？** 打开数据时 `lqLidarConfigurationDialog` 会列出 `lidar_interpreters` 组里的所有条目让用户选。没有这条，UI 里看不到你的雷达。

**步骤 9：在顶层 `CMakeLists.txt` 注册默认插件**

```cmake
set(lidarview_default_plugins
  LidarCorePlugin
  ...
  MySensorPlugin
)
```

> **为什么要注册？** `lidarview_default_plugins` 决定哪些插件默认编译并随产品发布。LidarCorePlugin 必须排第一，因为它提供解释器基类。

**验证方法**

1. 重新构建（参考 `BUILD.md`）。
2. 打开一个 MySensor 的 pcap，确认点云出现。
3. 在 `lqLidarConfigurationDialog` 里能看到 "My Sensor" 选项。

---

### 4.2 案例二：增加去噪逻辑（RadialDistanceDenoise）

#### 目标

给 Senfoto008 点云增加两级径向距差去噪：

- **L2（同帧）**：同一 `laser_id` 内按方位角排序，若某点显著偏离左右邻居插值，判为噪点。
- **L1（跨帧）**：以 `(laser_id, 方位角分箱)` 为 key 缓存上一帧径向距，若当前帧同 key 突变，判为噪点。

#### 你会学到

- 怎么写一个 filter 并注册到 ParaView
- 怎么在打开源时自动挂载 filter
- 怎么让 filter 级联在另一个 filter 之后

#### 前置条件

- 点云已包含 `distance_m`、`laser_id`、`azimuth` 数组（由解释器创建）。

#### 完整步骤

**步骤 1：写 `vtkRadialDistanceDenoise` 类**

```cpp
class LVFILTERSPROCESSING_EXPORT vtkRadialDistanceDenoise : public vtkPolyDataAlgorithm
{
public:
  static vtkRadialDistanceDenoise* New();
  vtkTypeMacro(vtkRadialDistanceDenoise, vtkPolyDataAlgorithm);

  vtkSetMacro(Level1Enabled, bool);
  vtkSetMacro(Level1Threshold, double);
  vtkSetMacro(Level2Enabled, bool);
  vtkSetMacro(Level2Threshold, double);
  vtkSetMacro(NumberOfLasers, int);
  vtkSetMacro(AzimuthBinSize, double);

protected:
  int RequestData(vtkInformation*, vtkInformationVector**, vtkInformationVector*) override;
  // ...
};
```

把文件放在 `LidarCore/Filters/Processing/vtkRadialDistanceDenoise.cxx` / `.h`。

> **为什么放 `LidarCore/Filters/Processing`？** 这是所有点云处理 filter 的家。和去噪、选择同类的算法都在这，便于统一注册和测试。

**步骤 2：在 `LidarCore/Filters/Processing/CMakeLists.txt` 登记类**

```cmake
set(classes
  ...
  vtkRadialDistanceDenoise
)
```

> **为什么要在 CMakeLists 登记？** `vtk_module_add_module` 靠这个列表知道要编译哪些类并导出符号。

**步骤 3：写 ServerManager XML 注册到 `filters` 组**

`LidarCore/Plugin/Filters/RadialDistanceDenoise.xml`：

```xml
<ServerManagerConfiguration>
  <ProxyGroup name="filters">
    <SourceProxy name="RadialDistanceDenoise" class="vtkRadialDistanceDenoise"
                 label="Senfoto008 Radial Distance Denoise">
      <InputProperty name="Input" port_index="0" command="SetInputConnection">
        <DataTypeDomain name="input_type"><DataType value="vtkPolyData"/></DataTypeDomain>
      </InputProperty>
      <OutputPort name="Output" index="0" />
      <IntVectorProperty name="Level 1 enabled" command="SetLevel1Enabled"
          number_of_elements="1" default_values="1">
        <BooleanDomain name="bool"/>
      </IntVectorProperty>
      <DoubleVectorProperty name="Level 1 threshold" command="SetLevel1Threshold"
          number_of_elements="1" default_values="10.24"/>
      <!-- Level 2、NumberOfLasers、AzimuthBinSize 类似 -->
    </SourceProxy>
  </ProxyGroup>
</ServerManagerConfiguration>
```

并在 `LidarCore/Plugin/CMakeLists.txt` 里把这份 XML 加进 XML 列表：

```cmake
lidarcoreplugin_add_module_xml(MODULE_TARGET LidarView::FiltersProcessing
  XML_MODULE_FILES Filters/RadialDistanceDenoise.xml)
```

> **为什么写 XML？** 没有 XML，ParaView 不认识 `RadialDistanceDenoise` 这个 proxy，你无法在 UI 里用 `createFilter("filters", "RadialDistanceDenoise", ...)` 创建它，属性面板也不会自动生成。

**步骤 4：在 `RequestData()` 里实现算法**

读取三个数组，分别跑 L2、L1，把要删除的点用 `vtkRemovePolyData` + `vtkCleanPolyData` 移除（不置 0，因为坐标在 `Points` 里，置 0 点仍会显示）。

**步骤 5：在 `lqOpenLidarReaction` 中自动挂载**

```cpp
bool IsRadialDenoiseAutoAttachEnabled()
{
  return QSettings().value("LidarView/AutoAttachRadialDenoise", true).toBool();
}

void AutoAttachRadialDenoise(pqPipelineSource* source)
{
  if (!source) return;

  // 如果 source 下游已经有 LaserSelection filter，级联在它后面
  pqPipelineSource* input = source;
  for (pqPipelineSource* consumer : source->getAllConsumers())
  {
    if (consumer && consumer->getProxy() &&
        std::strcmp(consumer->getProxy()->GetXMLName(), "LaserSelection") == 0)
    {
      input = consumer;
      break;
    }
  }

  pqObjectBuilder* builder = pqApplicationCore::instance()->getObjectBuilder();
  pqPipelineSource* filter = builder->createFilter("filters", "RadialDistanceDenoise", input);
  if (!filter) return;
  filter->getProxy()->UpdateVTKObjects();
  ::InitAndDisplaySource(filter, filter->getProxy(), true);
}
```

在 `openLidarPcap()` / `openLidarStream()` 中调用：

```cpp
::AutoAttachLaserSelection(source);
if (QString(prototype->GetXMLName()).startsWith("Senfoto008") &&
    IsRadialDenoiseAutoAttachEnabled())
{
  AutoAttachRadialDenoise(source);
}
```

> **为什么遍历 consumers 找 LaserSelection？** 我们希望「先按通道筛选，再做去噪」。如果直接挂在 source 上，去噪会把被禁用通道的点也算进去，浪费且可能误判。级联在 LaserSelection 之后，输入已经是筛选后的点云。

**验证方法**

1. 打开 Senfoto008 流，确认 filter 自动出现在 Pipeline Browser。
2. 在 Properties 面板调阈值，离群点实时减少。
3. 单独关掉某一级，验证开关有效。

---

### 4.3 案例三：增加自定义面板并接入点云显示管线（Laser Selection）

#### 目标

提供一个面板，让用户勾选要显示的激光线；被禁用的通道从点云中剔除，且选择可跨会话保存。

#### 你会学到

- 怎么写一个 Qt 对话框（Dialog）
- 怎么写一个 filter 在管线里过滤点云
- 怎么用 SM 属性把 UI 选择同步到服务端 filter
- 怎么把面板入口加进菜单

#### 完整步骤

**步骤 1：写 filter `vtkLaserSelectionFilter`**

```cpp
class LVFILTERSPROCESSING_EXPORT vtkLaserSelectionFilter : public vtkPolyDataAlgorithm
{
public:
  void SetLaserSelection(int index, int value);
  vtkIntArray* GetLaserSelection();
protected:
  int RequestData(vtkInformation*, vtkInformationVector**, vtkInformationVector*) override;
private:
  vtkNew<vtkIntArray> LaserSelection;  // 128 个 int，1=显示 0=过滤
};
```

`RequestData()` 中：读 `laser_id` 数组，按掩码保留点，拷贝保留点 + 点数据 + 重建顶点单元。无 `laser_id` 或没禁用任何通道时直接 `ShallowCopy` 透传。

> **为什么 filter 端用 `vtkIntArray` 掩码？** 掩码按 `laser_id` 索引，服务器 filter 不关心 UI 长什么样，只管「这个通道开不开」。

**步骤 2：写 `LaserSelection.xml` 注册到 `filters` 组**，关键属性：

```xml
<IntVectorProperty name="LaserSelection"
                   command="SetLaserSelection"
                   number_of_elements="1"
                   default_values="1"
                   repeat_command="1"
                   use_index="1" />
```

> **为什么 `repeat_command="1"` + `use_index="1"`？** 这表示一个属性对应「按 index 重复的多个值」，正好映射 `SetLaserSelection(int index, int value)`。客户端一次 `Set(mask, size)` 就能把整张掩码推过去。

**步骤 3：自动挂载（同 4.2 的 `AutoAttachLaserSelection`）**

```cpp
void AutoAttachLaserSelection(pqPipelineSource* source)
{
  if (!source) return;
  pqObjectBuilder* builder = pqApplicationCore::instance()->getObjectBuilder();
  pqPipelineSource* filter = builder->createFilter("filters", "LaserSelection", source);
  if (!filter) return;
  filter->getProxy()->UpdateVTKObjects();
  ::InitAndDisplaySource(filter, filter->getProxy(), true);
}
```

**步骤 4：写对话框 `lqLaserSelectionDialog`**

UI 文件 `lqLaserSelectionDialog.ui` 用 `QTableWidget` 列出通道、俯仰角，加勾选列和 Apply 按钮。

`lqLaserSelectionDialog.cxx` 核心：

```cpp
namespace
{
  vtkSMProxy* GetLaserSelectionFilterProxy(pqPipelineSource* src)
  {
    if (!src) return nullptr;
    for (pqPipelineSource* consumer : src->getAllConsumers())
    {
      if (consumer && consumer->getProxy() &&
          std::strcmp(consumer->getProxy()->GetXMLName(), "LaserSelection") == 0)
        return consumer->getProxy();
    }
    return nullptr;
  }
}

void lqLaserSelectionDialog::setLidarSource(pqPipelineSource* src)
{
  if (!IsLidarSource(src)) return;
  this->LidarSource = src;
  this->LaserSelectionFilterProxy = GetLaserSelectionFilterProxy(src);  // 复用自动挂载的 filter
  // 根据解释器 GetNumberOfChannels() 和校准表初始化表格
}

void lqLaserSelectionDialog::onApply()
{
  if (!this->LidarSource) return;

  // 重新解析：如果还没挂 filter 就创建一个
  this->LaserSelectionFilterProxy = GetLaserSelectionFilterProxy(this->LidarSource);
  if (!this->LaserSelectionFilterProxy)
  {
    pqPipelineSource* filter =
      pqApplicationCore::instance()->getObjectBuilder()->createFilter(
        "filters", "LaserSelection", this->LidarSource);
    if (filter)
    {
      this->LaserSelectionFilterProxy = filter->getProxy();
      this->LaserSelectionFilterProxy->UpdateVTKObjects();
    }
  }
  if (!this->LaserSelectionFilterProxy) return;

  // 构造掩码并写入 SM 属性
  QVector<int> mask = this->getLaserSelectionSelector();  // 128 个 int
  vtkSMPropertyHelper(this->LaserSelectionFilterProxy, "LaserSelection").Set(mask.data(), mask.size());
  this->LaserSelectionFilterProxy->UpdateVTKObjects();
  this->LaserSelectionFilterProxy->MarkModified(this->LaserSelectionFilterProxy);  // 触发重算

  if (pqView* view = pqActiveObjects::instance().activeView())
    view->render();
}
```

> **为什么先找已有的 filter 再考虑新建？** 自动挂载已经建了一个 `LaserSelection` filter。对话框如果又建一个，管线里会出现两个，点云被过滤两次且用户困惑。所以优先复用，只有用户从菜单手动打开、但源不是经 `lqOpenLidarReaction` 打开时才新建。

**步骤 5：写 Reaction 打开对话框**

```cpp
// lqLaserSelectionReaction.cxx
lqLaserSelectionReaction::lqLaserSelectionReaction(QAction* parent) : Superclass(parent)
{
  QObject::connect(parent, &QAction::triggered, this, &lqLaserSelectionReaction::showDialog);
}
void lqLaserSelectionReaction::showDialog()
{
  lqLaserSelectionDialog* dlg = new lqLaserSelectionDialog(lqLidarCoreManager::getMainWindow());
  dlg->setAttribute(Qt::WA_DeleteOnClose);
  dlg->show();  // 非模态：用户边勾选边看点云变化
}
```

**步骤 6：在 Tools 菜单注册入口**

```cpp
// Application/Client/LidarViewMainWindow.cxx
pqParaViewMenuBuilders::buildToolsMenu(*this->Internals->menuTools);
QAction* actionLaserSelection = new QAction(tr("Laser Selection"), this);
this->Internals->menuTools->addAction(actionLaserSelection);
new lqLaserSelectionReaction(actionLaserSelection);
```

> **为什么同时在 Tools 菜单和工具栏都放入口？** 菜单入口适合「不常用但要有」，工具栏入口适合「常用」。两者复用同一个 Reaction，逻辑只写一份。

**验证方法**

1. 打开 pcap，Tools → Laser Selection。
2. 取消勾选若干通道，Apply，确认对应激光线的点消失。
3. 勾选「Apply in future sessions」，重启后选择仍在。

---

## 5. 基于 UI 展示链路的功能扩展案例

### 5.1 案例四：给已有功能增加 toolbar 图标（Spreadsheet 视图）

#### 目标

Spreadsheet 视图本身已实现（封装在 `lqDockableSpreadSheetReaction`）。现在给它一个主工具栏按钮，点击后右侧 dock 显示/隐藏 Spreadsheet。

#### 你会学到

- 怎么在工具栏 UI 里声明一个 action
- 怎么把 action 绑定到已有 Reaction
- 怎么用 SVG 图标
- 怎么让按钮在界面模式下可见

#### 前置条件

- 已有 `lqDockableSpreadSheetReaction` 负责创建/显示 dock。
- 了解 3.2 的 UI 链路。

#### 完整步骤

**步骤 1：准备 SVG 图标**

把 `lqSpreadSheet.svg` 放进 `Application/Qt/Components/Resources/Icons/`。

**步骤 2：在 `lvComponents.qrc` 注册资源**

```xml
<qresource prefix="/lvComponents">
  <file>Icons/lqSpreadSheet.svg</file>
</qresource>
```

> **为什么用 qrc？** Qt 资源系统把图标编译进二进制，运行时用 `:/lvComponents/Icons/lqSpreadSheet.svg` 引用，不依赖外部文件。

**步骤 3：在 `lqMainControlsToolbar.ui` 声明 action**

```xml
<action name="actionSpreadSheet">
  <property name="icon">
    <iconset resource="../../../Components/Resources/lvComponents.qrc">
      <normaloff>:/lvComponents/Icons/lqSpreadSheet.svg</normaloff>
    </iconset>
  </property>
  <property name="text"><string>Spreadsheet</string></property>
  <property name="toolTip"><string>Show the point cloud data in a spreadsheet view</string></property>
</action>
```

并在 `<addaction>` 列表加入 `<addaction name="actionSpreadSheet"/>`。

> **为什么在 `.ui` 声明 action 而不是代码里 new？** `.ui` 让 UI 设计器和 `interface_modes_config.json` 能用 action 的 `objectName` 引用它；`filterToolbar()` 正是按 `objectName` 过滤显隐。

**步骤 4：在 `lqMainControlsToolbar.cxx` 绑定 Reaction**

```cpp
void lqMainControlsToolbar::constructor()
{
  Ui::lqMainControlsToolbar ui;
  ui.setupUi(this);

  // 其他 action ...
  QMainWindow* mainWindow = qobject_cast<QMainWindow*>(this->parent());
  if (!mainWindow)
    mainWindow = qobject_cast<QMainWindow*>(pqCoreUtilities::mainWidget());

  new lqDockableSpreadSheetReaction(ui.actionSpreadSheet, mainWindow);
}
```

**步骤 5：在 `interface_modes_config.json` 注册显隐**

在 `lidarViewer` / `pointCloudTool` / `advancedMode` 三个模式的 `mainControlsToolbar` 列表里都加上 `"actionSpreadSheet"`：

```json
"mainControlsToolbar": [
  "actionOpenPcap",
  "actionSavePcap",
  "actionOpenSensorStream",
  "actionUpdateConfiguration",
  "actionToggleAdvancedArrays",
  "actionLoadPalette",
  "actionDeleteAll",
  "actionLaserSelection",
  "actionSpreadSheet"
]
```

> **为什么三处都要加？** 每种界面模式独立维护自己的工具栏内容。漏掉某个模式，切换过去后按钮就消失了。

**Reaction 内部怎么呼起界面（供理解）**

```cpp
lqDockableSpreadSheetReaction::lqDockableSpreadSheetReaction(QAction* action, QMainWindow* mainWindow)
  : pqReaction(action), mainWindow(mainWindow)
{
  this->dock = new QDockWidget("SpreadSheet", this->mainWindow);
  this->dock->setObjectName("lqDockableSpreadSheetReactionDock");
  this->dock->hide();
  this->mainWindow->addDockWidget(Qt::RightDockWidgetArea, this->dock);
  connect(this->dock, &QDockWidget::visibilityChanged, this, &lqDockableSpreadSheetReaction::onDockVisibilityChanged);
}

void lqDockableSpreadSheetReaction::onTriggered()
{
  this->dock->setVisible(!this->dock->isVisible());  // 点按钮 = 切换 dock 显隐
}

void lqDockableSpreadSheetReaction::onDockVisibilityChanged()
{
  if (this->dock->isVisible())
  {
    this->createSpreadSheet();   // 创建 pqSpreadSheetView 并显示
    this->restorePersistanceState();
  }
  else
  {
    this->savePersistanceState();
    this->destroySpreadSheet();
  }
  this->parentAction()->setChecked(this->dock->isVisible());
}
```

**界面模式如何按 JSON 过滤（供理解）**

```cpp
// lqLidarViewManager.cxx
void updateToolBarsLayout()
{
  QList<QToolBar*> toolbars = this->mainWindow->findChildren<QToolBar*>(QString(), Qt::FindDirectChildrenOnly);
  nlohmann::json cfg = this->currentJsonMode["toolbars"];

  for (auto toolbar : toolbars)
    toolbar->setVisible(isObjectInList(toolbar, vectorFrom(cfg["show"])));

  for (auto it = cfg.begin(); it != cfg.end(); ++it)
  {
    if (it.key() == "show" || it.key() == "toolbarBreaks") continue;
    QToolBar* tb = findObjectByObjectName(toolbars, it.key());
    if (tb) filterToolbar(tb, vectorFrom(it.value()));  // 只显示列表里的 action
  }
}
```

**验证方法**

1. 构建后打开软件，主工具栏出现 Spreadsheet 图标。
2. 点击，右侧 dock 出现 Spreadsheet 视图，点云数据以表格呈现。
3. 切换界面模式，按钮仍在。

---

## 6. 总结

| 功能 | 所属链路 | 核心插入点 | 关键文件 |
|---|---|---|---|
| **新增雷达插件** | 数据流 | 新增 ParaView 插件，提供解释器 + reader/stream XML | `Plugins/MySensorPlugin/`、`CMakeLists.txt` |
| **新增去噪逻辑** | 数据流 | 作为 `filters` 组 filter 挂 source 下游 | `LidarCore/Filters/Processing/vtkRadialDistanceDenoise.*`、`.xml`、`lqOpenLidarReaction.cxx` |
| **新增自定义面板** | 数据流 | Qt 对话框改 filter 的 SM 属性，filter 管线里过滤点云 | `Qt/Components/lqLaserSelectionDialog.*`、`LidarCore/Filters/Processing/vtkLaserSelectionFilter.*` |
| **新增 toolbar 图标** | UI 展示 | 工具栏 UI 声明 action 并绑定已有 Reaction | `Application/Qt/ApplicationComponents/Resources/UI/lqMainControlsToolbar.ui`、`interface_modes_config.json` |

**记住两条判断：**

1. 改数据还是改界面？
2. 在链路哪个节点插入？

答案清楚后，要新增/修改的文件就自然浮现：数据类放 `LidarCore`/`Plugins`，界面类放 `Qt`/`Application`，最后别忘了在 `vtk.module` / `CMakeLists` / `*.xml` / `interface_modes_config.json` 里登记。
