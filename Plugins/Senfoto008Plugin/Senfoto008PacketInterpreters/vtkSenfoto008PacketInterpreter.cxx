/*=========================================================================

  Program:   LidarView
  Module:    vtkSenfoto008PacketInterpreter.cxx

  Copyright (c) Kitware Inc.
  All rights reserved.
  See LICENSE or http://www.apache.org/licenses/LICENSE-2.0 for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

// ============================================================
// 功能：SenFoToView 新增功能 —— Senfoto008 激光雷达数据包解释器实现；
//        将原始 UDP/MSOP 包解码为点云（位置/强度/时间戳等）。
// 作者：acelan
// 新建时间：2026-08-28
// 修改时间：2026-09-01
// ============================================================

#include "vtkSenfoto008PacketInterpreter.h"
#include "InterpreterHelper.h"

#include "Senfoto008PacketFormat.h"

#include <vtkDoubleArray.h>
#include <vtkFloatArray.h>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkUnsignedCharArray.h>

#include <algorithm>
#include <cmath>

namespace
{
// FOV 盲区（方位角接缝）防护参数，借鉴 RoboSense Airy 的 rs_driver
// TwoInOneBlockIterator（block_iterator.hpp）：相邻块方位角跳变超过 1° 视为
// 传感器跳过了接缝盲区，用"标称块间隔"替换实测差值，避免异常跳变传入
// 逐激光方位角插值（SF008 实测每圈在 0° 附近有一次 ~10° 的包内跳变）。
constexpr int AZ_JUMP_THRESHOLD = 100; // 1°，0.01° 单位
constexpr int NOMINAL_PAIR_DIFF = 40;  // 相邻块对标称差 0.4°；块对内两块共享 az（差 0）

// Replicates SenFoTo reference ComputeCorrectedValues azimuth correction for the
// SF008 96-line path. Rotational positions are in 0.01-degree units; luminousMoment
// is 0 for laser 0 and 1 otherwise; Laser_fire_cycle = 18 (from SF.xml calibration).
// The inter-block delta is passed as an unsigned-short in the reference, so negative
// deltas wrap exactly as they do there.
double ComputeCorrectedAzimuthDeg(const int* rotUnits, const int* diffs,
  int blockIdx, std::uint8_t laserId)
{
  constexpr int numBlocks = static_cast<int>(senfoto008::DATA_BLOCKS); // 8
  // 包尾块对（blockIdx=7）的"下一块"在下一包里，包内 diffs[6] 是块对内差（恒 0），
  // 直接回退会让插值修正项丢失（实测 48-95 线 az 步长呈 0.19°/0.61° 交替）。
  // 与 rs_driver TwoInOneBlockIterator 一致：包尾块对使用对间标称差。
  const std::uint16_t adj = static_cast<std::uint16_t>(
    blockIdx == numBlocks - 1 ? NOMINAL_PAIR_DIFF : diffs[blockIdx]);
  const int firingWithinBlock = (laserId >= 48) ? 1 : 0;
  const double luminousMoment = (laserId == 0) ? 0.0 : 1.0;
  const double laserFireCycle = 18.0;
  const double term = static_cast<double>(adj) *
    (luminousMoment + firingWithinBlock * laserFireCycle) / (2.0 * laserFireCycle);
  const unsigned int lastCorrect =
    static_cast<unsigned int>(rotUnits[blockIdx] + term) % 36000;
  return static_cast<double>(lastCorrect) / 100.0;
}

const std::array<double, 96>& GetVerticalAngles96Line()
{
  // SF008 垂直角表（来源 SF.xml vertCorrection，已硬编码；注意步进非严格等差，
  // 0.94~0.95 渐变，勿用 i×0.95 近似核对——index 33 起偏差可达 0.25°）。
  // 参考：RoboSense Airy 实测标定（-0.04°→88.81°，~0.92°/通道）见
  // docs/senfoto008-protocol.md §10。
  static const std::array<double, 96> angles = {
    0.0,    0.95,   1.9,    2.85,   3.8,    4.75,   5.7,    6.65,
    7.6,    8.55,   9.5,    10.45,  11.4,   12.35,  13.3,   14.25,
    15.2,   16.15,  17.1,   18.05,  19.0,   19.95,  20.9,   21.85,
    22.8,   23.75,  24.7,   25.65,  26.6,   27.55,  28.5,   29.45,
    30.4,   31.35,  32.3,   33.24,  34.18,  35.12,  36.06,  37.0,
    37.94,  38.88,  39.82,  40.76,  41.7,   42.64,  43.58,  44.52,
    45.46,  46.4,   47.34,  48.28,  49.22,  50.16,  51.1,   52.04,
    52.98,  53.92,  54.86,  55.8,   56.75,  57.7,   58.65,  59.6,
    60.55,  61.5,   62.45,  63.4,   64.35,  65.3,   66.25,  67.2,
    68.15,  69.1,   70.05,  71.0,   71.95,  72.9,   73.85,  74.8,
    75.75,  76.7,   77.65,  78.6,   79.55,  80.5,   81.45,  82.4,
    83.35,  84.3,   85.25,  86.2,   87.15,  88.1,   89.05,  90.0
  };
  return angles;
}

const std::array<double, 48>& GetVerticalAngles48Line()
{
  // 48 线 = 96 线表的前 48 行
  static const std::array<double, 48> angles = {
      0.0,   0.95,   1.9,    2.85,   3.8,    4.75,   5.7,    6.65,
      7.6,   8.55,   9.5,    10.45,  11.4,   12.35,  13.3,   14.25,
      15.2,  16.15,  17.1,   18.05,  19.0,   19.95,  20.9,   21.85,
      22.8,  23.75,  24.7,    25.65,  26.6,   27.55,  28.5,   29.45,
      30.4,  31.35,  32.3,   33.24,  34.18,  35.12,  36.06,  37.0,
      37.94, 38.88,  39.82,  40.76,  41.7,   42.64,  43.58,  44.52
  };
  return angles;
}

// 水平角修正表（度，每通道，借鉴 Airy horizAdjust：az_final = az + horiz[chan]）。
// 【打墙自标定】两个来源：
//   - test.csv（laser 55-74，墙面选区）：55-69 残差 48-104mm 可信；70 低置信；
//     71/72/74 混入其它物体置 0；73 仅 1 点
//   - plan.csv（laser 0-27，墙面选区）：1-23 残差 86-120mm 可信；0 混点、
//     24-27 在墙顶以外的面，置 0
// 未覆盖的通道（28-54、75-95）待补充打墙数据后继续填充。
const std::array<double, 96>& GetHorizontalCorrections()
{
  static const std::array<double, 96> corrections = {
    //  0-7   （0 混点置 0；1-7 来自 plan.csv）
    0, -0.2516, -0.2390, -0.2507, -0.2084, -0.2258, -0.1904, -0.3620,
    // 8-15
    -0.1928, -0.1635, -0.3694, -0.3266, -0.3616, -0.3022, -0.1541, -0.1532,
    // 16-23
    -0.1022, -0.3075, -0.0637, -0.1913, -0.2661, -0.0821, +0.2179, +0.4142,
    // 24-31  （24-27 在墙顶以外的面，置 0）
    0, 0, 0, 0, 0, 0, 0, 0,
    // 32-39                                           40-47
    0, 0, 0, 0, 0, 0, 0, 0,                           0, 0, 0, 0, 0, 0, 0, 0,
    // 48-54
    0, 0, 0, 0, 0, 0, 0,
    // 55-63（test.csv）
    +0.5407, +0.5514, +0.7152, +0.9040, +0.9193, +1.0139, +1.1118, +1.0704, +0.9346,
    // 64-71
    +0.4078, +0.8515, +1.1563, +1.3558, +1.2464, +1.0497,
    -0.5719, // 70 (低置信, RMS 104mm)
    0,       // 71 (点不在墙面, RMS 628mm)
    // 72-79
    0,       // 72 (同上, RMS 656mm)
    0,       // 73 (仅 1 点)
    0,       // 74 (同上, RMS 299mm)
    0, 0, 0, 0,
    // 80-87
    0, 0, 0, 0, 0, 0, 0, 0,
    // 88-95
    0, 0, 0, 0, 0, 0, 0, 0
  };
  return corrections;
}

} // namespace

//-----------------------------------------------------------------------------
class vtkSenfoto008PacketInterpreter::vtkInternals
{
public:
  vtkSmartPointer<vtkPoints> Points;
  vtkSmartPointer<vtkFloatArray> PointsX;
  vtkSmartPointer<vtkFloatArray> PointsY;
  vtkSmartPointer<vtkFloatArray> PointsZ;
  vtkSmartPointer<vtkUnsignedCharArray> Intensity;
  vtkSmartPointer<vtkUnsignedCharArray> LaserId;
  vtkSmartPointer<vtkDoubleArray> Distance;
  vtkSmartPointer<vtkDoubleArray> Azimuth;
  vtkSmartPointer<vtkDoubleArray> Timestamp;

  double LastBlockAz = 0.0;        // 上一块的方位角（回放/实时用）
  bool HasLastBlockAz = false;
  double PreLastBlockAz = 0.0;     // 上一块的方位角（帧索引预扫描用，与回放状态独立）
  bool HasPreLastBlockAz = false;
  bool WarnedUnknownModel = false;
};

//-----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSenfoto008PacketInterpreter)

//-----------------------------------------------------------------------------
vtkSenfoto008PacketInterpreter::vtkSenfoto008PacketInterpreter()
  : Internals(new vtkSenfoto008PacketInterpreter::vtkInternals())
{
  this->SetSensorVendor("Senfoto");
  this->SetSensorModelName("008");

  this->ResetCurrentFrame();
}

//-----------------------------------------------------------------------------
vtkSenfoto008PacketInterpreter::~vtkSenfoto008PacketInterpreter() = default;

//-----------------------------------------------------------------------------
void vtkSenfoto008PacketInterpreter::Initialize()
{
  auto& internals = this->Internals;
  internals->LastBlockAz = 0.0;
  internals->HasLastBlockAz = false;
  internals->PreLastBlockAz = 0.0;
  internals->HasPreLastBlockAz = false;
  internals->WarnedUnknownModel = false;
  Superclass::Initialize();
}

//-----------------------------------------------------------------------------
bool vtkSenfoto008PacketInterpreter::IsLidarPacket(
  unsigned char const* data, unsigned int dataLength)
{
  return senfoto008::IsValidPacket(data, static_cast<std::size_t>(dataLength));
}

//-----------------------------------------------------------------------------
bool vtkSenfoto008PacketInterpreter::CheckBlockWrap(double blockAz)
{
  auto& internals = this->Internals;
  const bool wrap = internals->HasLastBlockAz && blockAz < internals->LastBlockAz;
  internals->LastBlockAz = blockAz;
  internals->HasLastBlockAz = true;
  return wrap;
}

//-----------------------------------------------------------------------------
bool vtkSenfoto008PacketInterpreter::PreProcessPacket(
  unsigned char const* data, unsigned int dataLength, double& outLidarDataTime)
{
  auto& internals = this->Internals;

  if (!senfoto008::IsValidPacket(data, static_cast<std::size_t>(dataLength)))
  {
    return false;
  }

  outLidarDataTime = senfoto008::GetPacketTimestamp(data);

  // 块级回绕检测（与 ProcessPacket 的拆帧判定一致，保证帧索引与回放边界一致）；
  // 一旦本包内任一块发生回绕，即认为新帧从本包开始。走完全部块以维持状态连续。
  bool isNewFrame = false;
  for (std::size_t b = 0; b < senfoto008::DATA_BLOCKS; ++b)
  {
    const double az = senfoto008::GetBlockAzimuth(data, b);
    const bool wrap = internals->HasPreLastBlockAz && az < internals->PreLastBlockAz;
    internals->PreLastBlockAz = az;
    internals->HasPreLastBlockAz = true;
    if (wrap)
    {
      isNewFrame = true;
    }
  }
  return isNewFrame;
}

//-----------------------------------------------------------------------------
void vtkSenfoto008PacketInterpreter::ProcessPacket(
  unsigned char const* data, unsigned int dataLength)
{
  auto& internals = this->Internals;

  if (!senfoto008::IsValidPacket(data, static_cast<std::size_t>(dataLength)))
  {
    return;
  }

  const std::uint8_t lidarModel = senfoto008::GetLidarModel(data);
  if (lidarModel != senfoto008::LIDAR_MODEL_48_LINE &&
    lidarModel != senfoto008::LIDAR_MODEL_96_LINE)
  {
    if (!internals->WarnedUnknownModel)
    {
      vtkWarningMacro(
        "Senfoto008: unknown Lidar_model 0x" << std::hex << static_cast<int>(lidarModel));
      internals->WarnedUnknownModel = true;
    }
    return;
  }

  this->SetCalibrationForModel(lidarModel);

  vtkDoubleArray* vcArr = vtkDoubleArray::SafeDownCast(
    this->CalibrationData->GetColumnByName("verticalCorrection"));

  // 块级拆帧（借鉴 Airy SplitStrategyByAngle，粒度从包细化到块，0.2°）。
  // 预扫描 8 个块的方位角：回绕处之后的块属于新一圈。
  double blockAzimuths[senfoto008::DATA_BLOCKS] = {};
  int wrapIdx = -1;
  for (std::size_t b = 0; b < senfoto008::DATA_BLOCKS; ++b)
  {
    blockAzimuths[b] = senfoto008::GetBlockAzimuth(data, b);
    if (wrapIdx < 0 && this->CheckBlockWrap(blockAzimuths[b]))
    {
      wrapIdx = static_cast<int>(b);
    }
  }
  if (wrapIdx >= 0 && this->CurrentFrame && this->CurrentFrame->GetNumberOfPoints() > 0)
  {
    Superclass::SplitFrame();
  }
  // wrapIdx > 0 时，本包前 wrapIdx 个块在物理上属于上一圈。帧索引以包为粒度，
  // 回放无法把它们归还给上一帧，为保持帧首尾干净予以丢弃（约占单圈 0.06%）。

  const double packetTimestamp = senfoto008::GetPacketTimestamp(data);

  if (lidarModel == senfoto008::LIDAR_MODEL_48_LINE)
  {
    for (std::size_t blockIndex = 0; blockIndex < senfoto008::DATA_BLOCKS; ++blockIndex)
    {
      if (wrapIdx >= 0 && static_cast<int>(blockIndex) < wrapIdx)
      {
        continue;
      }
      if (!senfoto008::IsValidDataBlock(data, blockIndex))
      {
        continue;
      }
      const double azimuth = blockAzimuths[blockIndex];
      for (std::size_t channelIndex = 0; channelIndex < senfoto008::CHANNELS_PER_BLOCK;
           ++channelIndex)
      {
        const std::uint16_t distRaw =
          senfoto008::GetChannelDistanceRaw(data, blockIndex, channelIndex);
        if (distRaw == senfoto008::INVALID_DISTANCE)
        {
          continue;
        }
        const double distM = senfoto008::GetChannelDistance(data, blockIndex, channelIndex);
        const std::uint8_t intensity =
          senfoto008::GetChannelIntensity(data, blockIndex, channelIndex);
        const double elevationDeg =
          vcArr ? vcArr->GetTuple1(static_cast<vtkIdType>(channelIndex)) : 0.0;
        this->AddPoint(azimuth, elevationDeg, distM,
          static_cast<std::uint8_t>(channelIndex), intensity, packetTimestamp);
      }
    }
  }
  else // 96-line single return
  {
    // Precompute per-block rotational position (0.01-degree units) and the
    // inter-block azimuth deltas, mirroring the SenFoTo reference
    // (getRotationalPosition + diffs[] in ProcessPacket, HDL_FIRING_PER_PKT = 8).
    constexpr int numBlocks = static_cast<int>(senfoto008::DATA_BLOCKS); // 8
    int rotUnits[numBlocks];
    for (int b = 0; b < numBlocks; ++b)
    {
      rotUnits[b] = static_cast<int>(
        std::round(senfoto008::GetBlockAzimuth(data, b) * 100.0));
    }
    int diffs[numBlocks - 1];
    for (int b = 0; b < numBlocks - 1; ++b)
    {
      int diff = (36000 + 18000 + rotUnits[b + 1] - rotUnits[b]) % 36000 - 18000;
      // 盲区防护：|差值| > 1° 说明跨过了接缝，改用标称间隔（块对内 0、块对间 0.4°）
      if (diff > AZ_JUMP_THRESHOLD || diff < -AZ_JUMP_THRESHOLD)
      {
        diff = (b % 2 == 0) ? 0 : NOMINAL_PAIR_DIFF;
      }
      diffs[b] = diff;
    }

    for (std::size_t pairIndex = 0; pairIndex < senfoto008::DATA_BLOCKS / 2; ++pairIndex)
    {
      const std::size_t firstBlock = pairIndex * 2;
      const std::size_t secondBlock = firstBlock + 1;
      if (wrapIdx >= 0 && static_cast<int>(firstBlock) < wrapIdx)
      {
        continue;
      }
      if (!senfoto008::IsValidDataBlock(data, firstBlock) ||
        !senfoto008::IsValidDataBlock(data, secondBlock))
      {
        continue;
      }

      for (std::size_t channelIndex = 0; channelIndex < senfoto008::CHANNELS_PER_BLOCK;
           ++channelIndex)
      {
        // First sub-block: lasers 0..47, firingWithinBlock = 0.
        const std::uint16_t distRawFirst =
          senfoto008::GetChannelDistanceRaw(data, firstBlock, channelIndex);
        if (distRawFirst != senfoto008::INVALID_DISTANCE)
        {
          const std::uint8_t laserId = static_cast<std::uint8_t>(channelIndex);
          const double distM = senfoto008::GetChannelDistance(data, firstBlock, channelIndex);
          const std::uint8_t intensity =
            senfoto008::GetChannelIntensity(data, firstBlock, channelIndex);
          const double azimuthDeg =
            ComputeCorrectedAzimuthDeg(rotUnits, diffs, static_cast<int>(firstBlock), laserId);
          this->AddPoint(azimuthDeg, vcArr ? vcArr->GetTuple1(static_cast<vtkIdType>(laserId)) : 0.0, distM, laserId, intensity, packetTimestamp);
        }

        // Second sub-block: lasers 48..95, firingWithinBlock = 1.
        const std::uint16_t distRawSecond =
          senfoto008::GetChannelDistanceRaw(data, secondBlock, channelIndex);
        if (distRawSecond != senfoto008::INVALID_DISTANCE)
        {
          const std::uint8_t laserId =
            static_cast<std::uint8_t>(channelIndex + senfoto008::CHANNELS_PER_BLOCK);
          const double distM = senfoto008::GetChannelDistance(data, secondBlock, channelIndex);
          const std::uint8_t intensity =
            senfoto008::GetChannelIntensity(data, secondBlock, channelIndex);
          const double azimuthDeg =
            ComputeCorrectedAzimuthDeg(rotUnits, diffs, static_cast<int>(secondBlock), laserId);
          this->AddPoint(azimuthDeg, vcArr ? vcArr->GetTuple1(static_cast<vtkIdType>(laserId)) : 0.0, distM, laserId, intensity, packetTimestamp);
        }
      }
    }
  }
}

//-----------------------------------------------------------------------------
bool vtkSenfoto008PacketInterpreter::IsAzimuthInRange(double azimuthDeg) const
{
  const double start = this->StartAngle;
  const double end = this->EndAngle;
  if (start <= end)
  {
    return azimuthDeg >= start && azimuthDeg <= end;
  }
  // Wrap-around FOV (e.g. 350 deg -> 10 deg)
  return azimuthDeg >= start || azimuthDeg <= end;
}

//-----------------------------------------------------------------------------
void vtkSenfoto008PacketInterpreter::AddPoint(double azimuthDeg, double elevationDeg,
  double distanceM, std::uint8_t laserId, std::uint8_t intensity, double timestamp)
{
  // 【实验】水平角修正（借鉴 Airy horizAdjust）：az += horiz[laserId]
  azimuthDeg += GetHorizontalCorrections()[laserId];

  // Distance range filter (mirrors RoboSense Airy distance_section_.in())
  if (distanceM < this->MinDistance ||
    (this->MaxDistance > 0.0 && distanceM > this->MaxDistance))
  {
    return;
  }
  // Azimuth / FOV range filter (mirrors RoboSense Airy scan_section_.in())
  if (!this->IsAzimuthInRange(azimuthDeg))
  {
    return;
  }

  auto& internals = this->Internals;

  const double azRad = vtkMath::RadiansFromDegrees(azimuthDeg);
  const double elRad = vtkMath::RadiansFromDegrees(elevationDeg);
  const double cosEl = std::cos(elRad);

  const double pos[3] = { distanceM * cosEl * std::cos(azRad),
    -distanceM * cosEl * std::sin(azRad), distanceM * std::sin(elRad) };

  internals->Points->InsertNextPoint(pos);
  InsertNextValueIfNotNull(internals->PointsX, static_cast<float>(pos[0]));
  InsertNextValueIfNotNull(internals->PointsY, static_cast<float>(pos[1]));
  InsertNextValueIfNotNull(internals->PointsZ, static_cast<float>(pos[2]));
  InsertNextValueIfNotNull(internals->Intensity, intensity);
  InsertNextValueIfNotNull(internals->LaserId, laserId);
  InsertNextValueIfNotNull(internals->Distance, distanceM);
  InsertNextValueIfNotNull(internals->Azimuth, azimuthDeg);
  InsertNextValueIfNotNull(internals->Timestamp, timestamp);
}

//-----------------------------------------------------------------------------
vtkSmartPointer<vtkPolyData> vtkSenfoto008PacketInterpreter::CreateNewEmptyFrame(
  vtkIdType nbrOfPoints, vtkIdType prereservedNbrOfPoints)
{
  const int defaultPrereservedNbrOfPointsPerFrame = 60000;
  prereservedNbrOfPoints = std::max(
    static_cast<int>(prereservedNbrOfPoints * 1.5), defaultPrereservedNbrOfPointsPerFrame);

  vtkSmartPointer<vtkPolyData> polyData = vtkSmartPointer<vtkPolyData>::New();

  vtkNew<vtkPoints> points;
  points->SetDataTypeToFloat();
  points->Allocate(prereservedNbrOfPoints);
  if (nbrOfPoints > 0)
  {
    points->SetNumberOfPoints(nbrOfPoints);
  }
  points->GetData()->SetName("Points");
  polyData->SetPoints(points.GetPointer());

  auto& internals = this->Internals;
  internals->Points = points.GetPointer();

  // clang-format off
  InitArrayForPolyData(true, internals->PointsX, "X", nbrOfPoints, prereservedNbrOfPoints, polyData, this->EnableAdvancedArrays);
  InitArrayForPolyData(true, internals->PointsY, "Y", nbrOfPoints, prereservedNbrOfPoints, polyData, this->EnableAdvancedArrays);
  InitArrayForPolyData(true, internals->PointsZ, "Z", nbrOfPoints, prereservedNbrOfPoints, polyData, this->EnableAdvancedArrays);
  InitArrayForPolyData(false, internals->Intensity, "intensity", nbrOfPoints, prereservedNbrOfPoints, polyData);
  InitArrayForPolyData(false, internals->LaserId, "laser_id", nbrOfPoints, prereservedNbrOfPoints, polyData);
  InitArrayForPolyData(false, internals->Timestamp, "timestamp", nbrOfPoints, prereservedNbrOfPoints, polyData);
  InitArrayForPolyData(false, internals->Distance, "distance_m", nbrOfPoints, prereservedNbrOfPoints, polyData);
  InitArrayForPolyData(false, internals->Azimuth, "azimuth", nbrOfPoints, prereservedNbrOfPoints, polyData);
  // clang-format on

  polyData->GetPointData()->SetActiveScalars("intensity");
  return polyData;
}

//-----------------------------------------------------------------------------
void vtkSenfoto008PacketInterpreter::SetCalibrationForModel(std::uint8_t lidarModel)
{
  if (this->CalibrationReportedNumLasers > 0 &&
      this->CalibrationData->GetColumnByName("verticalCorrection") != nullptr)
  {
    return;
  }
  auto fill = [this](const auto& angles) {
    this->CalibrationReportedNumLasers = static_cast<int>(angles.size());
    vtkNew<vtkDoubleArray> vc;
    vc->SetName("verticalCorrection");
    vc->SetNumberOfComponents(1);
    vc->SetNumberOfTuples(static_cast<vtkIdType>(angles.size()));
    for (std::size_t i = 0; i < angles.size(); ++i)
    {
      vc->SetTuple1(static_cast<vtkIdType>(i), angles[i]);
    }
    this->CalibrationData->AddColumn(vc);
  };
  if (lidarModel == senfoto008::LIDAR_MODEL_48_LINE)
  {
    fill(GetVerticalAngles48Line());
  }
  else
  {
    fill(GetVerticalAngles96Line());
  }
}
