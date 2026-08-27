# Laser Selection Panel — Design Spec

**Date:** 2026-08-27
**Branch:** `feat/laser-selection-dialog`
**Project:** `/Users/acelan/workspace/senfoto-view/senfoto-view` (LidarView / ParaView fork)

## Goal

Replicate the old SenFoTo `vvLaserSelectionDialog` feature in the new project: a
configuration panel that lists every laser channel of the active LiDAR source,
lets the user toggle which channels (laser lines) are displayed, and filters the
rendered point cloud so only the selected lines are shown. Selection is persisted
across sessions.

This is a port of `SenFoToView/Ui/Widgets/vvLaserSelectionDialog.*` (old repo
`senfoto-old/02.Code`).

## Decisions (confirmed with user)

1. **Filtering mechanism:** A dedicated downstream VTK filter
   `vtkLaserSelectionFilter` (in the `filters` SM group) that runs after the
   reader/stream and drops points whose `laser_id` channel is disabled. This is
   architecture-aligned (mirrors `vtkRadialDistanceDenoise`) and avoids touching
   the core interpreter / frame-splitting path. **Chosen over** the earlier
   approach-A design (a per-channel mask living on the base
   `vtkLidarPacketInterpreter` applied in `SplitFrame()`), which was reverted
   because the mask could not reliably reach the server-side interpreter that
   actually splits frames.
2. **Sensor scope:** All sensors (any source that emits a `laser_id` point array).
3. **UI entry point:** A "Laser Selection" item under the **Tools** menu and a
   toolbar button with an icon.
4. **Persistence:** Yes — remember per-channel selection across sessions via
   `pqSettings`.
5. **Auto-attach:** The `LaserSelection` filter is auto-attached to every lidar
   source/stream opened via `lqOpenLidarReaction`, mirroring
   `AutoAttachRadialDenoise`. Radial denoise (Senfoto008) chains *after* the
   selection filter.

## Architecture

```
  LidarCore/Filters/Processing/vtkLaserSelectionFilter   (filters SM group)
    + vtkNew<vtkIntArray> LaserSelection            // mask, index = laser_id
    + SetLaserSelection(int index, int value)
    + vtkIntArray* GetLaserSelection()
    - RequestData(): drops points whose laser_id is disabled (manual copy of
      kept points + point data + vertex cells; no vtkExtractSelection dep)

  Qt/Components/lqLaserSelectionDialog(.h/.cxx/.ui)
    - QTableWidget of channels (Channel, Vertical Corr., ...)
    - getLaserSelectionSelector() -> QVector<int>
    - setLidarSource(pqPipelineSource*)  // resolves the LaserSelection filter proxy
    - on Apply -> push mask to the filter's "LaserSelection" SM property

  Qt/ApplicationComponents/lqLaserSelectionReaction(.h/.cxx)
    - wraps QAction, opens the dialog

  Qt/ApplicationComponents/lqOpenLidarReaction.cxx
    - AutoAttachLaserSelection(source): creates the LaserSelection filter
    - AutoAttachRadialDenoise(source): chains onto the LaserSelection filter

  Application/Client/LidarViewMainWindow.cxx
    - after buildToolsMenu(): add QAction "Laser Selection" + toolbar button
```

## Data flow

1. User opens **Tools → Laser Selection**. `lqLaserSelectionReaction` shows
   `lqLaserSelectionDialog`.
2. The dialog resolves the active lidar source via `pqServerManagerModel`, finds
   the auto-attached `LaserSelection` filter proxy among the source's consumers
   (creating it if absent), and initializes the checkbox table — overlaying the
   persisted `pqSettings` selection.
3. User toggles channels, optionally checks "Apply in future sessions".
4. On **Apply** the dialog builds the full mask and pushes it through the filter's
   `LaserSelection` SM property (`repeat_command` + `use_index`, one element per
   channel) via `vtkSMPropertyHelper`, then `UpdateVTKObjects()` +
   `MarkModified()` so the server-side filter re-executes.
5. `vtkLaserSelectionFilter::RequestData()` rebuilds the output polyData keeping
   only points whose `laser_id` is enabled; the result flows downstream (and, for
   Senfoto008, through the radial-denoiser that was chained after it) to the view.
6. If "Apply in future sessions", the mask is written to `pqSettings` under a
   `LidarPlugin/LaserSelection*` keyset (mirrors old dialog).

## Components / files to create or modify

### New files
- `LidarCore/Filters/Processing/vtkLaserSelectionFilter.h`
- `LidarCore/Filters/Processing/vtkLaserSelectionFilter.cxx`
- `LidarCore/Plugin/Filters/LaserSelection.xml` — `filters` group proxy
- `LidarCore/Filters/Processing/Testing/Cxx/TestLaserSelectionFilter.cxx` — unit test
- `Qt/Components/lqLaserSelectionDialog.h`
- `Qt/Components/lqLaserSelectionDialog.cxx`
- `Qt/Components/Resources/UI/lqLaserSelectionDialog.ui`
- `Qt/ApplicationComponents/lqLaserSelectionReaction.h`
- `Qt/ApplicationComponents/lqLaserSelectionReaction.cxx`

### Modified files
- `LidarCore/Filters/Processing/CMakeLists.txt` — add `vtkLaserSelectionFilter` to
  `classes`.
- `LidarCore/Plugin/CMakeLists.txt` — add `Filters/LaserSelection.xml` to the
  `FiltersProcessing` XML list.
- `Qt/Components/CMakeLists.txt` and `Qt/ApplicationComponents/CMakeLists.txt` —
  register new sources (and `.ui` if needed).
- `Qt/ApplicationComponents/lqOpenLidarReaction.cxx` — add `AutoAttachLaserSelection`
  and chain `AutoAttachRadialDenoise` onto it.
- `Application/Client/LidarViewMainWindow.cxx` — add Tools-menu action + toolbar
  button and wire `lqLaserSelectionReaction`.

## Implementation details

### Filter (`vtkLaserSelectionFilter`)
- Inherits `vtkPolyDataAlgorithm`, exported via `LVFILTERSPROCESSING_EXPORT`.
- `vtkNew<vtkIntArray> LaserSelection;` initialized to 128 ones (all enabled).
- `SetLaserSelection(int index, int value)`: lazily resize, set tuple, `Modified()`.
- `GetLaserSelection()`: return pointer.
- `RequestData()`:
  - If no `laser_id` array, or nothing is disabled, `ShallowCopy(input)` (pass-through).
  - Otherwise build the list of kept point indices, copy the kept points (and their
    point data) into the output, and rebuild the vertex cells so each kept point
    remains a standalone vertex. (Manual copy — avoids a `vtkExtractSelection`
    dependency and any output-type ambiguity.)

### UI dialog (`lqLaserSelectionDialog`)
- Port the old `.ui`: `QTableWidget` (checkbox column + Channel + correction
  columns), `Enable/Disable all` checkbox, `Toggle Selected`, `Display more
  corrections`, `Apply`, `Apply in future sessions`, OK/Cancel.
- `setLidarSource(pqPipelineSource*)` resolves the `LaserSelection` filter via the
  source's consumers (`getAllConsumers()`) and initializes the table.
- `getLaserSelectionSelector()` returns mask vector (index = laser_id).
- On Apply (or OK): find-or-create the filter, push the mask through its
  `LaserSelection` SM property, `MarkModified`, optionally save to `pqSettings`.
- Detects lidar sources via client-side `vtkLidarReader` / `vtkLidarStream` cast
  (project-conventional), independent of how the filter proxy is resolved.

### Reaction + menu entry
- `lqLaserSelectionReaction(QAction*)` — on trigger, construct and `show()` the
  dialog (non-modal, like old).
- In `LidarViewMainWindow.cxx` after `buildToolsMenu(*menuTools)`:
  ```cpp
  QAction* actionLaserSelection = new QAction(tr("Laser Selection"), this);
  this->Internals->menuTools->addAction(actionLaserSelection);
  new lqLaserSelectionReaction(actionLaserSelection);
  ```
  A toolbar button with an icon is added alongside the other lidar tools.

## Error handling / edge cases
- No active LiDAR source / no `LaserSelection` filter yet: dialog creates the
  filter on Apply.
- Filter with no `laser_id` array: `RequestData` is a pass-through (graceful).
- Out-of-range `laser_id`: treated as enabled (safe default).
- Channel-count changes: the mask is indexed by `laser_id`; unknown/higher ids
  default to enabled.

## Testing
- **Unit:** `LidarCore/Filters/Processing/Testing/Cxx/TestLaserSelectionFilter.cxx`
  builds a polyData with `laser_id` `[0,1,2,0,1]`, disables channel 1, runs the
  filter, and asserts 3 points remain (channel 1 removed). Compiles when
  `BUILD_TESTING` is enabled.
- **Manual:** build, open a Senfoto008 pcap/stream, open Tools → Laser Selection,
  uncheck several channels, Apply → only selected lines render. Toggle all / none
  works. Restart app → selection persisted.
- Follow existing test layout under `Testing/` and the project's CMake test
  registration.

## Out of scope
- Per-sensor custom correction editing UI beyond what the table already shows.
- Real-time streaming re-interpretation optimizations (acceptable re-run cost).
