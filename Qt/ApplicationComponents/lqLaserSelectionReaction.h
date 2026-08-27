/*=========================================================================

  Program: LidarView
  Module:  lqLaserSelectionReaction.h

  Copyright (c) Kitware Inc.
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#ifndef lqLaserSelectionReaction_h
#define lqLaserSelectionReaction_h

#include "lqApplicationComponentsModule.h"

#include <QObject>

class QAction;

/**
 * @ingroup Reactions
 * Reaction that shows the lqLaserSelectionDialog non-modally so the user can
 * enable/disable individual lidar channels of the active lidar source.
 */
class LQAPPLICATIONCOMPONENTS_EXPORT lqLaserSelectionReaction : public QObject
{
  Q_OBJECT
  typedef QObject Superclass;

public:
  /**
   * Constructor. Parent cannot be nullptr.
   */
  lqLaserSelectionReaction(QAction* parent);

public Q_SLOTS:
  void showDialog();

private:
  Q_DISABLE_COPY(lqLaserSelectionReaction)
};

#endif
