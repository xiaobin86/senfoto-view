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
#include "vtkLidarPacketInterpreter.h"
#include "pqPipelineSource.h"

#include <QDialog>
#include <QVector>

class QTableWidgetItem;

/**
 * lqLaserSelectionDialog allows the user to enable/disable individual lidar
 * channels (lasers) of the active lidar source. The selection mask is written
 * to the source's vtkLidarPacketInterpreter through the Task 1 API:
 *   SetLaserSelection(int, int) / IsLaserSelected(int) / GetLaserSelection().
 */
class LQCOMPONENTS_EXPORT lqLaserSelectionDialog : public QDialog
{
  Q_OBJECT
  typedef QDialog Superclass;

public:
  lqLaserSelectionDialog(QWidget* p = nullptr);
  ~lqLaserSelectionDialog() override;

  /// Returns the channel mask (1 = enabled, 0 = disabled), indexed by firing order.
  QVector<int> getLaserSelectionSelector();

  /// Resolves the interpreter from the active lidar source and sizes the table.
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
  vtkLidarPacketInterpreter* Interpreter = nullptr;
  int CurrentNumLaser = 0;
};

#endif // !lqLaserSelectionDialog_h
