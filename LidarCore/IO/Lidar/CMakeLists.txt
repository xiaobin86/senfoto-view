set(classes
  vtkLidarPacketInterpreter
  vtkLidarReader
  vtkLidarStream
  vtkLidarPoseReader
  vtkLidarPoseStream
  vtkPosePacketInterpreter
)

vtk_module_add_module(LidarView::IOLidar
  CLASSES ${classes})

if (BUILD_TESTING)
  add_subdirectory(Testing)
endif ()
