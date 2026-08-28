# ============================================================
# 功能：SenFoToView 新增功能 —— 编译 Senfoto008 数据包解释器模块
#        （vtkSenfoto008PacketInterpreter 及格式头文件）。
# 作者：acelan
# ============================================================

set(classes
  vtkSenfoto008PacketInterpreter
)

set(private_headers
  Senfoto008PacketFormat.h
)

vtk_module_add_module(Senfoto008Plugin::Senfoto008PacketInterpreters
  FORCE_STATIC
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
