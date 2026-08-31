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
// 功能：加载 PLY 点云时自动追加 Vertex Glyph filter 并切换到
//       Points representation，使无 cell 的 PLY 可直接用标准 Points 显示。
//       对所有界面模式生效。
// 作者：acelan
// 新建时间：2026-08-31
// 修改时间：2026-08-31
// ============================================================

#include "lqPointCloudAutoRepresentationBehavior.h"

#include <vtkNew.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMProxy.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMParaViewPipelineControllerWithRendering.h>

#include <pqActiveObjects.h>
#include <pqApplicationCore.h>
#include <pqObjectBuilder.h>
#include <pqPipelineSource.h>
#include <pqProxy.h>
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

  // Use a reasonable point size for meter-scale clouds.
  vtkSMProperty* sizeProp = reprProxy->GetProperty("PointSize");
  if (sizeProp)
  {
    vtkSMPropertyHelper(sizeProp).Set(2.0);
    reprProxy->UpdateVTKObjects();
  }

  pqActiveObjects::instance().setActiveSource(filter);
}
