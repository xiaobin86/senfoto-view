/*=========================================================================

  Program: LidarView
  Module:  vtkSFQ96ALidarReader.cxx

  Copyright (c) Kitware Inc.
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkSFQ96ALidarReader.h"

#include <vtkObjectFactory.h>

//-----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSFQ96ALidarReader)

//-----------------------------------------------------------------------------
vtkSFQ96ALidarReader::vtkSFQ96ALidarReader() = default;

//-----------------------------------------------------------------------------
vtkSFQ96ALidarReader::~vtkSFQ96ALidarReader() = default;

//-----------------------------------------------------------------------------
bool vtkSFQ96ALidarReader::Open()
{
  std::vector<int> ports;
  if (this->GetLidarPort() != -1)
  {
    ports.emplace_back(this->GetLidarPort());
    ports.emplace_back(this->CalibrationPort);
  }
  return Superclass::Open(ports);
}

//-----------------------------------------------------------------------------
void vtkSFQ96ALidarReader::SetCalibrationPort(int port)
{
  if (this->CalibrationPort != port)
  {
    this->CalibrationPort = port;
    this->Modified();
    this->ResetFrameIndexes();
  }
}

//------------------------------------------------------------------------------
void vtkSFQ96ALidarReader::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
