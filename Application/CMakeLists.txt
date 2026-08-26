if (LIDARVIEW_USE_QT)
  #--------------------------------------
  # Scan and find LidarViewApp vtk modules
  #--------------------------------------
  set(lidarview_source_directories
    "${CMAKE_CURRENT_SOURCE_DIR}/Qt"
  )

  vtk_module_find_modules(lidarview_module_files ${lidarview_source_directories})
  vtk_module_scan(
    MODULE_FILES          ${lidarview_module_files}
    PROVIDES_MODULES      lidarview_modules
    REQUIRES_MODULES      lidarview_required_modules
    UNRECOGNIZED_MODULES  lidarview_unrecognized_modules
    WANT_BY_DEFAULT       ON)

  set(lidarview_unrecognized_lidarview_modules)
  foreach (lidarview_module IN LISTS lidarview_unrecognized_modules)
    if (lidarview_module MATCHES "LidarViewApp::")
      list(APPEND lidarview_unrecognized_lidarview_modules
        "${lidarview_module}")
    endif ()
  endforeach ()

  if (lidarview_unrecognized_lidarview_modules)
    message(FATAL_ERROR
      "The following modules were requested or required, but not found: "
      "${lidarview_unrecognized_lidarview_modules}.")
  endif ()

  #--------------------------------------
  # Build LidarViewApp vtk modules
  #--------------------------------------
  vtk_module_build(
    MODULES                   ${lidarview_modules}
    PACKAGE                   LidarViewApp
    INSTALL_EXPORT            LidarViewApp
    INSTALL_HEADERS           "${LIDARVIEW_INSTALL_DEVELOPMENT_FILES}"
    HEADERS_DESTINATION       "${CMAKE_INSTALL_INCLUDEDIR}/lidarview"
    CMAKE_DESTINATION         "${CMAKE_INSTALL_LIBDIR}/cmake/lidarview"
    VERSION                   "${LIDARVIEW_VERSION}"
    ENABLE_WRAPPING           OFF)

  #--------------------------------------
  # macOS: record the Qt framework directory into the installed binary's
  # LC_RPATH. The superbuild builds Qt6 with MUST_USE_SYSTEM, so CMake does
  # not embed it automatically; without this the app fails to launch with
  # "no LC_RPATH's found" on a fresh build. Linux/Windows are unaffected.
  #--------------------------------------
  if (APPLE AND LIDARVIEW_USE_QT)
    if (Qt6_DIR)
      get_filename_component(_qt6_lib_dir "${Qt6_DIR}/../../../lib" ABSOLUTE)
      list(APPEND CMAKE_INSTALL_RPATH "${_qt6_lib_dir}")
    elseif (TARGET Qt6::Core)
      get_target_property(_qt6core_loc Qt6::Core IMPORTED_LOCATION)
      if (_qt6core_loc)
        get_filename_component(_qt6_lib_dir "${_qt6core_loc}" DIRECTORY)
        get_filename_component(_qt6_lib_dir "${_qt6_lib_dir}/../../.." ABSOLUTE)
        list(APPEND CMAKE_INSTALL_RPATH "${_qt6_lib_dir}")
      endif ()
    endif ()
    list(APPEND CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)
  endif ()

  #--------------------------------------
  # Build LidarView binary
  #--------------------------------------
  add_subdirectory(Client)
endif ()

#--------------------------------------
# Build lvpython, lvbatch, lvserver...
#--------------------------------------
add_subdirectory(CommandLineExecutables)

#--------------------------------------
# Export python wrapping
#--------------------------------------
if (LIDARVIEW_USE_PYTHON)
  add_subdirectory(Wrapping/Python)
endif ()
