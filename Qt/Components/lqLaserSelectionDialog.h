/*=========================================================================

  Program: LidarView
  Module:  lqLaserSelectionDialog.h

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See LICENSE or http://www.apache.org/licenses/LICENSE-2.0 for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#ifndef lqLaserSelectionDialog_h
#define lqLaserSelectionDialog_h

#include "lqComponentsModule.h"
#include "pqPipelineSource.h"

#include <QDialog>
#include <QVector>

class vtkSMProxy;
class QTableWidgetItem;

/**
 * lqLaserSelectionDialog allows the user to enable/disable individual lidar
 * channels (lasers) of the active lidar source. The selection mask is pushed
 * to the source's interpreter sub-proxy through the "LaserSelection" SM
 * property (which forwards, per channel, to vtkLidarPacketInterpreter::
 * SetLaserSelection(int index, int value)). Pushing via SM is required so the
 * mask reaches the server-side interpreter and the pipeline re-splits frames.
 */
class LQCOMPONENTS_EXPORT lqLaserSelectionDialog : public QDialog
{
  Q_OBJECT
  typedef QDialog Superclass;

public:
  lqLaserSelectionDialog(QWidget* p = nullptr);
  ~lqLaserSelectionDialog() override;

  /// Returns the channel mask (1 = enabled, 0 = disabled), indexed by laser_id.
  QVector<int> getLaserSelectionSelector();

  /// Resolves the interpreter sub-proxy from the active lidar source.
  void setLidarSource(pqPipelineSource* src);

public Q_SLOTS:
  void onItemChanged(QTableWidgetItem*);
  void onToggleSelected();
  void onEnableDisableAll(int);
  void onApply();
  void accept() override;

Q_SIGNALS:
  void laserSelectionChanged();

private:
  class pqInternal;
  pqInternal* Internal;

  pqPipelineSource* LidarSource = nullptr;
  vtkSMProxy* InterpreterProxy = nullptr;
};

#endif // !lqLaserSelectionDialog_h
