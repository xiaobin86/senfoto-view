// ============================================================
// 功能：Tools 菜单「显示 RPM」开关 —— 切换活动 Senfoto008 源上
//       RPMText 过滤器的可见性（对应旧版 actionShowRPM）。
// 作者：acelan
// 新建时间：2026-10-17
// 修改时间：2026-10-17
// ============================================================

#ifndef lqRPMTextReaction_h
#define lqRPMTextReaction_h

#include <pqReaction.h>

class lqRPMTextReaction : public pqReaction
{
  Q_OBJECT
  typedef pqReaction Superclass;

public:
  lqRPMTextReaction(QAction* parent);

public Q_SLOTS:
  void updateEnableState() override;

protected:
  void onTriggered() override;

private:
  Q_DISABLE_COPY(lqRPMTextReaction)
};

#endif
