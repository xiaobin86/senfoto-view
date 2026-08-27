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

#include <vtkLidarPacketInterpreter.h>
#include <vtkLidarReader.h>
#include <vtkLidarStream.h>
#include <vtkSMProperty.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMProxy.h>

namespace
{
constexpr int NUM_LASER_MAX = 96;

// Resolve the client-side vtkLidarPacketInterpreter from a lidar source proxy.
// The most reliable way (used by vvLaserSelectionDialog and lqSensorWidget) is to
// reach the reader/stream algorithm and call GetLidarInterpreter(). Resolving via
// the "PacketInterpreter"/"Interpreter" SM sub-proxy is kept only as a fallback.
vtkLidarPacketInterpreter* GetInterpreter(pqPipelineSource* src)
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
  vtkObjectBase* client = proxy->GetClientSideObject();
  if (client)
  {
    if (vtkLidarReader* reader = vtkLidarReader::SafeDownCast(client))
    {
      return reader->GetLidarInterpreter();
    }
    if (vtkLidarStream* stream = vtkLidarStream::SafeDownCast(client))
    {
      return stream->GetLidarInterpreter();
    }
  }
  // Fallback: some lidar sources expose the interpreter as an SM sub-proxy.
  // Check GetProperty() first to avoid vtkSMPropertyHelper's "Failed to locate
  // property" warning spam for sources that do not have these properties.
  if (vtkSMProperty* prop = proxy->GetProperty("PacketInterpreter"))
  {
    if (vtkSMProxy* interpProxy = vtkSMPropertyHelper(prop).GetAsProxy())
    {
      return vtkLidarPacketInterpreter::SafeDownCast(interpProxy->GetClientSideObject());
    }
  }
  if (vtkSMProperty* prop = proxy->GetProperty("Interpreter"))
  {
    if (vtkSMProxy* interpProxy = vtkSMPropertyHelper(prop).GetAsProxy())
    {
      return vtkLidarPacketInterpreter::SafeDownCast(interpProxy->GetClientSideObject());
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

  // Resolve the active lidar source's interpreter and size the table.
  // Only sources whose interpreter resolves non-null are kept; never overwrite a
  // valid interpreter with a later non-lidar source (which would null it out).
  pqServerManagerModel* smmodel = pqApplicationCore::instance()->getServerManagerModel();
  for (pqPipelineSource* src : smmodel->findItems<pqPipelineSource*>())
  {
    if (GetInterpreter(src))
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
  vtkLidarPacketInterpreter* interp = GetInterpreter(src);
  if (!interp)
  {
    return;
  }
  this->LidarSource = src;
  this->Interpreter = interp;
  this->CurrentNumLaser = interp->GetNumberOfChannels();
  // Keep the full NUM_LASER_MAX-row table so that, for sensors where laser_id
  // can exceed GetNumberOfChannels() (e.g. dual-return offsets), every channel
  // 0..NUM_LASER_MAX-1 remains toggleable and maps into the selection mask.

  // Read back the persisted selection so "Apply in future sessions" takes
  // effect immediately (applied to the interpreter, not only visually).
  pqSettings* settings = pqApplicationCore::instance()->settings();
  const int numRows = this->Internal->Table->rowCount();
  for (int i = 0; i < numRows; ++i)
  {
    int channel = this->Internal->Table->item(i, 1)->data(Qt::EditRole).toInt();
    if (channel < 0 || channel >= NUM_LASER_MAX)
    {
      continue;
    }
    const bool enabled =
      settings->value(QString("LidarPlugin/LaserSelection%1").arg(channel), true).toBool();
    this->Internal->Table->item(i, 0)->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
    this->Interpreter->SetLaserSelection(channel, enabled ? 1 : 0);
  }
}

//-----------------------------------------------------------------------------
QVector<int> lqLaserSelectionDialog::getLaserSelectionSelector()
{
  // Size the result to NUM_LASER_MAX so every possible channel index maps into
  // the mask, regardless of the sensor's reported channel count.
  QVector<int> result(NUM_LASER_MAX, 1);
  const int numRows = this->Internal->Table->rowCount();
  for (int i = 0; i < numRows; ++i)
  {
    int channel = this->Internal->Table->item(i, 1)->data(Qt::EditRole).toInt();
    if (channel >= 0 && channel < result.size())
    {
      result[channel] =
        (this->Internal->Table->item(i, 0)->checkState() == Qt::Checked) ? 1 : 0;
    }
  }
  return result;
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
void lqLaserSelectionDialog::onItemChanged(QTableWidgetItem* vtkNotUsed(item))
{
  bool all = true, none = true;
  for (int i = 0; i < this->Internal->Table->rowCount(); ++i)
  {
    bool c = this->Internal->Table->item(i, 0)->checkState() == Qt::Checked;
    all = all && c;
    none = none && !c;
  }
  this->Internal->EnableDisableAll->setCheckState(
    all ? Qt::Checked : (none ? Qt::Unchecked : Qt::PartiallyChecked));
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
  if (!this->LidarSource)
  {
    return;
  }
  // Re-resolve the interpreter in case it was re-created (e.g. a live stream
  // re-initializes its interpreter after the dialog was built).
  vtkLidarPacketInterpreter* interp = GetInterpreter(this->LidarSource);
  if (!interp)
  {
    return;
  }
  this->Interpreter = interp;
  QVector<int> mask = this->getLaserSelectionSelector();
  for (int i = 0; i < mask.size(); ++i)
  {
    interp->SetLaserSelection(i, mask[i]);
  }
  // vtkLidarPacketInterpreter::SetLaserSelection calls Modified() on the interpreter,
  // and vtkLidarReader/vtkLidarStream forward that to a re-interpretation via
  // OnInterpreterModifiedEvent. Force the SM proxy to push/update so the view
  // actually refreshes.
  if (vtkSMProxy* proxy = this->LidarSource->getProxy())
  {
    proxy->MarkModified();
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
