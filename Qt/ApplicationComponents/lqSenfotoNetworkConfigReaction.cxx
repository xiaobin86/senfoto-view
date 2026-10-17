// ============================================================
// 功能：Tools 菜单「雷达 IP 配置」Reaction 实现。
// 作者：acelan
// 新建时间：2026-10-17
// 修改时间：2026-10-17
// ============================================================

#include "lqSenfotoNetworkConfigReaction.h"

#include "lqSenfotoNetworkConfigDialog.h"

#include <pqActiveObjects.h>
#include <pqCoreUtilities.h>
#include <pqPipelineSource.h>

#include <QAction>
#include <QString>

#include <vtkSMProxy.h>

//-----------------------------------------------------------------------------
lqSenfotoNetworkConfigReaction::lqSenfotoNetworkConfigReaction(QAction* parent)
  : Superclass(parent)
{
  QObject::connect(&pqActiveObjects::instance(), &pqActiveObjects::sourceChanged, this,
    &lqSenfotoNetworkConfigReaction::updateEnableState);
  this->updateEnableState();
}

//-----------------------------------------------------------------------------
void lqSenfotoNetworkConfigReaction::updateEnableState()
{
  pqPipelineSource* source = pqActiveObjects::instance().activeSource();
  const bool isSenfoto = source && source->getProxy() &&
    QString(source->getProxy()->GetXMLName()).startsWith("Senfoto008");
  this->parentAction()->setEnabled(isSenfoto);
}

//-----------------------------------------------------------------------------
void lqSenfotoNetworkConfigReaction::onTriggered()
{
  lqSenfotoNetworkConfigDialog dialog(pqCoreUtilities::mainWidget());
  dialog.exec();
}
