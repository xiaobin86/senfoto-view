/*=========================================================================

  Program:   LidarView
  Module:    vtkSenfoto008PacketInterpreter.h

  Copyright (c) Kitware Inc.
  All rights reserved.
  See LICENSE or http://www.apache.org/licenses/LICENSE-2.0 for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#ifndef vtkSenfoto008PacketInterpreter_h
#define vtkSenfoto008PacketInterpreter_h

#include <vtkLidarPacketInterpreter.h>

#include <cstdint>
#include <memory>

#include "Senfoto008PacketInterpretersModule.h"

class SENFOTO008PACKETINTERPRETERS_EXPORT vtkSenfoto008PacketInterpreter
  : public vtkLidarPacketInterpreter
{
public:
  static vtkSenfoto008PacketInterpreter* New();
  vtkTypeMacro(vtkSenfoto008PacketInterpreter, vtkLidarPacketInterpreter);
  void PrintSelf(ostream& vtkNotUsed(os), vtkIndent vtkNotUsed(indent)) override {}

  // --- Filtering: distance range + azimuth/FOV range (ported from RoboSense Airy) ---
  vtkSetClampMacro(MinDistance, double, 0.0, 1e6);
  vtkGetMacro(MinDistance, double);
  vtkSetClampMacro(MaxDistance, double, 0.0, 1e6);
  vtkGetMacro(MaxDistance, double);
  vtkSetMacro(StartAngle, double);
  vtkGetMacro(StartAngle, double);
  vtkSetMacro(EndAngle, double);
  vtkGetMacro(EndAngle, double);

  /**
   * Initializes the lidar configuration.
   */
  void Initialize() override;

  /**
   * Checks if the current packet is a valid Senfoto 008 MSOP packet.
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

  vtkSenfoto008PacketInterpreter();
  ~vtkSenfoto008PacketInterpreter();

private:
  vtkSenfoto008PacketInterpreter(const vtkSenfoto008PacketInterpreter&) = delete;
  void operator=(const vtkSenfoto008PacketInterpreter&) = delete;

  void AddPoint(double azimuthDeg, double elevationDeg, double distanceM,
    std::uint8_t laserId, std::uint8_t intensity, double timestamp);

  bool IsAzimuthInRange(double azimuthDeg) const;

  void SetCalibrationForModel(std::uint8_t lidarModel);

  double MinDistance = 0.0;
  double MaxDistance = 10000.0;
  double StartAngle = 0.0;
  double EndAngle = 360.0;

  class vtkInternals;
  std::unique_ptr<vtkInternals> Internals;
};

#endif // vtkSenfoto008PacketInterpreter_h
