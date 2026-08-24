# Senfoto 008 LidarView Plugin Design

## 1. Goal

Add a LidarView plugin named `Senfoto008Plugin` that parses Senfoto 008 MSOP
packets (UDP port 8089) and produces `vtkPolyData` point clouds for the
**48-line** (`Lidar_model == 0x01`) and **96-line** (`Lidar_model == 0x02`)
variants. The plugin is modeled on the existing `LakiBeamPlugin`.

## 2. Background

LidarView already contains the `LakiBeamPlugin` (`lidarview/Plugins/LakiBeamPlugin`),
which implements a single-line mechanical LiDAR interpreter using the standard
LidarView plugin pattern:

- A `vtk*PacketInterpreter` subclass deriving from `vtkLidarPacketInterpreter`.
- A `*PacketFormat.h` header with protocol constants and parsing helpers.
- Three ServerManager XML files: interpreter, reader, stream.
- A `paraview.plugin` descriptor and CMake wiring.

The Senfoto 008 plugin will follow the same pattern.

## 3. Architecture & File Layout

New directory: `lidarview/Plugins/Senfoto008Plugin/`

```
Senfoto008Plugin/
├── paraview.plugin
├── CMakeLists.txt
├── Senfoto008Proxies.xml
└── Senfoto008PacketInterpreters/
    ├── CMakeLists.txt
    ├── vtk.module
    ├── Senfoto008PacketFormat.h
    ├── vtkSenfoto008PacketInterpreter.h
    ├── vtkSenfoto008PacketInterpreter.cxx
    ├── Senfoto008PacketInterpreter.xml
    ├── Senfoto008LidarReader.xml
    └── Senfoto008LidarStream.xml
```

One edit in `lidarview/CMakeLists.txt`: add `Senfoto008Plugin` to the
`lidarview_default_plugins` list so it is discovered and built by default.

### 3.1 Module wiring

- `vtk.module` declares the module `Senfoto008Plugin::Senfoto008PacketInterpreters`
  with a public dependency on `LidarView::IOLidar` and a private dependency on
  `VTK::CommonDataModel`.
- `Senfoto008PacketInterpreters/CMakeLists.txt` registers the interpreter class
  and the three ServerManager XML files.
- `Senfoto008Plugin/CMakeLists.txt` builds the ParaView plugin and exports
  `Senfoto008Proxies.xml`.

## 4. Packet Format & Parsing

The Senfoto 008 MSOP packet layout, as provided by the manufacturer document:

| Section | Size (bytes) | Notes |
|---------|--------------|-------|
| Header  | 42 | `pkt_head` = `0x00005346`, `Lidar_type` = `0x81` |
| Body    | 1184 | 8 data blocks × 148 bytes |
| Tail    | 22 | Product/echo-type bytes + reserved |
| **Total** | **1248** | Without UDP header |

### 4.1 Header fields used

| Field | Offset | Length | Meaning |
|-------|--------|--------|---------|
| `pkt_head` | 0 | 4 | Must be `0x00005346` |
| `pktcnt` | 12 | 4 | Packet counter 0–65535 |
| `timestamp` | 20 | 10 | 6 bytes seconds + 4 bytes microseconds |
| `Lidar_type` | 31 | 1 | `0x81` = 008 |
| `Lidar_model` | 32 | 1 | `0x01` = 48-line, `0x02` = 96-line |

### 4.2 Data block layout

Each block is 148 bytes:

| Field | Offset | Length |
|-------|--------|--------|
| Flag | 0 | 2 | Must be `0xEEFF` on the wire |
| Azimuth | 2 | 2 | Little-endian `uint16_t`, scale 0.01° |
| Channel 1 … 48 | 4 | 48 × 3 bytes |

Each channel is 3 bytes:

| Field | Length | Scale |
|-------|--------|-------|
| Distance | 2 bytes little-endian | **0.01 m** (see open questions) |
| Intensity | 1 byte | 0–255 |

### 4.3 Parsing helpers in `Senfoto008PacketFormat.h`

- `IsValidPacket(data, length)` — size, header magic, `Lidar_type`.
- `GetLidarModel(data)`.
- `GetBlockAzimuth(data, blockIndex)` — returns degrees.
- `GetChannelDistance(data, blockIndex, channelIndex)` — returns meters.
- `GetChannelIntensity(data, blockIndex, channelIndex)`.
- `GetPacketTimestamp(data)` — returns seconds as `double`.

## 5. Frame Detection & Data Flow

`PreProcessPacket` and `ProcessPacket` both:

1. Validate packet size and header.
2. Read the first block’s azimuth.
3. Detect a frame boundary when the azimuth decreases (wrap-around),
   mirroring `vtkLakiBeamPacketInterpreter`.

`ProcessPacket` iterates the 8 blocks:

- **48-line single-return:** each block is one complete 48-channel sweep.
- **96-line single-return:** blocks 0+1 form sweep 0 (laser IDs 0–95),
  blocks 2+3 form sweep 1, etc. Azimuth is shared between the two blocks of a
  pair.
- **96-line dual-return:** azimuth1–azimuth4 are equal. For the first pass this
  mode is detected but treated as single-return; dual-return splitting is a
  bounded future extension.

Points are inserted into `CurrentFrame`; on azimuth wrap, `SplitFrame()` flushes
the frame into `Frames` and a new empty frame is created.

## 6. 3D Point Computation

For each channel:

```text
azRad     = radians(azimuth)
elevRad   = radians(VerticalAngle[laserId])
x         = dist * cos(elevRad) * cos(azRad)
y         = dist * cos(elevRad) * sin(azRad)
z         = dist * sin(elevRad)
```

Point-cloud arrays created:

- `Points` (geometry)
- `X`, `Y`, `Z` (advanced arrays)
- `intensity`
- `laser_id`
- `timestamp`
- `distance_m`
- `azimuth`

The default active scalars are set to `intensity`.

## 7. Supported Modes

### 7.1 MVP

- 48-line single-return.
- 96-line single-return.

### 7.2 Future extensions

- 96-line dual-return (detected, but not split in MVP).
- DIFOP product-info parsing if needed for diagnostics.

## 8. Error Handling

- Drop packets that fail size/header/block-flag checks; emit a
  `vtkWarningMacro` in debug builds.
- Honor the inherited `IgnoreZeroDistances` property from
  `vtkLidarPacketInterpreter`.
- If `Lidar_model` is unrecognized, warn once and skip point conversion.

## 9. Testing Strategy

- Build the plugin with the LidarView superbuild.
- Add a CTest in `Senfoto008Plugin/Testing/` that:
  - Instantiates `vtkSenfoto008PacketInterpreter`.
  - Parses a synthetic 1248-byte packet with known header, azimuth, distance,
    and intensity values.
  - Verifies point count and the first/last point coordinates against
    hand-computed values.

## 10. Open Questions / Assumptions

1. **Vertical angle table.** The manufacturer document’s 48-line
   channel-to-pitch-angle table is empty. The design hardcodes the angles as
   `constexpr` arrays in `Senfoto008PacketFormat.h`. The actual table must be
   provided before the plugin can produce correct geometry. If it is not
   available, placeholder values will be used and marked with `TODO` comments.

2. **Distance scale.** The Channel Data table states
   `真实距离 * 100 (精度 0.01m)`. The document later mentions `0.5 cm` elsewhere.
   The design uses **0.01 m** (`distance = value / 100`). Please confirm this is
   correct.

3. **Default port.** The document states the default port is **8089**. The
   reader and stream XML files will default to 8089.

## 11. Implementation Approach

Use **Approach A** from the brainstorming discussion: clone the LakiBeam
plugin structure with hardcoded vertical angles. This is the fastest path to a
working plugin and follows an established, proven pattern in the codebase. A
calibration-file loader can be added later without changing the public
interface.
