/*=========================================================================

  Program: LidarView
  Module:  lqLaserSelectionDialog.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See LICENSE or http://www.apache.org/licenses/LICENSE-2.0 for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "lqLaserSelectionDialog.h"
#include "ui_lqLaserSelectionDialog.h"

#include <QCheckBox>
#include <QTableWidget>
#include <QTableWidgetItem>

#include <pqApplicationCore.h>
#include <pqServerManagerModel.h>
#include <pqSettings.h>

#include <vtkSMProperty.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMProxy.h>

namespace
{
constexpr int NUM_LASER_MAX = 128;

// Resolve the interpreter SUB-PROXY (SM) from a lidar source. Returns null when
// the source is not a lidar source (i.e. has no interpreter sub-proxy).
// Pushing the mask through this proxy is what makes it reach the server-side
// interpreter and actually re-split the produced frames.
vtkSMProxy* GetInterpreterProxy(pqPipelineSource* src)
{
  if (!src)
  {
    return nullptr;
  }
  vtkSMProxy* proxy = src->getProxy();
  if (!proxy)
  {
    return nullptr;
  }
  if (vtkSMProperty* prop = proxy->GetProperty("PacketInterpreter"))
  {
    if (vtkSMProxy* interpProxy = vtkSMPropertyHelper(prop).GetAsProxy())
    {
      return interpProxy;
    }
  }
  if (vtkSMProperty* prop = proxy->GetProperty("Interpreter"))
  {
    if (vtkSMProxy* interpProxy = vtkSMPropertyHelper(prop).GetAsProxy())
    {
      return interpProxy;
    }
  }
  return nullptr;
}
} // namespace

//-----------------------------------------------------------------------------
class lqLaserSelectionDialog::pqInternal : public Ui::lqLaserSelectionDialog
{
public:
  pqInternal() = default;
  QTableWidget* Table = nullptr;
};

//-----------------------------------------------------------------------------
lqLaserSelectionDialog::lqLaserSelectionDialog(QWidget* p)
  : Superclass(p)
  , Internal(new pqInternal())
{
  this->Internal->setupUi(this);
  this->Internal->Table = this->Internal->LaserTable;
  this->Internal->Table->setColumnCount(3);
  for (int i = 0; i < NUM_LASER_MAX; ++i)
  {
    this->Internal->Table->insertRow(i);
    auto* cb = new QTableWidgetItem();
    cb->setCheckState(Qt::Checked);
    cb->setFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    this->Internal->Table->setItem(i, 0, cb);
    auto* ch = new QTableWidgetItem();
    ch->setData(Qt::EditRole, i);
    ch->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    this->Internal->Table->setItem(i, 1, ch);
    auto* vc = new QTableWidgetItem();
    vc->setData(Qt::EditRole, 0.0);
    vc->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    this->Internal->Table->setItem(i, 2, vc);
  }

  QObject::connect(this->Internal->Table,
    &QTableWidget::itemChanged, this, &lqLaserSelectionDialog::onItemChanged);
  QObject::connect(this->Internal->Toggle,
    &QPushButton::clicked, this, &lqLaserSelectionDialog::onToggleSelected);
  QObject::connect(this->Internal->EnableDisableAll,
    QOverload<int>::of(&QCheckBox::stateChanged), this,
    &lqLaserSelectionDialog::onEnableDisableAll);
  QObject::connect(this->Internal->apply,
    &QPushButton::clicked, this, &lqLaserSelectionDialog::onApply);

  // Resolve the active lidar source's interpreter and initialize the table.
  pqServerManagerModel* smmodel = pqApplicationCore::instance()->getServerManagerModel();
  for (pqPipelineSource* src : smmodel->findItems<pqPipelineSource*>())
  {
    if (GetInterpreterProxy(src))
    {
      this->setLidarSource(src);
    }
  }
}

//-----------------------------------------------------------------------------
void lqLaserSelectionDialog::setLidarSource(pqPipelineSource* src)
{
  if (!src)
  {
    return;
  }
  vtkSMProxy* interpProxy = GetInterpreterProxy(src);
  if (!interpProxy)
  {
    return;
  }
  this->LidarSource = src;
  this->InterpreterProxy = interpProxy;

  // Initialize the checkboxes from the current SM property (server-side truth),
  // overlaid by any persisted per-session selection stored in pqSettings.
  const bool haveProp = (interpProxy->GetProperty("LaserSelection") != nullptr);
  vtkSMPropertyHelper propHelper(interpProxy, "LaserSelection");
  pqSettings* settings = pqApplicationCore::instance()->settings();
  const int numRows = this->Internal->Table->rowCount();
  for (int i = 0; i < numRows; ++i)
  {
    bool enabled = true;
    if (haveProp && i < propHelper.GetNumberOfElements())
    {
      enabled = propHelper.GetAsInt(i) != 0;
    }
    const QVariant v = settings->value(QString("LidarPlugin/LaserSelection%1").arg(i));
    if (v.isValid())
    {
      enabled = v.toBool();
    }
    this->Internal->Table->item(i, 0)->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
  }
}

//-----------------------------------------------------------------------------
QVector<int> lqLaserSelectionDialog::getLaserSelectionSelector()
{
  QVector<int> result(NUM_LASER_MAX, 1);
  const int numRows = this->Internal->Table->rowCount();
  for (int i = 0; i < numRows; ++i)
  {
    const int channel = this->Internal->Table->item(i, 1)->data(Qt::EditRole).toInt();
    if (channel >= 0 && channel < result.size())
    {
      result[channel] = (this->Internal->Table->item(i, 0)->checkState() == Qt::Checked) ? 1 : 0;
    }
  }
  return result;
}

//-----------------------------------------------------------------------------
void lqLaserSelectionDialog::onItemChanged(QTableWidgetItem* vtkNotUsed(item))
{
  bool all = true, none = true;
  for (int i = 0; i < this->Internal->Table->rowCount(); ++i)
  {
    const bool c = this->Internal->Table->item(i, 0)->checkState() == Qt::Checked;
    all = all && c;
    none = none && !c;
  }
  this->Internal->EnableDisableAll->setCheckState(
    all ? Qt::Checked : (none ? Qt::Unchecked : Qt::PartiallyChecked));
}

//-----------------------------------------------------------------------------
void lqLaserSelectionDialog::onToggleSelected()
{
  for (QTableWidgetItem* it : this->Internal->Table->selectedItems())
  {
    if (it->column() == 0)
    {
      it->setCheckState(it->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
    }
  }
  Q_EMIT laserSelectionChanged();
}

//-----------------------------------------------------------------------------
void lqLaserSelectionDialog::onEnableDisableAll(int state)
{
  if (state == Qt::PartiallyChecked)
  {
    return;
  }
  for (int i = 0; i < this->Internal->Table->rowCount(); ++i)
  {
    this->Internal->Table->item(i, 0)->setCheckState(Qt::CheckState(state));
  }
}

//-----------------------------------------------------------------------------
void lqLaserSelectionDialog::onApply()
{
  if (!this->LidarSource || !this->InterpreterProxy)
  {
    return;
  }
  // Build the full mask and push it through the SM property so it reaches the
  // server-side interpreter (which is the object that actually splits frames).
  QVector<int> mask = this->getLaserSelectionSelector();
  vtkSMPropertyHelper propHelper(this->InterpreterProxy, "LaserSelection");
  const int nElem = propHelper.GetNumberOfElements();
  if (nElem == mask.size())
  {
    propHelper.Set(mask.data(), mask.size());
  }
  else
  {
    QVector<int> m(nElem, 1);
    for (int i = 0; i < mask.size() && i < nElem; ++i)
    {
      m[i] = mask[i];
    }
    propHelper.Set(m.data(), nElem);
  }
  this->InterpreterProxy->UpdateVTKObjects();

  // Force the owning reader/stream proxy to re-execute and refresh the view.
  if (vtkSMProxy* proxy = this->LidarSource->getProxy())
  {
    proxy->MarkModified(proxy);
  }
  Q_EMIT laserSelectionChanged();
}

//-----------------------------------------------------------------------------
void lqLaserSelectionDialog::accept()
{
  this->onApply();
  if (this->Internal->saveCheckBox->isChecked())
  {
    pqSettings* settings = pqApplicationCore::instance()->settings();
    QVector<int> mask = this->getLaserSelectionSelector();
    for (int i = 0; i < mask.size(); ++i)
    {
      settings->setValue(QString("LidarPlugin/LaserSelection%1").arg(i), mask[i] == 1);
    }
  }
  Superclass::accept();
}

//-----------------------------------------------------------------------------
lqLaserSelectionDialog::~lqLaserSelectionDialog()
{
  delete this->Internal;
}
