// ============================================================
// 功能：Airy（SF-Q96A）雷达硬件参数对话框（自定义表格布局）：
//       设备信息只读 + 常用参数两列 + 回波模式单选；⚙ 内联展开其余参数；
//       底部 应用/取消/确定；IP 旁刷新图标；下发用获取时的旧 IP。
// 作者：acelan
// 新建时间：2026-10-20
// 修改时间：2026-10-20
// ============================================================

#ifndef lqAiryHardwareConfigDialog_h
#define lqAiryHardwareConfigDialog_h

#include "lqComponentsModule.h"

#include <QDialog>
#include <QMap>
#include <QString>

class QButtonGroup;
class QFormLayout;
class QLineEdit;
class QScrollArea;
class QWidget;

class LQCOMPONENTS_EXPORT lqAiryHardwareConfigDialog : public QDialog
{
  Q_OBJECT
  typedef QDialog Superclass;

public:
  explicit lqAiryHardwareConfigDialog(QWidget* parent = nullptr);
  ~lqAiryHardwareConfigDialog() override = default;

protected Q_SLOTS:
  void onFetch();
  void onApply();

private:
  void clearOtherForm();
  void addField(QFormLayout* form, const QString& key, const QString& label, const QString& value);

  QLineEdit* SrcIpEdit = nullptr;
  QLineEdit* SrcMaskEdit = nullptr;
  QLineEdit* SrcGatewayEdit = nullptr;
  QLineEdit* DstIpEdit = nullptr;
  QLineEdit* MPortEdit = nullptr;
  QLineEdit* DPortEdit = nullptr;
  QButtonGroup* ReturnModeGroup = nullptr;

  QLineEdit* SerialEdit = nullptr;
  QLineEdit* LinesEdit = nullptr;
  QLineEdit* MacEdit = nullptr;
  QLineEdit* MainboardFwEdit = nullptr;
  QLineEdit* BaseboardFwEdit = nullptr;
  QLineEdit* AppFwEdit = nullptr;
  QLineEdit* MotorFwEdit = nullptr;

  QScrollArea* OtherScroll = nullptr;
  QWidget* OtherHost = nullptr;
  QFormLayout* OtherForm = nullptr;
  QMap<QString, QWidget*> OtherEditors;
  QString LastTargetIp;

  Q_DISABLE_COPY(lqAiryHardwareConfigDialog)
};

#endif
