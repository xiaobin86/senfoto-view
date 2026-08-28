/*=========================================================================

  Program: LidarView
  Module:  lqLaserSelectionReaction.cxx

  Copyright (c) Kitware Inc.
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

// ============================================================
// 功能：SenFoToView 新增功能 —— 激光通道选择 Reaction 实现；
//        触发打开 lqLaserSelectionDialog。
// 作者：acelan
// 新建时间：2026-08-28
// 修改时间：2026-08-28
// ============================================================

#include "lqLaserSelectionReaction.h"

#include "lqLaserSelectionDialog.h"
#include "lqLidarCoreManager.h"

#include <QAction>

//-----------------------------------------------------------------------------
lqLaserSelectionReaction::lqLaserSelectionReaction(QAction* parent)
  : Superclass(parent)
{
  QObject::connect(parent, &QAction::triggered, this, &lqLaserSelectionReaction::showDialog);
}

//-----------------------------------------------------------------------------
void lqLaserSelectionReaction::showDialog()
{
  lqLaserSelectionDialog* dlg =
    new lqLaserSelectionDialog(lqLidarCoreManager::getMainWindow());
  dlg->setAttribute(Qt::WA_DeleteOnClose);
  dlg->show();
}
