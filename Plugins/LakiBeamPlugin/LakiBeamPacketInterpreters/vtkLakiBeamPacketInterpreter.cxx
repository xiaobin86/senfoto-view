/*=========================================================================

  Program:   LidarView
  Module:    vtkLakiBeamPacketInterpreter.cxx

  Copyright (c) Kitware Inc.
  All rights reserved.
  See LICENSE or http://www.apache.org/licenses/LICENSE-2.0 for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkLakiBeamPacketInterpreter.h"
#include "InterpreterHelper.h"

#include "LakiBeamPacketFormat.h"

#include <vtkDoubleArray.h>
#include <vtkFloatArray.h>
#include <vtkMath.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkUnsignedCharArray.h>

#include <algorithm>
#include <cmath>

//-----------------------------------------------------------------------------
class vtkLakiBeamPacketInterpreter::vtkInternals
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
};

//-----------------------------------------------------------------------------
vtkStandardNewMacro(vtkLakiBeamPacketInterpreter)

//-----------------------------------------------------------------------------
vtkLakiBeamPacketInterpreter::vtkLakiBeamPacketInterpreter()
  : Internals(new vtkLakiBeamPacketInterpreter::vtkInternals())
{
  // These information will be stored in point cloud field data
  this->SetSensorVendor("RichBeam");
  this->SetSensorModelName("LakiBeam1");

  this->ResetCurrentFrame();
}

//-----------------------------------------------------------------------------
vtkLakiBeamPacketInterpreter::~vtkLakiBeamPacketInterpreter() = default;

//-----------------------------------------------------------------------------
void vtkLakiBeamPacketInterpreter::Initialize()
{
  this->Internals->LastAzimuth = 0.0;
  this->Internals->HasLastAzimuth = false;
  Superclass::Initialize();
}

//-----------------------------------------------------------------------------
bool vtkLakiBeamPacketInterpreter::IsLidarPacket(unsigned char const* data, unsigned int dataLength)
{
  if (dataLength != lakibeam::MSOP_PACKET_SIZE)
  {
    return false;
  }
  return lakibeam::IsValidDataBlock(data, 0);
}

//-----------------------------------------------------------------------------
bool vtkLakiBeamPacketInterpreter::PreProcessPacket(unsigned char const* data,
  unsigned int dataLength,
  double& outLidarDataTime)
{
  auto& internals = this->Internals;

  if (dataLength != lakibeam::MSOP_PACKET_SIZE || !lakibeam::IsValidDataBlock(data, 0))
  {
    return false;
  }

  const double currentAzimuth = lakibeam::GetBlockAzimuth(data, 0);
  outLidarDataTime = static_cast<double>(lakibeam::GetPacketTimestampUs(data)) * 1e-6;

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
void vtkLakiBeamPacketInterpreter::ProcessPacket(unsigned char const* data, unsigned int dataLength)
{
  auto& internals = this->Internals;

  if (dataLength != lakibeam::MSOP_PACKET_SIZE || !lakibeam::IsValidDataBlock(data, 0))
  {
    return;
  }

  const double currentAzimuth = lakibeam::GetBlockAzimuth(data, 0);

  if (internals->HasLastAzimuth && currentAzimuth < internals->LastAzimuth)
  {
    if (this->CurrentFrame && this->CurrentFrame->GetNumberOfPoints() > 0)
    {
      Superclass::SplitFrame();
    }
  }
  internals->LastAzimuth = currentAzimuth;
  internals->HasLastAzimuth = true;

  const double packetTimestamp = static_cast<double>(lakibeam::GetPacketTimestampUs(data)) * 1e-6;

  // Pre-compute azimuths for intra-block interpolation. The resolution is the
  // difference between the first two blocks divided by the number of points per
  // block, mirroring the official LakiBeam ROS2 driver.
  double azimuths[lakibeam::MSOP_DATA_BLOCKS];
  for (std::size_t blockIndex = 0; blockIndex < lakibeam::MSOP_DATA_BLOCKS; ++blockIndex)
  {
    azimuths[blockIndex] = lakibeam::GetBlockAzimuth(data, blockIndex);
  }

  double resolution = 0.0;
  if (lakibeam::MSOP_DATA_BLOCKS >= 2)
  {
    double diff = azimuths[1] - azimuths[0];
    if (diff > 0.0)
    {
      resolution = diff / static_cast<double>(lakibeam::MSOP_POINTS_PER_BLOCK);
    }
  }

  for (std::size_t blockIndex = 0; blockIndex < lakibeam::MSOP_DATA_BLOCKS; ++blockIndex)
  {
    if (!lakibeam::IsValidDataBlock(data, blockIndex))
    {
      continue;
    }

    const unsigned char* block = data + blockIndex * lakibeam::MSOP_BLOCK_SIZE;
    const double baseAzimuth = azimuths[blockIndex];

    for (std::size_t pointIndex = 0; pointIndex < lakibeam::MSOP_POINTS_PER_BLOCK; ++pointIndex)
    {
      const unsigned char* result = block + lakibeam::MSOP_RESULT_OFFSET +
        pointIndex * lakibeam::MSOP_RESULT_SIZE;
      const std::uint16_t dist_mm = lakibeam::ReadUInt16LE(result);
      if (dist_mm == lakibeam::INVALID_DIST)
      {
        continue;
      }

      const double angleDeg = baseAzimuth + resolution * static_cast<double>(pointIndex);
      const double distM = static_cast<double>(dist_mm) * 0.001;
      const double azRad = vtkMath::RadiansFromDegrees(angleDeg);

      // LakiBeam is a single-line horizontal-scan lidar; points lie in the x-y plane.
      const double pos[3] = { distM * std::cos(azRad), distM * std::sin(azRad), 0.0 };

      internals->Points->InsertNextPoint(pos);
      InsertNextValueIfNotNull(internals->PointsX, static_cast<float>(pos[0]));
      InsertNextValueIfNotNull(internals->PointsY, static_cast<float>(pos[1]));
      InsertNextValueIfNotNull(internals->PointsZ, static_cast<float>(pos[2]));
      InsertNextValueIfNotNull(internals->Intensity, static_cast<unsigned char>(result[2]));
      InsertNextValueIfNotNull(internals->LaserId, static_cast<unsigned char>(0));
      InsertNextValueIfNotNull(internals->Distance, distM);
      InsertNextValueIfNotNull(internals->Azimuth, angleDeg);
      InsertNextValueIfNotNull(internals->Timestamp, packetTimestamp);
    }
  }
}

//-----------------------------------------------------------------------------
vtkSmartPointer<vtkPolyData> vtkLakiBeamPacketInterpreter::CreateNewEmptyFrame(vtkIdType nbrOfPoints,
  vtkIdType prereservedNbrOfPoints)
{
  const int defaultPrereservedNbrOfPointsPerFrame = 60000;
  // Prereserve for 50% points more than actually received in previous frame.
  prereservedNbrOfPoints =
    std::max(static_cast<int>(prereservedNbrOfPoints * 1.5), defaultPrereservedNbrOfPointsPerFrame);

  vtkSmartPointer<vtkPolyData> polyData = vtkSmartPointer<vtkPolyData>::New();

  // Initialize points
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

  // Set the default array to display in the application
  polyData->GetPointData()->SetActiveScalars("intensity");
  return polyData;
}
