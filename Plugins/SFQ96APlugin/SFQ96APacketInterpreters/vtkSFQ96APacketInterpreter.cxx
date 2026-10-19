/*=========================================================================

  Program:   LidarView
  Module:    vtkSFQ96APacketInterpreter.cxx

  Copyright (c) Kitware Inc.
  All rights reserved.
  See LICENSE or http://www.apache.org/licenses/LICENSE-2.0 for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkSFQ96APacketInterpreter.h"
#include "InterpreterHelper.h"

#include <vtk_rs_driver.h>

#include <vtkDoubleArray.h>
#include <vtkPointData.h>
#include <vtkPoints.h>

#include <cstring>

namespace rs_lidar = robosense::lidar;

typedef PointXYZIRT PointT;
typedef PointCloudT<PointT> RBPointCloud;

// DIFOP packet (UDP port 7788) carries the motor speed: 2-byte big-endian RPM
// at this payload offset. Verified on capture: 0x0258 = 600 at ~1 Hz.
constexpr unsigned int DIFOP_RPM_OFFSET = 373;

//-----------------------------------------------------------------------------
class vtkSFQ96APacketInterpreter::vtkInternals
{
public:
  vtkInternals()
    : SFQ96APointCloud(std::make_shared<RBPointCloud>())
  {
  }

  bool IsDifopPacket(const uint8_t* pkt)
  {
    const rs_lidar::RSDecoderConstParam& param = this->GetDecoderConstParam();
    return std::memcmp(pkt, param.DIFOP_ID, param.DIFOP_ID_LEN) == 0;
  }

  bool IsMsopPacket(const uint8_t* pkt)
  {
    const rs_lidar::RSDecoderConstParam& param = this->GetDecoderConstParam();
    return std::memcmp(pkt, param.MSOP_ID, param.MSOP_ID_LEN) == 0;
  }

  void CreateDecoder(rs_lidar::LidarType& lidarType, const rs_lidar::RSDecoderParam& decoderParams)
  {
    // TODO: Check if is valid type? != 0
    this->LidarType = lidarType;
    this->Decoder = rs_lidar::DecoderFactory<RBPointCloud>::createDecoder(lidarType, decoderParams);
    this->Decoder->point_cloud_ = this->SFQ96APointCloud;
    this->Decoder->getDecoderConstParam(this->DecoderConstParam);
  }

  const rs_lidar::LidarType& GetLidarType() const { return this->LidarType; }

  std::shared_ptr<rs_lidar::Decoder<RBPointCloud>>& GetDecoder() { return this->Decoder; }

  const rs_lidar::RSDecoderConstParam& GetDecoderConstParam() const
  {
    return this->DecoderConstParam;
  }

  std::shared_ptr<RBPointCloud>& GetCloud() { return this->SFQ96APointCloud; }

  void RunExceptionCallback(const robosense::lidar::Error& error) const
  {
    vtkGenericWarningMacro("SFQ96A driver internal error: " << error.toString());
  }

  bool IsPreProcessed = false;
  bool CurrentlyPreProcessing = false;

private:
  rs_lidar::LidarType LidarType = rs_lidar::RSAIRY;
  std::shared_ptr<rs_lidar::Decoder<RBPointCloud>> Decoder;
  rs_lidar::RSDecoderConstParam DecoderConstParam;
  std::shared_ptr<RBPointCloud> SFQ96APointCloud;
};

//-----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSFQ96APacketInterpreter)

//-----------------------------------------------------------------------------
vtkSFQ96APacketInterpreter::vtkSFQ96APacketInterpreter()
  : Internals(new vtkSFQ96APacketInterpreter::vtkInternals())
{
  this->ResetCurrentFrame();
  this->SetSensorVendor("SFQ96A");
  this->LidarModel = static_cast<int>(rs_lidar::RSAIRY);
  this->SetSensorModelName(rs_lidar::lidarTypeToStr(rs_lidar::RSAIRY));
}

//-----------------------------------------------------------------------------
void vtkSFQ96APacketInterpreter::ProcessPacket(unsigned char const* data,
  unsigned int dataLength)
{
  auto& internals = this->Internals;
  const uint8_t* pkt = reinterpret_cast<const uint8_t*>(data);

  if (internals->IsMsopPacket(pkt))
  {
    internals->GetDecoder()->processMsopPkt(pkt, dataLength);
  }
  else if (internals->IsDifopPacket(pkt))
  {
    internals->GetDecoder()->processDifopPkt(pkt, dataLength);
  }
  else
  {
    vtkWarningMacro("Not a Msop nor a Difop packet.");
  }
}

//-----------------------------------------------------------------------------
bool vtkSFQ96APacketInterpreter::IsLidarPacket(unsigned char const* data,
  unsigned int dataLength)
{
  // Check for DIFOP packet and save adapter
  const uint8_t* pkt = reinterpret_cast<const uint8_t*>(data);
  if (this->Internals->IsDifopPacket(pkt))
  {
    if (dataLength > DIFOP_RPM_OFFSET + 1)
    {
      this->Rpm = static_cast<double>(
        (static_cast<unsigned int>(pkt[DIFOP_RPM_OFFSET]) << 8) |
        static_cast<unsigned int>(pkt[DIFOP_RPM_OFFSET + 1]));
    }
    this->Internals->GetDecoder()->processDifopPkt(pkt, dataLength);
    Superclass::Initialize();
    // If this is true Difop packets will be called by ProcessPacket
    return true;
  }

  return this->Internals->IsMsopPacket(pkt);
}

//-----------------------------------------------------------------------------
vtkSmartPointer<vtkPolyData> vtkSFQ96APacketInterpreter::CreateNewEmptyFrame(
  vtkIdType nbrOfPoints,
  vtkIdType prereservedNbrOfPoints)
{
  const int defaultPrereservedNbrOfPointsPerFrame = 60000;
  // prereserve for 50% points more than actually received in previous frame
  prereservedNbrOfPoints =
    std::max(static_cast<int>(prereservedNbrOfPoints * 1.5), defaultPrereservedNbrOfPointsPerFrame);

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

  this->Points = points.GetPointer();
  // clang-format off
  InitArrayForPolyData(true, this->PointsX, "X", nbrOfPoints, prereservedNbrOfPoints, polyData, this->EnableAdvancedArrays);
  InitArrayForPolyData(true, this->PointsY, "Y", nbrOfPoints, prereservedNbrOfPoints, polyData, this->EnableAdvancedArrays);
  InitArrayForPolyData(true, this->PointsZ, "Z", nbrOfPoints, prereservedNbrOfPoints, polyData, this->EnableAdvancedArrays);
  InitArrayForPolyData(false, this->Intensity, "intensity", nbrOfPoints, prereservedNbrOfPoints, polyData);
  InitArrayForPolyData(false, this->LaserId, "laser_id", nbrOfPoints, prereservedNbrOfPoints, polyData);
  InitArrayForPolyData(false, this->Timestamp, "timestamp", nbrOfPoints, prereservedNbrOfPoints, polyData);
  InitArrayForPolyData(false, this->Distance, "distance_m", nbrOfPoints, prereservedNbrOfPoints, polyData);
  // clang-format on

  // Set the default array to display in the application
  polyData->GetPointData()->SetActiveScalars("intensity");
  return polyData;
}

//-----------------------------------------------------------------------------
bool vtkSFQ96APacketInterpreter::PreProcessPacket(unsigned char const* data,
  unsigned int dataLength,
  double& outLidarDataTime)
{
  auto& internals = this->Internals;
  internals->IsPreProcessed = true;

  const uint8_t* pkt = reinterpret_cast<const uint8_t*>(data);

  bool splitPacket = false;
  if (internals->IsMsopPacket(pkt) && this->GetIsInitialized())
  {
    internals->CurrentlyPreProcessing = true;
    splitPacket = internals->GetDecoder()->processMsopPkt(pkt, dataLength);
    internals->CurrentlyPreProcessing = false;
  }

  outLidarDataTime = internals->GetDecoder()->prevPktTs();

  // In pre-processing we do not want to keep points.
  // Ideally we should do all process in one step, reading the pcap
  // twice slow the pcap reader.
  internals->GetCloud()->points.clear();
  return splitPacket;
}

//-----------------------------------------------------------------------------
void vtkSFQ96APacketInterpreter::SetLidarModel(int model)
{
  if (this->LidarModel == model)
  {
    return;
  }

  rs_lidar::LidarType type = static_cast<rs_lidar::LidarType>(model);
  if (rs_lidar::isMech(type) || rs_lidar::isMems(type))
  {
    this->LidarModel = model;
    this->SetSensorModelName(rs_lidar::lidarTypeToStr(type));
    this->Modified();
    this->ResetInitializedState();
  }
}

//-----------------------------------------------------------------------------
void vtkSFQ96APacketInterpreter::Initialize()
{
  auto& internals = this->Internals;
  // SFQ96A sensors are not forced to use calibration files. (But they could)
  rs_lidar::LidarType type = static_cast<rs_lidar::LidarType>(this->LidarModel);
  // Default parameters
  rs_lidar::RSDecoderParam decoderParams;
  decoderParams.use_lidar_clock = true;
  internals->CreateDecoder(type, decoderParams);

  // clang-format off
  internals->GetDecoder()->regCallback(
    std::bind(&vtkSFQ96APacketInterpreter::vtkInternals::RunExceptionCallback, internals.get(), std::placeholders::_1),
    std::bind(&vtkSFQ96APacketInterpreter::SplitFrame, this, std::placeholders::_1, std::placeholders::_2));
  // clang-format on
}

//-----------------------------------------------------------------------------
bool vtkSFQ96APacketInterpreter::SplitFrame(uint16_t vtkNotUsed(height),
  double vtkNotUsed(timestamp))
{
  auto& internals = this->Internals;
  // Ignore split frame call while PreProcessing frames for indexing.
  if (internals->CurrentlyPreProcessing)
  {
    return false;
  }

  const rs_lidar::RSDecoderConstParam& param = internals->GetDecoderConstParam();
  // Number of point in a set of blocks
  const uint32_t numberOfPktPoints = param.BLOCKS_PER_PKT * param.CHANNELS_PER_BLOCK;

  auto& cloud = internals->GetCloud();

  // Do not split the frame if the registered points are only the ones of current packet
  // Also if this interpreter is used in pcap reader mode (`IsPreProcessed`) avoid overlapping
  // points, which are before and after the splitting point.
  if (cloud->points.size() <= numberOfPktPoints * 2)
  {
    if (internals->IsPreProcessed)
    {
      cloud->points.clear();
    }
    return false;
  }

  const double timestampLastPoint = cloud->points.back().timestamp;
  for (auto& point : cloud->points)
  {
    // Ignore points that remains from last splitting points.
    if (timestampLastPoint < point.timestamp)
    {
      continue;
    }
    double pos[3] = { point.x, point.y, point.z };
    if (std::isnan(point.x) || std::isnan(point.y) || std::isnan(point.z))
    {
      continue;
    }
    double distance =
      std::sqrt(std::pow(0 - point.x, 2) + std::pow(0 - point.y, 2) + std::pow(0 - point.z, 2));
    this->Points->InsertNextPoint(pos);
    InsertNextValueIfNotNull(this->PointsX, pos[0]);
    InsertNextValueIfNotNull(this->PointsY, pos[1]);
    InsertNextValueIfNotNull(this->PointsZ, pos[2]);
    InsertNextValueIfNotNull(this->Intensity, point.intensity);
    InsertNextValueIfNotNull(this->LaserId, point.ring);
    InsertNextValueIfNotNull(this->Timestamp, point.timestamp);
    InsertNextValueIfNotNull(this->Distance, distance);
  }
  cloud->points.clear();
  return this->Superclass::SplitFrame();
}

//----------------------------------------------------------------------------
void vtkSFQ96APacketInterpreter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
