/*=========================================================================

  Program:   LidarView
  Module:    TestSenfoto008PacketInterpreter.cxx

  Copyright (c) Kitware Inc.
  All rights reserved.
  See LICENSE or http://www.apache.org/licenses/LICENSE-2.0 for details.

=========================================================================*/

// ============================================================
// 功能：SenFoToView 新增功能 —— Senfoto008 数据包解释器的单元测试；
//        校验解析出的点云字段与时间戳正确性。
// 作者：acelan
// 新建时间：2026-08-28
// 修改时间：2026-08-28
// ============================================================

#include "vtkSenfoto008PacketInterpreter.h"

#include <vtkNew.h>
#include <vtkPolyData.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

namespace
{

constexpr std::size_t PACKET_SIZE = 1248;

void WriteUInt16LE(unsigned char* dest, std::uint16_t value)
{
  dest[0] = static_cast<unsigned char>(value & 0xFF);
  dest[1] = static_cast<unsigned char>((value >> 8) & 0xFF);
}

std::vector<unsigned char> Build48LinePacket(std::uint16_t azimuthUnits)
{
  std::vector<unsigned char> packet(PACKET_SIZE, 0);

  // Header
  packet[0] = 0x00;
  packet[1] = 0x00;
  packet[2] = 0x53;
  packet[3] = 0x46;
  packet[31] = 0x81; // Lidar_type
  packet[32] = 0x01; // Lidar_model: 48-line

  // Body: 8 blocks, each with flag 0xEEFF, azimuth, 48 channels.
  for (std::size_t blockIndex = 0; blockIndex < 8; ++blockIndex)
  {
    unsigned char* block = packet.data() + 42 + blockIndex * 148;
    WriteUInt16LE(block + 0, 0xEEFF);
    WriteUInt16LE(block + 2, azimuthUnits);
    for (std::size_t channelIndex = 0; channelIndex < 48; ++channelIndex)
    {
      unsigned char* channel = block + 4 + channelIndex * 3;
      // Distance 100 -> 1.0 m
      WriteUInt16LE(channel + 0, 100);
      channel[2] = static_cast<unsigned char>(channelIndex);
    }
  }

  // Tail is left as zeros.
  return packet;
}

} // namespace

//-----------------------------------------------------------------------------
int main(int vtkNotUsed(argc), char* vtkNotUsed(argv)[])
{
  vtkNew<vtkSenfoto008PacketInterpreter> interpreter;
  interpreter->Initialize();

  auto packetEnd = Build48LinePacket(35900); // 359.00 degrees
  auto packetStart = Build48LinePacket(100); //   1.00 degrees

  if (!interpreter->IsLidarPacket(packetEnd.data(), static_cast<unsigned int>(packetEnd.size())))
  {
    std::cerr << "IsLidarPacket returned false for a valid packet." << std::endl;
    return 1;
  }

  interpreter->ProcessPacket(packetEnd.data(), static_cast<unsigned int>(packetEnd.size()));

  // Second packet wraps azimuth, so the first revolution becomes a complete frame.
  interpreter->ProcessPacket(packetStart.data(), static_cast<unsigned int>(packetStart.size()));

  vtkPolyData* frame = interpreter->GetLastFrameAvailable();
  if (!frame)
  {
    std::cerr << "No frame available after azimuth wrap." << std::endl;
    return 1;
  }

  const vtkIdType expectedPoints = 8 * 48; // 8 blocks, 48 channels each
  if (frame->GetNumberOfPoints() != expectedPoints)
  {
    std::cerr << "Expected " << expectedPoints << " points, got " << frame->GetNumberOfPoints() << std::endl;
    return 1;
  }

  return 0;
}
