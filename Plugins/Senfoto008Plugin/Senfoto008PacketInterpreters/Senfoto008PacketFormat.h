/*=========================================================================

  Program:   LidarView
  Module:    Senfoto008PacketFormat.h

  Copyright (c) Kitware Inc.
  All rights reserved.
  See LICENSE or http://www.apache.org/licenses/LICENSE-2.0 for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#ifndef Senfoto008PacketFormat_h
#define Senfoto008PacketFormat_h

#include <array>
#include <cstddef>
#include <cstdint>

namespace senfoto008
{

// Packet layout
constexpr std::size_t HEADER_SIZE = 42;
constexpr std::size_t TAIL_SIZE = 22;
constexpr std::size_t DATA_BLOCKS = 8;
constexpr std::size_t CHANNELS_PER_BLOCK = 48;
constexpr std::size_t CHANNEL_DATA_SIZE = 3;
constexpr std::size_t BLOCK_SIZE = 2 + 2 + CHANNELS_PER_BLOCK * CHANNEL_DATA_SIZE; // 148 bytes
constexpr std::size_t BODY_SIZE = DATA_BLOCKS * BLOCK_SIZE; // 1184 bytes
constexpr std::size_t PACKET_SIZE = HEADER_SIZE + BODY_SIZE + TAIL_SIZE; // 1248 bytes

// Header offsets
constexpr std::size_t HEADER_MAGIC_OFFSET = 0;
constexpr std::size_t HEADER_MAGIC_SIZE = 4;
constexpr std::size_t HEADER_PKTCNT_OFFSET = 12;
constexpr std::size_t HEADER_TIMESTAMP_OFFSET = 20;
constexpr std::size_t HEADER_TIMESTAMP_SIZE = 10;
constexpr std::size_t HEADER_LIDAR_TYPE_OFFSET = 31;
constexpr std::size_t HEADER_LIDAR_MODEL_OFFSET = 32;

// Header magic: bytes 00 00 53 46.
constexpr std::uint8_t HEADER_MAGIC[HEADER_MAGIC_SIZE] = { 0x00, 0x00, 0x53, 0x46 };
constexpr std::uint8_t LIDAR_TYPE_008 = 0x81;

// Lidar models
constexpr std::uint8_t LIDAR_MODEL_48_LINE = 0x01;
constexpr std::uint8_t LIDAR_MODEL_96_LINE = 0x02;

// Block offsets (inside one 148-byte block)
constexpr std::size_t BLOCK_FLAG_OFFSET = 0;
constexpr std::size_t BLOCK_AZIMUTH_OFFSET = 2;
constexpr std::size_t BLOCK_CHANNEL_OFFSET = 4;

// On the wire little-endian bytes are 0xEE 0xFF, which reads as 0xEEFF.
constexpr std::uint16_t DATA_FLAG = 0xEEFF;

// Distance scale: real_distance = raw * 0.01 m.
constexpr double DISTANCE_SCALE_M = 0.01;
constexpr std::uint16_t INVALID_DISTANCE = 0;

// Azimuth scale: 0.01 degrees per LSB.
constexpr double AZIMUTH_SCALE_DEG = 0.01;

// 96-line vertical pitch angles, in degrees.
// Source: manufacturer table (channel index 0..95 maps to laser ID 0..95).
inline const std::array<double, 96>& GetVerticalAngles96Line()
{
  static const std::array<double, 96> angles = [] {
    std::array<double, 96> a{};
    for (std::size_t i = 0; i < 95; ++i)
    {
      a[i] = static_cast<double>(i) * 0.95;
    }
    a[95] = 90.0;
    return a;
  }();
  return angles;
}

// 48-line vertical pitch angles, in degrees.
// Source: odd rows (1,3,5,...,95) of the manufacturer 96-line table.
inline const std::array<double, CHANNELS_PER_BLOCK>& GetVerticalAngles48Line()
{
  static const std::array<double, CHANNELS_PER_BLOCK> angles = [] {
    std::array<double, CHANNELS_PER_BLOCK> a{};
    for (std::size_t i = 0; i < CHANNELS_PER_BLOCK; ++i)
    {
      a[i] = static_cast<double>(i) * 1.9;
    }
    return a;
  }();
  return angles;
}

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
inline bool IsHeaderValid(const unsigned char* data, std::size_t length)
{
  if (length < HEADER_SIZE)
  {
    return false;
  }
  for (std::size_t i = 0; i < HEADER_MAGIC_SIZE; ++i)
  {
    if (data[HEADER_MAGIC_OFFSET + i] != HEADER_MAGIC[i])
    {
      return false;
    }
  }
  return data[HEADER_LIDAR_TYPE_OFFSET] == LIDAR_TYPE_008;
}

//-----------------------------------------------------------------------------
inline bool IsValidPacket(const unsigned char* data, std::size_t length)
{
  if (length != PACKET_SIZE)
  {
    return false;
  }
  if (!IsHeaderValid(data, length))
  {
    return false;
  }
  const unsigned char* firstBlock = data + HEADER_SIZE;
  return ReadUInt16LE(firstBlock + BLOCK_FLAG_OFFSET) == DATA_FLAG;
}

//-----------------------------------------------------------------------------
inline std::uint8_t GetLidarModel(const unsigned char* data)
{
  return data[HEADER_LIDAR_MODEL_OFFSET];
}

//-----------------------------------------------------------------------------
inline bool IsValidDataBlock(const unsigned char* data, std::size_t blockIndex)
{
  const unsigned char* block = data + HEADER_SIZE + blockIndex * BLOCK_SIZE;
  return ReadUInt16LE(block + BLOCK_FLAG_OFFSET) == DATA_FLAG;
}

//-----------------------------------------------------------------------------
inline double GetBlockAzimuth(const unsigned char* data, std::size_t blockIndex)
{
  const unsigned char* block = data + HEADER_SIZE + blockIndex * BLOCK_SIZE;
  return static_cast<double>(ReadUInt16LE(block + BLOCK_AZIMUTH_OFFSET)) * AZIMUTH_SCALE_DEG;
}

//-----------------------------------------------------------------------------
inline std::uint16_t GetChannelDistanceRaw(
  const unsigned char* data, std::size_t blockIndex, std::size_t channelIndex)
{
  const unsigned char* channel = data + HEADER_SIZE + blockIndex * BLOCK_SIZE +
    BLOCK_CHANNEL_OFFSET + channelIndex * CHANNEL_DATA_SIZE;
  return ReadUInt16LE(channel);
}

//-----------------------------------------------------------------------------
inline double GetChannelDistance(
  const unsigned char* data, std::size_t blockIndex, std::size_t channelIndex)
{
  return static_cast<double>(GetChannelDistanceRaw(data, blockIndex, channelIndex)) *
    DISTANCE_SCALE_M;
}

//-----------------------------------------------------------------------------
inline std::uint8_t GetChannelIntensity(
  const unsigned char* data, std::size_t blockIndex, std::size_t channelIndex)
{
  const unsigned char* channel = data + HEADER_SIZE + blockIndex * BLOCK_SIZE +
    BLOCK_CHANNEL_OFFSET + channelIndex * CHANNEL_DATA_SIZE;
  return channel[2];
}

//-----------------------------------------------------------------------------
inline double GetPacketTimestamp(const unsigned char* data)
{
  // 6 bytes seconds + 4 bytes microseconds, both little-endian.
  const unsigned char* ts = data + HEADER_TIMESTAMP_OFFSET;
  const std::uint64_t seconds = static_cast<std::uint64_t>(ReadUInt16LE(ts)) |
    (static_cast<std::uint64_t>(ts[2]) << 16) |
    (static_cast<std::uint64_t>(ts[3]) << 24) |
    (static_cast<std::uint64_t>(ts[4]) << 32) |
    (static_cast<std::uint64_t>(ts[5]) << 40);
  const std::uint32_t microseconds = ReadUInt32LE(ts + 6);
  return static_cast<double>(seconds) + static_cast<double>(microseconds) * 1e-6;
}

} // namespace senfoto008

#endif // Senfoto008PacketFormat_h
