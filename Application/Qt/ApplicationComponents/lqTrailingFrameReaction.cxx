/*=========================================================================

  Program: LidarView
  Module:  lqTrailingFrameReaction.cxx

  Copyright (c) Kitware Inc.
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

// ============================================================
// 功能：SenFoToView 新增功能 —— Trailing Frame（拖尾帧）工具栏 Reaction 实现；
//        依据 TF 数值对当前雷达源应用/更新 TrailingFrame 过滤器。
// 作者：acelan
// 新建时间：2026-10-09
// 修改时间：2026-10-13
// ============================================================

#include "lqTrailingFrameReaction.h"

#include "lqSensorListWidget.h"

#include <QMap>
#include <QSpinBox>

#include <pqApplicationCore.h>
#include <pqObjectBuilder.h>
#include <pqOutputPort.h>
#include <pqPipelineFilter.h>
#include <pqPipelineSource.h>
#include <pqServer.h>

#include <vtkSMPropertyHelper.h>
#include <vtkSMProxy.h>
#include <vtkSMSessionProxyManager.h>

//-----------------------------------------------------------------------------
lqTrailingFrameReaction::lqTrailingFrameReaction(QSpinBox* spinBox, QObject* parent)
  : Superclass(parent)
  , SpinBox(spinBox)
{
  this->connect(this->SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
    &lqTrailingFrameReaction::applyNumberOfTrailingFrames);
}

//-----------------------------------------------------------------------------
void lqTrailingFrameReaction::applyNumberOfTrailingFrames(int numberOfFrames)
{
  pqPipelineSource* lidarSource = lqSensorListWidget::instance()->getActiveLidarSource();
  if (!lidarSource)
  {
    return;
  }

  // The cached filter is only valid for the source it was created on.
  if (this->Filter.isNull() || this->LidarSource != lidarSource)
  {
    this->Filter = nullptr;
    this->LidarSource = lidarSource;
  }

  if (this->Filter.isNull() && numberOfFrames > 0)
  {
    pqObjectBuilder* builder = pqApplicationCore::instance()->getObjectBuilder();

    // The convenience overload createFilter(group, name, source) hardcodes the
    // input property name "Input", but the "TrailingFrame" proxy names its
    // first input property "LIDAR Frame". Using it leaves input port 0
    // unconnected and vtkTrailingFrame then raises
    // "Input port 0 ... has 0 connections but is not optional." Resolve the
    // real port name from the proxy definition, as pqFiltersMenuReaction does.
    vtkSMSessionProxyManager* pxm = lidarSource->getServer()->proxyManager();
    vtkSMProxy* prototype = pxm->GetPrototypeProxy("filters", "TrailingFrame");
    QList<const char*> inputPortNames = pqPipelineFilter::getInputPorts(prototype);

    QMap<QString, QList<pqOutputPort*>> namedInputs;
    if (!inputPortNames.isEmpty())
    {
      namedInputs[QString::fromLatin1(inputPortNames[0])].push_back(lidarSource->getOutputPort(0));
    }

    this->Filter =
      builder->createFilter("filters", "TrailingFrame", namedInputs, lidarSource->getServer());
  }

  if (this->Filter)
  {
    vtkSMPropertyHelper(this->Filter->getProxy(), "NumberOfTrailingFrames").Set(numberOfFrames);
    this->Filter->getProxy()->UpdateSelfAndAllInputs();
    pqApplicationCore::instance()->render();
  }
}
