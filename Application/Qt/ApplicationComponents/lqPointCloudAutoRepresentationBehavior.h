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
// 功能：1) 加载 PLY 点云时自动追加 Vertex Glyph filter 并切换到
//       Points representation，使无 cell 的 PLY 可直接用标准 Points 显示。
//       2) 所有雷达/Lidar 数据源（<Sensor>LidarReader / <Sensor>LidarStream，
//       含其派生图层）的表示默认点大小设为 1 像素，属性框默认值也显示 1。
//       对所有界面模式生效。
// 作者：acelan
// 新建时间：2026-08-31
// 修改时间：2026-10-16
// ============================================================

#ifndef lqPointCloudAutoRepresentationBehavior_h
#define lqPointCloudAutoRepresentationBehavior_h

#include "lvApplicationComponentsModule.h"

#include <QObject>

class pqPipelineSource;
class pqRepresentation;
class pqView;

/**
 * @brief Automatically display point clouds with a sensible representation.
 *
 * PLY files containing only vertex positions have no cells, so the default
 * Surface representation shows nothing and the standard Points representation
 * cannot render them either. This behavior appends a VertexGlyph filter to
 * PLY sources and switches the resulting representation to Points so the
 * cloud is visible immediately, in every interface mode.
 *
 * It also forces the point size of every Lidar/radar source representation
 * (and representations of filters derived from them) to 1 pixel, so radar
 * point clouds and the Display properties panel default to a 1-pixel size.
 */
class LVAPPLICATIONCOMPONENTS_EXPORT lqPointCloudAutoRepresentationBehavior : public QObject
{
  Q_OBJECT

public:
  lqPointCloudAutoRepresentationBehavior(QObject* parent = nullptr);
  ~lqPointCloudAutoRepresentationBehavior() override = default;

private Q_SLOTS:
  void onSourceAdded(pqPipelineSource* source);
  void onRepresentationAdded(pqRepresentation* representation);

private:
  /**
   * @brief Return true if the source proxy corresponds to a point-cloud reader
   *        that needs vertex cells to be displayed with the Points representation.
   */
  static bool NeedsVertexGlyph(pqPipelineSource* source);

  /**
   * @brief Return true if the source is a Lidar/radar data source
   *        (<Sensor>LidarReader / <Sensor>LidarStream) or if it is a filter
   *        whose pipeline originates from such a source.
   */
  static bool IsLidarPointCloudSource(pqPipelineSource* source);

  /**
   * @brief Hide the source's existing representation in the active view, if any.
   */
  static void HideSourceRepresentation(pqPipelineSource* source, pqView* view);

  Q_DISABLE_COPY(lqPointCloudAutoRepresentationBehavior)
};

#endif // lqPointCloudAutoRepresentationBehavior_h
