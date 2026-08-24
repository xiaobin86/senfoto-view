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
if (BUILD_TESTING)
  add_subdirectory(Testing)
endif ()
