// ============================================================
// 功能：Senfoto008 雷达 IP / 端口配置对话框实现。
// 作者：acelan
// 新建时间：2026-10-17
// 修改时间：2026-10-17
// ============================================================

#include "lqSenfotoNetworkConfigDialog.h"
#include "lqSenfotoPasswordDialog.h"
#include "ui_lqSenfotoNetworkConfigDialog.h"

#include <QHostAddress>
#include <QMessageBox>
#include <QMetaType>
#include <QSettings>

//-----------------------------------------------------------------------------
lqSenfotoNetworkConfigDialog::lqSenfotoNetworkConfigDialog(QWidget* parent)
  : QDialog(parent)
  , Ui(new Ui::lqSenfotoNetworkConfigDialog)
  , Config(new lqSenfotoNetworkConfig(this))
{
  qRegisterMetaType<lqSenfotoNetworkConfig::DeviceInfo>("lqSenfotoNetworkConfig::DeviceInfo");

  this->Ui->setupUi(this);
  this->Ui->lineEditTargetIp->setText(
    QSettings().value("LidarView/SenfotoNetworkConfig/TargetIp", "192.168.1.30").toString());

  QObject::connect(
    this->Ui->pushButtonGet, &QPushButton::clicked, this, &lqSenfotoNetworkConfigDialog::onGet);
  QObject::connect(
    this->Ui->pushButtonSet, &QPushButton::clicked, this, &lqSenfotoNetworkConfigDialog::onSet);
  QObject::connect(this->Ui->pushButtonClose, &QPushButton::clicked, this, &QDialog::close);
  QObject::connect(this->Config, &lqSenfotoNetworkConfig::configReceived, this,
    &lqSenfotoNetworkConfigDialog::onConfigReceived);
  QObject::connect(this->Config, &lqSenfotoNetworkConfig::errorOccurred, this,
    &lqSenfotoNetworkConfigDialog::onError);
}

//-----------------------------------------------------------------------------
lqSenfotoNetworkConfigDialog::~lqSenfotoNetworkConfigDialog()
{
  delete this->Ui;
}

//-----------------------------------------------------------------------------
void lqSenfotoNetworkConfigDialog::onGet()
{
  const QString target = this->Ui->lineEditTargetIp->text();
  if (!lqSenfotoNetworkConfig::IsValidIp(target))
  {
    QMessageBox::information(this, tr("错误"), tr("目标雷达IP地址错误"));
    return;
  }
  this->Config->request(QHostAddress(target));
}

//-----------------------------------------------------------------------------
void lqSenfotoNetworkConfigDialog::onSet()
{
  const QString target = this->Ui->lineEditTargetIp->text();
  const QString lidar = this->Ui->lineEditLidarIp->text();
  const QString pc = this->Ui->lineEditPcIp->text();
  const QString mac = this->Ui->lineEditMac->text();
  const int port = this->Ui->lineEditPort->text().toInt();

  if (!lqSenfotoNetworkConfig::IsValidIp(target) || !lqSenfotoNetworkConfig::IsValidIp(lidar) ||
    !lqSenfotoNetworkConfig::IsValidIp(pc) || !lqSenfotoNetworkConfig::IsValidMac(mac) ||
    !lqSenfotoNetworkConfig::IsValidPort(port))
  {
    QMessageBox::information(this, tr("错误"), tr("参数格式错误（IP/MAC/端口）"));
    return;
  }

  const int answer = QMessageBox::question(
    this, tr("问题"), tr("是否更改?"), QMessageBox::Ok | QMessageBox::Cancel);
  if (answer != QMessageBox::Ok)
  {
    return;
  }

  lqSenfotoPasswordDialog passwordDialog(this);
  if (passwordDialog.exec() != QDialog::Accepted)
  {
    return;
  }

  if (this->Config->apply(QHostAddress(target), lidar, pc, mac, port))
  {
    QSettings().setValue("LidarView/SenfotoNetworkConfig/TargetIp", target);
  }
}

//-----------------------------------------------------------------------------
void lqSenfotoNetworkConfigDialog::onConfigReceived(
  const lqSenfotoNetworkConfig::DeviceInfo& info)
{
  this->Ui->lineEditLidarIp->setText(info.lidarIp);
  this->Ui->lineEditPcIp->setText(info.pcIp);
  this->Ui->lineEditMac->setText(info.mac);
  this->Ui->lineEditPort->setText(QString::number(info.port));
}

//-----------------------------------------------------------------------------
void lqSenfotoNetworkConfigDialog::onError(const QString& message)
{
  QMessageBox::warning(this, tr("错误"), message);
}
