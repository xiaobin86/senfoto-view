set(rs_plugin_classes
  vtkSFQ96ALidarReader
  vtkSFQ96ALidarStream
)

vtk_module_add_module(SFQ96APlugin::SFQ96AIO
  CLASSES ${rs_plugin_classes}
)

paraview_add_server_manager_xmls(
  MODULE SFQ96APlugin::SFQ96AIO
  XMLS
    SFQ96ALidarReader.xml
    SFQ96ALidarStream.xml
)
