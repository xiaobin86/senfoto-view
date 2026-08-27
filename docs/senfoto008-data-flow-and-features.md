# SenFoToView 数据流与新功能插入点架构说明

> 本文聚焦 SenFoToView（LidarView 的 Senfoto008 定制版本）中，一帧激光雷达数据从原始报文到屏幕渲染的完整路径，以及近期合并的两个功能——**激光通道选择**与**径向距差去噪**——分别在哪个阶段、哪些文件、哪些方法中生效。
> 
> 所有路径相对源码根 `senfoto-view/`。

---

## 1. 总体分层

SenFoToView 的数据生命周期可以粗分为两大阶段：

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        Stage 1：数据解析 / 过滤阶段                          │
│  PCAP / 网络报文  →  vtkLidarPacketInterpreter  →  vtkPolyData（Frame）    │
└─────────────────────────────────────────────────────────────────────────────┘
                                      │
                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                        Stage 2：渲染阶段                                     │
│  vtkPolyData  →  ParaView Source/Filter Proxy  →  Representation  →  屏幕  │
└─────────────────────────────────────────────────────────────────────────────┘
```

- **Stage 1 在 C++ VTK 模块中完成**，产出是一个 `vtkPolyData`，包含点坐标、`laser_id`、强度、距离、方位角、时间戳等数组。
- **Stage 2 由 ParaView 的 ServerManager / Qt Client 完成**，把 `vtkPolyData` 通过 representation 渲染到 `pqRenderView`。

下面分别展开。

---

## 2. Stage 1：数据解析 / 过滤阶段

### 2.1 入口

| 数据源 | 文件 | 关键方法 |
|---|---|---|
| 离线 PCAP 文件 | `LidarCore/IO/Lidar/vtkLidarReader.cxx` | `RequestInformation()` / `RequestData()` |
| 实时网络流 | `LidarCore/IO/Lidar/vtkLidarStream.cxx` | `Start()` / `ConsumePacket()` / `RequestData()` |
| 报文解释器基类 | `LidarCore/IO/Lidar/vtkLidarPacketInterpreter.cxx` | `PreProcessPacketWrapped()` / `ProcessPacketWrapped()` / `SplitFrame()` |

### 2.2 PCAP 文件路径

```
vtkLidarReader::RequestInformation()
  └─ vtkLidarReader::BuildFramesIndex()
       ├─ Open() / ReadNextPacket()                 // 读取 pcap 包
       ├─ vtkLidarPacketInterpreter::IsLidarPacket()
       └─ vtkLidarPacketInterpreter::PreProcessPacketWrapped()   // 仅用于建立帧索引

vtkLidarReader::RequestData()
  └─ vtkLidarReader::ReadFrame(index, output)
       ├─ ResetCurrentFrame() / ClearAllFramesAvailable()
       ├─ ReadNextPacket()
       ├─ vtkLidarPacketInterpreter::ProcessPacketWrapped()
       ├─ vtkLidarPacketInterpreter::IsNewFrameReady()
       └─ output->ShallowCopy( vtkLidarPacketInterpreter::GetLastFrameAvailable() )
```

### 2.3 实时网络流路径

```
vtkLidarStream::Start()
  └─ 后台线程持续喂给 vtkLidarStream::ConsumePacket(pkt, timestamp)
       ├─ vtkLidarPacketInterpreter::IsValidPacket()   // 内部调用 IsLidarPacket()
       ├─ vtkLidarPacketInterpreter::ProcessPacketWrapped()
       ├─ vtkLidarPacketInterpreter::IsNewData()
       └─ vtkLidarStream::AddNewData()
            └─ Frames.push_back( interpreter->GetLastFrameAvailable() )

vtkLidarStream::RequestData()
  └─ output->ShallowCopy( Frames.back() )
```

### 2.4 每帧 vtkPolyData 的诞生

具体协议解释器（如 Senfoto008）实现 `ProcessPacket()`，逐包解析并填充 `CurrentFrame`；当检测到帧边界时调用 `SplitFrame()`。

在基类 `vtkLidarPacketInterpreter::SplitFrame()` 中完成帧的最终化：

1. 添加 RPM/FPS、厂商/型号等 field data。
2. 应用传感器坐标变换（除非开启 `PassthroughTransformMode`）。
3. 构建顶点单元格 `SetVerts()`。
4. 调用 `Squeeze()` 回收多余内存。
5. **调用 `ApplyLaserSelection(CurrentFrame)`**（激光通道选择在此生效，见 4.1）。
6. 将 `CurrentFrame` 推入 `Frames` 缓冲区。
7. 新建空帧继续接收数据。

随后 `GetLastFrameAvailable()` 返回 `Frames.back()`，这就是 Stage 1 的输出。

以 Senfoto008 为例：

```
Plugins/Senfoto008Plugin/Senfoto008PacketInterpreters/vtkSenfoto008PacketInterpreter.cxx
  └─ vtkSenfoto008PacketInterpreter::ProcessPacket()
       ├─ AddPoint(...)       // 填充 Points / intensity / laser_id / distance_m / azimuth / timestamp
       └─ Superclass::SplitFrame()
```

---

## 3. Stage 2：渲染阶段

### 3.1 UI / Reaction 层

用户在客户端点击「打开 PCAP」或「打开传感器流」时，触发：

```
Qt/ApplicationComponents/lqOpenLidarReaction.cxx
  ├─ lqOpenLidarReaction::openLidarPcap()
  └─ lqOpenLidarReaction::openLidarStream()
       └─ InitAndDisplaySource(source, prototype, true)
            ├─ controller->Show(source->getSourceProxy(), 0, view->getViewProxy())
            └─ source->updatePipeline()
```

`InitAndDisplaySource()` 创建 ParaView pipeline source，更新管线，并把输出端口 `0`（即 `Frame` 端口）显示到当前视图中。

### 3.2 ParaView Proxy / 管线层

| 代理 | 文件 | 说明 |
|---|---|---|
| Reader Proxy | `LidarCore/Remoting/ServerManager/vtkSMLidarReaderProxy.cxx` | PCAP  reader 的服务器端代理 |
| Stream Proxy | `LidarCore/Remoting/ServerManager/vtkSMLidarStreamProxy.cxx` | 实时流的服务器端代理 |
| Lidar Proxy | `LidarCore/Remoting/ServerManager/vtkSMLidarProxy.cxx` | 通用 lidar source 代理基类 |
| XML 定义 | `LidarCore/Plugin/IO/Interpreters/LidarReader.xml` | `CommonLidarReader`，输出端口 `0` = `Frame` |
| XML 定义 | `LidarCore/Plugin/IO/Interpreters/LidarStream.xml` | `CommonLidarStream`，输出端口 `0` = `Frame` |

`controller->Show()` 会创建一个 representation proxy（通常是 `vtkSMPVRepresentationProxy`），并把它绑定到 `pqRenderView` 的 `vtkSMRenderViewProxy` 上。

### 3.3 视图与最终渲染

- 当前激活的视图是标准 ParaView 3D 视图：
  - 客户端：`pqRenderView`
  - 服务器端代理：`vtkSMRenderViewProxy`
- `vtkPolyData` 驻留在服务器端的 representation 中。
- representation 的 mapper（ParaView 标准几何/点云 representation）将顶点渲染到屏幕上。

对于实时流，颜色映射初始化由 `Qt/ApplicationComponents/lqLidarStreamColorByInitBehavior.cxx` 中的 `tryLidarStreamInitColorBy()` 完成，通常按 `reflectivity` 或 `intensity` 着色。

---

## 4. 新功能插入点

### 4.1 激光通道选择（Laser Selection）

#### 控制路径

```
Qt/ApplicationComponents/lqLaserSelectionReaction.cxx/h
  └─ showDialog()
       └─ 打开 Qt/Components/lqLaserSelectionDialog

Qt/Components/lqLaserSelectionDialog.cxx/h
  ├─ setLidarSource()      // 遍历 pqServerManagerModel 找到当前 lidar source 的 interpreter
  └─ onApply() / accept()  // 把掩码写回 interpreter
       └─ interpreter->SetLaserSelection(i, mask[i])
```

#### 数据流生效点

激光掩码在**帧最终化阶段、进入渲染之前**生效：

```cpp
// LidarCore/IO/Lidar/vtkLidarPacketInterpreter.cxx
vtkLidarPacketInterpreter::SplitFrame()
{
    ...
    this->ApplyLaserSelection(this->CurrentFrame);   // <-- 插入点
    this->Frames.push_back(this->CurrentFrame);
    ...
}
```

相关 API：

```cpp
vtkIntArray* vtkLidarPacketInterpreter::GetLaserSelection();
void vtkLidarPacketInterpreter::SetLaserSelection(int index, int value);
bool vtkLidarPacketInterpreter::IsLaserSelected(int laserId);
void vtkLidarPacketInterpreter::ApplyLaserSelection(vtkPolyData* frame);
```

`ApplyLaserSelection()` 的工作流程：

1. 读取每点的 `"laser_id"` 数组。
2. 根据 `IsLaserSelected()` 构建被保留点的 `vtkSelection`。
3. 调用 `vtkExtractSelection` 提取子集。
4. 通过 `frame->ShallowCopy(extracted)` 替换当前帧内容。

因为 `SetLaserSelection()` 会触发 interpreter 的 `Modified()`，而 `vtkLidarReader`/`vtkLidarStream` 都监听了 interpreter 的 `ModifiedEvent`，所以用户调整通道后管线会自动重新执行，新掩码立即生效。

---

### 4.2 径向距差去噪（Radial Distance Denoise）

#### 算法实现

```
LidarCore/Filters/Processing/vtkRadialDistanceDenoise.cxx/h   // 核心算法
LidarCore/Plugin/Filters/RadialDistanceDenoise.xml            // ServerManager 代理定义
```

代理信息：

- Group：`filters`
- Name：`RadialDistanceDenoise`
- Class：`vtkRadialDistanceDenoise`

#### 管线插入点

该 filter 不是内置在 reader/stream 中，而是**作为 ParaView pipeline 的一个外部 filter，在打开 Senfoto008 数据源后自动挂载**。

```
Qt/ApplicationComponents/lqOpenLidarReaction.cxx
  ├─ bool IsRadialDenoiseAutoAttachEnabled()       // 读取 QSettings("LidarView/AutoAttachRadialDenoise")
  └─ void AutoAttachRadialDenoise(pqPipelineSource* source)
       {
           pqPipelineSource* filter =
               builder->createFilter("filters", "RadialDistanceDenoise", source);
           ...
           ::InitAndDisplaySource(filter, filter->getProxy(), true);
       }
```

在两种打开路径中都被调用，但仅对 Senfoto008 生效：

```cpp
// openLidarPcap() 与 openLidarStream() 中类似的判断
if (QString(prototype->GetXMLName()).startsWith("Senfoto008") &&
    IsRadialDenoiseAutoAttachEnabled())
{
    AutoAttachRadialDenoise(source);
}
```

最终管线结构：

```
Senfoto008 LidarReader / LidarStream
              │
              ▼
   RadialDistanceDenoise filter
              │
              ▼
   Representation → pqRenderView → 屏幕
```

#### 算法两级逻辑

`vtkRadialDistanceDenoise` 内部实现两级去噪：

- **L2（帧内去噪）**：对同一 `laser_id` 的点按方位角排序，计算每个点与左右邻近点的径向距离差；若某点显著偏离其邻居，则判定为孤立噪点。
- **L1（跨帧去噪）**：以 `(laser_id, azimuth_bin)` 为 key 缓存历史帧的径向距离，若当前帧同 key 的距离出现突变，则剔除。
- 被剔除的点通过 `vtkRemovePolyData` + `vtkCleanPolyData` 从输出中移除，而不是把距离置零。

---

## 5. 完整数据流时序图

```text
用户点击 "Open PCAP" / "Open Stream"
         │
         ▼
┌──────────────────────────────────────┐
│ lqOpenLidarReaction::openLidarPcap() │
│ lqOpenLidarReaction::openLidarStream()│
└──────────────────────────────────────┘
         │
         ▼ 创建 source
┌──────────────────────────────────────┐
│ vtkLidarReader / vtkLidarStream      │
└──────────────────────────────────────┘
         │
         ▼ 逐包解析
┌──────────────────────────────────────┐
│ vtkLidarPacketInterpreter::          │
│ ProcessPacketWrapped()               │
└──────────────────────────────────────┘
         │
         ▼ 帧边界触发
┌──────────────────────────────────────┐
│ vtkLidarPacketInterpreter::SplitFrame()│
│   ├─ 加 field data / 坐标变换        │
│   ├─ 构建顶点单元                    │
│   ├─ ApplyLaserSelection() ◄─────────┼── 激光通道选择生效
│   └─ Frames.push_back(CurrentFrame)  │
└──────────────────────────────────────┘
         │
         ▼ 如果数据源是 Senfoto008 且设置开启
┌──────────────────────────────────────┐
│ lqOpenLidarReaction::                │
│ AutoAttachRadialDenoise()            │
│   └─ createFilter("RadialDistanceDenoise")
└──────────────────────────────────────┘
         │
         ▼
┌──────────────────────────────────────┐
│ vtkRadialDistanceDenoise             │
│   ├─ L2 帧内径向距差去噪             │
│   └─ L1 跨帧径向距差去噪             │
└──────────────────────────────────────┘
         │
         ▼
┌──────────────────────────────────────┐
│ controller->Show(source/filter, 0)   │
│   └─ vtkSMPVRepresentationProxy      │
│        └─ vtkSMRenderViewProxy       │
└──────────────────────────────────────┘
         │
         ▼
        屏幕
```

---

## 6. 关键结论

| 功能 | 生效阶段 | 生效文件 | 生效方式 |
|---|---|---|---|
| **激光通道选择** | Stage 1：数据解析 / 过滤 | `LidarCore/IO/Lidar/vtkLidarPacketInterpreter.cxx` 的 `SplitFrame()` → `ApplyLaserSelection()` | 在帧最终化时根据掩码提取子集，改变输出的 `vtkPolyData` |
| **径向距差去噪** | Stage 1 与 Stage 2 之间 | `LidarCore/Filters/Processing/vtkRadialDistanceDenoise.cxx` | 作为外部 filter 挂载在 source 与 representation 之间，对 `vtkPolyData` 做二次过滤 |

也就是说：

- **激光通道选择**发生在数据源头，直接决定哪些激光线的点会被产出。
- **径向距差去噪**发生在管线中，对已经产出的点云做后处理，再交给渲染。

两者都是对 `vtkPolyData` 的修改，但一个在 source 内部、一个在 source 外部的 filter 中。
