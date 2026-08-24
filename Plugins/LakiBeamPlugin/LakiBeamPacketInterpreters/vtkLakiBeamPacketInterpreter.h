/*=========================================================================

  Program:   LidarView
  Module:    vtkLakiBeamPacketInterpreter.h

  Copyright (c) Kitware Inc.
  All rights reserved.
  See LICENSE or http://www.apache.org/licenses/LICENSE-2.0 for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#ifndef vtkLakiBeamPacketInterpreter_h
#define vtkLakiBeamPacketInterpreter_h

#include <vtkLidarPacketInterpreter.h>

#include <memory>

#include "LakiBeamPacketInterpretersModule.h"

class LAKIBEAMPACKETINTERPRETERS_EXPORT vtkLakiBeamPacketInterpreter
  : public vtkLidarPacketInterpreter
{
public:
  static vtkLakiBeamPacketInterpreter* New();
  vtkTypeMacro(vtkLakiBeamPacketInterpreter, vtkLidarPacketInterpreter);
  void PrintSelf(ostream& vtkNotUsed(os), vtkIndent vtkNotUsed(indent)) override {};

  /**
   * Initializes the lidar configuration.
   */
  void Initialize() override;

  /**
   * Checks if the current packet is a valid LakiBeam MSOP packet.
   */
  bool IsLidarPacket(unsigned char const* data, unsigned int dataLength) override;

  /**
   * Builds a frame index for random access when reading a pcap file.
   * Returns true when a new frame (revolution) is detected.
   */
  bool PreProcessPacket(unsigned char const* data,
    unsigned int dataLength,
    double& outLidarDataTime) override;

  /**
   * Processes the packet, filling point information using packet data,
   * and calling SplitFrame when a revolution wraps.
   */
  void ProcessPacket(unsigned char const* data, unsigned int dataLength) override;

protected:
  /**
   * Creates a new empty frame object, which will be filled by ProcessPacket.
   */
  vtkSmartPointer<vtkPolyData> CreateNewEmptyFrame(vtkIdType nbrOfPoints,
    vtkIdType prereservedNbrOfPoints = 60000) override;

  vtkLakiBeamPacketInterpreter();
  ~vtkLakiBeamPacketInterpreter();

private:
  vtkLakiBeamPacketInterpreter(const vtkLakiBeamPacketInterpreter&) = delete;
  void operator=(const vtkLakiBeamPacketInterpreter&) = delete;

  class vtkInternals;
  std::unique_ptr<vtkInternals> Internals;
};

#endif // vtkLakiBeamPacketInterpreter_h
