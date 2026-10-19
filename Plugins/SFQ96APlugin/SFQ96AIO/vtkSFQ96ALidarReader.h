/*=========================================================================

  Program: LidarView
  Module:  vtkSFQ96ALidarReader.h

  Copyright 2018 (c) Kitware Inc.
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#ifndef vtkSFQ96ALidarReader_h
#define vtkSFQ96ALidarReader_h

#include <vtkLidarReader.h>

#include "SFQ96AIOModule.h"

/**
 * @class vtkSFQ96ALidarReader
 *
 * Reads pcap files using vtkLidarPacketInterpreter implementations.
 */
class SFQ96AIO_EXPORT vtkSFQ96ALidarReader : public vtkLidarReader
{
public:
  static vtkSFQ96ALidarReader* New();
  vtkTypeMacro(vtkSFQ96ALidarReader, vtkLidarReader);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  ///@{
  /**
   * Set/Get calibration port.
   */
  vtkGetMacro(CalibrationPort, int);
  void SetCalibrationPort(int port);
  ///@}

protected:
  vtkSFQ96ALidarReader();
  ~vtkSFQ96ALidarReader() override;

  ///@{
  /**
   * Open/Close the pcap file.
   */
  bool Open() override;
  ///@}

private:
  vtkSFQ96ALidarReader(const vtkSFQ96ALidarReader&) = delete;
  void operator=(const vtkSFQ96ALidarReader&) = delete;

  int CalibrationPort = -1;
};

#endif
