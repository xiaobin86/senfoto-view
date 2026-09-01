# ============================================================
# 功能：SenFoToView 新增功能 —— Senfoto008 插件构建入口；
#        通过 paraview_add_plugin 编译并安装该插件及其 Server Manager XML。
# 作者：acelan
# 新建时间：2026-08-28
# 修改时间：2026-09-01
# ============================================================

paraview_add_plugin(Senfoto008Plugin
  REQUIRED_ON_CLIENT
  REQUIRED_ON_SERVER
  VERSION "1.0"
  MODULES
    Senfoto008Plugin::Senfoto008PacketInterpreters
  MODULE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/Senfoto008PacketInterpreters/vtk.module"
  SERVER_MANAGER_XML
    "${CMAKE_CURRENT_SOURCE_DIR}/Senfoto008Proxies.xml"
)

option(BUILD_TESTING "Enable testing" OFF)
add_subdirectory(Testing)
