// ============================================================
// 功能：编辑菜单「雷达硬件参数设置」Reaction —— 对 SF-Q96A(Airy) 打开
//       硬件参数对话框；对 Senfoto008 仍打开原网络配置对话框。
// 作者：acelan
// 新建时间：2026-10-20
// 修改时间：2026-10-20
// ============================================================

#ifndef lqAiryHardwareConfigReaction_h
#define lqAiryHardwareConfigReaction_h

#include <pqReaction.h>

class lqAiryHardwareConfigReaction : public pqReaction
{
  Q_OBJECT
  typedef pqReaction Superclass;

public:
  explicit lqAiryHardwareConfigReaction(QAction* parent);

public Q_SLOTS:
  void updateEnableState() override;

protected:
  void onTriggered() override;

private:
  Q_DISABLE_COPY(lqAiryHardwareConfigReaction)
};

#endif
