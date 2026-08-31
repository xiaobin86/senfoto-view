/*=========================================================================

  Program: LidarView
  Module:  lqPointCloudAutoRepresentationBehavior.h

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

#ifndef lqPointCloudAutoRepresentationBehavior_h
#define lqPointCloudAutoRepresentationBehavior_h

#include "lvApplicationComponentsModule.h"

#include <QObject>

class pqPipelineSource;
class pqView;

/**
 * @brief Automatically display PLY point clouds with Points representation.
 *
 * PLY files containing only vertex positions have no cells, so the default
 * Surface representation shows nothing and the standard Points representation
 * cannot render them either. This behavior appends a VertexGlyph filter to
 * PLY sources and switches the resulting representation to Points so the
 * cloud is visible immediately, in every interface mode.
 */
class LVAPPLICATIONCOMPONENTS_EXPORT lqPointCloudAutoRepresentationBehavior : public QObject
{
  Q_OBJECT

public:
  lqPointCloudAutoRepresentationBehavior(QObject* parent = nullptr);
  ~lqPointCloudAutoRepresentationBehavior() override = default;

private Q_SLOTS:
  void onSourceAdded(pqPipelineSource* source);

private:
  /**
   * @brief Return true if the source proxy corresponds to a point-cloud reader
   *        that needs vertex cells to be displayed with the Points representation.
   */
  static bool NeedsVertexGlyph(pqPipelineSource* source);

  /**
   * @brief Hide the source's existing representation in the active view, if any.
   */
  static void HideSourceRepresentation(pqPipelineSource* source, pqView* view);

  Q_DISABLE_COPY(lqPointCloudAutoRepresentationBehavior)
};

#endif // lqPointCloudAutoRepresentationBehavior_h
