set(testing_libraries
  Senfoto008Plugin::Senfoto008PacketInterpreters
  LidarView::TestingCore
  VTK::CommonSystem
)

set(test_binary_dir)
if (WIN32)
  set(test_binary_dir "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/${lidarview_plugins_library_subdirectory}/Senfoto008Plugin")
else ()
  set(test_binary_dir "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}/${lidarview_plugins_library_subdirectory}/Senfoto008Plugin")
endif ()

function(custom_add_executable)
  add_executable(${ARGV})
  set_target_properties(${ARGV0} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${test_binary_dir}")
  if (WIN32)
    install(TARGETS ${ARGV0}
      RUNTIME DESTINATION ${test_binary_dir}
      LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
  endif(WIN32)
endfunction(custom_add_executable)

custom_add_executable(TestSenfoto008PacketInterpreter TestSenfoto008PacketInterpreter.cxx)
target_link_libraries(TestSenfoto008PacketInterpreter ${testing_libraries})

add_test(TestSenfoto008PacketInterpreter
  ${test_binary_dir}/TestSenfoto008PacketInterpreter
)
