/*=========================================================================

  Program: LidarView
  Module:  lqLidarViewMenuBuilders.cxx

  Copyright (c) Kitware Inc.
  All rights reserved.
  See LICENSE or http://www.apache.org/licenses/LICENSE-2.0 for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "lqLidarViewMenuBuilders.h"

#include "ui_lqEditMenuBuilder.h"
#include "ui_lqFileMenuBuilder.h"
#include "ui_lqHelpMenuBuilder.h"

#include <vtkSMProxyManager.h>

#include <pqApplicationCore.h>
#include <pqAxesToolbar.h>
#include <pqCameraToolbar.h>
#include <pqCustomViewpointsToolbar.h>
#include <pqDeleteReaction.h>
#include <pqDesktopServicesReaction.h>
#include <pqParaViewMenuBuilders.h>
#include <pqPropertiesPanel.h>
#include <pqPythonManager.h>
#include <pqSaveDataReaction.h>
#include <pqSetName.h>

#include "vtkLVVersion.h"
#include "vtkSMInterpretersManagerProxy.h"

#include "lqAboutDialogReaction.h"
#include "lqColorToolbar.h"
#include "lqInterfaceControlsToolbar.h"
#include "lqMainControlsToolbar.h"
#include "lqMeasurementGridReaction.h"
#include "lqMenuSaveAsReaction.h"
#include "lqOpenLidarReaction.h"
#include "lqRulersToolbar.h"
#include "lqSavePcapReaction.h"
#include "lqSensorListWidget.h"
#include "lqUpdateConfigurationReaction.h"

#include <QLayout>
#include <QMainWindow>
#include <QMenu>
#include <QString>

//-----------------------------------------------------------------------------
void lqLidarViewMenuBuilders::buildFileMenu(QMenu& menu)
{
  QString objectName = menu.objectName();
  // Build first paraview menu
  pqParaViewMenuBuilders::buildFileMenu(menu);
  // Add in the same menu lidarview functionalities
  Ui::lqFileMenuBuilder ui;
  ui.setupUi(&menu);
  // Since the UI file tends to change the name of the menu.
  menu.setObjectName(objectName);

  new lqOpenLidarReaction(ui.actionOpenPcap, vtkSMInterpretersManagerProxy::Mode::READER);
  new lqOpenLidarReaction(ui.actionOpenSensorStream, vtkSMInterpretersManagerProxy::Mode::STREAM);
  new lqSavePcapReaction(ui.actionSavePcap);
  new pqSaveDataReaction(ui.actionSaveData);
  new lqMenuSaveAsReaction(ui.menuSaveAs);

  // Add fileOpen action from ParaView to menuOpen
  for (QAction* action : menu.actions())
  {
    if (action->objectName() == "actionFileOpen")
    {
      ui.menuOpen->addAction(action);
    }
    if (action->objectName() == "actionServerConnect")
    {
      action->setText("Connect to lvserver...");
      action->setStatusTip("Connect to server (ParaView client / server mode)");
    }
    if (action->objectName() == "actionServerDisconnect")
    {
      action->setText("Disconnect from lvserver");
    }
  }
}

//-----------------------------------------------------------------------------
void lqLidarViewMenuBuilders::buildEditMenu(QMenu& menu, pqPropertiesPanel* propertiesPanel)
{
  QString objectName = menu.objectName();
  // Build first paraview menu
  pqParaViewMenuBuilders::buildEditMenu(menu, propertiesPanel);
  // Add in the same menu lidarview functionalities
  Ui::lqEditMenuBuilder ui;
  ui.setupUi(&menu);
  // Since the UI file tends to change the name of the menu.
  menu.setObjectName(objectName);

  new lqUpdateConfigurationReaction(ui.actionUpdateConfiguration);

  auto* gridButton = new lqMeasurementGridReaction(ui.actionShowMeasurementGrid);
  QObject::connect(
    &menu, &QMenu::aboutToShow, gridButton, &lqMeasurementGridReaction::onRefreshButton);

  if (propertiesPanel)
  {
    /// Hook delete to pqDeleteReaction in pipeline browser.
    QAction* tempDeleteAction = new QAction(propertiesPanel);
    pqDeleteReaction* deleteReaction = new pqDeleteReaction(tempDeleteAction);
    deleteReaction->connect(
      propertiesPanel, SIGNAL(deleteRequested(pqProxy*)), SLOT(deleteSource(pqProxy*)));
  }
}

//-----------------------------------------------------------------------------
void lqLidarViewMenuBuilders::buildHelpMenu(QMenu& menu)
{
  QString objectName = menu.objectName();
  Ui::lqHelpMenuBuilder ui;
  ui.setupUi(&menu);
  // Since the UI file tends to change the name of the menu.
  menu.setObjectName(objectName);

  // Keep only the "About" entry in the Help menu.
  QAction* aboutAction = ui.actionAboutLidarView;
  for (QAction* action : menu.actions())
  {
    if (action != aboutAction)
    {
      menu.removeAction(action);
    }
  }
  new lqAboutDialogReaction(aboutAction);
}

//-----------------------------------------------------------------------------
void lqLidarViewMenuBuilders::buildToolbars(QMainWindow& mainWindow)
{
  QToolBar* mainToolBar = new lqMainControlsToolbar(&mainWindow)
    << pqSetName("mainControlsToolbar");
  mainToolBar->layout()->setSpacing(0);
  mainWindow.addToolBar(Qt::TopToolBarArea, mainToolBar);

  QToolBar* colorToolbar = new lqColorToolbar(&mainWindow) << pqSetName("colorToolBar");
  colorToolbar->layout()->setSpacing(0);
  mainWindow.addToolBar(Qt::TopToolBarArea, colorToolbar);

  QToolBar* interfaceControlsToolbar = new lqInterfaceControlsToolbar(&mainWindow)
    << pqSetName("interfaceControlsToolbar");
  interfaceControlsToolbar->layout()->setSpacing(0);
  mainWindow.addToolBar(Qt::TopToolBarArea, interfaceControlsToolbar);

  // Give the macros menu to the pqPythonMacroSupervisor
  pqPythonManager* manager =
    qobject_cast<pqPythonManager*>(pqApplicationCore::instance()->manager("PYTHON_MANAGER"));
  if (manager)
  {
    QToolBar* macrosToolbar =
      new QToolBar(QCoreApplication::translate("pqmacrosToolbar", "Macros Toolbars"), &mainWindow)
      << pqSetName("macrosToolbar");
    macrosToolbar->hide();
    manager->addWidgetForRunMacros(macrosToolbar);
    mainWindow.addToolBar(Qt::TopToolBarArea, macrosToolbar);
  }

  QToolBar* cameraToolbar = new pqCameraToolbar(&mainWindow) << pqSetName("cameraToolbar");
  cameraToolbar->layout()->setSpacing(0);
  mainWindow.addToolBar(Qt::TopToolBarArea, cameraToolbar);

  QToolBar* axesToolbar = new pqAxesToolbar(&mainWindow) << pqSetName("axesToolbar");
  axesToolbar->layout()->setSpacing(0);
  mainWindow.addToolBar(Qt::TopToolBarArea, axesToolbar);

  QToolBar* rulersToolbar = new lqRulersToolbar(&mainWindow) << pqSetName("rulersToolbar");
  rulersToolbar->layout()->setSpacing(0);
  mainWindow.addToolBar(Qt::TopToolBarArea, rulersToolbar);

  QToolBar* customViewpointsToolbar = new pqCustomViewpointsToolbar(&mainWindow)
    << pqSetName("customViewpointsToolbar");
  customViewpointsToolbar->layout()->setSpacing(0);
  mainWindow.addToolBar(Qt::TopToolBarArea, customViewpointsToolbar);
}
