vtk_module_third_party_internal(
  HEADER_ONLY
  SUBDIRECTORY
    "vtkrsdriver"
  LICENSE_FILES
    "vtkrsdriver/LICENSE"
  VERSION
    "1.5.9"
  STANDARD_INCLUDE_DIRS
)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/vtk_rs_driver.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/vtk_rs_driver.h")

vtk_module_install_headers(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/vtk_rs_driver.h")
