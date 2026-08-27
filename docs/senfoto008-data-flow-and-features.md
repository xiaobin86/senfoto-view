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
5. 将 `CurrentFrame` 推入 `Frames` 缓冲区。
6. 新建空帧继续接收数据。

> 注：激光通道选择（Laser Selection）已**不在** `SplitFrame()` 中生效。它现在由一个挂载在 source 下游的独立过滤器 `vtkLaserSelectionFilter` 完成（见 4.1）。

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

> 实现方案（approach B）：激光通道选择是一个挂载在 source 下游的**独立 VTK 过滤器**
> `vtkLaserSelectionFilter`（位于 `LidarCore/Filters/Processing`，注册在 `filters` SM 组），
> 不再修改基类 `vtkLidarPacketInterpreter`。该过滤器读取每点的 `laser_id` 并丢弃被禁用的
> 通道；掩码经由过滤器的 `LaserSelection` SM 属性推送，因此能可靠到达服务端并触发重算。
> （早期方案 A 曾把掩码放在解释器基类、在 `SplitFrame()` 中 `ApplyLaserSelection`，但因客户端
> 掩码无法传播到服务端解释器、视图不变，已回退。）

#### 控制路径

```
Qt/ApplicationComponents/lqLaserSelectionReaction.cxx/h
  └─ onTriggered() -> 打开 Qt/Components/lqLaserSelectionDialog

Qt/Components/lqLaserSelectionDialog.cxx/h
  ├─ setLidarSource()      // 找到 source 的 LaserSelection 过滤器代理（消费者中查找）
  └─ onApply() / accept()  // 把掩码写回过滤器
       └─ vtkSMPropertyHelper(filterProxy, "LaserSelection").Set(mask)
       └─ filterProxy->UpdateVTKObjects(); filterProxy->MarkModified(filterProxy)

Qt/ApplicationComponents/lqOpenLidarReaction.cxx
  └─ AutoAttachLaserSelection(source)   // 打开 pcap/stream 时自动挂载 LaserSelection 过滤器
  └─ AutoAttachRadialDenoise(source)    // 径向去噪级联在 LaserSelection 过滤器之后
```

#### 数据流生效点

激光掩码在**管线下游、渲染之前**生效（与径向去噪平级、且去噪级联在其后）：

```cpp
// LidarCore/Filters/Processing/vtkLaserSelectionFilter.cxx
int vtkLaserSelectionFilter::RequestData(...)
{
    // 读取 "laser_id" 数组，构建被保留点的索引
    // 拷贝保留点 + 点数据 + 顶点单元到输出（禁用通道被丢弃）
}
```

相关 API：

```cpp
void vtkLaserSelectionFilter::SetLaserSelection(int index, int value);
vtkIntArray* vtkLaserSelectionFilter::GetLaserSelection();
```

`RequestData()` 的工作流程：

1. 若不存在 `laser_id` 数组，或没有任何通道被禁用，则 `ShallowCopy(input)` 直接透传。
2. 否则遍历每点，按 `laser_id` 查掩码，收集被保留点的索引。
3. 将保留点及其点数据拷贝到输出，并重建顶点单元（每个保留点一个顶点），使禁用通道的点被剔除。

因为掩码通过过滤器的 `LaserSelection` SM 属性（`repeat_command` + `use_index`）推送并
`MarkModified()`，服务端过滤器会重新执行，新掩码立即在视图中生效。

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
│   └─ Frames.push_back(CurrentFrame)  │
└──────────────────────────────────────┘
         │
         ▼ 每个 lidar source 打开时自动挂载（AutoAttachLaserSelection）
┌──────────────────────────────────────┐
│ vtkLaserSelectionFilter              │
│   └─ 按 laser_id 掩码剔除禁用通道    │ ◄── 激光通道选择生效
└──────────────────────────────────────┘
         │
         ▼ 如果数据源是 Senfoto008 且设置开启（AutoAttachRadialDenoise 级联在其后）
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
| **激光通道选择** | Stage 1 与 Stage 2 之间 | `LidarCore/Filters/Processing/vtkLaserSelectionFilter.cxx` | 作为外部 filter 挂载在 source 下游，按 `laser_id` 掩码剔除禁用通道 |
| **径向距差去噪** | Stage 1 与 Stage 2 之间 | `LidarCore/Filters/Processing/vtkRadialDistanceDenoise.cxx` | 作为外部 filter 挂载在 `LaserSelection` 过滤器之后，对 `vtkPolyData` 做二次过滤 |

也就是说：

- **激光通道选择**发生在管线下游（source 之外的独立 filter），决定哪些激光线的点进入后续管线与渲染。
- **径向距差去噪**发生在管线中、且级联在激光通道选择之后，对已经过通道筛选的点云做后处理，再交给渲染。

两者都是挂在 source 外部、对 `vtkPolyData` 做修改的 filter；激光通道选择位于去噪之前。
