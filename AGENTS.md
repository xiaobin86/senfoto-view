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

## 汉化（i18n）翻译规范（必守）

> 完整方案/规范/避坑见 **`docs/senfotoview-i18n-plan.md`**；架构见 `docs/senfotoview-i18n-zh-cn.md`；
> 工作目录 `lidarview/i18n/`。改动 `.ts` 后 `./i18n/build-zh-cn.sh` 重投 `.qm`，**无需重编译**。

1. **专有名词保留英文**：`ParaView`、`LidarView`、`SenFoToView`、`Kitware`、`VTK`、`Qt`、`Python`
   一律原样，不翻译、不音译。（曾误译 `ParaView→帕拉维尤/参数`、`VTK→维塔克`。）
2. **过滤器/读卡器/插件显示名保留英文**：`<SourceProxy>` 的 `label`（`filters`/`sources` 组）与插件名保持英文。
3. **快捷键/功能键不翻译**：`Ctrl+/Alt+/Shift+/Meta+*`、`F1-F12` 译文必须等于源串。
4. **占位符/HTML/加速键保护**：保留 `%1/%L1/{…}` 数量与顺序；HTML 只译文本节点；`&X` 加速键保留。
5. **单字母/轴/按键标签保持原样**（`x/X/P/N`、`-Z` 等），勿经词表翻译。
6. **术语统一**：见 `docs/senfotoview-i18n-plan.md` §6。
7. **机器翻译必须用带专有名词遮蔽的 `i18n/translate_zh.py`**；改完跑 `python3 i18n/qa_zh.py 'i18n/zh_CN/*.ts'`
   复核（其 `PROPER_NOUN`/`SHORTCUT`/`SUSPECT` 即守则检测）。
8. **新增功能必须同步补中文翻译（必做）**：凡新增用户可见字符串（`tr("...")`、`.ui` 文本、XML `label` 等），
   必须在同一批次内把 zh_CN 译文加入 `i18n/zh_CN/<Catalog>.ts`（客户端字符串通常在 `LidarView.ts`），
   并执行 `./i18n/build-zh-cn.sh` 重建并投放 `.qm`；**不允许只留英文**。`.ui` 里的硬编码中文可直接显示，
   但走 `tr()` 的字符串必须有 `.ts` 条目。

## 雷达 Web API（Airy / SF-Q96A）HTTP 坑位（必守）

> 详细定位过程/证据见 `docs/airy-http-lwip-notes.md`。**这个坑不要再踩。**

1. **设备端是 lwIP httpd**（响应 `HTTP/1.0`），对 **HTTP header 名大小写敏感**。
2. **Qt 在 Windows 上会把自定义 header 名规范化为小写**。用 `QNetworkAccessManager::post()`
   发出的 `content-length` / `content-type` 设备认不出 → **直接断开、配置不生效**
   （Ubuntu 正常，因为 Qt 那边保留大小写）。
3. **任何带 body 的请求必须用 `QTcpSocket` 裸发原始 HTTP 报文**：header 名首字母大写、
   顺序照浏览器；发送后保持约 1s 再 `disconnectFromHost()`（优雅 FIN，避免 RST 被 lwIP 丢弃）。
   **禁止**对这些请求使用 `QNetworkAccessManager::post()`。
4. 保存端点 = `POST /Parameter_Setting.html`（`/cgi-bin/param_setting.cgi` **不存在**，
   只在 HTML 注释 `saved from url=(...)cgi-bin/param_setting.cgi` 里出现，属误导）。
5. 保存 body 必须是**通用页 41 字段固定顺序**，含 `tabs-two=on`、`save_param=Save`、
   `mode2=Mode2`；**不要多发**额外字段（`MVEn/DVEn/PVEn`、`RotaSpe`、`MotorR`、`MSideEn`、
   `msop_vlan` 等）。
6. 设备对 POST **从不回包**（浏览器亦然）；成功与否用「1s 后回读 `setting_data.json` 比对」判定。
7. Windows 上**系统代理/梯子**会劫持 `192.168.x` → 必须 `QNetworkProxy::NoProxy`。
8. `GET /setting_data.json`（可写参数）与 `GET /device_data.json`（只读身份）可继续用 QNAM。
9. 改 IP 后设备立刻切网断开 → 验证前对**新 IP 做 ~2s 短超时探测**，避免卡系统 TCP 超时。
