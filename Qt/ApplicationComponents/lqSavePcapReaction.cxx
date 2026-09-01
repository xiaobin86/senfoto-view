/*=========================================================================

  Program: LidarView
  Module:  lqSavePcapReaction.cxx

  Copyright (c) Kitware Inc.
  All rights reserved.
  See LICENSE or http://www.apache.org/licenses/LICENSE-2.0 for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "lqSavePcapReaction.h"

#include <cstring>
#include <sstream>

#include <vtkSMDoubleVectorProperty.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMSourceProxy.h>

#include <QFileInfo>
#include <QMessageBox>
#include <QProgressDialog>

#include <pqActiveObjects.h>
#include <pqCoreUtilities.h>
#include <pqOutputPort.h>
#include <pqPipelineSource.h>

#include <vtkSMViewProxy.h>

#include "lqSelectLidarFrameDialog.h"
#include "vtkSMLidarReaderProxy.h"

//-----------------------------------------------------------------------------
lqSavePcapReaction::lqSavePcapReaction(QAction* action, bool displaySettings)
  : lqSaveLidarFrameReaction(action, "", "pcap", displaySettings)
{
  pqActiveObjects* activeObjects = &pqActiveObjects::instance();
  QObject::connect(
    activeObjects, SIGNAL(portChanged(pqOutputPort*)), this, SLOT(updateEnableState()));

  this->updateEnableState();
}

//-----------------------------------------------------------------------------
void lqSavePcapReaction::updateEnableState()
{
  pqOutputPort* port = pqActiveObjects::instance().activePort();
  bool enableState = false;
  if (port)
  {
    enableState = vtkSMLidarReaderProxy::SafeDownCast(port->getSource()->getProxy()) != nullptr;
  }
  this->parentAction()->setEnabled(enableState);
}

//-----------------------------------------------------------------------------
void lqSavePcapReaction::onTriggered()
{
  pqPipelineSource* lidar = this->getCorrectLidar();

  if (!lidar)
  {
    return;
  }

  int nbFrame = 0;
  auto* tsv =
    vtkSMDoubleVectorProperty::SafeDownCast(lidar->getProxy()->GetProperty("TimestepValues"));
  if (tsv)
  {
    nbFrame = tsv->GetNumberOfElements() ? tsv->GetNumberOfElements() - 1 : 0;
  }

  // Set BaseName and FolderPath
  std::string pcapName = vtkSMPropertyHelper(lidar->getProxy(), "FileName").GetAsString();
  QFileInfo fileInfo(QString::fromStdString(pcapName));
  this->FolderPath = fileInfo.path();
  this->BaseName = fileInfo.baseName();

  lqSelectLidarFrameDialog dialog(
    nbFrame, pqCoreUtilities::mainWidget(), lqSelectLidarFrameDialog::ALL_FRAMES);
  if (dialog.exec())
  {
    qInfo() << "lqSavePcapReaction: frame mode"
            << static_cast<int>(dialog.frameMode()) << "start" << dialog.StartFrame()
            << "stop" << dialog.StopFrame() << "of" << nbFrame;
    if (dialog.frameMode() == lqSelectLidarFrameDialog::FRAME_RANGE)
    {
      std::stringstream ss;
      ss << fileInfo.baseName().toStdString() << " (Frame " << dialog.StartFrame() << " to "
         << dialog.StopFrame() << ")";
      this->BaseName = ss.str().c_str();
    }
  int startFrame = dialog.StartFrame();
  int stopFrame = dialog.StopFrame();
  if (dialog.frameMode() == lqSelectLidarFrameDialog::CURRENT_FRAME)
  {
    // Resolve the currently displayed frame index: SaveFrames takes frame
    // indices and its parameters are unsigned, so the dialog's -1 sentinel
    // would wrap and always fail the bounds check.
    double viewTime = 0.0;
    if (auto* view = pqActiveObjects::instance().activeView())
    {
      viewTime = vtkSMPropertyHelper(view->getViewProxy(), "ViewTime").GetAsDouble();
    }
    startFrame = 0;
    stopFrame = 0;
    if (tsv)
    {
      const unsigned int n = tsv->GetNumberOfElements();
      for (unsigned int i = 0; i < n; ++i)
      {
        if (tsv->GetElement(i) <= viewTime + 1e-6)
        {
          startFrame = stopFrame = static_cast<int>(i);
        }
        else
        {
          break;
        }
      }
    }
    qInfo() << "lqSavePcapReaction: current frame resolved to" << startFrame
            << "(view time" << viewTime << ")";
  }
  this->saveFrame(lidar->getProxy(), startFrame, stopFrame);
  }
}

//-----------------------------------------------------------------------------
bool lqSavePcapReaction::saveFrame(vtkSMProxy* lidar, int start, int stop)
{
  if (!this->GetFolderAndBaseNameFromUser())
  {
    return false;
  }

  vtkSMLidarReaderProxy* lidarProxy = vtkSMLidarReaderProxy::SafeDownCast(lidar);
  if (!lidarProxy)
  {
    return false;
  }

  QString filename = this->FolderPath + "/" + this->BaseName + "." + this->Extension;

  // Self-overwrite protection: SaveFrames() opens the source pcap for reading
  // and then the output file for writing. Writing to the source path truncates
  // the source file before any packet is written (data loss).
  std::string sourceFileName =
    vtkSMPropertyHelper(lidarProxy, "FileName").GetAsString();
  if (!sourceFileName.empty() &&
    QFileInfo(filename).absoluteFilePath() ==
    QFileInfo(QString::fromStdString(sourceFileName)).absoluteFilePath())
  {
    QMessageBox::warning(pqCoreUtilities::mainWidget(), "Export pcap",
      "The output file is the same as the source pcap.\n"
      "Exporting would destroy the source file.\n"
      "Please choose a different file name.");
    return false;
  }

  lidarProxy->SaveFrames(start, stop, filename.toStdString());
  qInfo() << "lqSavePcapReaction: exporting pcap to" << filename
          << "(session stream call)";
  return true;
}
