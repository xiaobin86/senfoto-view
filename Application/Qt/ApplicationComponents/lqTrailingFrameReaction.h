/*=========================================================================

  Program: LidarView
  Module:  lqTrailingFrameReaction.h

  Copyright (c) Kitware Inc.
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

// ============================================================
// 功能：SenFoToView 新增功能 —— Trailing Frame（拖尾帧）工具栏 Reaction；
//        把工具栏上的 TF 数值接入 TrailingFrame 过滤器的 NumberOfTrailingFrames。
// 作者：acelan
// 新建时间：2026-10-09
// 修改时间：2026-10-09
// ============================================================

#ifndef lqTrailingFrameReaction_h
#define lqTrailingFrameReaction_h

#include "lvApplicationComponentsModule.h"

#include <QObject>
#include <QPointer>

class QSpinBox;
class pqPipelineSource;

/**
 * Reaction that shows a spin-box controlled number of trailing frames.
 * On value change it applies (and keeps in sync) the "TrailingFrame" filter
 * on the active lidar source. A value of 0 disables the trailing frame.
 */
class LVAPPLICATIONCOMPONENTS_EXPORT lqTrailingFrameReaction : public QObject
{
  Q_OBJECT
  typedef QObject Superclass;

public:
  lqTrailingFrameReaction(QSpinBox* spinBox, QObject* parent = nullptr);

public Q_SLOTS:
  void applyNumberOfTrailingFrames(int numberOfFrames);

private:
  Q_DISABLE_COPY(lqTrailingFrameReaction)

  QSpinBox* SpinBox;
  QPointer<pqPipelineSource> Filter;
  QPointer<pqPipelineSource> LidarSource;
};

#endif
