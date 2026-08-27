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

1. **Filtering mechanism:** Base-class post-processing. The base
   `vtkLidarPacketInterpreter` owns a `LaserSelection` int array; a single hook
   near frame finalization removes points whose `laser_id` is disabled. All
   interpreters that emit a `laser_id` array benefit automatically.
2. **Sensor scope:** All sensors (base-class approach).
3. **UI entry point:** A "Laser Selection" item under the **Tools** menu.
4. **Persistence:** Yes — remember per-channel selection across sessions via
   `pqSettings`.

## Architecture

```
 LidarCore/IO/Lidar/vtkLidarPacketInterpreter
   + vtkNew<vtkIntArray> LaserSelection            // size = GetNumberOfChannels()
   + SetLaserSelection(int index, int value)
   + vtkIntArray* GetLaserSelection()
   + IsLaserSelected(int laserId)                  // helper
   + ApplyLaserSelection(vtkPolyData*)             // removes deselected points
   - hook in SplitFrame() before Frames.push_back  // single injection point

 Qt/Components/lqLaserSelectionDialog(.h/.cxx/.ui)
   - QTableWidget of channels (Channel, Vertical Corr., ...)
   - getLaserSelectionSelector() -> QVector<int>
   - setLidarSource(pqPipelineSource*)             // resolves interpreter
   - on Apply -> interpreter->SetLaserSelection(...)

 Qt/ApplicationComponents/lqLaserSelectionReaction(.h/.cxx)
   - wraps QAction, opens the dialog

 Application/Client/LidarViewMainWindow.cxx
   - after buildToolsMenu(): add QAction "Laser Selection"
     -> new lqLaserSelectionReaction(action)
```

## Data flow

1. User opens **Tools → Laser Selection**. `lqLaserSelectionReaction` shows
   `lqLaserSelectionDialog`.
2. Dialog resolves the active LiDAR source via `pqServerManagerModel`, gets its
   proxy, reads the `Interpreter` subproxy's client-side object
   (`vtkLidarPacketInterpreter*`), and calls `GetNumberOfChannels()` to size the
   table. (Pattern from `lqSensorWidget.cxx:409`.)
3. User toggles channels, optionally checks "Apply in future sessions".
4. On **Apply** the dialog calls
   `interpreter->SetLaserSelection(channel, checked)` for each channel and
   triggers an interpreter re-run / `Modified()`.
5. `vtkLidarPacketInterpreter::SplitFrame()` (single hook at
   `vtkLidarPacketInterpreter.cxx:136`) calls `ApplyLaserSelection(CurrentFrame)`
   before `Frames.push_back`, dropping points whose `laser_id` is disabled.
6. The updated frame flows to the render view — only selected laser lines show.
7. If "Apply in future sessions", selection mask is written to `pqSettings`
   under a `LidarPlugin/LaserSelection*` keyset (mirrors old dialog).

## Components / files to create or modify

### New files
- `LidarCore/IO/Lidar/vtkLidarPacketInterpreter` — add members/methods
  (in existing header/cxx, not a new file).
- `Qt/Components/lqLaserSelectionDialog.h`
- `Qt/Components/lqLaserSelectionDialog.cxx`
- `Qt/Components/lqLaserSelectionDialog.ui`
- `Qt/ApplicationComponents/lqLaserSelectionReaction.h`
- `Qt/ApplicationComponents/lqLaserSelectionReaction.cxx`

### Modified files
- `LidarCore/IO/Lidar/vtkLidarPacketInterpreter.h` — add `LaserSelection` array,
  `SetLaserSelection`, `GetLaserSelection`, `IsLaserSelected`,
  `ApplyLaserSelection` declarations.
- `LidarCore/IO/Lidar/vtkLidarPacketInterpreter.cxx` — implement above; call
  `ApplyLaserSelection` in `SplitFrame()` before `Frames.push_back`.
- `Application/Client/LidarViewMainWindow.cxx` — add Tools-menu action and wire
  `lqLaserSelectionReaction`.
- `Qt/Components/CMakeLists.txt` and `Qt/ApplicationComponents/CMakeLists.txt` —
  register new sources (and `.ui` if needed).
- Possibly `LidarCore/IO/Lidar/CMakeLists.txt` if new public API needs wrapping
  for ParaView/Python (mirror old `vtkLidarPacketInterpreterClientServer.cxx`
  generation — confirm auto-wrapped).

## Implementation details

### Base interpreter (`vtkLidarPacketInterpreter`)
- `vtkNew<vtkIntArray> LaserSelection;` default-initialized so every channel is
  enabled (1). Resize to `GetNumberOfChannels()` lazily / when calibration is
  known; default value 1.
- `SetLaserSelection(int index, int value)`: bounds-check, set tuple, `Modified()`.
- `GetLaserSelection()`: return pointer.
- `IsLaserSelected(int laserId)`: return `LaserSelection` value for that id != 0,
  with safe default (enabled) when out of range / not yet sized.
- `ApplyLaserSelection(vtkPolyData* frame)`:
  - If no channel is disabled, return early (zero-cost).
  - Read the `"laser_id"` point array; build a mask of kept point ids.
  - Produce a new `vtkPolyData` containing only kept points + their point data
    (use `vtkExtractSelection` with a POINT id selection, or a manual copy).
    Replace `frame` contents in place.

### UI dialog (`lqLaserSelectionDialog`)
- Port the old `.ui`: `QTableWidget` (checkbox column + Channel + correction
  columns), `Enable/Disable all` checkbox, `Toggle Selected`, `Display more
  corrections`, `Apply`, `Apply in future sessions`, OK/Cancel.
- `setLidarSource(pqPipelineSource*)` resolves interpreter and sizes table to
  `GetNumberOfChannels()`.
- `getLaserSelectionSelector()` returns mask vector.
- On Apply (or OK): write mask to interpreter, optionally save to `pqSettings`.
- Connect to `pqServerManagerModel` sourceAdded/sourceRemoved so the dialog
  tracks the active LiDAR source.

### Reaction + menu entry
- `lqLaserSelectionReaction(QAction*)` — on trigger, construct and `show()` the
  dialog (non-modal, like old).
- In `LidarViewMainWindow.cxx` after `buildToolsMenu(*menuTools)`:
  ```cpp
  QAction* actionLaserSelection = new QAction(tr("Laser Selection"), this);
  this->Internals->menuTools->addAction(actionLaserSelection);
  new lqLaserSelectionReaction(actionLaserSelection);
  ```

## Error handling / edge cases
- No active LiDAR source: dialog opens but Apply is a no-op (or disabled) until a
  source is present.
- Interpreter with no `laser_id` array: `ApplyLaserSelection` is a no-op (graceful
  degradation). Senfoto008 already emits `laser_id`; other interpreters can be
  extended later.
- Channel count changes (different sensor/calibration): resize `LaserSelection`,
  preserve previous settings where channel ids still valid.
- Out-of-range `laserId`: treat as enabled (safe default).

## Testing
- **Unit:** small test in `LidarCore` verifying `ApplyLaserSelection` removes the
  correct points given a known `laser_id` array and selection mask.
- **Manual:** build, open a Senfoto008 pcap/stream, open Tools → Laser Selection,
  uncheck several channels, Apply → only selected lines render. Toggle all / none
  works. Restart app → selection persisted.
- Follow existing test layout under `Testing/` and the project's CMake test
  registration.

## Out of scope
- Per-sensor custom correction editing UI beyond what the table already shows.
- Real-time streaming re-interpretation optimizations (acceptable re-run cost).
