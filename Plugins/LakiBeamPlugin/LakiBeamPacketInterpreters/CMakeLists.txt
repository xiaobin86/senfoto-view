set(classes
  vtkLakiBeamPacketInterpreter
)

set(private_headers
  LakiBeamPacketFormat.h
)

vtk_module_add_module(LakiBeamPlugin::LakiBeamPacketInterpreters
  FORCE_STATIC
  CLASSES ${classes}
  PRIVATE_HEADERS ${private_headers}
)

paraview_add_server_manager_xmls(
  MODULE LakiBeamPlugin::LakiBeamPacketInterpreters
  XMLS
    LakiBeamPacketInterpreter.xml
    LakiBeamLidarReader.xml
    LakiBeamLidarStream.xml
)
