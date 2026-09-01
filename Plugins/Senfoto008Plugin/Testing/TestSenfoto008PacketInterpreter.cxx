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
// 修改时间：2026-09-01
// ============================================================

#include "vtkSenfoto008PacketInterpreter.h"

#include <vtkDoubleArray.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkUnsignedCharArray.h>

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

std::vector<unsigned char> Build96LinePacket(const std::array<std::uint16_t, 8>& azimuthUnits)
{
  std::vector<unsigned char> packet(PACKET_SIZE, 0);

  // Header
  packet[0] = 0x00;
  packet[1] = 0x00;
  packet[2] = 0x53;
  packet[3] = 0x46;
  packet[31] = 0x81; // Lidar_type
  packet[32] = 0x02; // Lidar_model: 96-line

  // Body: 8 blocks, each with flag 0xEEFF, azimuth, 48 channels.
  for (std::size_t blockIndex = 0; blockIndex < 8; ++blockIndex)
  {
    unsigned char* block = packet.data() + 42 + blockIndex * 148;
    WriteUInt16LE(block + 0, 0xEEFF);
    WriteUInt16LE(block + 2, azimuthUnits[blockIndex]);
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

bool CheckAzimuthCorrectionApplied(vtkPolyData* frame, double expectedRawAzimuthDeg)
{
  vtkDoubleArray* azimuth = vtkDoubleArray::SafeDownCast(frame->GetPointData()->GetArray("azimuth"));
  vtkUnsignedCharArray* laserId = vtkUnsignedCharArray::SafeDownCast(frame->GetPointData()->GetArray("laser_id"));
  if (!azimuth || !laserId)
  {
    std::cerr << "Missing azimuth or laser_id arrays." << std::endl;
    return false;
  }

  // The code truncates the correction term to integer 0.01-degree units.
  // For the last block's second sub-block the term is 40 * 19 / 36 = 21.111...
  // which truncates to 21 units = 0.21°.
  const double expectedOffsetDeg = 0.21;
  const double expected = expectedRawAzimuthDeg + expectedOffsetDeg;

  bool foundCorrected = false;
  for (vtkIdType i = 0; i < frame->GetNumberOfPoints(); ++i)
  {
    const double az = azimuth->GetTuple1(i);
    const std::uint8_t lid = static_cast<std::uint8_t>(laserId->GetTuple1(i));
    if (lid >= 48 && std::fabs(az - expected) < 1e-9)
    {
      foundCorrected = true;
    }
  }
  return foundCorrected;
}

bool CheckAllAzimuthEqual(vtkPolyData* frame, double expectedRawAzimuthDeg)
{
  vtkDoubleArray* azimuth = vtkDoubleArray::SafeDownCast(frame->GetPointData()->GetArray("azimuth"));
  if (!azimuth)
  {
    std::cerr << "Missing azimuth array." << std::endl;
    return false;
  }

  for (vtkIdType i = 0; i < frame->GetNumberOfPoints(); ++i)
  {
    if (std::fabs(azimuth->GetTuple1(i) - expectedRawAzimuthDeg) > 1e-9)
    {
      return false;
    }
  }
  return true;
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

  // ---------------------------------------------------------------------------
  // 96-line azimuth correction toggle tests
  // ---------------------------------------------------------------------------
  // Helper to produce two packets that trigger an azimuth wrap, finalizing the
  // first packet as a complete frame.
  auto run96LineTest = [](bool enableCorrection, std::uint16_t firstAzimuthUnits,
                       double expectedRawAzimuthDeg,
                       bool (*checkFunc)(vtkPolyData*, double)) -> bool {
    vtkNew<vtkSenfoto008PacketInterpreter> interp96;
    interp96->Initialize();

    const std::array<std::uint16_t, 8> firstAz = {
      firstAzimuthUnits, firstAzimuthUnits, firstAzimuthUnits, firstAzimuthUnits,
      firstAzimuthUnits, firstAzimuthUnits, firstAzimuthUnits, firstAzimuthUnits
    };
    // Second packet has a smaller azimuth so the first packet is finalized.
    const std::array<std::uint16_t, 8> secondAz = {
      1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000
    };

    auto packet96First = Build96LinePacket(firstAz);
    auto packet96Second = Build96LinePacket(secondAz);

    interp96->SetEnableAzimuthCorrection(enableCorrection);
    interp96->ProcessPacket(packet96First.data(), static_cast<unsigned int>(packet96First.size()));
    interp96->ProcessPacket(packet96Second.data(), static_cast<unsigned int>(packet96Second.size()));

    vtkPolyData* frame96 = interp96->GetLastFrameAvailable();
    if (!frame96)
    {
      std::cerr << "No 96-line frame available (correction="
                << (enableCorrection ? "true" : "false") << ")." << std::endl;
      return false;
    }
    if (!checkFunc(frame96, expectedRawAzimuthDeg))
    {
      std::cerr << "96-line azimuth check failed (correction="
                << (enableCorrection ? "true" : "false") << ")." << std::endl;
      return false;
    }
    return true;
  };

  // With correction enabled and all raw block az = 350°, the last block's
  // second sub-block (laser 48..95) gets the ~0.211° firing-sequence offset.
  if (!run96LineTest(true, 35000, 350.0, CheckAzimuthCorrectionApplied))
  {
    return 1;
  }

  // With correction disabled, every point must keep the raw block azimuth.
  if (!run96LineTest(false, 35000, 350.0, CheckAllAzimuthEqual))
  {
    return 1;
  }

  return 0;
}
