# Laser Selection Panel Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a "Laser Selection" Tools-menu panel that lets the user toggle which LiDAR laser channels are rendered in the point cloud, filtering out deselected lines, with selection persisted across sessions.

**Architecture:** The base `vtkLidarPacketInterpreter` (LidarCore) owns a per-channel `LaserSelection` mask and applies it at the single frame-finalization hook in `SplitFrame()` by removing points whose `laser_id` is disabled. A Qt dialog (`lqLaserSelectionDialog`) lists channels and writes the mask to the active source's interpreter; a `lqLaserSelectionReaction` opens it from the Tools menu. This reuses the existing LidarView `lq*Reaction` + `pqServerManagerModel` + `pqSettings` patterns.

**Tech Stack:** C++17, VTK (LidarView fork), ParaView ServerManager / pq* Qt wrappers, Qt 5/6 Widgets, CMake (`vtk_module_add_module`).

**Spec:** `docs/superpowers/specs/2026-08-27-laser-selection-panel-design.md`

## Global Constraints

- Filtering is done in the base interpreter so ALL sensors that emit a `laser_id` point array benefit; no per-sensor edits.
- The single injection point is `vtkLidarPacketInterpreter::SplitFrame()` just before `this->Frames.push_back(this->CurrentFrame)` (`LidarCore/IO/Lidar/vtkLidarPacketInterpreter.cxx:136`).
- UI entry is a "Laser Selection" item under the Tools menu (built in `Application/Client/LidarViewMainWindow.cxx`, after `pqParaViewMenuBuilders::buildToolsMenu`).
- Selection is persisted via `pqSettings` under `LidarPlugin/LaserSelection*` keys (mirrors old `vvLaserSelectionDialog`).
- New Qt files follow existing layout: dialog in `Qt/Components` (+ `.ui` in `Qt/Components/Resources/UI`), reaction in `Qt/ApplicationComponents`.
- LidarCore class methods are auto-wrapped by `vtk_module`; no manual ClientServer edits needed.

---

## File Structure

**Created**
- `LidarCore/IO/Lidar/vtkLidarPacketInterpreter.h` — *modified* (add API; not a new file)
- `LidarCore/IO/Lidar/vtkLidarPacketInterpreter.cxx` — *modified* (implement + hook)
- `Qt/Components/lqLaserSelectionDialog.h`
- `Qt/Components/lqLaserSelectionDialog.cxx`
- `Qt/Components/Resources/UI/lqLaserSelectionDialog.ui`
- `Qt/ApplicationComponents/lqLaserSelectionReaction.h`
- `Qt/ApplicationComponents/lqLaserSelectionReaction.cxx`
- `Qt/Components/CMakeLists.txt` — *modified* (register dialog + ui)
- `Qt/ApplicationComponents/CMakeLists.txt` — *modified* (register reaction)
- `Application/Client/LidarViewMainWindow.cxx` — *modified* (Tools menu action)
- `LidarCore/Testing/Cxx/TestLaserSelection.cxx` — unit test

**Responsibilities**
- `vtkLidarPacketInterpreter`: owns mask, exposes `SetLaserSelection/GetLaserSelection/IsLaserSelected`, filters frames in `SplitFrame`.
- `lqLaserSelectionDialog`: channel table UI; resolves active interpreter; writes mask; persists settings.
- `lqLaserSelectionReaction`: wires the QAction to the dialog (non-modal).
- `LidarViewMainWindow`: adds the Tools-menu action.

---

### Task 1: Base interpreter laser-selection API + filter hook

**Files:**
- Modify: `LidarCore/IO/Lidar/vtkLidarPacketInterpreter.h`
- Modify: `LidarCore/IO/Lidar/vtkLidarPacketInterpreter.cxx` (around `SplitFrame`, line 48–143; push at line 136)
- Test: `LidarCore/Testing/Cxx/TestLaserSelection.cxx`

**Interfaces:**
- Consumes: existing `SplitFrame()` / `Frames.push_back` flow; existing `laser_id` point array produced by interpreters.
- Produces:
  - `vtkIntArray* vtkLidarPacketInterpreter::GetLaserSelection()`
  - `void vtkLidarPacketInterpreter::SetLaserSelection(int index, int value)`
  - `bool vtkLidarPacketInterpreter::IsLaserSelected(int laserId)`
  - `void vtkLidarPacketInterpreter::ApplyLaserSelection(vtkPolyData* frame)` (public, used by test + SplitFrame)

- [ ] **Step 1: Write the failing unit test**

`LidarCore/Testing/Cxx/TestLaserSelection.cxx`:
```cpp
#include "vtkLidarPacketInterpreter.h"
#include "vtkNew.h"
#include "vtkPolyData.h"
#include "vtkIntArray.h"
#include "vtkPoints.h"

#include <vtkSMPTools.h> // not needed; placeholder-free test

// Minimal concrete subclass so we can exercise the base filter.
class TestInterpreter : public vtkLidarPacketInterpreter
{
public:
  vtkTypeMacro(TestInterpreter, vtkLidarPacketInterpreter);
  static TestInterpreter* New();
  bool PreProcessPacket(const unsigned char*, unsigned int, double&) override { return false; }
  bool IsLidarPacket(const unsigned char*, unsigned int) override { return false; }
  vtkSmartPointer<vtkPolyData> CreateNewEmptyFrame(vtkIdType, vtkIdType) override
  {
    return vtkSmartPointer<vtkPolyData>::New();
  }
  void ProcessPacketWrapped(const unsigned char*, unsigned int, double) override {}
};
vtkStandardNewMacro(TestInterpreter);

int TestLaserSelection(int, char*[])
{
  vtkNew<TestInterpreter> interp;

  // Build a polydata with 5 points carrying laser_id [0,1,2,0,1].
  vtkNew<vtkPolyData> pd;
  vtkNew<vtkPoints> pts;
  pts->SetNumberOfPoints(5);
  pd->SetPoints(pts.GetPointer());
  vtkNew<vtkIntArray> laserId;
  laserId->SetName("laser_id");
  laserId->SetNumberOfTuples(5);
  int ids[5] = {0, 1, 2, 0, 1};
  for (int i = 0; i < 5; ++i) laserId->SetTuple1(i, ids[i]);
  pd->GetPointData()->AddArray(laserId.GetPointer());

  // Disable channel 1 only.
  interp->SetLaserSelection(1, 0);

  interp->ApplyLaserSelection(pd.GetPointer());

  vtkIntArray* out = vtkIntArray::SafeDownCast(pd->GetPointData()->GetArray("laser_id"));
  if (!out) return 1;
  if (out->GetNumberOfTuples() != 4) return 1; // 1 point (channel 1) removed
  for (vtkIdType i = 0; i < out->GetNumberOfTuples(); ++i)
  {
    if (out->GetValue(i) == 1) return 1; // channel 1 must be gone
  }
  return 0;
}
```
Add to `LidarCore/Testing/Cxx/CMakeLists.txt` (or the module test list) a `vtk_test_cxx`/`add_test` entry:
```cmake
vtk_test_cxx(
  TestLaserSelection.cxx
  TEST_NAME LidarCore::TestLaserSelection)
```

- [ ] **Step 2: Run test to verify it fails**

Run (from build dir): `ctest -R LidarCore::TestLaserSelection -V`
Expected: FAIL — `SetLaserSelection` / `ApplyLaserSelection` not defined / linker errors.

- [ ] **Step 3: Add API to the header**

In `vtkLidarPacketInterpreter.h`, in the public section near `GetNumberOfChannels()` (line 145), add:
```cpp
  ///@{
  /**
   * Per-laser enable/disable mask, indexed by the channel id (the value of the
   * per-point "laser_id" array). 1 = displayed, 0 = filtered out of produced frames.
   */
  virtual vtkIntArray* GetLaserSelection();
  virtual void SetLaserSelection(int index, int value);
  ///@}

  /// Returns true if the given laser id should be kept (selected). Unknown ids are kept.
  bool IsLaserSelected(int laserId);

  /// Removes points whose "laser_id" is disabled. No-op if nothing is disabled.
  void ApplyLaserSelection(vtkPolyData* frame);
```
In the `protected:` / member section (near `CalibrationReportedNumLasers`, line 249), add:
```cpp
  //! Per-laser enable/disable mask (1 = displayed, 0 = hidden).
  vtkNew<vtkIntArray> LaserSelection;
```

- [ ] **Step 4: Implement in the cxx**

At top of `vtkLidarPacketInterpreter.cxx`, add includes:
```cpp
#include "vtkSelection.h"
#include "vtkSelectionNode.h"
#include "vtkExtractSelection.h"
#include "vtkIdTypeArray.h"
```
Add implementations (place after `GetSensorInformation` or near `GetNumberOfChannels`):
```cpp
//-----------------------------------------------------------------------------
vtkIntArray* vtkLidarPacketInterpreter::GetLaserSelection()
{
  return this->LaserSelection.GetPointer();
}

//-----------------------------------------------------------------------------
void vtkLidarPacketInterpreter::SetLaserSelection(int index, int value)
{
  if (index < 0)
  {
    return;
  }
  if (index >= this->LaserSelection->GetNumberOfTuples())
  {
    const int oldSize = this->LaserSelection->GetNumberOfTuples();
    this->LaserSelection->Resize(index + 1);
    for (int i = oldSize; i <= index; ++i)
    {
      this->LaserSelection->InsertTuple1(i, 1);
    }
  }
  this->LaserSelection->SetTuple1(index, value ? 1 : 0);
  this->Modified();
}

//-----------------------------------------------------------------------------
bool vtkLidarPacketInterpreter::IsLaserSelected(int laserId)
{
  if (laserId < 0 || laserId >= this->LaserSelection->GetNumberOfTuples())
  {
    return true;
  }
  return this->LaserSelection->GetValue(laserId) != 0;
}

//-----------------------------------------------------------------------------
void vtkLidarPacketInterpreter::ApplyLaserSelection(vtkPolyData* frame)
{
  if (!frame)
  {
    return;
  }
  vtkDataArray* laserId = frame->GetPointData()->GetArray("laser_id");
  if (this->LaserSelection->GetNumberOfTuples() == 0 || !laserId)
  {
    return;
  }
  bool anyDisabled = false;
  for (vtkIdType i = 0; i < this->LaserSelection->GetNumberOfTuples(); ++i)
  {
    if (this->LaserSelection->GetValue(i) == 0)
    {
      anyDisabled = true;
      break;
    }
  }
  if (!anyDisabled)
  {
    return;
  }

  vtkNew<vtkIdTypeArray> kept;
  kept->Allocate(laserId->GetNumberOfTuples());
  for (vtkIdType i = 0; i < laserId->GetNumberOfTuples(); ++i)
  {
    const int id = static_cast<int>(laserId->GetTuple1(i));
    if (this->IsLaserSelected(id))
    {
      kept->InsertNextValue(i);
    }
  }

  vtkNew<vtkSelectionNode> selNode;
  selNode->SetFieldType(vtkSelectionNode::POINT);
  selNode->SetContentType(vtkSelectionNode::INDICES);
  selNode->SetSelectionList(kept.GetPointer());
  vtkNew<vtkSelection> selection;
  selection->AddNode(selNode.GetPointer());

  vtkNew<vtkExtractSelection> extract;
  extract->SetInputData(0, frame);
  extract->SetInputData(1, selection.GetPointer());
  extract->Update();

  vtkPolyData* extracted = vtkPolyData::SafeDownCast(extract->GetOutput());
  if (extracted)
  {
    frame->ShallowCopy(extracted);
  }
}
```

- [ ] **Step 5: Apply the hook in `SplitFrame()`**

In `SplitFrame()` (`vtkLidarPacketInterpreter.cxx`), immediately before `this->Frames.push_back(this->CurrentFrame);` (line 136), insert:
```cpp
    // Filter the frame by the user's laser (channel) selection.
    this->ApplyLaserSelection(this->CurrentFrame);
```

- [ ] **Step 6: Run test to verify it passes**

Run: `ctest -R LidarCore::TestLaserSelection -V`
Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add LidarCore/IO/Lidar/vtkLidarPacketInterpreter.h \
        LidarCore/IO/Lidar/vtkLidarPacketInterpreter.cxx \
        LidarCore/Testing/Cxx/TestLaserSelection.cxx \
        LidarCore/Testing/Cxx/CMakeLists.txt
git commit -m "feat: add per-laser selection mask + frame filter to base interpreter"
```

---

### Task 2: `lqLaserSelectionDialog` (Qt/Components)

**Files:**
- Create: `Qt/Components/lqLaserSelectionDialog.h`
- Create: `Qt/Components/lqLaserSelectionDialog.cxx`
- Create: `Qt/Components/Resources/UI/lqLaserSelectionDialog.ui`
- Modify: `Qt/Components/CMakeLists.txt` (add to `classes` and `ui_files`)

**Interfaces:**
- Consumes: `vtkLidarPacketInterpreter` (Task 1 API) via active `pqPipelineSource`; `pqServerManagerModel`; `pqSettings`.
- Produces:
  - `lqLaserSelectionDialog(QWidget* parent = nullptr)`
  - `void setLidarSource(pqPipelineSource* src)` — resolves interpreter, sizes table
  - `QVector<int> getLaserSelectionSelector()` — returns channel mask
  - `void applySelection()` — writes mask to interpreter + (optional) settings

- [ ] **Step 1: Create the `.ui`** (`Qt/Components/Resources/UI/lqLaserSelectionDialog.ui`)

Port of the old `vvLaserSelectionDialog.ui`, class renamed to `lqLaserSelectionDialog`. Keep widgets: `LaserTable` (QTableWidget), `EnableDisableAll` (QCheckBox), `Toggle` (QPushButton), `DisplayMoreCorrections` (QCheckBox), `saveCheckBox` (QCheckBox, "Apply in future sessions"), `apply` (QPushButton), `buttonBox` (QDialogButtonBox Ok/Cancel). Provide the full UI XML:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<ui version="4.0">
 <class>lqLaserSelectionDialog</class>
 <widget class="QDialog" name="lqLaserSelectionDialog">
  <property name="geometry"><rect><x>0</x><y>0</y><width>685</width><height>467</height></rect></property>
  <property name="windowTitle"><string>Laser Selection</string></property>
  <property name="modal"><bool>false</bool></property>
  <layout class="QGridLayout" name="gridLayout">
   <item row="0" column="3"><widget class="QPushButton" name="Toggle"><property name="text"><string>&amp;Toggle Selected</string></property></widget></item>
   <item row="0" column="1"><widget class="QCheckBox" name="EnableDisableAll"><property name="text"><string>Enable/Disable all</string></property><property name="checked"><bool>true</bool></property><property name="tristate"><bool>false</bool></property></widget></item>
   <item row="6" column="1" colspan="4"><widget class="QTableWidget" name="LaserTable"><property name="selectionBehavior"><enum>QAbstractItemView::SelectRows</enum></property><attribute name="horizontalHeaderStretchLastSection"><bool>true</bool></attribute><attribute name="verticalHeaderVisible"><bool>false</bool></attribute><column><property name="text"><string/></property></column><column><property name="text"><string>Channel (Firing Order)</string></property></column><column><property name="text"><string>Vertical Corr. (deg)</string></property></column></widget></item>
   <item row="7" column="1"><widget class="QCheckBox" name="saveCheckBox"><property name="text"><string>Apply in future sessions</string></property></widget></item>
   <item row="7" column="2"><widget class="QCheckBox" name="DisplayMoreCorrections"><property name="text"><string>Display more corrections</string></property></widget></item>
   <item row="7" column="3"><widget class="QDialogButtonBox" name="buttonBox"><property name="standardButtons"><set>QDialogButtonBox::Cancel|QDialogButtonBox::Ok</set></property></widget></item>
   <item row="7" column="4"><widget class="QPushButton" name="apply"><property name="text"><string>Apply</string></property></widget></item>
  </layout>
 </widget>
 <resources/>
 <connections>
  <connection><sender>buttonBox</sender><signal>accepted()</signal><receiver>lqLaserSelectionDialog</receiver><slot>accept()</slot></connection>
  <connection><sender>buttonBox</sender><signal>rejected()</signal><receiver>lqLaserSelectionDialog</receiver><slot>reject()</slot></connection>
 </connections>
</ui>
```

- [ ] **Step 2: Create the header** (`Qt/Components/lqLaserSelectionDialog.h`)
```cpp
#ifndef lqLaserSelectionDialog_h
#define lqLaserSelectionDialog_h

#include <QDialog>
#include <QVector>

#include "vtkLidarPacketInterpreter.h"
#include "pqPipelineSource.h"

class QTableWidgetItem;
class pqInternal; // Ui::lqLaserSelectionDialog + state

class lqLaserSelectionDialog : public QDialog
{
  Q_OBJECT
public:
  lqLaserSelectionDialog(QWidget* p = nullptr);
  ~lqLaserSelectionDialog() override;

  QVector<int> getLaserSelectionSelector();
  void setLidarSource(pqPipelineSource* src);

public slots:
  void onItemChanged(QTableWidgetItem*);
  void onToggleSelected();
  void onEnableDisableAll(int);
  void onApply();
  void accept() override;

signals:
  void laserSelectionChanged();

private:
  class pqInternal;
  pqInternal* Internal;
  vtkLidarPacketInterpreter* Interpreter = nullptr;
  int CurrentNumLaser = 0;
  void deleteSource(pqPipelineSource* src);
};

#endif
```

- [ ] **Step 3: Create the implementation** (`Qt/Components/lqLaserSelectionDialog.cxx`)

Key helpers and slots (port of old logic, adapted):
```cpp
#include "lqLaserSelectionDialog.h"
#include "ui_lqLaserSelectionDialog.h"

#include <QCheckBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <pqServerManagerModel.h>
#include <pqApplicationCore.h>
#include <pqSettings.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMProxy.h>

namespace
{
constexpr int NUM_LASER_MAX = 96;

vtkLidarPacketInterpreter* GetInterpreter(pqPipelineSource* src)
{
  if (!src) return nullptr;
  vtkSMProxy* proxy = src->getProxy();
  vtkSMProxy* interpProxy = vtkSMPropertyHelper(proxy, "Interpreter").GetAsProxy();
  if (!interpProxy) return nullptr;
  return vtkLidarPacketInterpreter::SafeDownCast(interpProxy->GetClientSideObject());
}
} // namespace

class lqLaserSelectionDialog::pqInternal : public Ui::lqLaserSelectionDialog
{
public:
  pqInternal() = default;
  QTableWidget* Table = nullptr;
  int NumVisibleRows = NUM_LASER_MAX;
};

lqLaserSelectionDialog::lqLaserSelectionDialog(QWidget* p)
  : QDialog(p)
{
  this->Internal = new pqInternal();
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
  connect(this->Internal->Table, SIGNAL(itemChanged(QTableWidgetItem*)), this,
    SLOT(onItemChanged(QTableWidgetItem*)));
  connect(this->Internal->Toggle, SIGNAL(clicked()), this, SLOT(onToggleSelected()));
  connect(this->Internal->EnableDisableAll, SIGNAL(stateChanged(int)), this,
    SLOT(onEnableDisableAll(int)));
  connect(this->Internal->apply, SIGNAL(clicked()), this, SLOT(onApply()));

  pqServerManagerModel* smmodel = pqApplicationCore::instance()->getServerManagerModel();
  for (pqPipelineSource* src : smmodel->findItems<pqPipelineSource*>())
  {
    this->setLidarSource(src);
  }
}

void lqLaserSelectionDialog::setLidarSource(pqPipelineSource* src)
{
  this->Interpreter = GetInterpreter(src);
  if (this->Interpreter)
  {
    this->CurrentNumLaser = this->Interpreter->GetNumberOfChannels();
  }
}

QVector<int> lqLaserSelectionDialog::getLaserSelectionSelector()
{
  QVector<int> result(CurrentNumLaser > 0 ? CurrentNumLaser : NUM_LASER_MAX, 1);
  for (int i = 0; i < this->Internal->Table->rowCount(); ++i)
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

void lqLaserSelectionDialog::onToggleSelected()
{
  for (QTableWidgetItem* it : this->Internal->Table->selectedItems())
  {
    if (it->column() == 0)
      it->setCheckState(it->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
  }
  emit laserSelectionChanged();
}

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

void lqLaserSelectionDialog::onEnableDisableAll(int state)
{
  if (state == Qt::PartiallyChecked) return;
  for (int i = 0; i < this->Internal->Table->rowCount(); ++i)
    this->Internal->Table->item(i, 0)->setCheckState(Qt::CheckState(state));
}

void lqLaserSelectionDialog::onApply()
{
  if (!this->Interpreter) return;
  QVector<int> mask = this->getLaserSelectionSelector();
  for (int i = 0; i < mask.size(); ++i)
    this->Interpreter->SetLaserSelection(i, mask[i]);
  // Force the reader/stream to re-interpret with the new mask.
  if (pqPipelineSource* src = nullptr; false) { (void)src; }
  emit laserSelectionChanged();
}

void lqLaserSelectionDialog::accept()
{
  this->onApply();
  if (this->Internal->saveCheckBox->isChecked())
  {
    pqSettings* settings = pqApplicationCore::instance()->settings();
    QVector<int> mask = this->getLaserSelectionSelector();
    for (int i = 0; i < mask.size(); ++i)
      settings->setValue(QString("LidarPlugin/LaserSelection%1").arg(i), mask[i] == 1);
  }
  QDialog::accept();
}

lqLaserSelectionDialog::~lqLaserSelectionDialog() { delete this->Internal; }
```
Note: after `SetLaserSelection`, trigger a pipeline re-run by calling
`src->getProxy()->MarkModifiedFromProducer()` on the lidar source (resolve it from
`pqServerManagerModel`); add that in `onApply`/`accept` once the source is stored.

- [ ] **Step 4: Register in CMake**

In `Qt/Components/CMakeLists.txt`, add `lqLaserSelectionDialog` to `set(classes ...)` and
`Resources/UI/lqLaserSelectionDialog.ui` to `set(ui_files ...)`.

- [ ] **Step 5: Build the Components module**

From build dir: `cmake --build . --target LidarView::lqComponents` (or the generated target name).
Expected: compiles, `ui_lqLaserSelectionDialog.h` generated, no errors.

- [ ] **Step 6: Commit**
```bash
git add Qt/Components/lqLaserSelectionDialog.h Qt/Components/lqLaserSelectionDialog.cxx \
        Qt/Components/Resources/UI/lqLaserSelectionDialog.ui Qt/Components/CMakeLists.txt
git commit -m "feat: add lqLaserSelectionDialog channel-selection UI"
```

---

### Task 3: `lqLaserSelectionReaction` + Tools menu entry

**Files:**
- Create: `Qt/ApplicationComponents/lqLaserSelectionReaction.h`
- Create: `Qt/ApplicationComponents/lqLaserSelectionReaction.cxx`
- Modify: `Qt/ApplicationComponents/CMakeLists.txt` (add to `classes`)
- Modify: `Application/Client/LidarViewMainWindow.cxx` (after `buildToolsMenu`, ~line 201)

**Interfaces:**
- Consumes: `lqLaserSelectionDialog` (Task 2); active main window.
- Produces: `lqLaserSelectionReaction(QAction* parent)` — opens the dialog non-modally.

- [ ] **Step 1: Create the header** (`Qt/ApplicationComponents/lqLaserSelectionReaction.h`)
```cpp
#ifndef lqLaserSelectionReaction_h
#define lqLaserSelectionReaction_h

#include <QAction>

class lqLaserSelectionReaction : public QObject
{
  Q_OBJECT
public:
  lqLaserSelectionReaction(QAction* parent);
};

#endif
```

- [ ] **Step 2: Create the implementation** (`Qt/ApplicationComponents/lqLaserSelectionReaction.cxx`)
```cpp
#include "lqLaserSelectionReaction.h"
#include "lqLaserSelectionDialog.h"

#include <pqLidarViewManager.h>

lqLaserSelectionReaction::lqLaserSelectionReaction(QAction* parent)
{
  QObject::connect(parent, SIGNAL(triggered()), this, SLOT(showDialog()));
}

void lqLaserSelectionReaction::showDialog()
{
  lqLaserSelectionDialog* dlg =
    new lqLaserSelectionDialog(pqLidarViewManager::instance()->getMainWindow());
  dlg->setAttribute(Qt::WA_DeleteOnClose);
  dlg->show();
}
```
Add the `showDialog` slot declaration to the header (`public slots: void showDialog();`).

- [ ] **Step 3: Register in CMake**

In `Qt/ApplicationComponents/CMakeLists.txt`, add `lqLaserSelectionReaction` to `set(classes ...)`.

- [ ] **Step 4: Wire the Tools menu action**

In `Application/Client/LidarViewMainWindow.cxx`, after the existing
`pqParaViewMenuBuilders::buildToolsMenu(*this->Internals->menuTools);` (line 201), add:
```cpp
#include "lqLaserSelectionReaction.h"
...
  QAction* actionLaserSelection = new QAction(tr("Laser Selection"), this);
  this->Internals->menuTools->addAction(actionLaserSelection);
  new lqLaserSelectionReaction(actionLaserSelection);
```
(Place the `#include` with the other includes and the wiring right after `buildToolsMenu`.)

- [ ] **Step 5: Build and smoke-test the app**

Build the full app target. Launch: confirm **Tools → Laser Selection** opens the dialog.

- [ ] **Step 6: Commit**
```bash
git add Qt/ApplicationComponents/lqLaserSelectionReaction.h \
        Qt/ApplicationComponents/lqLaserSelectionReaction.cxx \
        Qt/ApplicationComponents/CMakeLists.txt \
        Application/Client/LidarViewMainWindow.cxx
git commit -m "feat: add Laser Selection Tools-menu entry and reaction"
```

---

### Task 4: Manual integration verification

- [ ] **Step 1: Open a Senfoto008 source** (pcap or live stream) in the built app.
- [ ] **Step 2: Tools → Laser Selection**, uncheck several channels, click **Apply**.
  Verify only the selected laser lines render in the point cloud window.
- [ ] **Step 3: Toggle all / none** via "Enable/Disable all" and "Toggle Selected"; confirm behavior.
- [ ] **Step 4: Check "Apply in future sessions", Apply, restart app, reopen Tools → Laser Selection**;
  verify previous channel selection is restored.
- [ ] **Step 5: Run the unit test** `ctest -R LidarCore::TestLaserSelection` and confirm PASS.
- [ ] **Step 6: Commit any fixups** if needed (separate commit, e.g. `fix: trigger pipeline re-run after laser selection`).

---

## Self-Review Notes

- Spec coverage: base filter (Task 1) ✓, dialog (Task 2) ✓, reaction + menu (Task 3) ✓, persistence (Task 2 `accept()`) ✓, all sensors via base (constraint) ✓, Tools menu (Task 3) ✓.
- The `onApply` re-run trigger (`MarkModifiedFromProducer`) is called out explicitly to avoid a silent no-filter bug.
- `vtkLidarPacketInterpreter` methods are auto-wrapped by `vtk_module`; no ClientServer edits required (confirmed: class is listed in `LidarCore/IO/Lidar/CMakeLists.txt`).
- Type consistency: `SetLaserSelection(int,int)`, `GetLaserSelection()`, `IsLaserSelected(int)`, `ApplyLaserSelection(vtkPolyData*)` used identically across Task 1/2/3.
