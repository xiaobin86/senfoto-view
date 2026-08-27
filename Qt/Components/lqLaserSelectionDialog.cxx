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

#include <cstring>

#include <pqApplicationCore.h>
#include <pqActiveObjects.h>
#include <pqObjectBuilder.h>
#include <pqPipelineSource.h>
#include <pqServerManagerModel.h>
#include <pqSettings.h>
#include <pqView.h>

#include <vtkLidarReader.h>
#include <vtkLidarStream.h>

#include <vtkAbstractArray.h>
#include <vtkDoubleArray.h>
#include <vtkLidarPacketInterpreter.h>
#include <vtkTable.h>

#include <vtkSMProperty.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMProxy.h>

namespace
{
constexpr int NUM_LASER_MAX = 128;

// True when the pipeline source is a lidar source (reader or stream).
bool IsLidarSource(pqPipelineSource* src)
{
  if (!src)
  {
    return false;
  }
  vtkObjectBase* client = src->getProxy() ? src->getProxy()->GetClientSideObject() : nullptr;
  return vtkLidarReader::SafeDownCast(client) || vtkLidarStream::SafeDownCast(client);
}

// Resolve the "LaserSelection" filter proxy (SM) attached to a lidar source,
// by walking the source's consumers. Returns null if no filter is attached
// yet (in which case onApply will create one).
vtkSMProxy* GetLaserSelectionFilterProxy(pqPipelineSource* src)
{
  if (!src)
  {
    return nullptr;
  }
  for (pqPipelineSource* consumer : src->getAllConsumers())
  {
    if (consumer && consumer->getProxy() &&
      std::strcmp(consumer->getProxy()->GetXMLName(), "LaserSelection") == 0)
    {
      return consumer->getProxy();
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
  this->Internal->Table->setHorizontalHeaderLabels(
    QStringList() << "Enable" << "Channel" << "Pitch Angle (°)");

  QObject::connect(this->Internal->Table,
    &QTableWidget::itemChanged, this, &lqLaserSelectionDialog::onItemChanged);
  QObject::connect(this->Internal->Toggle,
    &QPushButton::clicked, this, &lqLaserSelectionDialog::onToggleSelected);
  QObject::connect(this->Internal->EnableDisableAll,
    QOverload<int>::of(&QCheckBox::stateChanged), this,
    &lqLaserSelectionDialog::onEnableDisableAll);
  QObject::connect(this->Internal->apply,
    &QPushButton::clicked, this, &lqLaserSelectionDialog::onApply);

  // Resolve the active lidar source and initialize the table.
  pqServerManagerModel* smmodel = pqApplicationCore::instance()->getServerManagerModel();
  for (pqPipelineSource* src : smmodel->findItems<pqPipelineSource*>())
  {
    if (IsLidarSource(src))
    {
      this->setLidarSource(src);
    }
  }
}

//-----------------------------------------------------------------------------
void lqLaserSelectionDialog::setLidarSource(pqPipelineSource* src)
{
  if (!IsLidarSource(src))
  {
    return;
  }
  this->LidarSource = src;
  this->LaserSelectionFilterProxy = GetLaserSelectionFilterProxy(src);

  this->Internal->Table->blockSignals(true);

  vtkLidarPacketInterpreter* interp = nullptr;
  vtkObjectBase* client = src->getProxy() ? src->getProxy()->GetClientSideObject() : nullptr;
  if (auto* r = vtkLidarReader::SafeDownCast(client))
  {
    interp = r->GetLidarInterpreter();
  }
  else if (auto* s = vtkLidarStream::SafeDownCast(client))
  {
    interp = s->GetLidarInterpreter();
  }

  if (interp)
  {
    this->setWindowTitle(
      QString("Laser Selection - %1").arg(interp->GetSensorModelName().c_str()));
  }

  int nChannels = interp ? interp->GetNumberOfChannels() : 0;
  if (nChannels <= 0)
  {
    nChannels = NUM_LASER_MAX;
  }

  vtkTable* calib = interp ? interp->GetCalibrationTable() : nullptr;
  vtkAbstractArray* vcCol = calib ? calib->GetColumnByName("verticalCorrection") : nullptr;

  this->Internal->Table->setRowCount(nChannels);
  for (int i = 0; i < nChannels; ++i)
  {
    auto* cb = new QTableWidgetItem();
    cb->setFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    cb->setCheckState(Qt::Checked);
    this->Internal->Table->setItem(i, 0, cb);

    auto* ch = new QTableWidgetItem();
    ch->setData(Qt::EditRole, i);
    ch->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    this->Internal->Table->setItem(i, 1, ch);

    auto* vc = new QTableWidgetItem();
    double angle = 0.0;
    if (vcCol && i < vcCol->GetNumberOfTuples())
    {
      angle = vtkDoubleArray::SafeDownCast(vcCol)->GetTuple1(i);
    }
    vc->setData(Qt::EditRole, angle);
    vc->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    this->Internal->Table->setItem(i, 2, vc);
  }

  const bool haveProp =
    this->LaserSelectionFilterProxy && this->LaserSelectionFilterProxy->GetProperty("LaserSelection");
  pqSettings* settings = pqApplicationCore::instance()->settings();
  for (int i = 0; i < nChannels; ++i)
  {
    bool enabled = true;
    if (haveProp)
    {
      vtkSMPropertyHelper propHelper(this->LaserSelectionFilterProxy, "LaserSelection");
      const vtkIdType n = propHelper.GetNumberOfElements();
      if (i < n)
      {
        enabled = propHelper.GetAsInt(i) != 0;
      }
    }
    else
    {
      const QVariant v = settings->value(QString("LidarPlugin/LaserSelection%1").arg(i));
      if (v.isValid())
      {
        enabled = v.toBool();
      }
    }
    this->Internal->Table->item(i, 0)->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
  }
  this->Internal->Table->blockSignals(false);
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
    QTableWidgetItem* it = this->Internal->Table->item(i, 0);
    if (!it)
    {
      continue;
    }
    const bool c = it->checkState() == Qt::Checked;
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
  if (!this->LidarSource)
  {
    return;
  }

  // Re-resolve the filter proxy: reuse the one auto-attached to the source so we
  // never create duplicate pipeline nodes. Only create one if none exists yet.
  this->LaserSelectionFilterProxy = GetLaserSelectionFilterProxy(this->LidarSource);
  if (!this->LaserSelectionFilterProxy)
  {
    pqObjectBuilder* builder = pqApplicationCore::instance()->getObjectBuilder();
    pqPipelineSource* filter =
      builder->createFilter("filters", "LaserSelection", this->LidarSource);
    if (filter)
    {
      this->LaserSelectionFilterProxy = filter->getProxy();
      this->LaserSelectionFilterProxy->UpdateVTKObjects();
    }
  }
  if (!this->LaserSelectionFilterProxy)
  {
    return;
  }

  // Build the full mask and push it through the SM property so it reaches the
  // server-side filter, which removes the disabled channels from the output.
  QVector<int> mask = this->getLaserSelectionSelector();
  vtkSMPropertyHelper propHelper(this->LaserSelectionFilterProxy, "LaserSelection");
  propHelper.Set(mask.data(), mask.size());
  this->LaserSelectionFilterProxy->UpdateVTKObjects();
  // Re-execute the filter and refresh the view.
  this->LaserSelectionFilterProxy->MarkModified(this->LaserSelectionFilterProxy);
  if (pqView* view = pqActiveObjects::instance().activeView())
  {
    view->render();
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
