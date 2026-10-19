vtk_module_install_headers(
  SUBDIR  "vtkrsdriver/src/rs_driver/common"
  FILES   "src/rs_driver/common/rs_common.hpp"
          "src/rs_driver/common/rs_log.hpp"
          "src/rs_driver/common/error_code.hpp")
vtk_module_install_headers(
  SUBDIR  "vtkrsdriver/src/rs_driver/driver"
  FILES   "src/rs_driver/driver/driver_param.hpp")
vtk_module_install_headers(
  SUBDIR  "vtkrsdriver/src/rs_driver/driver/decoder"
  FILES   "src/rs_driver/driver/decoder/basic_attr.hpp"
          "src/rs_driver/driver/decoder/block_iterator.hpp"
          "src/rs_driver/driver/decoder/chan_angles.hpp"
          "src/rs_driver/driver/decoder/decoder.hpp"
          "src/rs_driver/driver/decoder/decoder_factory.hpp"
          "src/rs_driver/driver/decoder/decoder_mech.hpp"
          "src/rs_driver/driver/decoder/decoder_RS16.hpp"
          "src/rs_driver/driver/decoder/decoder_RS32.hpp"
          "src/rs_driver/driver/decoder/decoder_RS48.hpp"
          "src/rs_driver/driver/decoder/decoder_RS80.hpp"
          "src/rs_driver/driver/decoder/decoder_RS128.hpp"
          "src/rs_driver/driver/decoder/decoder_RSBP.hpp"
          "src/rs_driver/driver/decoder/decoder_RSE1.hpp"
          "src/rs_driver/driver/decoder/decoder_RSHELIOS.hpp"
          "src/rs_driver/driver/decoder/decoder_RSHELIOS_16P.hpp"
          "src/rs_driver/driver/decoder/decoder_RSM1.hpp"
          "src/rs_driver/driver/decoder/decoder_RSM1_Jumbo.hpp"
          "src/rs_driver/driver/decoder/decoder_RSM2.hpp"
          "src/rs_driver/driver/decoder/decoder_RSM3.hpp"
          "src/rs_driver/driver/decoder/decoder_RSMX.hpp"
          "src/rs_driver/driver/decoder/decoder_RSP48.hpp"
          "src/rs_driver/driver/decoder/decoder_RSP80.hpp"
          "src/rs_driver/driver/decoder/decoder_RSP128.hpp"
          "src/rs_driver/driver/decoder/member_checker.hpp"
          "src/rs_driver/driver/decoder/section.hpp"
          "src/rs_driver/driver/decoder/split_strategy.hpp"
          "src/rs_driver/driver/decoder/trigon.hpp")
vtk_module_install_headers(
  SUBDIR  "vtkrsdriver/src/rs_driver/msg"
  FILES   "src/rs_driver/msg/packet.hpp"
          "src/rs_driver/msg/point_cloud_msg.hpp")
vtk_module_install_headers(
  SUBDIR  "vtkrsdriver/src/rs_driver/macro"
  FILES   "src/rs_driver/macro/version.hpp")
