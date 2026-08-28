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
// 修改时间：2026-08-28
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
// Replicates SenFoTo reference ComputeCorrectedValues azimuth correction for the
// SF008 96-line path. Rotational positions are in 0.01-degree units; luminousMoment
// is 0 for laser 0 and 1 otherwise; Laser_fire_cycle = 18 (from SF.xml calibration).
// The inter-block delta is passed as an unsigned-short in the reference, so negative
// deltas wrap exactly as they do there.
double ComputeCorrectedAzimuthDeg(const int* rotUnits, const int* diffs,
  int blockIdx, std::uint8_t laserId)
{
  constexpr int numBlocks = static_cast<int>(senfoto008::DATA_BLOCKS); // 8
  const int diffIdx = (blockIdx == numBlocks - 1) ? numBlocks - 2 : blockIdx;
  const std::uint16_t adj = static_cast<std::uint16_t>(diffs[diffIdx]);
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

  double LastAzimuth = 0.0;
  bool HasLastAzimuth = false;
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
  internals->LastAzimuth = 0.0;
  internals->HasLastAzimuth = false;
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
bool vtkSenfoto008PacketInterpreter::PreProcessPacket(
  unsigned char const* data, unsigned int dataLength, double& outLidarDataTime)
{
  auto& internals = this->Internals;

  if (!senfoto008::IsValidPacket(data, static_cast<std::size_t>(dataLength)))
  {
    return false;
  }

  const double currentAzimuth = senfoto008::GetBlockAzimuth(data, 0);
  outLidarDataTime = senfoto008::GetPacketTimestamp(data);

  bool isNewFrame = false;
  if (internals->HasLastAzimuth && currentAzimuth < internals->LastAzimuth)
  {
    isNewFrame = true;
  }

  internals->LastAzimuth = currentAzimuth;
  internals->HasLastAzimuth = true;

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

  const double currentAzimuth = senfoto008::GetBlockAzimuth(data, 0);
  if (internals->HasLastAzimuth && currentAzimuth < internals->LastAzimuth)
  {
    if (this->CurrentFrame && this->CurrentFrame->GetNumberOfPoints() > 0)
    {
      Superclass::SplitFrame();
    }
  }
  internals->LastAzimuth = currentAzimuth;
  internals->HasLastAzimuth = true;

  const double packetTimestamp = senfoto008::GetPacketTimestamp(data);

  if (lidarModel == senfoto008::LIDAR_MODEL_48_LINE)
  {
    for (std::size_t blockIndex = 0; blockIndex < senfoto008::DATA_BLOCKS; ++blockIndex)
    {
      if (!senfoto008::IsValidDataBlock(data, blockIndex))
      {
        continue;
      }
      const double azimuth = senfoto008::GetBlockAzimuth(data, blockIndex);
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
      diffs[b] = (36000 + 18000 + rotUnits[b + 1] - rotUnits[b]) % 36000 - 18000;
    }

    for (std::size_t pairIndex = 0; pairIndex < senfoto008::DATA_BLOCKS / 2; ++pairIndex)
    {
      const std::size_t firstBlock = pairIndex * 2;
      const std::size_t secondBlock = firstBlock + 1;
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
