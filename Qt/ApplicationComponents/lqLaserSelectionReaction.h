/*=========================================================================

  Program: LidarView
  Module:  lqLaserSelectionReaction.h

  Copyright (c) Kitware Inc.
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

// ============================================================
// 功能：SenFoToView 新增功能 —— 激光通道选择 Reaction 类声明；
//        把菜单/工具栏动作接到打开 LaserSelection 对话框。
// 作者：acelan
// 新建时间：2026-08-28
// 修改时间：2026-08-28
// ============================================================

#ifndef lqLaserSelectionReaction_h
#define lqLaserSelectionReaction_h

#include "lqApplicationComponentsModule.h"

#include <QObject>

class QAction;

/**
 * @ingroup Reactions
 * Reaction that shows the lqLaserSelectionDialog non-modally so the user can
 * enable/disable individual lidar channels of the active lidar source.
 */
class LQAPPLICATIONCOMPONENTS_EXPORT lqLaserSelectionReaction : public QObject
{
  Q_OBJECT
  typedef QObject Superclass;

public:
  /**
   * Constructor. Parent cannot be nullptr.
   */
  lqLaserSelectionReaction(QAction* parent);

public Q_SLOTS:
  void showDialog();

private:
  Q_DISABLE_COPY(lqLaserSelectionReaction)
};

#endif
