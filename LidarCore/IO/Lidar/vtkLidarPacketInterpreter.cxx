/*=========================================================================

  Program: LidarView
  Module:  vtkLidarPacketInterpreter.cxx

  Copyright (c) Kitware Inc.
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkLidarPacketInterpreter.h"

#include <vtkDoubleArray.h>
#include <vtkExtractSelection.h>
#include <vtkFieldData.h>
#include <vtkIdTypeArray.h>
#include <vtkLVUtilities.h>
#include <vtkPointData.h>
#include <vtkSelection.h>
#include <vtkSelectionNode.h>
#include <vtkStringArray.h>
#include <vtkTransform.h>
#include <vtkVersion.h>

#include <ctime>

namespace
{
constexpr const char* SPEED_FIELD_DATA_NAME[2] = { "RPM", "FPS" };
constexpr const char* INFO_FIELD_DATA_NAME[2] = { "Vendor", "Model" };
}

//-----------------------------------------------------------------------------
void vtkLidarPacketInterpreter::Initialize()
{
  this->IsInitialized = true;
  // AdvancedArrays could have been changed so we need to reset current frame.
  this->ResetCurrentFrame();
}

//-----------------------------------------------------------------------------
void vtkLidarPacketInterpreter::ResetInitializedState()
{
  this->IsInitialized = false;
}

//-----------------------------------------------------------------------------
bool vtkLidarPacketInterpreter::SplitFrame(bool force,
  FramingMethod_t framingMethodAskingForSplitFrame)
{
  if ((force || this->FramingMethod == framingMethodAskingForSplitFrame) && this->CurrentFrame)
  {
    const vtkIdType nPtsOfCurrentDataset = this->CurrentFrame->GetNumberOfPoints();
    if (this->IgnoreEmptyFrames && (nPtsOfCurrentDataset == 0) && !force)
    {
      return false;
    }

    // Create a field data array with RPM info
    double fsValues[2];
    fsValues[0] = this->GetRpm();
    fsValues[1] = this->GetFrequency();
    for (unsigned int i = 0; i < 2; i++)
    {
      if (fsValues[i] != 0)
      {
        vtkSmartPointer<vtkDoubleArray> fsArray = vtkSmartPointer<vtkDoubleArray>::New();
        fsArray->SetName(::SPEED_FIELD_DATA_NAME[i]);
        fsArray->SetNumberOfComponents(1);
        fsArray->SetNumberOfTuples(1);
        fsArray->InsertComponent(0, 0, fsValues[i]);
        this->CurrentFrame->GetFieldData()->AddArray(fsArray);
      }
    }

    // Add sensor name and model if implemented in subclasses
    std::string infoValues[2];
    infoValues[0] = this->GetSensorVendor();
    infoValues[1] = this->GetSensorModelName();
    for (unsigned int idx = 0; idx < 2; idx++)
    {
      if (!infoValues[idx].empty())
      {
        vtkSmartPointer<vtkStringArray> strArray = vtkSmartPointer<vtkStringArray>::New();
        strArray->SetName(::INFO_FIELD_DATA_NAME[idx]);
        strArray->SetNumberOfComponents(1);
        strArray->SetNumberOfTuples(1);
        strArray->InsertValue(0, infoValues[idx].c_str());
        this->CurrentFrame->GetFieldData()->AddArray(strArray);
      }
    }

    // Apply transform on all points if not identity
    vtkTransform* transform = this->GetSensorTransform();
    if (transform && !transform->GetMatrix()->IsIdentity())
    {
      if (this->PassthroughTransformMode)
      {
        vtkLVUtilities::SetTransformInFieldData(
          this->CurrentFrame->GetFieldData(), transform, "BaseToLiDAR");
      }
      else
      {
        // Transform pointcloud from LiDAR coordinate to BASE coordinate
        vtkSmartPointer<vtkPoints> newPts = vtkSmartPointer<vtkPoints>::New();
#if VTK_VERSION_NUMBER >= VTK_VERSION_CHECK(9, 7, 0)
        newPts->Reserve(this->CurrentFrame->GetNumberOfPoints());
#else
        newPts->Allocate(this->CurrentFrame->GetNumberOfPoints());
#endif
        newPts->GetData()->SetName(this->CurrentFrame->GetPoints()->GetData()->GetName());
        transform->TransformPoints(this->CurrentFrame->GetPoints(), newPts);
        this->CurrentFrame->SetPoints(newPts);

        vtkNew<vtkTransform> inverseTransform;
        inverseTransform->DeepCopy(transform);
        inverseTransform->Inverse();
        vtkLVUtilities::SetTransformInFieldData(
          this->CurrentFrame->GetFieldData(), inverseTransform, "LiDARToBase");
      }
    }

    // add vertex to the polydata
    vtkNew<vtkCellArray> verts;
    verts->AllocateEstimate(this->CurrentFrame->GetNumberOfPoints(), 1);
    for (vtkIdType i = 0; i < this->CurrentFrame->GetNumberOfPoints(); i++)
    {
      verts->InsertNextCell(1, &i);
    }
    this->CurrentFrame->SetVerts(verts);

    // free extra memory allocated
    this->CurrentFrame->Squeeze();

    // Filter the frame by the user's laser (channel) selection.
    this->ApplyLaserSelection(this->CurrentFrame);

    // split the frame
    this->Frames.push_back(this->CurrentFrame);
    // create a new frame
    this->CurrentFrame = this->CreateNewEmptyFrame(0, nPtsOfCurrentDataset);

    return true;
  }
  return false;
}

//-----------------------------------------------------------------------------
void vtkLidarPacketInterpreter::SetCalibrationFileName(const char* filename)
{
  if (this->CalibrationFileName && filename && strcmp(this->CalibrationFileName, filename) != 0)
  {
    this->ResetInitializedState();
  }
  vtkSetStringBodyMacro(CalibrationFileName, filename);
};

//-----------------------------------------------------------------------------
bool vtkLidarPacketInterpreter::IsNewData()
{
  return this->IsNewFrameReady();
}

//-----------------------------------------------------------------------------
bool vtkLidarPacketInterpreter::IsValidPacket(unsigned char const* data, unsigned int dataLength)
{
  return this->IsLidarPacket(data, dataLength);
}

//-----------------------------------------------------------------------------
void vtkLidarPacketInterpreter::ResetCurrentData()
{
  this->ResetCurrentFrame();
}

//-----------------------------------------------------------------------------
void vtkLidarPacketInterpreter::ProcessPacketWrapped(unsigned char const* data,
  unsigned int dataLength,
  double PacketNetworkTime_s)
{
  // if the framing Method is the NetworkPacketTime one
  // We check if the frame has te be split.
  if (this->IsLidarPacket(data, dataLength) &&
    this->FramingMethod == FramingMethod_t::NETWORK_PACKET_TIME_FRAMING)
  {
    auto currentFrameNumber =
      static_cast<unsigned long long>(PacketNetworkTime_s / this->FrameDuration_s);
    if (this->LastNetworkTimeFrameNumber != 0 // do not frame on first call of this function
      && currentFrameNumber != this->LastNetworkTimeFrameNumber) // new frame found
    {
      this->SplitFrame(false, FramingMethod_t::NETWORK_PACKET_TIME_FRAMING);
    }
    this->LastNetworkTimeFrameNumber = currentFrameNumber;
  }

  // Interpreter the packet
  this->ProcessPacket(data, dataLength);
}

//-----------------------------------------------------------------------------
bool vtkLidarPacketInterpreter::PreProcessPacketWrapped(unsigned char const* data,
  unsigned int dataLength,
  double packetNetworkTime,
  double& outLidarDataTime)
{
  switch (this->FramingMethod)
  {
    case FramingMethod_t::NETWORK_PACKET_TIME_FRAMING:
    {
      unsigned long long currentFrameNumber =
        static_cast<unsigned long long>(packetNetworkTime / this->FrameDuration_s);
      if (currentFrameNumber != this->previousFrameNumber)
      {
        // FirstPacketDataTime is not well-defined, because we do not parse
        // the data and thus cannot get data time, however providing a
        // plausible value can help prevent breaking some algorithms.
        // Possible improvement: first pass using INTERPRETER_FRAMING to
        // compute the time shift, then apply it to packetNetworkTime
        outLidarDataTime = packetNetworkTime;
        this->previousFrameNumber = currentFrameNumber;
        return true;
      }
      break;
    }

    default:
    case FramingMethod_t::INTERPRETER_FRAMING:
      return this->PreProcessPacket(data, dataLength, outLidarDataTime);
  }
  return false;
}

//-----------------------------------------------------------------------------
std::string vtkLidarPacketInterpreter::GetSensorInformation(bool vtkNotUsed(shortVersion))
{
  return this->GetSensorVendor() + " - " + this->GetSensorModelName();
}

//-----------------------------------------------------------------------------
vtkIntArray* vtkLidarPacketInterpreter::GetLaserSelection()
{
  return this->LaserSelection.GetPointer();
}

//-----------------------------------------------------------------------------
void vtkLidarPacketInterpreter::SetLaserSelection(int index, int value)
{
  if (index < 0)
  {
    return;
  }
  if (index >= this->LaserSelection->GetNumberOfTuples())
  {
    const int oldSize = this->LaserSelection->GetNumberOfTuples();
    this->LaserSelection->Resize(index + 1);
    for (int i = oldSize; i <= index; ++i)
    {
      this->LaserSelection->InsertTuple1(i, 1);
    }
  }
  this->LaserSelection->SetTuple1(index, value ? 1 : 0);
  this->Modified();
}

//-----------------------------------------------------------------------------
bool vtkLidarPacketInterpreter::IsLaserSelected(int laserId)
{
  if (laserId < 0 || laserId >= this->LaserSelection->GetNumberOfTuples())
  {
    return true;
  }
  return this->LaserSelection->GetValue(laserId) != 0;
}

//-----------------------------------------------------------------------------
void vtkLidarPacketInterpreter::ApplyLaserSelection(vtkPolyData* frame)
{
  if (!frame)
  {
    return;
  }
  vtkDataArray* laserId = frame->GetPointData()->GetArray("laser_id");
  if (this->LaserSelection->GetNumberOfTuples() == 0 || !laserId)
  {
    return;
  }
  bool anyDisabled = false;
  for (vtkIdType i = 0; i < this->LaserSelection->GetNumberOfTuples(); ++i)
  {
    if (this->LaserSelection->GetValue(i) == 0)
    {
      anyDisabled = true;
      break;
    }
  }
  if (!anyDisabled)
  {
    return;
  }

  vtkNew<vtkIdTypeArray> kept;
  kept->Allocate(laserId->GetNumberOfTuples());
  for (vtkIdType i = 0; i < laserId->GetNumberOfTuples(); ++i)
  {
    const int id = static_cast<int>(laserId->GetTuple1(i));
    if (this->IsLaserSelected(id))
    {
      kept->InsertNextValue(i);
    }
  }

  vtkNew<vtkSelectionNode> selNode;
  selNode->SetFieldType(vtkSelectionNode::POINT);
  selNode->SetContentType(vtkSelectionNode::INDICES);
  selNode->SetSelectionList(kept.GetPointer());
  vtkNew<vtkSelection> selection;
  selection->AddNode(selNode.GetPointer());

  vtkNew<vtkExtractSelection> extract;
  extract->SetInputData(0, frame);
  extract->SetInputData(1, selection.GetPointer());
  extract->Update();

  vtkPolyData* extracted = vtkPolyData::SafeDownCast(extract->GetOutput());
  if (extracted)
  {
    frame->ShallowCopy(extracted);
  }
}
