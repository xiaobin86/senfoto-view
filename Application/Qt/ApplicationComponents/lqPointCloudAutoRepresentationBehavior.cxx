/*=========================================================================

  Program: LidarView
  Module:  lqPointCloudAutoRepresentationBehavior.cxx

  Copyright (c) Kitware Inc.
  All rights reserved.
  See LICENSE or http://www.apache.org/licenses/LICENSE-2.0 for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

// ============================================================
// 功能：1) 加载 PLY 点云时自动追加 Vertex Glyph filter 并切换到
//       Points representation，使无 cell 的 PLY 可直接用标准 Points 显示。
//       2) 所有雷达/Lidar 数据源（<Sensor>LidarReader / <Sensor>LidarStream，
//       含其派生图层）的表示默认点大小设为 1 像素，属性框默认值也显示 1。
//       对所有界面模式生效。
// 作者：acelan
// 新建时间：2026-08-31
// 修改时间：2026-10-16
// ============================================================

#include "lqPointCloudAutoRepresentationBehavior.h"

#include <vtkNew.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMProxy.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMParaViewPipelineControllerWithRendering.h>

#include <pqActiveObjects.h>
#include <pqApplicationCore.h>
#include <pqDataRepresentation.h>
#include <pqObjectBuilder.h>
#include <pqOutputPort.h>
#include <pqPipelineFilter.h>
#include <pqPipelineSource.h>
#include <pqProxy.h>
#include <pqRepresentation.h>
#include <pqServerManagerModel.h>
#include <pqView.h>

#include <cstring>

namespace
{
constexpr const char* POINTS_REPRESENTATION = "Points";
constexpr const char* VERTEX_GLYPH_FILTER = "VertexGlyph";
constexpr const char* PLY_READER = "PLYReader";
}

//-----------------------------------------------------------------------------
lqPointCloudAutoRepresentationBehavior::lqPointCloudAutoRepresentationBehavior(QObject* parent)
  : QObject(parent)
{
  pqServerManagerModel* smmodel = pqApplicationCore::instance()->getServerManagerModel();
  QObject::connect(smmodel,
    &pqServerManagerModel::sourceAdded,
    this,
    &lqPointCloudAutoRepresentationBehavior::onSourceAdded,
    Qt::QueuedConnection);
  QObject::connect(smmodel,
    &pqServerManagerModel::representationAdded,
    this,
    &lqPointCloudAutoRepresentationBehavior::onRepresentationAdded,
    Qt::QueuedConnection);
}

//-----------------------------------------------------------------------------
bool lqPointCloudAutoRepresentationBehavior::NeedsVertexGlyph(pqPipelineSource* source)
{
  if (!source)
  {
    return false;
  }
  vtkSMProxy* proxy = source->getProxy();
  if (!proxy)
  {
    return false;
  }
  const char* xmlName = proxy->GetXMLName();
  return xmlName && std::strcmp(xmlName, PLY_READER) == 0;
}

//-----------------------------------------------------------------------------
bool lqPointCloudAutoRepresentationBehavior::IsLidarPointCloudSource(pqPipelineSource* source)
{
  if (!source)
  {
    return false;
  }

  vtkSMProxy* proxy = source->getProxy();
  if (proxy)
  {
    const char* xmlName = proxy->GetXMLName();
    if (xmlName &&
      (std::strstr(xmlName, "LidarReader") != nullptr ||
        std::strstr(xmlName, "LidarStream") != nullptr))
    {
      return true;
    }
  }

  // Filters auto-attached to a lidar source (e.g. LaserSelection,
  // RadialDistanceDenoise) are not lidar proxies themselves; walk upstream so
  // their point clouds are covered too.
  pqPipelineFilter* filter = qobject_cast<pqPipelineFilter*>(source);
  if (filter)
  {
    for (pqOutputPort* port : filter->getInputs())
    {
      if (port && IsLidarPointCloudSource(port->getSource()))
      {
        return true;
      }
    }
  }

  return false;
}

//-----------------------------------------------------------------------------
void lqPointCloudAutoRepresentationBehavior::HideSourceRepresentation(
  pqPipelineSource* source, pqView* view)
{
  if (!source || !view)
  {
    return;
  }
  pqDataRepresentation* repr = source->getRepresentation(view);
  if (!repr || !repr->getProxy())
  {
    return;
  }
  vtkSMPropertyHelper(repr->getProxy(), "Visibility").Set(0);
  repr->getProxy()->UpdateVTKObjects();
}

//-----------------------------------------------------------------------------
void lqPointCloudAutoRepresentationBehavior::onSourceAdded(pqPipelineSource* source)
{
  if (!NeedsVertexGlyph(source))
  {
    return;
  }

  pqView* view = pqActiveObjects::instance().activeView();
  if (!view)
  {
    return;
  }

  // Hide the raw PLY source representation so only the glyph output is shown.
  this->HideSourceRepresentation(source, view);

  // Append VertexGlyph filter to create a vertex cell per point.
  pqObjectBuilder* builder = pqApplicationCore::instance()->getObjectBuilder();
  pqPipelineSource* filter = builder->createFilter("filters", VERTEX_GLYPH_FILTER, source);
  if (!filter)
  {
    return;
  }

  vtkSMProxy* filterProxy = filter->getProxy();
  filterProxy->UpdateVTKObjects();
  filter->updatePipeline();
  filter->setModifiedState(pqProxy::UNMODIFIED);

  // Show the glyph output in the active view.
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  vtkSMProxy* reprProxy = controller->Show(filter->getSourceProxy(), 0, view->getViewProxy());
  if (!reprProxy)
  {
    return;
  }

  // Switch to Points representation.
  vtkSMProperty* repProp = reprProxy->GetProperty("Representation");
  if (repProp)
  {
    vtkSMPropertyHelper(repProp).Set(POINTS_REPRESENTATION);
    reprProxy->UpdateVTKObjects();
  }

  vtkSMProperty* sizeProp = reprProxy->GetProperty("PointSize");
  if (sizeProp)
  {
    vtkSMPropertyHelper(sizeProp).Set(1.0);
    reprProxy->UpdateVTKObjects();
  }

  pqActiveObjects::instance().setActiveSource(filter);
}

//-----------------------------------------------------------------------------
void lqPointCloudAutoRepresentationBehavior::onRepresentationAdded(
  pqRepresentation* representation)
{
  pqDataRepresentation* dataRepresentation = qobject_cast<pqDataRepresentation*>(representation);
  if (!dataRepresentation || !IsLidarPointCloudSource(dataRepresentation->getInput()))
  {
    return;
  }

  vtkSMProxy* reprProxy = dataRepresentation->getProxy();
  if (!reprProxy)
  {
    return;
  }

  vtkSMProperty* sizeProp = reprProxy->GetProperty("PointSize");
  if (!sizeProp)
  {
    return;
  }

  vtkSMPropertyHelper(sizeProp).Set(1.0);
  reprProxy->UpdateVTKObjects();
}
