// ============================================================
// 功能：编辑菜单「雷达硬件参数设置」Reaction 实现。
// 作者：acelan
// 新建时间：2026-10-20
// 修改时间：2026-10-20
// ============================================================

#include "lqAiryHardwareConfigReaction.h"

#include "lqAiryHardwareConfigDialog.h"
#include "lqSenfotoNetworkConfigDialog.h"

#include <pqActiveObjects.h>
#include <pqCoreUtilities.h>
#include <pqPipelineSource.h>

#include <QAction>
#include <QString>

#include <vtkSMProxy.h>

namespace
{
QString ActiveReaderName()
{
  pqPipelineSource* source = pqActiveObjects::instance().activeSource();
  return source && source->getProxy() ? QString(source->getProxy()->GetXMLName()) : QString();
}
}

//-----------------------------------------------------------------------------
lqAiryHardwareConfigReaction::lqAiryHardwareConfigReaction(QAction* parent)
  : Superclass(parent)
{
  QObject::connect(&pqActiveObjects::instance(), &pqActiveObjects::sourceChanged, this,
    &lqAiryHardwareConfigReaction::updateEnableState);
  this->updateEnableState();
}

//-----------------------------------------------------------------------------
void lqAiryHardwareConfigReaction::updateEnableState()
{
  const QString name = ::ActiveReaderName();
  this->parentAction()->setEnabled(
    name.startsWith(QStringLiteral("SFQ96A")) || name.startsWith(QStringLiteral("Senfoto008")));
}

//-----------------------------------------------------------------------------
void lqAiryHardwareConfigReaction::onTriggered()
{
  if (::ActiveReaderName().startsWith(QStringLiteral("Senfoto008")))
  {
    lqSenfotoNetworkConfigDialog dialog(pqCoreUtilities::mainWidget());
    dialog.exec();
    return;
  }

  lqAiryHardwareConfigDialog dialog(pqCoreUtilities::mainWidget());
  dialog.exec();
}
