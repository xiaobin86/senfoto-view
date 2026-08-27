/*=========================================================================

  Program: LidarView
  Module:  lqMainControlsToolbar.cxx

  Copyright (c) Kitware Inc.
  All rights reserved.
  See LICENSE or http://www.apache.org/licenses/LICENSE-2.0 for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "lqMainControlsToolbar.h"
#include "ui_lqMainControlsToolbar.h"

#include <QToolButton>

#include <pqAutoApplyReaction.h>
#include <pqDataQueryReaction.h>
#include <pqDeleteReaction.h>
#include <pqLoadDataReaction.h>
#include <pqSaveDataReaction.h>
#include <pqUndoRedoReaction.h>

#include "vtkSMInterpretersManagerProxy.h"

#include "lqEnableAdvancedArraysReaction.h"
#include "lqLaserSelectionReaction.h"
#include "lqOpenLidarReaction.h"
#include "lqPythonShellReaction.h"
#include "lqSavePcapReaction.h"
#include "lqUpdateConfigurationReaction.h"
#include "pqLoadPaletteReaction.h"

//-----------------------------------------------------------------------------
void lqMainControlsToolbar::constructor()
{
  Ui::lqMainControlsToolbar ui;
  ui.setupUi(this);

  new lqOpenLidarReaction(ui.actionOpenPcap, vtkSMInterpretersManagerProxy::Mode::READER);
  new lqSavePcapReaction(ui.actionSavePcap);
  new lqOpenLidarReaction(ui.actionOpenSensorStream, vtkSMInterpretersManagerProxy::Mode::STREAM);
  new lqUpdateConfigurationReaction(ui.actionUpdateConfiguration);
  new lqEnableAdvancedArraysReaction(ui.actionToggleAdvancedArrays);
  new pqLoadDataReaction(ui.actionOpenData);
  new pqSaveDataReaction(ui.actionSaveData);
  new pqDeleteReaction(ui.actionDeleteAll, pqDeleteReaction::DeleteModes::ALL);
  new pqUndoRedoReaction(ui.actionUndo, true);
  new pqUndoRedoReaction(ui.actionRedo, false);
  new pqLoadPaletteReaction(ui.actionLoadPalette);
  new pqAutoApplyReaction(ui.actionAutoApply);
  new pqDataQueryReaction(ui.actionQuery);
  new lqPythonShellReaction(ui.actionPythonShell);
  new lqLaserSelectionReaction(ui.actionLaserSelection);

  // The main controls toolbar is icon-only; this action has no icon, so force
  // its button to show text, otherwise it renders as a zero-size (invisible)
  // button.
  if (QToolButton* tb = qobject_cast<QToolButton*>(this->widgetForAction(ui.actionLaserSelection)))
  {
    tb->setToolButtonStyle(Qt::ToolButtonTextOnly);
  }

  QToolButton* tb = qobject_cast<QToolButton*>(this->widgetForAction(ui.actionLoadPalette));
  if (tb)
  {
    tb->setPopupMode(QToolButton::InstantPopup);
  }
}
