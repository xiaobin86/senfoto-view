set(rs_plugin_classes
  vtkSFQ96APacketInterpreter
)

vtk_module_add_module(SFQ96APlugin::SFQ96APacketInterpreters
  CLASSES ${rs_plugin_classes}
)

paraview_add_server_manager_xmls(
  MODULE SFQ96APlugin::SFQ96APacketInterpreters
  XMLS
    SFQ96APacketInterpreter.xml
)
