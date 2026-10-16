// ============================================================
// 功能：Senfoto008 网络配置密码输入对话框。
// 作者：acelan
// 新建时间：2026-10-17
// 修改时间：2026-10-17
// ============================================================

#ifndef lqSenfotoPasswordDialog_h
#define lqSenfotoPasswordDialog_h

#include "lqComponentsModule.h"

#include <QDialog>

namespace Ui
{
class lqSenfotoPasswordDialog;
}

class LQCOMPONENTS_EXPORT lqSenfotoPasswordDialog : public QDialog
{
  Q_OBJECT

public:
  explicit lqSenfotoPasswordDialog(QWidget* parent = nullptr);
  ~lqSenfotoPasswordDialog() override;

private Q_SLOTS:
  void onShowToggled(int state);
  void onOk();
  void onCancel();

private:
  Ui::lqSenfotoPasswordDialog* Ui;

  Q_DISABLE_COPY(lqSenfotoPasswordDialog)
};

#endif
