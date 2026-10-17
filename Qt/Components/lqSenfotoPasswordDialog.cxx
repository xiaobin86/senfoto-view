// ============================================================
// 功能：Senfoto008 网络配置密码输入对话框实现。
// 作者：acelan
// 新建时间：2026-10-17
// 修改时间：2026-10-17
// ============================================================

#include "lqSenfotoPasswordDialog.h"
#include "ui_lqSenfotoPasswordDialog.h"

#include <QMessageBox>

//-----------------------------------------------------------------------------
lqSenfotoPasswordDialog::lqSenfotoPasswordDialog(QWidget* parent)
  : QDialog(parent)
  , Ui(new Ui::lqSenfotoPasswordDialog)
{
  this->Ui->setupUi(this);
  this->Ui->lineEditPwd->setEchoMode(QLineEdit::Password);
  this->Ui->checkBoxShow->setChecked(false);
  QObject::connect(
    this->Ui->checkBoxShow, &QCheckBox::stateChanged, this, &lqSenfotoPasswordDialog::onShowToggled);
  QObject::connect(this->Ui->pushButtonOk, &QPushButton::clicked, this, &lqSenfotoPasswordDialog::onOk);
  QObject::connect(
    this->Ui->pushButtonCancel, &QPushButton::clicked, this, &lqSenfotoPasswordDialog::onCancel);
}

//-----------------------------------------------------------------------------
lqSenfotoPasswordDialog::~lqSenfotoPasswordDialog()
{
  delete this->Ui;
}

//-----------------------------------------------------------------------------
void lqSenfotoPasswordDialog::onShowToggled(int state)
{
  this->Ui->lineEditPwd->setEchoMode(state ? QLineEdit::Normal : QLineEdit::Password);
}

//-----------------------------------------------------------------------------
void lqSenfotoPasswordDialog::onOk()
{
  if (this->Ui->lineEditPwd->text() == QStringLiteral("senfoto"))
  {
    QDialog::accept();
  }
  else
  {
    QMessageBox::information(this, tr("提示"), tr("密码不正确, 请重新输入!"));
    QDialog::reject();
  }
}

//-----------------------------------------------------------------------------
void lqSenfotoPasswordDialog::onCancel()
{
  QDialog::reject();
}
