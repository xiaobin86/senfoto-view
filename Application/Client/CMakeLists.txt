# Sanitize checks
if(NOT LIDARVIEW_VERSION_FULL)
  message(FATAL_ERROR "LIDARVIEW_VERSION_ variables not set")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if (APPLE)
  # Enable high resolution when using Qt5
  set(high_resolution_capable "true")
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/MacOSXBundleInfo.plist.in.in"
    "${CMAKE_CURRENT_BINARY_DIR}/MacOSXBundleInfo.plist.in"
    @ONLY)
  unset(high_resolution_capable)
endif ()

set(sources
  LidarViewMainWindow.h
  LidarViewMainWindow.cxx
  LidarViewMainWindow.ui
)

set(resource_files
  Resources/lvResources.qrc
)

set(plugins_targets
  ParaView::paraview_plugins
  LidarView::lidarview_plugins
)

if (LIDARVIEW_USE_LIDARSLAM_PLUGIN)
  list(APPEND plugins_targets LidarSlam::paraview_wrapping)
endif ()

paraview_client_add(
  NAME              "LidarView"
  NAMESPACE         "LidarView"
  EXPORT            "LidarViewClient"
  VERSION           "${LIDARVIEW_VERSION_MAJOR}.${LIDARVIEW_VERSION_MINOR}.${LIDARVIEW_VERSION_PATCH}"
  APPLICATION_NAME  "SenFoToView"
  ORGANIZATION      "SenFoToView"
  TITLE             "SenFoToView ${LIDARVIEW_VERSION_FULL}"
  SPLASH_IMAGE      "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Images/About.png"
  BUNDLE_ICON       "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Images/logo.icns"
  BUNDLE_PLIST      "${CMAKE_CURRENT_BINARY_DIR}/MacOSXBundleInfo.plist.in"
  APPLICATION_ICON  "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Images/logo.ico"

  PLUGINS_TARGETS   ${plugins_targets}
  REQUIRED_PLUGINS  LidarCorePlugin

  MAIN_WINDOW_CLASS LidarViewMainWindow
  MAIN_WINDOW_INCLUDE LidarViewMainWindow.h
  SOURCES ${sources} ${resource_files}
  APPLICATION_XMLS
    ${CMAKE_CURRENT_SOURCE_DIR}/lvSources.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/lvFilters.xml

  RUNTIME_DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY_DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Rename the produced executable to SenFoToView (mirror 02.code's SenFoToView.exe)
# while keeping the internal CMake target name "LidarView" so existing target
# references (e.g. LidarView::LidarView used by tests) stay valid.
set_target_properties(LidarView PROPERTIES OUTPUT_NAME "SenFoToView")

target_include_directories(LidarView
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(LidarView
  PRIVATE
    LidarView::lqApplicationComponents # Common LVCore code base
    LidarViewApp::lvApplicationComponents # Specific LidarView code base
)

#--------------------------------------
# 为什么保留 NAME="LidarView"，却要在这里把 conf 额外复制进品牌化的 SenFoToView.app：
#
# 1) 加载器只认 "LidarView.conf"：pqLidarViewInitializer / lvserver / lvpython
#    三处源码都硬编码了 LoadPluginConfigurationXMLs("LidarView")，而 conf 文件名
#    由 paraview_client_add 的 NAME 参数生成。若把 NAME 改成 SenFoToView，conf
#    会变成 SenFoToView.conf，与源码里的查找名不一致，插件照样加载失败；此外
#    NAME 还决定 NAMESPACE / EXPORT 以及 LidarView:: 系列 CMake target、lidarview
#    Python 包名等“内部产品名”，本 fork 有意保留 "LidarView"（见上方注释），改名
#    会让其它模块、测试、Python 包失配。因此 NAME 维持 "LidarView" 不动。
#
# 2) 但 paraview_client_add 会把生成的 LidarView.conf 装进 LidarView.app 包，而真正
#    带品牌名的可执行程序在 SenFoToView.app。于是 SenFoToView.app 的
#    Contents/Resources 缺 LidarView.conf -> 启动后所有必需插件（含
#    LidarCorePlugin）注册失败 -> 打开雷达流时 InterpretersManager 代理缺失而崩溃。
#    这里在安装阶段把 LidarView.conf 复制到品牌化包内，补上这一打包缺口。
#--------------------------------------
if (APPLE)
  install(CODE "
    set(_src \"\${CMAKE_INSTALL_PREFIX}/Applications/LidarView.app/Contents/Resources/LidarView.conf\")
    set(_dst_dir \"\${CMAKE_INSTALL_PREFIX}/Applications/SenFoToView.app/Contents/Resources\")
    if(EXISTS \"\${_src}\")
      file(COPY \"\${_src}\" DESTINATION \"\${_dst_dir}\")
      message(STATUS \"Copied LidarView.conf into SenFoToView.app/Contents/Resources\")
    else()
      message(WARNING \"LidarView.conf not found at \${_src}; SenFoToView.app plugins may fail to load\")
    endif()
  ")
endif ()
