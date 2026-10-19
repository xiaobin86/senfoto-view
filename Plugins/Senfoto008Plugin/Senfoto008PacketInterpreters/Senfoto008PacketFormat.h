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

// ============================================================
// 功能：SenFoToView 新增功能 —— 定义 Senfoto008 数据包的二进制线格式
//        与结构体（包类型、帧头、点字段布局等）。
// 作者：acelan
// 新建时间：2026-08-28
// 修改时间：2026-10-19
// ============================================================

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
// RPM field: bytes at offset 4 (high) and 5 (low); concatenated and read as a
// hexadecimal value (i.e. big-endian 16-bit) to give revolutions per minute.
constexpr std::size_t HEADER_RPM_HIGH_OFFSET = 4;
constexpr std::size_t HEADER_RPM_LOW_OFFSET = 5;

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

constexpr std::uint16_t INVALID_DISTANCE = 0;
// Azimuth scale: 0.01 degrees per LSB.
constexpr double AZIMUTH_SCALE_DEG = 0.01;


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
// SF008 distance encoding (matches SenFoTo reference getDisFromBytes):
// the two distance bytes are read little-endian, then byte-swapped and
// interpreted as a 12.4 fixed-point value, scaled by 0.15 m.
inline double GetChannelDistance(
  const unsigned char* data, std::size_t blockIndex, std::size_t channelIndex)
{
  const std::uint16_t le = GetChannelDistanceRaw(data, blockIndex, channelIndex);
  const std::uint16_t swapped =
    static_cast<std::uint16_t>((le >> 8) | (le << 8));
  const std::uint16_t high12Bits = (swapped >> 4) & 0xFFF;
  const std::uint16_t low4Bits = swapped & 0xF;
  const double result =
    static_cast<double>(high12Bits) + static_cast<double>(low4Bits) / 16.0;
  return result * 0.15;
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
inline std::uint16_t GetRpmRaw(const unsigned char* data)
{
  return static_cast<std::uint16_t>(
    (static_cast<std::uint16_t>(data[HEADER_RPM_HIGH_OFFSET]) << 8) |
    static_cast<std::uint16_t>(data[HEADER_RPM_LOW_OFFSET]));
}

//-----------------------------------------------------------------------------
inline double GetRpm(const unsigned char* data)
{
  return static_cast<double>(GetRpmRaw(data));
}

// Timestamp tick duration: the 10-byte counter runs at 200 MHz (5 ns/tick).
// Verified against pcap capture elapsed time: consecutive MSOP packets 440 us
// apart differ by 88000 ticks => 88000 / 440e-6 = 2.0e8 ticks/s.
constexpr double TIMESTAMP_TICK_SECONDS = 5e-9;

//-----------------------------------------------------------------------------
inline double GetPacketTimestamp(const unsigned char* data)
{
  // 10-byte big-endian tick counter (5 ns per tick), see TIMESTAMP_TICK_SECONDS.
  const unsigned char* ts = data + HEADER_TIMESTAMP_OFFSET;
  double ticks = 0.0;
  for (std::size_t i = 0; i < HEADER_TIMESTAMP_SIZE; ++i)
  {
    ticks = ticks * 256.0 + static_cast<double>(ts[i]);
  }
  return ticks * TIMESTAMP_TICK_SECONDS;
}

} // namespace senfoto008

#endif // Senfoto008PacketFormat_h
