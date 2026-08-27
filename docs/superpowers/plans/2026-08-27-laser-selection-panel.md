# Laser Selection Panel Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a "Laser Selection" Tools-menu panel (plus a toolbar button) that lets the user toggle which LiDAR laser channels are rendered in the point cloud, filtering out deselected lines, with selection persisted across sessions.

**Architecture:** A dedicated downstream VTK filter `vtkLaserSelectionFilter` (in the `filters` SM group, in `LidarCore/Filters/Processing`) runs after the reader/stream and removes points whose `laser_id` channel is disabled. The filter is auto-attached to every opened lidar source (mirroring `AutoAttachRadialDenoise`). A Qt dialog (`lqLaserSelectionDialog`) lists channels and pushes the mask to the filter's `LaserSelection` SM property; a `lqLaserSelectionReaction` opens it from the Tools menu. This reuses the existing LidarView `lq*Reaction` + `pqServerManagerModel` + `pqSettings` patterns.

**Tech Stack:** C++17, VTK (LidarView fork), ParaView ServerManager / pq* Qt wrappers, Qt 5/6 Widgets, CMake (`vtk_module_add_module`).

**Spec:** `docs/superpowers/specs/2026-08-27-laser-selection-panel-design.md`

> **Note:** This plan was rewritten for *approach B* (a dedicated filter). The earlier approach-A plan (a per-channel mask on the base `vtkLidarPacketInterpreter` applied in `SplitFrame()`) was implemented and then reverted: the mask set on the client-side interpreter object did not propagate to the server-side interpreter that actually splits frames, so the view never changed. The filter approach pushes the mask through an SM property on a downstream filter, which reliably reaches the server.

## Global Constraints

- Filtering is done by a dedicated `vtkLaserSelectionFilter` so the core interpreter / frame-splitting path is untouched; all sensors that emit a `laser_id` point array benefit automatically.
- The filter is registered in the `filters` SM group; its `LaserSelection` property uses `repeat_command` + `use_index` with 128 explicit `default_values` (ParaView does NOT replicate a single scalar default across elements — list all 128).
- The filter is auto-attached in `lqOpenLidarReaction` (`AutoAttachLaserSelection`); radial denoise (Senfoto008) chains after it.
- UI entry is a "Laser Selection" item under the Tools menu and a toolbar button (built in `Application/Client/LidarViewMainWindow.cxx`, after `pqParaViewMenuBuilders::buildToolsMenu`).
- Selection is persisted via `pqSettings` under `LidarPlugin/LaserSelection*` keys (mirrors old `vvLaserSelectionDialog`).
- New Qt files follow existing layout: dialog in `Qt/Components` (+ `.ui` in `Qt/Components/Resources/UI`), reaction in `Qt/ApplicationComponents`.
- LidarCore class methods are auto-wrapped by `vtk_module`; no manual ClientServer edits needed.

---

## File Structure

**Created**
- `LidarCore/Filters/Processing/vtkLaserSelectionFilter.h`
- `LidarCore/Filters/Processing/vtkLaserSelectionFilter.cxx`
- `LidarCore/Plugin/Filters/LaserSelection.xml`
- `LidarCore/Filters/Processing/Testing/Cxx/TestLaserSelectionFilter.cxx`
- `Qt/Components/lqLaserSelectionDialog.h`
- `Qt/Components/lqLaserSelectionDialog.cxx`
- `Qt/Components/Resources/UI/lqLaserSelectionDialog.ui`
- `Qt/ApplicationComponents/lqLaserSelectionReaction.h`
- `Qt/ApplicationComponents/lqLaserSelectionReaction.cxx`

**Modified**
- `LidarCore/Filters/Processing/CMakeLists.txt` — add `vtkLaserSelectionFilter` to `classes`.
- `LidarCore/Plugin/CMakeLists.txt` — add `Filters/LaserSelection.xml` to the `FiltersProcessing` XML list.
- `Qt/Components/CMakeLists.txt` — register dialog + ui.
- `Qt/ApplicationComponents/CMakeLists.txt` — register reaction.
- `Qt/ApplicationComponents/lqOpenLidarReaction.cxx` — add `AutoAttachLaserSelection` + chain radial denoise.
- `Application/Client/LidarViewMainWindow.cxx` — Tools menu action + toolbar button.

**Responsibilities**
- `vtkLaserSelectionFilter`: filters points by `laser_id` mask in `RequestData`.
- `lqLaserSelectionDialog`: channel table UI; resolves the `LaserSelection` filter proxy; writes mask; persists settings.
- `lqLaserSelectionReaction`: wires the QAction to the dialog (non-modal).
- `lqOpenLidarReaction`: auto-attaches the filter and chains denoise.
- `LidarViewMainWindow`: adds the Tools-menu action + toolbar button.

---

### Task 1: `vtkLaserSelectionFilter` + XML + unit test

**Files:**
- Create: `LidarCore/Filters/Processing/vtkLaserSelectionFilter.h`
- Create: `LidarCore/Filters/Processing/vtkLaserSelectionFilter.cxx`
- Create: `LidarCore/Plugin/Filters/LaserSelection.xml`
- Create: `LidarCore/Filters/Processing/Testing/Cxx/TestLaserSelectionFilter.cxx`
- Modify: `LidarCore/Filters/Processing/CMakeLists.txt`
- Modify: `LidarCore/Plugin/CMakeLists.txt`

**Interfaces:**
- Consumes: input `vtkPolyData` with a `laser_id` point array.
- Produces:
  - `void vtkLaserSelectionFilter::SetLaserSelection(int index, int value)`
  - `vtkIntArray* vtkLaserSelectionFilter::GetLaserSelection()`

- [ ] **Step 1: Create the filter header**

`LidarCore/Filters/Processing/vtkLaserSelectionFilter.h`:
```cpp
#ifndef vtkLaserSelectionFilter_h
#define vtkLaserSelectionFilter_h

#include "lvFiltersProcessingModule.h"
#include <vtkPolyDataAlgorithm.h>

class LVFILTERSPROCESSING_EXPORT vtkLaserSelectionFilter : public vtkPolyDataAlgorithm
{
public:
  static vtkLaserSelectionFilter* New();
  vtkTypeMacro(vtkLaserSelectionFilter, vtkPolyDataAlgorithm)

  void SetLaserSelection(int index, int value);
  vtkIntArray* GetLaserSelection();

protected:
  vtkLaserSelectionFilter();
  ~vtkLaserSelectionFilter() override = default;
  int RequestData(vtkInformation*, vtkInformationVector**, vtkInformationVector*) override;

private:
  vtkLaserSelectionFilter(const vtkLaserSelectionFilter&) = delete;
  void operator=(const vtkLaserSelectionFilter&) = delete;
  vtkNew<vtkIntArray> LaserSelection;
};

#endif
```

- [ ] **Step 2: Implement the filter**

`RequestData` reads `laser_id`, builds the kept-point index list, and copies the
kept points + their point data into the output, rebuilding vertex cells. If no
`laser_id` exists or nothing is disabled, `ShallowCopy(input)`. `SetLaserSelection`
lazily resizes the mask and sets the tuple. Constructor initializes 128 ones.

- [ ] **Step 3: Create the SM XML**

`LidarCore/Plugin/Filters/LaserSelection.xml` — `SourceProxy name="LaserSelection"
class="vtkLaserSelectionFilter"`, `Input`/`Output` ports, and:
```xml
<IntVectorProperty name="LaserSelection"
                   command="SetLaserSelection"
                   number_of_elements="1"
                   default_values="1 1 1 ... (128 ones) ..."
                   repeat_command="1"
                   use_index="1">
  <Documentation>Enable/disable individual lidar channels. Index 0 = laser_id 0.</Documentation>
</IntVectorProperty>
```

- [ ] **Step 4: Register in CMake**
  - `LidarCore/Filters/Processing/CMakeLists.txt`: add `vtkLaserSelectionFilter` to `set(classes ...)`.
  - `LidarCore/Plugin/CMakeLists.txt`: add `Filters/LaserSelection.xml` to `filters_processing_xml_files`.

- [ ] **Step 5: Unit test**

`LidarCore/Filters/Processing/Testing/Cxx/TestLaserSelectionFilter.cxx` builds a
polyData with `laser_id` `[0,1,2,0,1]`, calls `filter->SetLaserSelection(1, 0)`,
`SetInputData` + `Update`, and asserts the output has 3 points and none carry
`laser_id == 1`. Register in `LidarCore/Filters/Processing/Testing/Cxx/CMakeLists.txt`
(`vtk_add_test_cxx` / `vtk_test_cxx_executable`).

- [ ] **Step 6: Build + run test** (when `BUILD_TESTING` is on): `ctest -R lvFiltersProcessingCxxTests`.

---

### Task 2: `lqLaserSelectionDialog` (Qt/Components)

**Files:**
- Create: `Qt/Components/lqLaserSelectionDialog.h`
- Create: `Qt/Components/lqLaserSelectionDialog.cxx`
- Create: `Qt/Components/Resources/UI/lqLaserSelectionDialog.ui`
- Modify: `Qt/Components/CMakeLists.txt`

**Interfaces:**
- Consumes: the `LaserSelection` filter proxy (via the active lidar source's
  consumers), `pqServerManagerModel`, `pqSettings`.
- Produces:
  - `lqLaserSelectionDialog(QWidget* parent = nullptr)`
  - `void setLidarSource(pqPipelineSource* src)` — resolves the `LaserSelection` filter
  - `QVector<int> getLaserSelectionSelector()` — returns channel mask
  - `void onApply()` — pushes mask to the filter SM property + (optional) settings

- [ ] **Step 1: Create the `.ui`** (port of old `vvLaserSelectionDialog.ui`, class
  renamed to `lqLaserSelectionDialog`; widgets `LaserTable`, `EnableDisableAll`,
  `Toggle`, `DisplayMoreCorrections`, `saveCheckBox`, `apply`, `buttonBox`).

- [ ] **Step 2: Create the header** — member `vtkSMProxy* LaserSelectionFilterProxy`
  (the filter's SM proxy), `pqPipelineSource* LidarSource`.

- [ ] **Step 3: Create the implementation**
  - Detect lidar sources via client-side `vtkLidarReader` / `vtkLidarStream` cast.
  - `setLidarSource` resolves the `LaserSelection` filter among
    `src->getAllConsumers()` and initializes the checkbox table (overlay `pqSettings`).
  - `onApply`: find-or-create the filter (`pqObjectBuilder::createFilter("filters",
    "LaserSelection", src)`), push the full mask via
    `vtkSMPropertyHelper(filterProxy, "LaserSelection").Set(mask.data(), mask.size())`,
    `UpdateVTKObjects()` + `MarkModified(filterProxy)`.
  - `accept()` also persists to `pqSettings` when "Apply in future sessions" is checked.

- [ ] **Step 4: Register in CMake** (`Qt/Components/CMakeLists.txt`: `classes` + `ui_files`).

- [ ] **Step 5: Build the Components module.**

---

### Task 3: `lqLaserSelectionReaction` + Tools menu + toolbar entry

**Files:**
- Create: `Qt/ApplicationComponents/lqLaserSelectionReaction.h` / `.cxx`
- Modify: `Qt/ApplicationComponents/CMakeLists.txt`
- Modify: `Application/Client/LidarViewMainWindow.cxx`

- [ ] **Step 1-4:** reaction opens the dialog non-modally; add a Tools-menu
  `QAction("Laser Selection")` wired to `lqLaserSelectionReaction`, plus a toolbar
  button with an icon (added via `.qrc` / `lqResources.qrc`).

- [ ] **Step 5: Build and smoke-test** — confirm **Tools → Laser Selection** (and the
  toolbar button) opens the dialog.

---

### Task 4: Auto-attach in `lqOpenLidarReaction`

**Files:**
- Modify: `Qt/ApplicationComponents/lqOpenLidarReaction.cxx`

- [ ] **Step 1:** Add `AutoAttachLaserSelection(pqPipelineSource* source)` creating
  `builder->createFilter("filters", "LaserSelection", source)` and
  `InitAndDisplaySource(filter, ...)`.
- [ ] **Step 2:** In `openLidarReader` and `openLidarStream`, after
  `InitAndDisplaySource(source)`, call `::AutoAttachLaserSelection(source)` before
  the existing radial-denoise block.
- [ ] **Step 3:** Update `AutoAttachRadialDenoise` to chain onto the `LaserSelection`
  filter when present (find it among `source->getAllConsumers()`).
- [ ] **Step 4: Build + manual verification** (Task 5 below).

---

### Task 5: Manual integration verification

- [ ] **Step 1:** Open a Senfoto008 source (pcap or live stream) in the built app.
- [ ] **Step 2:** Tools → Laser Selection, uncheck several channels, click **Apply**.
  Verify only the selected laser lines render.
- [ ] **Step 3:** Toggle all / none via "Enable/Disable all" and "Toggle Selected".
- [ ] **Step 4:** Check "Apply in future sessions", Apply, restart, reopen — verify
  previous selection restored.
- [ ] **Step 5:** Run `ctest -R lvFiltersProcessingCxxTests` and confirm PASS.

---

## Self-Review Notes

- Spec coverage: dedicated filter (Task 1) ✓, dialog (Task 2) ✓, reaction + menu +
  toolbar (Task 3) ✓, auto-attach + denoise chaining (Task 4) ✓, persistence (Task 2
  `accept()`) ✓, all sensors via filter (constraint) ✓, Tools menu + toolbar (Task 3) ✓.
- Pushing the mask through the filter's SM property (`repeat_command` + `use_index`)
  is what reaches the server-side filter and actually changes the rendered cloud —
  this is the key fix versus approach A (which set the mask on the client-side
  interpreter and never propagated).
- 128 explicit `default_values` are required: ParaView does not replicate a single
  scalar default across `repeat_command` elements.
- `vtkLidarPacketInterpreter` is intentionally left untouched (approach A reverted).
