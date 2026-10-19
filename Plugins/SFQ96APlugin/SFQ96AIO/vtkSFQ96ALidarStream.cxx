/*=========================================================================

  Program: LidarView
  Module:  vtkSFQ96ALidarStream.cxx

  Copyright (c) Kitware Inc.
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkSFQ96ALidarStream.h"

//-----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSFQ96ALidarStream)

//-----------------------------------------------------------------------------
vtkSFQ96ALidarStream::vtkSFQ96ALidarStream() = default;

//-----------------------------------------------------------------------------
vtkSFQ96ALidarStream::~vtkSFQ96ALidarStream() = default;

//----------------------------------------------------------------------------
void vtkSFQ96ALidarStream::Start()
{
  if (!this->GetLidarInterpreter())
  {
    vtkErrorMacro("No packet interpreter selected.");
    return;
  }
  if (!this->GetLidarInterpreter()->GetIsInitialized())
  {
    this->GetLidarInterpreter()->Initialize();
  }

  this->vtkStream::Start({ this->GetListeningPort(), this->CalibrationPort });
}