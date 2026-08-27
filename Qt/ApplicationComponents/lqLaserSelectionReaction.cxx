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

#include "lqLaserSelectionReaction.h"

#include "lqLaserSelectionDialog.h"
#include "lqLidarViewManager.h"

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
    new lqLaserSelectionDialog(lqLidarViewManager::instance()->getMainWindow());
  dlg->setAttribute(Qt::WA_DeleteOnClose);
  dlg->show();
}
