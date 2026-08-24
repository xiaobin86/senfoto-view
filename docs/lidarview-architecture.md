# LidarView 文件架构与二次开发指南（架构篇）

> 配套文档：`docs/lidarview-development-guide.md`（编译/重编/启动速查）。
> 本文聚焦**架构**：LidarView 如何基于 ParaView 组织代码、如何扩展 ParaView、如何改界面、如何加功能。
> 所有路径相对 `lidarview/`（源码根）。

---

## 1. 一句话定位

LidarView 是一个 **基于 ParaView 的自定义客户端应用（ParaView-based custom application）**。它的代码分为两层：

1. **管线能力层（ParaView Plugins）**：所有算法、读卡器、数据源、视图、属性面板，都以 **ParaView 插件** 形式存在（`paraview.plugin` + ServerManager XML + VTK 模块）。
2. **应用外壳层（ParaView Client）**：一个 Qt 主窗口应用，由 ParaView 的 `paraview_client_add` 宏自动生成 `main()`、splash、插件加载逻辑，并继承 ParaView 的主窗口与行为框架。

理解这一点是后续所有修改的前提：**加算法 → 写插件；改界面/菜单 → 改 Client 或插件里的 UI 宏**。

---

## 2. 仓库与目录结构

```
lidarview/                          # 源码根
├── CMakeLists.txt                  # 顶层：扫描并构建所有 VTK 模块 + ParaView 插件
├── CMake/                          # 自定义 CMake 宏（LidarViewOptions / FindLidarViewDependencies / ...）
├── LidarCore/                      # 核心 C++ 库（纯 VTK 模块，无 Qt 依赖的部分）
│   ├── Common/  IO/  Filters/  Sources/  Remoting/  Utilities/
│   └── Plugin/                     # LidarCorePlugin —— 把 LidarCore 暴露给 ParaView 的"枢纽插件"
├── Qt/                             # 共享 Qt UI 基础模块（lq 前缀类）
│   ├── Core/  Components/  ApplicationComponents/
├── Plugins/                        # 设备/厂商插件（Velodyne、Hesai、Robosense、Livox、
│                                   #   Leishen、LakiBeam、Senfoto008、Ouster、DatasetIO、MotionDetector）
├── Examples/Plugins/               # 官方参考样例（必读！）
│   ├── CustomDockWidget/           # 如何加一个 dock 面板 + 工具栏
│   ├── ProcessingSamplePlugin/     # 如何加一个 C++ 处理 Filter
│   ├── *LidarInterpreter/          # 如何加一个新雷达协议解释器
│   └── AverageSelectedPointsPlugin/  ThresholdFromFilePlugin/
├── Application/                    # 应用外壳（真正的可执行文件 LidarView）
│   ├── Client/                     # LidarViewMainWindow.ui/.cxx/.h + lvSources.xml/lvFilters.xml
│   ├── Qt/ApplicationComponents/   # LidarView 专属 UI（菜单构造、工具栏、Manager）
│   ├── Qt/Components/              # 通用 UI（关于框等）
│   ├── CommandLineExecutables/     # lvserver / lvpython / lvbatch
│   └── Wrapping/Python/            # Python 辅助包 `lidarview`（simple.py 等）
├── Wrapping/Python/                # C++ 的 Python 封装入口（lidarview.modules）
├── Testing/                        # 测试框架（BUILD_TESTING=ON 时构建）
├── Utilities/                      # Sphinx / Doxygen 文档生成
└── docs/                           # 本文档所在
```

构建产物（由 `lidarview-superbuild` 生成，不在源码树内）：

```
senfoto-view/
├── lidarview-superbuild/           # 超级构建：编译 ParaView/VTK、Boost、PCAP++ 等依赖
├── build/superbuild/lidarview/build # LidarView 本体的 ninja 构建目录
└── build/install/                  # 最终运行目录（该程序实际从这里加载插件）
```

**模块划分准则**：
- `LidarCore/` 与 `Qt/` 是**库**，被插件和 Client 链接使用，自身不直接产生可执行文件。
- `Plugins/` 是**功能插件**（每个雷达一个独立插件），可独立编译（`ninja <PluginName>`）。
- `Application/` 是**外壳**，把插件与主窗口拼成 `LidarView` 可执行文件。

---

## 3. 两大构建系统：VTK 模块 + ParaView 插件

LidarView 同时使用了 VTK 和 ParaView 两套现代构建系统。

### 3.1 VTK 模块（`vtk.module`）

每个算法/库目录放一个 `vtk.module`，声明模块名、导出库名、依赖：

```cmake
# LidarCore/Filters/General/vtk.module
NAME
  LidarView::FiltersGeneral
LIBRARY_NAME
  lvFiltersGeneral
DEPENDS
  VTK::CommonCore
  VTK::CommonDataModel
PRIVATE_DEPENDS
  LidarView::CommonCore
  VTK::FiltersGeneral
TEST_DEPENDS
  LidarView::TestingCore
```

根 `CMakeLists.txt` 通过 `vtk_module_find_modules` → `vtk_module_scan` → `vtk_module_build` 自动收集、编译、并生成 Python 封装（`ENABLE_WRAPPING ON`）。

### 3.2 ParaView 插件（`paraview.plugin`）

每个插件目录放一个 `paraview.plugin`，声明插件名与所需模块：

```cmake
# LidarCore/Plugin/paraview.plugin
NAME
  LidarCorePlugin
DESCRIPTION
  Readers, writer and filters from LidarCore (required for LidarView)
REQUIRES_MODULES
  ParaView::RemotingServerManager
  LidarView::UtilitiesUsedModules
```

### 3.3 根 CMake 如何串起来

`lidarview/CMakeLists.txt` 的核心流程：

```cmake
# 1) 声明默认启用的插件（顺序敏感：LidarCorePlugin 必须第一，
#    因为各解释器插件依赖它的 XML 先被加载）
set(lidarview_default_plugins
  LidarCorePlugin  VelodynePlugin  LeishenPlugin  LakiBeamPlugin
  Senfoto008Plugin  LivoxPlugin  RobosensePlugin  HesaiPlugin  OusterPlugin
  MotionDetectorToolbox)

# 2) 扫描插件目录
set(lidarview_plugin_directories
  "${CMAKE_CURRENT_SOURCE_DIR}/LidarCore/Plugin/"
  "${CMAKE_CURRENT_SOURCE_DIR}/Plugins/")
paraview_plugin_find_plugins(lidarview_plugin_files ${lidarview_plugin_directories})
paraview_plugin_scan(PLUGIN_FILES ${lidarview_plugin_files}
  PROVIDES_PLUGINS lidarview_plugins REQUIRES_MODULES lidarview_plugin_required_modules)

# 3) 扫描 VTK 模块（LidarCore + Qt）
vtk_module_find_modules(...)
vtk_module_scan(...)

# 4) 构建模块
vtk_module_build(MODULES ${lidarview_modules} ...)

# 5) 构建并安装插件（生成 lidarview.plugins.xml）
paraview_plugin_build(PLUGINS ${lidarview_plugins}
  PLUGINS_FILE_NAME "lidarview.plugins.xml"
  AUTOLOAD ${autoload_plugins})

# 6) 构建外壳
add_subdirectory(Application)
```

**新增插件的关键一步**：把你插件的名字加进 `lidarview_default_plugins`（根 `CMakeLists.txt`），否则不会被扫描/构建/自动加载。

---

## 4. 如何扩展 ParaView（核心机制）

把"一段 C++ 算法"变成"ParaView 管线里可用的一个节点"，需要四步走：

```
C++ 算法  ──① vtk.module──►  VTK 模块
    │
    └─② ServerManager XML ─►  ParaView 代理(Proxy)定义（属性/端口/文档）
            │
            └─③ paraview_add_plugin ─►  ParaView 插件（.so/.dll）
                    │
                    └─④ 注册到 lidarview_default_plugins ─► 被 LidarView 自动加载
```

### 4.1 ① 写成 VTK 模块

新建目录，放 `vtk.module` + `vtkXxx.h/.cxx`。算法类继承 VTK 算法基类：

```cpp
// 参考 Examples/Plugins/ProcessingSamplePlugin/ProcessingSample/vtkProcessingSample.h
#include <vtkPolyDataAlgorithm.h>
class PROCESSINGSAMPLE_EXPORT vtkProcessingSample : public vtkPolyDataAlgorithm
{
public:
  vtkTypeMacro(vtkProcessingSample, vtkPolyDataAlgorithm)
  static vtkProcessingSample* New();            // 必须
  int RequestData(vtkInformation*,             // 真正实现在 RequestData
    vtkInformationVector**, vtkInformationVector*) override;
protected:
  vtkProcessingSample(); ~vtkProcessingSample();
};
```

`.cxx` 里用 `vtkStandardNewMacro(vtkProcessingSample)` 注册工厂。

### 4.2 ② 写 ServerManager XML（暴露给管线）

XML 是 ParaView 理解你这个算法的"契约"——它声明：类名是什么、有哪些输入/输出端口、有哪些可调参数、在菜单里显示什么名字。

**Filter 示例**（`LidarCore/Plugin/Filters/ClipPoints.xml`）：

```xml
<ServerManagerConfiguration>
  <ProxyGroup name="filters">
    <SourceProxy class="vtkExtractPolyDataGeometry"
                 name="ClipPoints"            <!-- 管线里引用的名字 -->
                 label="Clip Points">         <!-- 菜单里显示的名字 -->
      <Documentation short_help="..."
                      long_help="...">...</Documentation>

      <InputProperty name="Input" command="SetInputConnection">
        <ProxyGroupDomain name="groups">
          <Group name="sources"/><Group name="filters"/>
        </ProxyGroupDomain>
        <DataTypeDomain name="input_type"><DataType value="vtkDataSet"/></DataTypeDomain>
      </InputProperty>

      <IntVectorProperty name="Invert" command="SetExtractInside"
                         default_values="0" number_of_elements="1">
        <BooleanDomain name="bool"/>
      </IntVectorProperty>

      <PropertyGroup label="Reader Options">   <!-- 属性面板里的分组 -->
        <Property name="Input"/>
        <Property name="Invert"/>
      </PropertyGroup>

      <Hints><ShowInMenu icon=":/pqWidgets/Icons/pqClip.svg"/></Hints>
    </SourceProxy>
  </ProxyGroup>
</ServerManagerConfiguration>
```

**Reader 示例**（`LidarCore/Plugin/IO/Interpreters/LidarReader.xml`）：
雷达读取器使用 ParaView 的自定义代理类型 `<LidarReaderProxy>`（LidarView 扩展的），可声明多个 `OutputPort`、文件属性、时间步属性等。设备插件（如 `Senfoto008Plugin`）的 `*.LidarReader.xml` / `*.LidarStream.xml` 都遵循此模式。

**把 XML 注册进插件**（在插件的 `CMakeLists.txt` 里）：

```cmake
paraview_add_server_manager_xmls(
  MODULE <vtk模块target>        # 例如 LidarView::FiltersGeneral
  XMLS  Filters/ClipPoints.xml)
```

`LidarCore/Plugin/CMakeLists.txt` 用宏 `lidarcoreplugin_add_module_xml(MODULE_TARGET ... XML_MODULE_FILES ...)` 批量做了这件事（把每个 Filter/Reader 的 XML 挂到对应模块上）。**你照这个宏加一行即可**，不用手写 `paraview_add_server_manager_xmls`。

### 4.3 ③ 打包成 ParaView 插件

```cmake
# 参考 Examples/Plugins/ProcessingSamplePlugin/CMakeLists.txt
paraview_add_plugin(ProcessingSamplePlugin
  REQUIRED_ON_CLIENT
  REQUIRED_ON_SERVER
  VERSION "1.0"
  MODULES ProcessingSample                       # 你 4.1 的 VTK 模块
  MODULE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/ProcessingSample/vtk.module")
```

含 UI 的插件（`LidarCore/Plugin` 是范本）还会传 `UI_FILES` / `UI_RESOURCES`(.qrc) / `UI_INTERFACES`（由下面 5.x 的 `paraview_plugin_add_*` 宏生成）。

### 4.4 ④ 注册并构建

1. 在根 `CMakeLists.txt` 的 `lidarview_default_plugins` 加入插件名。
2. `cd build/superbuild/lidarview/build && ninja <PluginName> && ninja install`
3. 重启 `LidarView`。

（详见 `docs/lidarview-development-guide.md` 第 3–5 节。）

### 4.5 自定义 ParaView 客户端外壳

`Application/Client/CMakeLists.txt` 用 `paraview_client_add` 生成 `main()` 与窗口外壳：

```cmake
paraview_client_add(
  NAME "LidarView"
  MAIN_WINDOW_CLASS LidarViewMainWindow      # 你的主窗口类
  MAIN_WINDOW_INCLUDE LidarViewMainWindow.h
  PLUGINS_TARGETS ParaView::paraview_plugins LidarView::lidarview_plugins
  REQUIRED_PLUGINS LidarCorePlugin
  APPLICATION_XMLS
    ${CMAKE_CURRENT_SOURCE_DIR}/lvSources.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/lvFilters.xml
  SPLASH_IMAGE .../Splash.png
  ...
)
```

这意味着 **`main()` 由 ParaView 自动生成**，你只需提供 `LidarViewMainWindow`。该类的职责见第 5 节。

---

## 5. 如何修改界面

界面相关文件分散在 **Client（外壳）** 和 **插件（可扩展 UI）** 两处。

### 5.1 主窗口布局 —— `Application/Client/LidarViewMainWindow.ui`

Qt Designer 文件，定义菜单栏、各 dock（pipeline browser、properties、python shell、lidar player 等）、中央视图区。对应代码：

- `LidarViewMainWindow.cxx`：在构造函数里 `setupUi(this)` 后：
  - 通过 `tabifyDockWidget` 组织 dock 标签页；
  - 用 `pqApplicationCore::instance()->registerManager("PYTHON_SHELL_PANEL", ...)` 把面板注册给 ParaView 框架，方便其它模块取用；
  - 用 `pqParaViewBehaviors`（含 `setEnableXxxBehavior` 开关）启用/禁用 ParaView 内置行为；
  - 用 `pqInterfaceTracker::addInterface(new lqXxxImplementation(pgm))` 注入自定义接口实现（如替换默认 view frame actions、最近使用的 pcap 加载器、PCAP 录制器）；
  - 菜单用 `lqLidarViewMenuBuilders::buildXxxMenu` 与 ParaView 自带的 `pqParaViewMenuBuilders::buildXxxMenu` 混合构建。

> **修改主窗口**优先改 `.ui`；若需加逻辑（按钮槽、面板联动），改 `LidarViewMainWindow.cxx/.h`，必要时把新类通过 `pqInterfaceTracker` 注册。

### 5.2 菜单里出现哪些 Filter / Source —— `lvFilters.xml` / `lvSources.xml`

这两个 XML 控制 ParaView 菜单/工具栏里**展示哪些代理、分哪些类、用哪个图标**：

```xml
<!-- lvFilters.xml -->
<ParaViewFilters>
  <Category name="Common" menu_label="&Common" preserve_order="1">
    <Proxy group="filters" name="ClipPoints" />
    <Proxy group="filters" name="ThresholdPoints" />
  </Category>
  ...
  <Proxy group="filters" name="ClipPoints" icon=":/pqWidgets/Icons/pqClip.svg" />
</ParaViewFilters>
```

**想让你新写的 Filter 出现在菜单**：在 `lvFilters.xml` 的 `<Category>` 和底部代理列表里各加一行 `<Proxy group="filters" name="你的ProxyName"/>`（`name` 必须与 ServerManager XML 里的 `name` 一致）。

### 5.3 菜单 / 工具栏构造 —— `lqLidarViewMenuBuilders`

`Application/Qt/ApplicationComponents/lqLidarViewMenuBuilders.cxx` 负责 File/Edit/Help 菜单与工具栏；对应的 `.ui` 片段在 `Application/Qt/ApplicationComponents/Resources/UI/`（如 `lqMainControlsToolbar.ui`、`lqFileMenuBuilder.ui`）。改这些 `.ui` 与 `.cxx` 即可调整 LidarView 专属菜单和工具栏。

### 5.4 通过插件加 Dock 面板 / 工具栏（推荐做法）

不碰主窗口代码，用 ParaView 提供的 CMake 宏，在**插件内**声明 UI 扩展：

```cmake
# 参考 Examples/Plugins/CustomDockWidget/CMakeLists.txt
paraview_plugin_add_dock_window(
  CLASS_NAME lqCustomDockWidget
  DOCK_AREA Right
  INTERFACES dock_interfaces
  SOURCES dock_sources)

paraview_plugin_add_toolbar(
  CLASS_NAME lqDockToolBar
  INTERFACES toolbar_interfaces
  SOURCES toolbar_sources)

paraview_add_plugin(CustomDockWidget
  REQUIRED_ON_CLIENT VERSION "1.0"
  UI_FILES Resources/UI/CustomDockWidget.ui
  UI_RESOURCES Resources/CustomDockResources.qrc
  UI_INTERFACES ${interfaces}
  SOURCES ${sources}
  MODULES CustomDockWidget::DummyExternalAPI
  MODULE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/DummyExternalAPI/vtk.module")
```

运行时：
- `lqCustomDockWidget.cxx` 在 `constructor()` 里 `setupUi`、连接按钮信号，并用 `pqApplicationCore::instance()->registerManager("CUSTOM_PANEL", this)` 把自己注册出去（这样工具栏能找到它）。
- `lqDockToolBar.cxx` 用 `manager("CUSTOM_PANEL")` 取回该 dock，把工具栏按钮与其显隐联动。

> 这是**添加新 UI 面板的范本**：dock 与 toolbar 解耦，通过 `registerManager`/`manager` 通信。

### 5.5 自定义属性面板控件 / 装饰器

`LidarCore/Plugin/CMakeLists.txt` 示范了在插件里注入 ParaView 属性面板组件：

```cmake
paraview_plugin_add_property_widget(
  KIND WIDGET  TYPE "calibration_file_widget"
  CLASS_NAME lqCalibrationFilePropertyWidget
  INTERFACES ... SOURCES ...)

paraview_plugin_add_property_widget(
  KIND WIDGET_DECORATOR  TYPE "ConditionalDefaultValueDecorator"
  CLASS_NAME lqConditionalDefaultPropertyWidgetDecorator ...)

paraview_plugin_add_auto_start(           # 插件加载时自动执行的逻辑
  CLASS_NAME lqLidarCorePluginManager
  STARTUP onStartup  SHUTDOWN onShutdown ...)
```

- `KIND WIDGET`：给某个属性类型替换/新增编辑控件（`TYPE` 在 ServerManager XML 的 `<Property>` 上通过 `panel_widget="..."` 引用）。
- `KIND WIDGET_DECORATOR`：属性可见性/启用态的动态装饰器。
- `paraview_plugin_add_auto_start`：插件启动/关闭钩子（如初始化默认设置、注册管理器）。

### 5.6 界面模式（可切换布局）

`lqLidarViewManager`（`Application/Qt/ApplicationComponents/`）+ `interface_modes_config.json`（同目录）实现多套 UI 布局切换（`lqChangeInterfaceReaction` 触发）。要加一套新布局：在 json 里描述各面板显隐/位置，并在 Manager 里加对应恢复逻辑。

### 5.7 资源 / 图标 / 品牌

- **qrc 资源文件**：`Application/Client/Resources/lvResources.qrc`、`Qt/ApplicationComponents/Resources/lqResources.qrc`、`LidarCore/Plugin/Resources/lqFilters.qrc` 等。图标放 `Resources/Icons/`、图片放 `Resources/Images/`。
- 在 XML/C++ 里用 `:/lvResources/Icons/xxx.svg` 这样的资源路径引用。
- **品牌元素**：Splash（`Splash.png`，由 `paraview_client_add(SPLASH_IMAGE ...)` 指定）、logo（`logo.ico/.icns`）、关于框（`lqAboutDialog.ui` + `lqAboutDialog.cxx`）。

---

## 6. 如何引入功能（按类型速查）

| 功能类型 | 放哪 | 关键文件 | 注册动作 |
|---|---|---|---|
| **新 Filter** | `LidarCore/Filters/<X>/` 或 插件子目录 | `vtk.module` + `vtkXxx.h/.cxx` + `<X>.xml` | 在插件 CMake 调 `paraview_add_server_manager_xmls`；菜单里加 `lvFilters.xml` |
| **新 Reader** | `LidarCore/IO/...` 或 `Plugins/<Device>/` | `vtkXxxReader` + `<X>Reader.xml`（`<LidarReaderProxy>`） | 同上；设备读卡器通常随厂商插件提供 |
| **新 Source** | `LidarCore/Sources/` | `vtk.module` + `vtkXxxSource` + `<X>Source.xml` | 加进 `lvSources.xml` |
| **新雷达协议** | `Plugins/<Device>/<Device>PacketInterpreters/` | `vtkXxxPacketInterpreter` + `vtk.module` + `Reader/Stream.xml` | 在 `paraview.plugin`/`CMakeLists.txt` 声明；加进 `lidarview_default_plugins` |
| **Python 辅助** | `Application/Wrapping/Python/` 或 `Wrapping/Python` | `*.py` + `CMakeLists.txt`(python_module_install) | 安装为 `lidarview` 包；Python Shell 自动 `from lidarview.simple import *` |
| **新 UI 面板** | 任意插件 | `.ui` + `paraview_plugin_add_dock_window` | 见 5.4 |
| **新属性控件** | 任意插件 | `.cxx/.h` + `paraview_plugin_add_property_widget` | 见 5.5 |

### 6.1 新增 Filter 完整清单（以 ProcessingSample 为模板）

```
MyFilterPlugin/
├── paraview.plugin          # NAME MyFilterPlugin
├── CMakeLists.txt           # paraview_add_plugin(... MODULES MyFilter MODULE_FILES MyFilter/vtk.module)
└── MyFilter/
    ├── vtk.module           # NAME LidarView::MyFilter, DEPENDS VTK::...
    ├── vtkMyFilter.h        # 继承 vtkPolyDataAlgorithm, vtkTypeMacro, static New()
    ├── vtkMyFilter.cxx      # vtkStandardNewMacro + RequestData 实现
    └── MyFilter.xml         # <ServerManagerConfiguration><ProxyGroup name="filters"><SourceProxy ...>
```

然后：
1. 插件 CMake 里 `paraview_add_server_manager_xmls(MODULE LidarView::MyFilter XMLS MyFilter.xml)`；
2. 根 `CMakeLists.txt` 的 `lidarview_default_plugins` 加 `MyFilterPlugin`；
3. `lvFilters.xml` 加 `<Proxy group="filters" name="MyFilter"/>`（如需进菜单）；
4. `ninja MyFilterPlugin && ninja install`，重启。

### 6.2 Python 功能

- **封装 C++**：根 CMake 的 `vtk_module_wrap_python` 把 `lidarview.modules` 暴露给 Python（`import lidarview.modules`）。**不要**用 `GetClientSideObject()` 直接拿 vtkObject。
- **纯 Python 脚本**：`Application/Wrapping/Python/`（如 `simple.py`、`planefit.py`）经 `python_module_install(NAME lidarview ...)` 安装为 `lidarview` 包，并在 Python Shell 的 preamble 里自动 `from lidarview.simple import *`，让用户可直接 `lidarview.simple.xxx()` 操作管线。

---

## 7. 测试

- 测试目录：`Testing/`（框架）与各插件 `Testing/`（如 `Plugins/Senfoto008Plugin/Testing/TestSenfoto008PacketInterpreter.cxx`）。
- 开关：`BUILD_TESTING=ON`（默认关；在 build 目录 `cmake -DBUILD_TESTING=ON .`）。
- 运行：`ctest -R <TestName> -V`。
- 编译单个测试：`ninja <TestName>`。
- 还有 Python/UI XML 测试（在 `Wrapping/Python` 与 `Testing` 下）。

---

## 8. 编译 / 安装速查（详见既有开发指南）

```bash
cd /mnt/d/work/senfoto-view/build/superbuild/lidarview/build
ninja <PluginName>     # 只编某个插件
ninja                  # 全量
ninja install          # 必须！否则 UI 看不到改动（运行时读 build/install 下的插件）
ctest -R <TestName> -V
/mnt/d/work/senfoto-view/build/install/bin/LidarView   # 启动
```

排查：插件未出现 → 检查是否 `ninja install`、是否在 `lidarview.plugins.xml` 中、是否完全重启、启动日志有无加载错误。

---

## 9. 命名约定小抄

- `vtkXxx`：VTK 算法/源（C++）；`lv` 前缀的库名（`lvFiltersGeneral`）。
- `lqXxx`：LidarView 的 Qt/ParaView 客户端类（如 `lqLidarViewManager`、`lqCustomDockWidget`）。
- `paraview_*` / `pq*` / `vtkSM*`：ParaView 框架原类，LidarView 通过继承或接口注入来扩展。
- `LidarReaderProxy` / `LidarStream`：LidarView 对 ParaView 代理模型的雷达读流扩展。

---

## 10. 最佳入手样例（按你想做的事挑一个读）

| 目标 | 直接读 |
|---|---|
| 加一个 C++ 处理算法 | `Examples/Plugins/ProcessingSamplePlugin/` |
| 加一个 dock 面板 + 工具栏 | `Examples/Plugins/CustomDockWidget/` |
| 加一个新雷达协议/读卡器 | `Plugins/Senfoto008Plugin/` + `Examples/Plugins/RotativeLidarInterpreter/` |
| 改主窗口/菜单/工具栏 | `Application/Client/LidarViewMainWindow.*` + `Application/Qt/ApplicationComponents/lqLidarViewMenuBuilders.*` |
| 改"菜单里有哪些 Filter" | `Application/Client/lvFilters.xml` |
| 理解插件如何挂 XML/属性控件 | `LidarCore/Plugin/CMakeLists.txt` |
