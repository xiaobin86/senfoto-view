/*=========================================================================

  Program:   LidarView
  Module:    LakiBeamPacketFormat.h

  Copyright (c) Kitware Inc.
  All rights reserved.
  See LICENSE or http://www.apache.org/licenses/LICENSE-2.0 for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#ifndef LakiBeamPacketFormat_h
#define LakiBeamPacketFormat_h

#include <cstddef>
#include <cstdint>
#include <vector>

namespace lakibeam
{
constexpr std::size_t MSOP_DATA_BLOCKS = 12;
constexpr std::size_t MSOP_POINTS_PER_BLOCK = 16;
constexpr std::size_t MSOP_BLOCK_SIZE = 100;
constexpr std::size_t MSOP_PACKET_SIZE =
  MSOP_DATA_BLOCKS * MSOP_BLOCK_SIZE + 4 + 2;

constexpr std::size_t MSOP_DATA_FLAG_OFFSET = 0;
constexpr std::size_t MSOP_AZIMUTH_OFFSET = 2;
constexpr std::size_t MSOP_RESULT_OFFSET = 4;
constexpr std::size_t MSOP_RESULT_SIZE = 6;
constexpr std::size_t MSOP_TIMESTAMP_OFFSET = MSOP_DATA_BLOCKS * MSOP_BLOCK_SIZE;

// On the wire the little-endian bytes are 0xEE 0xFF, which read as 0xEEFF.
constexpr std::uint16_t DATA_FLAG = 0xEEFF;
constexpr std::uint16_t INVALID_DIST = 0;

struct ScanPoint
{
  double angle;     // degrees
  std::uint16_t dist_mm;
  std::uint8_t rssi;
};

//-----------------------------------------------------------------------------
inline std::uint16_t ReadUInt16LE(const unsigned char* data)
{
  return static_cast<std::uint16_t>(data[0]) |
    (static_cast<std::uint16_t>(data[1]) << 8);
}

//-----------------------------------------------------------------------------
inline std::uint32_t ReadUInt32LE(const unsigned char* data)
{
  return static_cast<std::uint32_t>(data[0]) |
    (static_cast<std::uint32_t>(data[1]) << 8) |
    (static_cast<std::uint32_t>(data[2]) << 16) |
    (static_cast<std::uint32_t>(data[3]) << 24);
}

//-----------------------------------------------------------------------------
inline bool IsValidDataBlock(const unsigned char* data, std::size_t blockIndex)
{
  const unsigned char* block = data + blockIndex * MSOP_BLOCK_SIZE;
  return ReadUInt16LE(block + MSOP_DATA_FLAG_OFFSET) == DATA_FLAG;
}

//-----------------------------------------------------------------------------
inline double GetBlockAzimuth(const unsigned char* data, std::size_t blockIndex)
{
  const unsigned char* block = data + blockIndex * MSOP_BLOCK_SIZE;
  return static_cast<double>(ReadUInt16LE(block + MSOP_AZIMUTH_OFFSET)) * 0.01;
}

//-----------------------------------------------------------------------------
inline std::uint32_t GetPacketTimestampUs(const unsigned char* data)
{
  return ReadUInt32LE(data + MSOP_TIMESTAMP_OFFSET);
}

//-----------------------------------------------------------------------------
inline std::vector<ScanPoint> ParsePacket(const unsigned char* data, std::size_t length)
{
  std::vector<ScanPoint> points;
  points.reserve(MSOP_DATA_BLOCKS * MSOP_POINTS_PER_BLOCK);

  if (length < MSOP_PACKET_SIZE)
  {
    return points;
  }

  // Pre-compute azimuth for each block so the intra-block resolution can be
  // derived from the first two blocks, as the official ROS2 driver does.
  double azimuths[MSOP_DATA_BLOCKS];
  for (std::size_t blockIndex = 0; blockIndex < MSOP_DATA_BLOCKS; ++blockIndex)
  {
    azimuths[blockIndex] = GetBlockAzimuth(data, blockIndex);
  }

  double resolution = 0.0;
  if (MSOP_DATA_BLOCKS >= 2)
  {
    double diff = azimuths[1] - azimuths[0];
    if (diff > 0.0)
    {
      resolution = diff / static_cast<double>(MSOP_POINTS_PER_BLOCK);
    }
  }

  for (std::size_t blockIndex = 0; blockIndex < MSOP_DATA_BLOCKS; ++blockIndex)
  {
    if (!IsValidDataBlock(data, blockIndex))
    {
      continue;
    }

    const unsigned char* block = data + blockIndex * MSOP_BLOCK_SIZE;
    const double baseAzimuth = azimuths[blockIndex];

    for (std::size_t pointIndex = 0; pointIndex < MSOP_POINTS_PER_BLOCK; ++pointIndex)
    {
      const unsigned char* result = block + MSOP_RESULT_OFFSET +
        pointIndex * MSOP_RESULT_SIZE;
      const std::uint16_t dist_mm = ReadUInt16LE(result);
      if (dist_mm == INVALID_DIST)
      {
        continue;
      }

      const double angle = baseAzimuth + resolution * static_cast<double>(pointIndex);
      points.push_back({ angle, dist_mm, result[2] });
    }
  }

  return points;
}

} // namespace lakibeam

#endif // LakiBeamPacketFormat_h
