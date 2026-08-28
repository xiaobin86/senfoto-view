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

// ============================================================
// 功能：SenFoToView 新增功能 —— 激光通道选择对话框类声明；
//        列出各激光通道的勾选框与俯仰角，向 LaserSelection filter 推送使能掩码。
// 作者：acelan
// 新建时间：2026-08-28
// 修改时间：2026-08-28
// ============================================================

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
 * to the "LaserSelection" filter (vtkLaserSelectionFilter) through the
 * "LaserSelection" SM property (which forwards, per channel, to
 * vtkLaserSelectionFilter::SetLaserSelection(int index, int value)). The
 * filter runs downstream of the reader/stream, so toggling channels actually
 * removes the corresponding points from the displayed point cloud.
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

  /// Resolves the laser-selection filter proxy from the active lidar source.
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
  vtkSMProxy* LaserSelectionFilterProxy = nullptr;
};

#endif // !lqLaserSelectionDialog_h
