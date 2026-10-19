#!/usr/bin/env bash

set -e
set -x
shopt -s dotglob

readonly name="rsdriver"
readonly ownership="SFQ96A Driver Upstream <kwrobot@kitware.com>"
readonly subtree="SFQ96ASDK/vtk$name"
readonly repo="https://gitlab.kitware.com/LidarView/third-party/robosense-driver.git"
readonly tag="for/lidarview-20260216-1.5.18"
readonly paths="
src

LICENSE

CMakeLists.vtk.txt
README.md
README.kitware.md
"

extract_source () {
    git_archive
    pushd "$extractdir/$name-reduced"

    # Remove unused headers.
    rm -rvf src/rs_driver/api/
    rm -rvf src/rs_driver/utility/
    rm -rvf src/rs_driver/driver/input/
    rm -rvf src/rs_driver/driver/lidar_driver_impl.hpp
    rm -rvf src/rs_driver/msg/pcl_point_cloud_msg.hpp

    mv -v CMakeLists.vtk.txt CMakeLists.txt
    popd
}

. "${BASH_SOURCE%/*}/../Utilities/update-common.sh"
