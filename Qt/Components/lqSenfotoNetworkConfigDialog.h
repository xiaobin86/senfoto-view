// ============================================================
// 功能：Senfoto008 雷达 IP / 端口配置对话框。
// 作者：acelan
// 新建时间：2026-10-17
// 修改时间：2026-10-17
// ============================================================

#ifndef lqSenfotoNetworkConfigDialog_h
#define lqSenfotoNetworkConfigDialog_h

#include "lqComponentsModule.h"
#include "lqSenfotoNetworkConfig.h"

#include <QDialog>

namespace Ui
{
class lqSenfotoNetworkConfigDialog;
}

class LQCOMPONENTS_EXPORT lqSenfotoNetworkConfigDialog : public QDialog
{
  Q_OBJECT

public:
  explicit lqSenfotoNetworkConfigDialog(QWidget* parent = nullptr);
  ~lqSenfotoNetworkConfigDialog() override;

private Q_SLOTS:
  void onGet();
  void onSet();
  void onConfigReceived(const lqSenfotoNetworkConfig::DeviceInfo& info);
  void onError(const QString& message);

private:
  Ui::lqSenfotoNetworkConfigDialog* Ui;
  lqSenfotoNetworkConfig* Config;

  Q_DISABLE_COPY(lqSenfotoNetworkConfigDialog)
};

#endif
