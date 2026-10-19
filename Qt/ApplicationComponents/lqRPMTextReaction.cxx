// ============================================================
// 功能：Tools 菜单「显示 RPM」开关实现 —— 查/建活动 Senfoto008 源的
//       RPMText 过滤器并切换其可见性。
// 作者：acelan
// 新建时间：2026-10-17
// 修改时间：2026-10-17
// ============================================================

#include "lqRPMTextReaction.h"

#include <pqActiveObjects.h>
#include <pqApplicationCore.h>
#include <pqDataRepresentation.h>
#include <pqObjectBuilder.h>
#include <pqPipelineSource.h>
#include <pqView.h>

#include <QAction>
#include <QSettings>

#include <vtkSMProxy.h>

#include <cstring>

namespace
{
pqPipelineSource* FindRPMTextConsumer(pqPipelineSource* source)
{
  if (!source)
  {
    return nullptr;
  }
  for (pqPipelineSource* consumer : source->getAllConsumers())
  {
    if (consumer && consumer->getProxy() &&
      std::strcmp(consumer->getProxy()->GetXMLName(), "RPMText") == 0)
    {
      return consumer;
    }
  }
  return nullptr;
}

bool IsRpmCapableReader(const QString& xmlName)
{
  return xmlName.startsWith("Senfoto008") || xmlName.startsWith("SFQ96A");
}

bool IsActiveRpmCapableSource()
{
  pqPipelineSource* source = pqActiveObjects::instance().activeSource();
  return source && source->getProxy() &&
    IsRpmCapableReader(source->getProxy()->GetXMLName());
}
}

//-----------------------------------------------------------------------------
lqRPMTextReaction::lqRPMTextReaction(QAction* parent)
  : Superclass(parent)
{
  QObject::connect(&pqActiveObjects::instance(),
    &pqActiveObjects::sourceChanged,
    this,
    &lqRPMTextReaction::updateEnableState);
  this->updateEnableState();
}

//-----------------------------------------------------------------------------
void lqRPMTextReaction::updateEnableState()
{
  this->parentAction()->setEnabled(::IsActiveRpmCapableSource());
}

//-----------------------------------------------------------------------------
void lqRPMTextReaction::onTriggered()
{
  pqPipelineSource* source = pqActiveObjects::instance().activeSource();
  if (!source || !source->getProxy() ||
    !IsRpmCapableReader(source->getProxy()->GetXMLName()))
  {
    return;
  }

  pqPipelineSource* rpm = ::FindRPMTextConsumer(source);
  if (!rpm)
  {
    pqObjectBuilder* builder = pqApplicationCore::instance()->getObjectBuilder();
    rpm = builder->createFilter("filters", "RPMText", source);
    if (!rpm)
    {
      return;
    }
    rpm->getProxy()->UpdateVTKObjects();
  }

  const bool visible = this->parentAction()->isChecked();
  if (pqView* view = pqActiveObjects::instance().activeView())
  {
    if (pqDataRepresentation* representation = rpm->getRepresentation(view))
    {
      representation->setVisible(visible);
    }
  }
  QSettings().setValue("LidarView/ShowRPM", visible);
  pqApplicationCore::instance()->render();
}
