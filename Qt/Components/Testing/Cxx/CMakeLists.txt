# ============================================================
# 功能：Senfoto008 网络配置协议单元测试注册。
# 作者：acelan
# 新建时间：2026-10-17
# 修改时间：2026-10-17
# ============================================================

add_executable(lqSenfotoNetworkConfigTest lqSenfotoNetworkConfigTest.cxx)
target_link_libraries(lqSenfotoNetworkConfigTest
  PRIVATE
    LidarView::lqComponents
    "Qt${PARAVIEW_QT_MAJOR_VERSION}::Core")
add_test(NAME lqSenfotoNetworkConfigTest COMMAND lqSenfotoNetworkConfigTest)
