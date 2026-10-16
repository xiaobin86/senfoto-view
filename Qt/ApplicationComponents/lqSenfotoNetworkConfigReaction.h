// ============================================================
// 功能：Tools 菜单「雷达 IP 配置」Reaction —— 打开 Senfoto008 网络配置
//       对话框（仅活动源为 Senfoto008 时可用）。
// 作者：acelan
// 新建时间：2026-10-17
// 修改时间：2026-10-17
// ============================================================

#ifndef lqSenfotoNetworkConfigReaction_h
#define lqSenfotoNetworkConfigReaction_h

#include <pqReaction.h>

class lqSenfotoNetworkConfigReaction : public pqReaction
{
  Q_OBJECT
  typedef pqReaction Superclass;

public:
  explicit lqSenfotoNetworkConfigReaction(QAction* parent);

public Q_SLOTS:
  void updateEnableState() override;

protected:
  void onTriggered() override;

private:
  Q_DISABLE_COPY(lqSenfotoNetworkConfigReaction)
};

#endif
