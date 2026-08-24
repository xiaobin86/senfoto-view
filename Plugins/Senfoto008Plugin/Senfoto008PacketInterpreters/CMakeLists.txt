set(classes
  vtkSenfoto008PacketInterpreter
)

set(private_headers
  Senfoto008PacketFormat.h
)

vtk_module_add_module(Senfoto008Plugin::Senfoto008PacketInterpreters
  CLASSES ${classes}
  PRIVATE_HEADERS ${private_headers}
)

paraview_add_server_manager_xmls(
  MODULE Senfoto008Plugin::Senfoto008PacketInterpreters
  XMLS
    Senfoto008PacketInterpreter.xml
    Senfoto008LidarReader.xml
    Senfoto008LidarStream.xml
)
