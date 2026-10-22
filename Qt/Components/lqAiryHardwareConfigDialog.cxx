// ============================================================
// 功能：Airy（SF-Q96A）雷达硬件参数对话框实现（自定义表格布局）。
// 作者：acelan
// 新建时间：2026-10-20
// 修改时间：2026-10-20
// ============================================================

#include "lqAiryHardwareConfigDialog.h"

#include "lqAiryHardwareConfig.h"

#include <QButtonGroup>
#include <QDebug>
#include <QIntValidator>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSet>
#include <QSettings>
#include <QSpinBox>
#include <QStyle>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariantMap>
#include <QWidget>

namespace
{
const QSet<QString> FixedKeys = { "SrcIp", "SrcMask", "SrcGateWay", "DstIp", "MPort", "DPort",
  "ReMode" };

struct FieldSpec
{
  enum Kind
  {
    Text,
    Enum,
    IntRange
  } kind = Text;
  QStringList options;
  int min = 0;
  int max = 0;
};

const QHash<QString, FieldSpec>& Specs()
{
  static const QHash<QString, FieldSpec> specs = {
    { "ReMode", { FieldSpec::Enum, { "0=Strongest", "1=First", "2=Last", "3=Dual" } } },
    { "SynSrc", { FieldSpec::Enum, { "0=GPS", "1=PTP-E2E", "2=PTP-P2P", "3=PTP-GPTP",
                                     "4=PTP-E2E-L2" } } },
    { "RTPdelay", { FieldSpec::Enum, { "0=OFF", "1=ON" } } },
    { "NoLeap", { FieldSpec::Enum, { "0=OFF", "1=ON" } } },
    { "RefEn", { FieldSpec::Enum, { "0=OFF", "1=ON" } } },
    { "TFilLvl", { FieldSpec::Enum, { "1=1", "2=2", "3=3", "4=4", "5=5", "6=6", "7=7" } } },
    { "OpM", { FieldSpec::Enum, { "0=Standby", "1=High-Performance" } } },
    { "RotaSpe", { FieldSpec::Enum, { "1=300", "2=600", "3=1200" } } },
    { "GPSBRate", { FieldSpec::Enum, { "0=1200", "1=2400", "2=4800", "3=9600", "4=14400",
                                       "5=19200", "6=38400", "7=43200", "8=57600", "9=76800",
                                       "10=115200" } } },
    { "MotorR", { FieldSpec::Enum, { "0=FORWARD", "1=REVERSE" } } },
    { "MSideEn", { FieldSpec::Enum, { "0=Off", "1=On" } } },
    { "ImuCtrl", { FieldSpec::Enum, { "0=OFF", "1=ON" } } },
    { "ImuOutR", { FieldSpec::Enum, { "0=25Hz", "1=50Hz", "2=100Hz", "3=200Hz" } } },
    { "AccRg", { FieldSpec::Enum, { "0=[-2g, 2g]", "1=[-4g, 4g]", "2=[-8g, 8g]", "3=[-16g, 16g]" } } },
    { "GyroRg", { FieldSpec::Enum, { "0=[-250, 250]dps", "1=[-500, 500]dps",
                                     "2=[-1000, 1000]dps", "3=[-2000, 2000]dps" } } },
    { "ResDefault", { FieldSpec::Enum, { "0=OFF", "1=ON" } } },
    { "TFilNrEn", { FieldSpec::Enum, { "0=OFF", "1=ON" } } },
    { "TSpeM", { FieldSpec::Enum, { "0=OFF", "1=ON" } } },
    { "BlkRnDis", { FieldSpec::Enum, { "0=30cm", "1=20cm", "2=10cm" } } },
    { "Rn", { FieldSpec::Enum, { "0=high", "1=medium", "2=low" } } },
    { "Blk", { FieldSpec::Enum, { "0=high", "1=medium", "2=low" } } },
    { "DZ10cm", { FieldSpec::Enum, { "0=On", "1=Off" } } },
    { "C81858993", { FieldSpec::Enum, { "0=Off", "1=On" } } },
    { "GapFill", { FieldSpec::Enum, { "0=Off", "1=On" } } },
    { "RanM", { FieldSpec::Enum, { "0=Precision", "1=RainFog", "2=Balance" } } },
    { "RnFgSwt", { FieldSpec::Enum, { "0=Off", "1=On" } } },
    { "L48", { FieldSpec::Enum, { "0=96", "1=48" } } },
    { "IPort", { FieldSpec::IntRange, {}, 1025, 65535 } },
    { "PDomNum", { FieldSpec::IntRange, {}, 0, 127 } },
    { "SyTOut", { FieldSpec::IntRange, {}, 1, 255 } },
    { "UToL", { FieldSpec::IntRange, {}, 1, 255 } },
    { "LToU", { FieldSpec::IntRange, {}, 1, 255 } },
    { "PhaLockSet", { FieldSpec::IntRange, {}, 0, 359 } },
    { "FSAgl", { FieldSpec::IntRange, {}, 0, 359 } },
    { "Lpf", { FieldSpec::IntRange, {}, 0, 255 } },
    { "FovS", { FieldSpec::IntRange, {}, 0, 360 } },
    { "FovE", { FieldSpec::IntRange, {}, 0, 360 } },
  };
  return specs;
}

const QHash<QString, QString>& Labels()
{
  static const QHash<QString, QString> labels = {
    { "FovS", lqAiryHardwareConfigDialog::tr("FOV Start (deg)") },
    { "FovE", lqAiryHardwareConfigDialog::tr("FOV End (deg)") },
    { "SynSrc", lqAiryHardwareConfigDialog::tr("Time Sync Source") },
    { "PDomNum", lqAiryHardwareConfigDialog::tr("PTP Domain Number") },
    { "RTPdelay", lqAiryHardwareConfigDialog::tr("Respond To PeerDelayRequest") },
    { "NoLeap", lqAiryHardwareConfigDialog::tr("No Leap Second") },
    { "SyTOut", lqAiryHardwareConfigDialog::tr("Sync Timeout (s)") },
    { "UToL", lqAiryHardwareConfigDialog::tr("Unlock To Lock Threshold (ms)") },
    { "LToU", lqAiryHardwareConfigDialog::tr("Lock To Unlock Threshold (ms)") },
    { "RefEn", lqAiryHardwareConfigDialog::tr("Reflectivity Enhancement") },
    { "TFilLvl", lqAiryHardwareConfigDialog::tr("Trail Filter Level") },
    { "OpM", lqAiryHardwareConfigDialog::tr("Operation Mode") },
    { "PhaLockSet", lqAiryHardwareConfigDialog::tr("Phase Lock Setting (deg)") },
    { "RotaSpe", lqAiryHardwareConfigDialog::tr("Rotation Speed") },
    { "GPSBRate", lqAiryHardwareConfigDialog::tr("GPS Baud Rate") },
    { "MotorR", lqAiryHardwareConfigDialog::tr("Motor Reversal") },
    { "MSideEn", lqAiryHardwareConfigDialog::tr("Side Installation Mode") },
    { "ImuCtrl", lqAiryHardwareConfigDialog::tr("Imu Ctrl") },
    { "IPort", lqAiryHardwareConfigDialog::tr("Imu Port") },
    { "ImuOutR", lqAiryHardwareConfigDialog::tr("Imu Output Rate") },
    { "AccRg", lqAiryHardwareConfigDialog::tr("Accel Range") },
    { "GyroRg", lqAiryHardwareConfigDialog::tr("Gyro Range") },
    { "Lpf", lqAiryHardwareConfigDialog::tr("Imu LPF Config") },
    { "ResDefault", lqAiryHardwareConfigDialog::tr("Restore Default") },
    { "msop_vlan", lqAiryHardwareConfigDialog::tr("MSOP Vlan") },
    { "difop_vlan", lqAiryHardwareConfigDialog::tr("DIFOP Vlan") },
    { "ptp_vlan", lqAiryHardwareConfigDialog::tr("PTP Vlan") },
    { "TFilNrEn", lqAiryHardwareConfigDialog::tr("Trail Filter Near Enhancement") },
    { "TSpeM", lqAiryHardwareConfigDialog::tr("Trail Special Mode") },
    { "BlkRnDis", lqAiryHardwareConfigDialog::tr("Rain/Blockage Detection Distance") },
    { "Rn", lqAiryHardwareConfigDialog::tr("Rain Detection Sensitivity") },
    { "Blk", lqAiryHardwareConfigDialog::tr("Blockage Detection Sensitivity") },
    { "FSAgl", lqAiryHardwareConfigDialog::tr("Frame Start Angle (deg)") },
    { "DZ10cm", lqAiryHardwareConfigDialog::tr("Dead Zone 10cm Enable") },
    { "C81858993", lqAiryHardwareConfigDialog::tr("Channel 81 85 89 93 Enable") },
    { "GapFill", lqAiryHardwareConfigDialog::tr("Gap Filling Enable") },
    { "RanM", lqAiryHardwareConfigDialog::tr("Ranging Mode") },
    { "RnFgSwt", lqAiryHardwareConfigDialog::tr("Rain Fog Switch") },
    { "L48", lqAiryHardwareConfigDialog::tr("Number Of Lines") },
    { "MPort1", lqAiryHardwareConfigDialog::tr("Secondary MSOP Port") },
  };
  return labels;
}

bool IsValidIPv4(const QString& ip)
{
  const QStringList parts = ip.split('.');
  if (parts.size() != 4)
  {
    return false;
  }
  for (const QString& part : parts)
  {
    bool ok = false;
    const int value = part.toInt(&ok);
    if (!ok || value < 0 || value > 255)
    {
      return false;
    }
  }
  return true;
}

bool IsValidPort(const QString& port)
{
  bool ok = false;
  const int value = port.toInt(&ok);
  return ok && value >= 1025 && value <= 65535;
}

QString LabelFor(const QString& key)
{
  return Labels().value(key, key);
}
}

//-----------------------------------------------------------------------------
lqAiryHardwareConfigDialog::lqAiryHardwareConfigDialog(QWidget* parent)
  : Superclass(parent)
{
  this->setWindowTitle(tr("LiDAR Hardware Parameters"));

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(9, 9, 9, 9);
  layout->setSpacing(6);

  auto addSectionTitle = [this](QVBoxLayout* target, const QString& text) {
    auto* title = new QLabel(text, this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    target->addWidget(title);
  };

  addSectionTitle(layout, tr("Device Info"));
  auto* device = new QWidget(this);
  auto* deviceGrid = new QGridLayout(device);
  auto addInfo = [&](int row, int column, const QString& label, QLineEdit*& edit) {
    auto* cell = new QHBoxLayout;
    cell->addWidget(new QLabel(label, device));
    edit = new QLineEdit(device);
    edit->setEnabled(false);
    cell->addWidget(edit);
    deviceGrid->addLayout(cell, row, column);
  };
  addInfo(0, 0, tr("Serial Number:"), this->SerialEdit);
  addInfo(0, 1, tr("Number Of Lines:"), this->LinesEdit);
  addInfo(1, 0, tr("MAC Address:"), this->MacEdit);
  addInfo(1, 1, tr("Mainboard Firmware:"), this->MainboardFwEdit);
  addInfo(2, 0, tr("Baseboard Firmware:"), this->BaseboardFwEdit);
  addInfo(2, 1, tr("APP Firmware:"), this->AppFwEdit);
  addInfo(3, 0, tr("Motor Firmware:"), this->MotorFwEdit);
  layout->addWidget(device);

  addSectionTitle(layout, tr("Common Parameters"));
  auto* common = new QWidget(this);
  auto* grid = new QGridLayout(common);
  auto addRow = [&](int row, int column, const QString& label, QLineEdit*& edit) {
    auto* cell = new QHBoxLayout;
    cell->addWidget(new QLabel(label, common));
    edit = new QLineEdit(common);
    cell->addWidget(edit);
    grid->addLayout(cell, row, column);
  };
  addRow(0, 0, tr("Host (PC) IP:"), this->DstIpEdit);
  addRow(0, 1, tr("LiDAR IP:"), this->SrcIpEdit);
  addRow(1, 0, tr("Subnet Mask:"), this->SrcMaskEdit);
  addRow(1, 1, tr("Gateway:"), this->SrcGatewayEdit);
  addRow(2, 0, tr("MSOP Port:"), this->MPortEdit);
  addRow(2, 1, tr("DIFOP Port:"), this->DPortEdit);

  const QRegularExpression ipPattern(QStringLiteral("^[0-9]{0,3}(\\.[0-9]{0,3}){0,3}$"));
  auto* ipValidator = new QRegularExpressionValidator(ipPattern, this);
  for (QLineEdit* edit : { this->SrcIpEdit, this->SrcMaskEdit, this->SrcGatewayEdit, this->DstIpEdit })
  {
    edit->setValidator(ipValidator);
  }
  auto* portValidator = new QIntValidator(1025, 65535, this);
  this->MPortEdit->setValidator(portValidator);
  this->DPortEdit->setValidator(portValidator);

  auto* refreshButton = new QPushButton(this);
  refreshButton->setIcon(this->style()->standardIcon(QStyle::SP_BrowserReload));
  refreshButton->setToolTip(tr("Refresh"));
  refreshButton->setFlat(true);
  refreshButton->setFixedWidth(28);
  if (QLayoutItem* cell = grid->itemAtPosition(0, 1))
  {
    if (QLayout* cellLayout = cell->layout())
    {
      cellLayout->addWidget(refreshButton);
    }
  }
  QObject::connect(refreshButton, &QPushButton::clicked, this,
    &lqAiryHardwareConfigDialog::onFetch);

  auto* modeRow = new QHBoxLayout;
  modeRow->addWidget(new QLabel(tr("Return Mode:"), common));
  this->ReturnModeGroup = new QButtonGroup(this);
  const QStringList modeLabels = { tr("Strongest"), tr("First"), tr("Last"), tr("Dual") };
  for (int i = 0; i < modeLabels.size(); ++i)
  {
    auto* button = new QRadioButton(modeLabels[i], common);
    this->ReturnModeGroup->addButton(button, i);
    modeRow->addWidget(button);
  }
  modeRow->addStretch();
  grid->addLayout(modeRow, 3, 0, 1, 2);
  layout->addWidget(common);

  // "More settings" form is kept in code but not exposed.
  this->OtherScroll = new QScrollArea(this);
  this->OtherScroll->setWidgetResizable(true);
  this->OtherHost = new QWidget(this->OtherScroll);
  this->OtherForm = new QFormLayout(this->OtherHost);
  this->OtherScroll->setWidget(this->OtherHost);
  this->OtherScroll->setVisible(false);
  layout->addWidget(this->OtherScroll);

  auto* buttonBox = new QDialogButtonBox(
    QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this);
  layout->addWidget(buttonBox);
  QObject::connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
    this->onApply();
    this->accept();
  });
  QObject::connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  if (QPushButton* apply = buttonBox->button(QDialogButtonBox::Apply))
  {
    QObject::connect(apply, &QPushButton::clicked, this, &lqAiryHardwareConfigDialog::onApply);
  }

  this->SrcIpEdit->setText(
    QSettings().value("LidarView/AiryConfigIp", "192.168.1.200").toString());

  this->setMinimumWidth(720);
  this->adjustSize();
  QTimer::singleShot(0, this, &lqAiryHardwareConfigDialog::onFetch);
}

//-----------------------------------------------------------------------------
void lqAiryHardwareConfigDialog::addField(
  QFormLayout* form, const QString& key, const QString& label, const QString& value)
{
  const FieldSpec& spec = Specs()[key];
  QWidget* editor = nullptr;
  if (spec.kind == FieldSpec::Enum)
  {
    auto* combo = new QComboBox(this->OtherHost);
    for (const QString& option : spec.options)
    {
      const int equal = option.indexOf('=');
      combo->addItem(option.mid(equal + 1), option.left(equal));
    }
    const int index = combo->findData(value);
    combo->setCurrentIndex(index >= 0 ? index : 0);
    editor = combo;
  }
  else if (spec.kind == FieldSpec::IntRange)
  {
    auto* spin = new QSpinBox(this->OtherHost);
    spin->setRange(spec.min, spec.max);
    spin->setValue(value.toInt());
    editor = spin;
  }
  else
  {
    editor = new QLineEdit(value, this->OtherHost);
  }
  form->addRow(label, editor);
  this->OtherEditors.insert(key, editor);
}

//-----------------------------------------------------------------------------
void lqAiryHardwareConfigDialog::clearOtherForm()
{
  while (this->OtherForm->count() > 0)
  {
    QLayoutItem* item = this->OtherForm->takeAt(0);
    if (QWidget* widget = item->widget())
    {
      widget->deleteLater();
    }
    delete item;
  }
  this->OtherEditors.clear();
}

//-----------------------------------------------------------------------------
void lqAiryHardwareConfigDialog::onFetch()
{
  // The field is authoritative (so a device on a new IP can be reached);
  // the saved IP is only the default value of that field.
  const QString typedIp = this->SrcIpEdit->text().trimmed();
  this->LastTargetIp = typedIp.isEmpty()
    ? QSettings().value("LidarView/AiryConfigIp", "192.168.1.200").toString()
    : typedIp;

  qDebug() << "[AiryFetch] target ip=" << this->LastTargetIp;
  lqAiryHardwareConfig config;
  QVariantMap values;
  QString error;
  if (!config.fetch(this->LastTargetIp, values, error))
  {
    QMessageBox::warning(this, tr("Refresh"), error);
    return;
  }
  QSettings().setValue("LidarView/AiryConfigIp", this->LastTargetIp);

  auto assign = [&values](QLineEdit* edit, const char* key) {
    if (edit)
    {
      edit->setText(values.value(QLatin1String(key)).toString());
    }
  };
  assign(this->SrcIpEdit, "SrcIp");
  assign(this->SrcMaskEdit, "SrcMask");
  assign(this->SrcGatewayEdit, "SrcGateWay");
  assign(this->DstIpEdit, "DstIp");
  assign(this->MPortEdit, "MPort");
  assign(this->DPortEdit, "DPort");
  if (auto* button = this->ReturnModeGroup->button(values.value("ReMode").toInt()))
  {
    button->setChecked(true);
  }

  lqAiryHardwareConfig::DeviceInfo info;
  QString deviceError;
  const bool deviceOk = config.fetchDeviceInfo(this->LastTargetIp, info, deviceError);
  qDebug() << "[AiryFetch] device info" << (deviceOk ? "ok" : "failed") << deviceError;
  if (deviceOk)
  {
    this->SerialEdit->setText(info.serial);
    this->LinesEdit->setText(QString::number(info.lines));
    this->MacEdit->setText(info.mac);
    this->MainboardFwEdit->setText(info.mainboardFw);
    this->BaseboardFwEdit->setText(info.baseboardFw);
    this->AppFwEdit->setText(info.appFw);
    this->MotorFwEdit->setText(info.motorFw);
  }

  this->clearOtherForm();
  this->adjustSize();
  for (auto it = values.constBegin(); it != values.constEnd(); ++it)
  {
    if (!FixedKeys.contains(it.key()))
    {
      this->addField(this->OtherForm, it.key(), LabelFor(it.key()), it.value().toString());
    }
  }
}

//-----------------------------------------------------------------------------
void lqAiryHardwareConfigDialog::onApply()
{
  QStringList invalid;
  if (!IsValidIPv4(this->SrcIpEdit->text().trimmed())) invalid << tr("LiDAR IP");
  if (!IsValidIPv4(this->SrcMaskEdit->text().trimmed())) invalid << tr("Subnet Mask");
  if (!IsValidIPv4(this->SrcGatewayEdit->text().trimmed())) invalid << tr("Gateway");
  if (!IsValidIPv4(this->DstIpEdit->text().trimmed())) invalid << tr("Host (PC) IP");
  if (!IsValidPort(this->MPortEdit->text().trimmed())) invalid << tr("MSOP Port");
  if (!IsValidPort(this->DPortEdit->text().trimmed())) invalid << tr("DIFOP Port");
  if (!invalid.isEmpty())
  {
    QMessageBox::warning(this, tr("Apply"),
      tr("Invalid value(s): %1").arg(invalid.join(QStringLiteral(", "))));
    return;
  }

  QVariantMap values;
  values.insert(QStringLiteral("SrcIp"), this->SrcIpEdit->text().trimmed());
  values.insert(QStringLiteral("SrcMask"), this->SrcMaskEdit->text().trimmed());
  values.insert(QStringLiteral("SrcGateWay"), this->SrcGatewayEdit->text().trimmed());
  values.insert(QStringLiteral("DstIp"), this->DstIpEdit->text().trimmed());
  values.insert(QStringLiteral("MPort"), this->MPortEdit->text().trimmed());
  values.insert(QStringLiteral("DPort"), this->DPortEdit->text().trimmed());
  if (int mode = this->ReturnModeGroup->checkedId(); mode >= 0)
  {
    values.insert(QStringLiteral("ReMode"), QString::number(mode));
  }
  for (auto it = this->OtherEditors.constBegin(); it != this->OtherEditors.constEnd(); ++it)
  {
    if (auto* combo = qobject_cast<QComboBox*>(it.value()))
    {
      values.insert(it.key(), combo->currentData().toString());
    }
    else if (auto* spin = qobject_cast<QSpinBox*>(it.value()))
    {
      values.insert(it.key(), QString::number(spin->value()));
    }
    else if (auto* line = qobject_cast<QLineEdit*>(it.value()))
    {
      values.insert(it.key(), line->text());
    }
  }

  // Submit to the last successfully used device IP (the old IP).
  const QString target =
    QSettings().value("LidarView/AiryConfigIp", "192.168.1.200").toString();

  qDebug() << "[AiryApply] target(old ip)=" << target
           << "new SrcIp=" << values.value("SrcIp").toString()
           << "DstIp=" << values.value("DstIp").toString()
           << "MPort=" << values.value("MPort").toString()
           << "DPort=" << values.value("DPort").toString()
           << "fields=" << values.size();

  lqAiryHardwareConfig config;
  QString error;
  bool ok = config.apply(target, values, error);

  const QString newIp = this->SrcIpEdit->text().trimmed();
  const bool ipChanged = !newIp.isEmpty() && newIp != target;
  const QString verifyIp = ipChanged ? newIp : target;

  // Verify by reading the device back and comparing the submitted values.
  QVariantMap current;
  QString verifyError;
  bool reachable = config.fetch(verifyIp, current, verifyError);
  for (int attempt = 0; !reachable && attempt < 5; ++attempt)
  {
    QThread::msleep(2000);
    reachable = config.fetch(verifyIp, current, verifyError);
  }

  if (!reachable)
  {
    if (ipChanged)
    {
      QMessageBox::information(this, tr("Apply"),
        tr("New IP sent. The device disconnects while switching its address; use Refresh to confirm."));
    }
    else
    {
      QMessageBox::warning(this, tr("Apply"), error);
    }
    return;
  }

  QStringList mismatched;
  for (const QString& key :
    { "SrcIp", "DstIp", "MPort", "DPort", "ReMode", "FovS", "FovE", "OpM", "RotaSpe" })
  {
    if (values.contains(key) && current.value(key).toString() != values.value(key).toString())
    {
      mismatched << key;
    }
  }
  if (!mismatched.isEmpty())
  {
    QMessageBox::warning(this, tr("Apply"),
      tr("Not applied: %1").arg(mismatched.join(QStringLiteral(", "))));
    return;
  }

  if (!newIp.isEmpty())
  {
    QSettings().setValue("LidarView/AiryConfigIp", newIp);
  }
  QMessageBox::information(this, tr("Apply"), tr("Settings applied."));

}
