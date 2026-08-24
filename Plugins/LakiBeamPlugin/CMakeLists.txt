paraview_add_plugin(LakiBeamPlugin
  REQUIRED_ON_CLIENT
  REQUIRED_ON_SERVER
  VERSION "1.0"
  MODULES
    LakiBeamPlugin::LakiBeamPacketInterpreters
  MODULE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/LakiBeamPacketInterpreters/vtk.module"
  SERVER_MANAGER_XML
    "${CMAKE_CURRENT_SOURCE_DIR}/LakiBeamProxies.xml"
)
