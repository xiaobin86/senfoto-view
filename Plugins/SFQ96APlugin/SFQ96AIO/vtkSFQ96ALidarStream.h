/*=========================================================================

  Program: LidarView
  Module:  vtkSFQ96ALidarStream.h

  Copyright 2018 (c) Kitware Inc.
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#ifndef vtkSFQ96ALidarStream_h
#define vtkSFQ96ALidarStream_h

#include "vtkLidarStream.h"

#include "SFQ96AIOModule.h"

/**
 * @class vtkSFQ96ALidarStream
 *
 * Reads pcap files using vtkLidarPacketInterpreter implementations.
 */
class SFQ96AIO_EXPORT vtkSFQ96ALidarStream : public vtkLidarStream
{
public:
  static vtkSFQ96ALidarStream* New();
  vtkTypeMacro(vtkSFQ96ALidarStream, vtkLidarStream)

  /**
   * Start the stream with the lidar & calibration port
   */
  void Start() override;

  ///@{
  /**
   * Set/Get calibration port.
   */
  vtkGetMacro(CalibrationPort, unsigned int);
  vtkSetMacro(CalibrationPort, unsigned int);
  ///@}

protected:
  vtkSFQ96ALidarStream();
  ~vtkSFQ96ALidarStream();

private:
  vtkSFQ96ALidarStream(const vtkSFQ96ALidarStream&) = delete;
  void operator=(const vtkSFQ96ALidarStream&) = delete;

  /*!< The port to receive information*/
  unsigned int CalibrationPort = 8308;
};

#endif // vtkSFQ96ALidarStream_h
