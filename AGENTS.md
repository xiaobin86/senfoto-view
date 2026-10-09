# AGENTS.md

项目约定（长期记忆，所有会话必须遵守）。

## 提交后更新 Changelog（必做）

每次向 `develop` 提交并推送到远程后，**必须**把本批次（本次推送的所有提交）的修改
记录追加到项目根目录的 `CHANGELOG.md`：

- 每个批次一个小节，格式：`## YYYY-MM-DD 批次：<主题>`
- 按提交列出：`<commit-id> <一句话摘要>` + 必要的问题/方案说明
- 若批次含未提交但已验证的修复（随本批次一起提交的），一并说明
- 推送成功后再写 changelog，写完与下批改动一起提交（或单独 docs 提交）

## 新增文件头注释规范

凡是因为需求而**净新增（net-new）**的文件，必须在文件头部添加注释块，包含以下字段：

- **功能**：一句话描述本文件的功能 / 用途
- **作者**：acelan
- **新建时间**：YYYY-MM-DD（文件首次创建的日期）
- **修改时间**：YYYY-MM-DD（最近一次修改的日期；后续改动时同步更新）

### 适用范围

- 仅适用于本 fork **净新增**的文件（即上游 `Kitware/LidarView` 不存在的文件）。
- **不要**在仅被修改的上游文件上添加该头注释，例如：
  - `Qt/ApplicationComponents/lqOpenLidarReaction.cxx`（上游已有，只在其内新增了 `AutoAttachRadialDenoise` 等逻辑）
  - `lvComponents.qrc` 及各类 `CMakeLists.txt` 中对上游注册的改动
  - 任何标准 VTK/ParaView 上游文件
- 上游判定方法：用 `raw.githubusercontent.com/Kitware/LidarView/master/<path>` 取文件，返回 404 即净新增。

### 模板

C/C++（`.h` / `.cxx` / `.cpp` / `.cc`）：放在标准 VTK `/*===*/` 头之后；若文件无该头，则用 `//` 块置于文件最顶部。

```cpp
// ============================================================
// 功能：<一句话描述本文件功能>
// 作者：acelan
// 新建时间：YYYY-MM-DD
// 修改时间：YYYY-MM-DD
// ============================================================
```

XML（`.ui` / `.xml`）：置于文件最顶部。

```xml
<!--
  功能：<一句话描述本文件功能>
  作者：acelan
  新建时间：YYYY-MM-DD
  修改时间：YYYY-MM-DD
-->
```

CMake（`CMakeLists.txt`）：置于文件最顶部。

```cmake
# ============================================================
# 功能：<一句话描述本文件功能>
# 作者：acelan
# 新建时间：YYYY-MM-DD
# 修改时间：YYYY-MM-DD
# ============================================================
```

其它文本文件（如 `.md`、`.json`）若需标注，参照上述字段，使用对应注释语法；JSON 等不支持注释的格式不要强行插入。

## 界面布局持久化文件与验证前清理（必做）

Qt 界面默认布局（dock 位置/大小、工具栏、窗口尺寸）会被 **ParaView 的 `pqPersistentMainWindowStateBehavior`** 持久化（`LidarViewMainWindow.cxx` 中 `pqParaViewBehaviors` 默认启用）：

- **存储文件**：`~/.config/SenFoToView/SenFoToView.ini`（QSettings IniFormat，非 plist）
- **关键段**：`[MainWindow]`，含 `Layout=@ByteArray(...)`（dock/工具栏布局）和 `Size`（窗口尺寸）
- **行为**：app **退出时写入**、**启动时恢复**，会覆盖 `LidarViewMainWindow.ui`、`interface_modes_config.json` 里的新默认值

### 规则

凡修改了布局相关默认值（如 `LidarViewMainWindow.ui` 的 `dockWidgetArea`、`LidarViewMainWindow.cxx` 的 `tabifyDockWidget`/`resizeDocks`、`interface_modes_config.json`），**验证效果前必须先清理该 ini 的 `[MainWindow]` 段**，否则看到的是旧布局缓存，会误判改动无效。

### 清理方法（app 必须先退出）

```bash
python3 -c "
import configparser
p = '/Users/acelan/.config/SenFoToView/SenFoToView.ini'
c = configparser.RawConfigParser(strict=False, allow_unnamed_section=True)
c.optionxform = str
c.read(p, encoding='utf-8')
c.remove_section('MainWindow')
with open(p, 'w', encoding='utf-8') as f:
    c.write(f, space_around_delimiters=False)
print('ini 已清理')
"
```

### 验证前构建注意

本项目是 superbuild，改完源码需两步（只跑第一步会用旧副本）：

```bash
ninja -C ../build/superbuild/lidarview/build   # 内层编译 → bin/SenFoToView.app
ninja -C ../build superbuild/lidarview         # install 步骤 → build/install/Applications/SenFoToView.app（日常启动的副本）
```

### 编译缓存（ccache）

内层构建已配置 ccache launcher（`CMAKE_C/CXX_COMPILER_LAUNCHER=ccache`，缓存上限 10G）。
若 superbuild 重新 configure 导致缓存失效，重跑：

```bash
cmake -S . -B ../build/superbuild/lidarview/build \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
```
## 项目长期记忆（架构 / 扩展方式 / 官方案例）

> 来源：`../MEMORY.md`（工作区根目录的长期记忆，合并入本文件统一管理）。
> 工作区：`/mnt/d/work/senfoto-view`；LidarView 源码：`lidarview/`
> 详细架构/扩展机制见 `lidarview/docs/lidarview-architecture.md`；编译/重编/启动见 `lidarview/docs/lidarview-development-guide.md`。
> 本文只记**扩展的主要方式 + 对应官方案例**，供快速检索。

### 一句话定位
LidarView 是**基于 ParaView 的自定义客户端**（ParaView-based custom application）。
- 管线能力（算法/读卡器/源/视图/属性面板）= **ParaView 插件**：`paraview.plugin` + ServerManager XML + VTK 模块(`vtk.module`)。
- 应用外壳 = `paraview_client_add`（自动生成 `main()`），主窗口 `Application/Client/LidarViewMainWindow.*` 继承 ParaView 窗口并接 `pqParaViewBehaviors` / `pqInterfaceTracker`。
- 任何新功能本质都是**写一个插件**，再把名字加进根 `CMakeLists.txt` 的 `lidarview_default_plugins`。

### 主要扩展方式（做什么 → 学哪个案例 → 关键宏/文件）

| # | 想做的事 | 官方案例（必读） | 关键做法 |
|---|---|---|---|
| 1 | 新增 C++ 处理 Filter | `Examples/Plugins/ProcessingSamplePlugin` | `vtk.module` + `vtkXxx(:vtkPolyDataAlgorithm)`，`.cxx` 用 `vtkStandardNewMacro` + `RequestData`；`<X>.xml` 里 `<SourceProxy>`；`paraview_add_plugin(MODULES ...)` |
| 2 | 新增 dock 面板 / 工具栏 | `Examples/Plugins/CustomDockWidget` | `paraview_plugin_add_dock_window(CLASS_NAME ... DOCK_AREA ...)`、`paraview_plugin_add_toolbar(...)`；dock 用 `pqApplicationCore::instance()->registerManager("NAME",this)` 注册，toolbar 用 `manager("NAME")` 取回联动 |
| 3 | **新增雷达协议（最常见、最核心）** | `Examples/Plugins/TimeBasedLidarInterpreter`（看齐 `Plugins/Senfoto008Plugin`） | 继承 `vtkLidarPacketInterpreter`，实现 `Initialize / IsLidarPacket / PreProcessPacket / ProcessPacket / CreateNewEmptyFrame`；`vtk_module_add_module(... FORCE_STATIC)`；`LidarReader.xml`/`LidarStream.xml` 用 `<LidarReaderProxy>` 继承 `CommonLidarReader`/`CommonLidarStream`；在 `lidar_interpreters` ProxyGroup 注册解释器名（让 "Open Stream / Open PCAP" 对话框能选） |
| 4 | 新增自定义（交互式）属性控件 | `Examples/Plugins/AverageSelectedPointsPlugin` | `paraview_plugin_add_property_widget(KIND GROUP_WIDGET TYPE "MyXxx" CLASS_NAME lqXxx)`；XML 里 `<PropertyGroup panel_widget="MyXxx">`；本例还示了 `implicit_functions` 代理组（基于 `vtkPVBox`） |
| 5 | Filter 读外部文件参数 | `Examples/Plugins/ThresholdFromFilePlugin` | `StringVectorProperty` + `<FileListDomain name="files"/>` |
| 6 | 改菜单/界面/工具栏 | （无独立案例，看外壳代码） | 菜单白名单 `Application/Client/lvFilters.xml` & `lvSources.xml`；主窗口 `LidarViewMainWindow.ui/.cxx`；菜单构造 `Application/Qt/ApplicationComponents/lqLidarViewMenuBuilders.*`；资源 `*.qrc` + `Resources/Icons` |

### 注册与生效（所有插件通用）
1. 根 `lidarview/CMakeLists.txt` 的 `lidarview_default_plugins` 加入插件名（**LidarCorePlugin 必须排第一**，解释器插件依赖它的 XML）。
2. 编译：`cd build/superbuild/lidarview/build && ninja <PluginName> && ninja install`
3. **必须 `ninja install` 并完全重启 `LidarView`**，否则 UI 看不到（运行时读 `build/install/` 下的插件）。
4. 调试：`ctest -R <TestName> -V`；插件缺失先查 `build/install/lib/lidarview/plugins/lidarview.plugins.xml`。

### 命名约定
- `vtkXxx` = VTK 算法/源；`lv` 前缀 = 库名（`lvFiltersGeneral`）。
- `lqXxx` = LidarView 的 Qt/ParaView 客户端类（`lqLidarViewManager`、`lqCustomDockWidget`…）。
- `LidarReaderProxy` / `LidarStream` = LidarView 对 ParaView 代理模型的雷达读流扩展。
- XML `name` 必须与 `lvFilters.xml`/`lvSources.xml` 里引用的 `name` 一致，才会出现在菜单。

### ServerManager XML 关键点
- Filter：`<ProxyGroup name="filters"><SourceProxy class="vtkXxx" name="Xxx" label="...">` + `<InputProperty>`/`<IntVectorProperty>`/`<DoubleVectorProperty>`/`<ProxyProperty>` + `<PropertyGroup label=...>` + `<Hints><ShowInMenu icon=.../>`。
- Reader：用 `<LidarReaderProxy>`（可多 `OutputPort`），继承 `CommonLidarReader`（`base_proxygroup/base_proxyname`）。
- 顶层 plugin 可直接 `paraview_add_plugin(... SERVER_MANAGER_XML <file>)`；或 per-module 用 `paraview_add_server_manager_xmls(MODULE <vtk target> XMLS <files>)`（`LidarCore/Plugin/CMakeLists.txt` 的 `lidarcoreplugin_add_module_xml` 宏即此模式）。
