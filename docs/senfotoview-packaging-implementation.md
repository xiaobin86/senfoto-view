<!--
  功能：SenFoToView 三平台（Linux / macOS / Windows）安装包实现文档 ——
        现状、缺口、CPack/AppImage/NSIS/DMG 方案、命令与产物清单。
  作者：acelan
  新建时间：2026-10-20
  修改时间：2026-10-20
-->

# SenFoToView 三平台安装包 —— 实现文档

## 1. 现状（2026-10-20 检查）

| 项 | 结论 |
|---|---|
| 应用入口 | `Application/Client/CMakeLists.txt` 的 `paraview_client_add(...)`，NAME=`LidarView`、APPLICATION_NAME=`SenFoToView`，产物 `SenFoToView`（macOS 为 `SenFoToView.app`） |
| 安装包机制 | **无**：没有 CPack / NSIS / DragNDrop / DEB / AppImage 配置 |
| 安装树体积 | `build/install` ≈ **6.8 GB**（`lib` 6.5 GB），因为构建类型是 **RelWithDebInfo**（含调试符号） |
| 开发文件 | 243 个 `.a`、1139 个 `.so*`、`include/`、`lib/cmake/` —— 均不应进安装包 |
| Qt（Linux） | **来自系统**（`install/lib` 无 `libQt6*`）→ 便携包必须内置或自带依赖 |
| 交叉打包 | **不可行**：Windows/macOS 安装包必须在各自平台构建（ParaView/LidarView 不支持交叉编译打包） |

结论：现状**不能直接分发**；需补"Release 构建 + 去调试/开发文件 + 平台打包"三步。

## 2. 目标产物

| 平台 | 格式 | 依赖策略 |
|---|---|---|
| Linux | **AppImage**（首选）/ `.tar.gz` 或 `.deb` | `linuxdeploy` + `linuxdeploy-plugin-qt` 打包 Qt6 与三方 `.so` |
| macOS | **`.dmg`**（CPack DragNDrop） | `macdeployqt` + `fixup_bundle`（`paraview_client_add` 已产出 `.app`） |
| Windows | **NSIS `.exe`**（CPack NSIS / WIX） | 内置 MSVC 运行库 + Qt6 + ParaView DLL；无显卡机器另带 Mesa `opengl32.dll` |

## 3. 必带内容清单（三平台通用）

1. 可执行程序：`SenFoToView`（Win: `SenFoToView.exe`；mac: `SenFoToView.app`）
2. **`LidarView.conf`**（插件加载配置；缺失会导致必需插件注册失败 → 打开雷达流崩溃）
   - macOS 已有 `install(CODE ...)` 把它复制进 `SenFoToView.app/Contents/Resources`（见 `Application/Client/CMakeLists.txt:100`）
3. `plugins/`：`SFQ96APlugin`、`Senfoto008Plugin`、`LidarCorePlugin` 等 + `lidarview.plugins.xml`
4. `share/interface_modes_config.json`（菜单/停靠/管线浏览器等界面配置，本 fork 已定制）
5. 翻译：`i18n/*.qm`（中文界面必需）
6. **Qt 插件**：
   - Windows/macOS：`platforms/qwindows.dll`（mac: `libqcocoa`）、`styles/`、`imageformats/`、`iconengines/`
   - Linux AppImage：由 `linuxdeploy-plugin-qt` 自动收集
7. **MSVC 运行库**（Windows）：内置 VC++ Redistributable 并静默安装，或改用 `/MT` 静态运行库
8. **软件 OpenGL（无显卡机器）**：Mesa3D 的 `opengl32.dll`（Windows）/ `libGL`+`llvmpipe`（Linux），
   否则 VTK/ParaView 要求 OpenGL ≥ 3.2 不满足
9. Python 运行时：仅当需要 `lvpython` / Python 脚本功能时打包（当前 `install/lib/python3.12` ≈ 815 MB）

**不应包含**：`include/`、`lib/cmake/`、`*.a`、调试符号（`.debug`/PDB）、测试数据、`build/_data`。

## 4. 实施步骤

### 4.1 统一前提：Release 构建
RelWithDebInfo 会带调试符号导致 6.8 GB。打包前应以 **Release** 配置构建（或对产物 `strip`）：

```bash
# 内层（示意，实际路径以本仓库 superbuild 为准）
cmake -S lidarview -B build/superbuild/lidarview/build -DCMAKE_BUILD_TYPE=Release
ninja -C build/superbuild/lidarview/build
ninja -C build/superbuild/lidarview/build install
```

> 若不便重建，可对 `build/install` 执行：
> `find lib bin -name '*.so*' -exec strip --strip-unneeded {} \;` 并删除 `include/ lib/cmake/ **/*.a`。

### 4.2 Linux：AppImage（推荐）
1. 从 `build/install` 取运行时子集到 `AppDir/usr/`（bin、lib、share、plugins）。
2. 用 `linuxdeploy` + `linuxdeploy-plugin-qt` 收集 Qt6 与依赖：
   ```bash
   linuxdeploy --appdir AppDir -e usr/bin/SenFoToView \
     --plugin qt --desktop-file SenFoToView.desktop \
     --icon-file SenFoToView.png --output appimage
   ```
3. 校验：`./SenFoToView.AppImage --version`；在无 Qt 的干净系统上启动一次。

### 4.3 macOS：CPack DragNDrop
`paraview_client_add` 已生成 `SenFoToView.app`；补 CPack：
```cmake
set(CPACK_GENERATOR "DragNDrop")
set(CPACK_PACKAGE_NAME "SenFoToView")
set(CPACK_PACKAGE_VERSION "${LIDARVIEW_VERSION_FULL}")
set(CPACK_DMG_VOLUME_NAME "SenFoToView")
include(CPack)
```
构建后：`cpack -C Release` → `SenFoToView-<ver>.dmg`。
打包后需 `macdeployqt SenFoToView.app -dmg`（或 `fixup_bundle`）确保 Qt/VTK 依赖被复制且重写 `@rpath`。

### 4.4 Windows：CPack NSIS
```cmake
set(CPACK_GENERATOR "NSIS")
set(CPACK_PACKAGE_NAME "SenFoToView")
set(CPACK_PACKAGE_VERSION "${LIDARVIEW_VERSION_FULL}")
set(CPACK_NSIS_PACKAGE_NAME "SenFoToView")
set(CPACK_NSIS_DISPLAY_NAME "SenFoToView")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "SenFoToView")
include(CPack)
```
构建后：`cpack -C Release` → `SenFoToView-<ver>.exe`。

**Windows 额外必做**：
- 用 `windeployqt` 或 CPack 的 `install(DIRECTORY ...)` 收集 Qt6 DLL 与 `platforms/` 等插件；
- 内置 **VC++ Redistributable** 并在 NSIS 中静默安装；
- 无显卡机器：把 **Mesa `opengl32.dll`** 放到 `SenFoToView.exe` 同级（或安装包可选组件）。

## 5. 验证清单（每个平台）

1. 干净环境（未装 Qt/ParaView）双击/执行安装包装好，能启动。
2. 打开 `2025-10-28-...RS-96-Data.pcap`，Interpreter 选 **SF-Q96A**，能出点云。
3. `Help → About` 正常；菜单符合定制（无 Tools/Filters，编辑菜单含「显示 RPM」「雷达 IP 配置」）。
4. 中文界面正常（`.qm` 已加载）。
5. 无显卡机器：OpenGL 版本 ≥ 3.2（Mesa 软件渲染）能渲染。
6. 实时流：放行 UDP 6699/7788。

## 6. 已知风险

- **不可交叉打包**：Windows/macOS 必须在对应平台或 CI runner 上构建。
- **Linux Qt 来自系统**：不自带 Qt 就会在用户机上缺库；必须 AppImage/自带 Qt。
- **体积**：Python 运行时（815 MB）与 VTK 库较大；按需裁剪（不要 Python 就不带 `lib/python3.12`）。
- **无显卡/远程桌面**：软件渲染性能有限；重场景建议 `lvserver`（客户端/服务端模式）。
- **Windows Server**：需 **Desktop Experience**，Server Core 无法运行 Qt GUI。

## 7. 建议的落地顺序

1. 先在 `develop` 提交当前功能改动（避免打包版本混乱）。
2. 加 `CMake/Packaging.cmake`（`include(CPack)` + 各平台变量）与 `packaging/` 脚本。
3. 本机先做 **Release 构建 + Linux AppImage**（最快可验证）。
4. macOS / Windows 在各自环境跑同一套脚本出包。
