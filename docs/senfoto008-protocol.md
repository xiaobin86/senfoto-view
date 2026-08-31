<!--
  功能：SenFoToView 新增功能文档 —— Senfoto008 雷达线协议（Wire Protocol）参考。
  作者：acelan
  新建时间：2026-08-28
  修改时间：2026-08-31
-->

# Senfoto 008 (SF008) 雷达线协议参考 / Wire Protocol Reference

> 反编译自参考实现 `02.Code/SenFoToPlugins/VelodynePlugin`（VelodynePlugin 解释器）
> 与本项目 `Senfoto008PacketInterpreter`。本文档只记录**与雷达硬件/线格式相关**的事实，
> 供正确解码 SF008 UDP 数据包使用。
>
> 配套代码：`lidarview/Plugins/Senfoto008Plugin/Senfoto008PacketInterpreters/`

---

## 1. 数据包布局（MSOP）

总长度 **1248 字节** = 头部(42) + 主体(1184) + 尾部(22)。

- **头部 (42 B)**：magic `00 00 53 46`（偏移 0）；包序号（偏移 12，**实测为大端 BE**，
  见 §9）；时间戳（10 B，偏移 20）；
  雷达类型（偏移 31）= `0x81`（SF008）；雷达型号（偏移 32）= `0x01`（48 线）/ `0x02`（96 线）。
- **主体 (1184 B)**：8 个数据块 × 148 B。
- **尾部 (22 B)**。

### 数据块（148 B）

| 字段 | 偏移 | 大小 | 说明 |
|------|------|------|------|
| flag | 0 | 2 B | 线上为 `EE FF`，小端读为 `0xEEFF` |
| azimuth | 2 | 2 B | **小端**，0.01°/LSB，范围 0–36000（即 0–360°） |
| channels | 4 | 48×3 B | 每通道：2 B 距离 + 1 B 强度 |

---

## 2. 距离解码（关键 —— 大小端陷阱）

距离是 **大端 12.4 定点 × 0.15 m**，**不是**普通小端 `uint16 × 0.01`。

参考实现：`vtkVelodyneLegacyPacketInterpreter::getDisFromBytes`（cxx:936）。

```cpp
uint16_t le  = laserReturn->distance;              // 小端读入
uint16_t be  = (le >> 8) | (le << 8);             // 字节交换 -> 大端
uint16_t hi12 = (be >> 4) & 0xFFF;
uint16_t lo4  = be & 0xF;
double distance_m = (hi12 + lo4 / 16.0) * 0.15;
```

> ⚠️ **该协议大小端不一致**：方位角与块标志是小端，但距离实际上是大端
> （参考在 `getDisFromBytes` 内部做字节交换）。用 `ReadUInt16LE × 0.01` 读距离是**错误**的。

---

## 3. 方位角解码（96 线变体）

- 方位角取**该块自身**的 `rotationalPosition`（小端，0.01° 单位）。
- 96 线施加**逐发光时序修正**：

  ```
  LastCorrectAz = rot[block]
                + diffs[block] * (luminousMoment + firingWithinBlock * Laser_fire_cycle)
                  / (2 * Laser_fire_cycle)
  ```

  单位为 0.01°，最后 `% 36000`。

  - `diffs[i] = (36000 + 18000 + rot[i+1] - rot[i]) % 36000 - 18000`
  - `luminousMoment`：激光 0 为 0，其余为 1（来自 SF.xml）
  - `Laser_fire_cycle` = 18（来自 SF.xml）
  - `firingWithinBlock`：激光 0–47 为 0，激光 48–95 为 1
- `rotationalCorrection` = 0（全部通道）→ 直接使用修正后的方位角。

---

## 4. 垂直角

- **96 线**：0.0 … 90.0°（96 项），与 SF.xml `vertCorrection_` 完全一致。
- **48 线**：96 线表的前 48 行（0.0 … 44.52°）。
- 所有角度 ≥ 0 ⇒ 传感器扫描的是**上半球**；`z = r·sin(elev) ≥ 0`。
  要得到覆盖房间的完整壳，需要靠安装朝向 / SensorTransform 翻转。

---

## 5. 数据块配对（96 线）

块按对处理 `(0,1)(2,3)(4,5)(6,7)`；`HDL_FIRING_PER_PKT = 8`。
首块 → 激光 0–47（`firingWithinBlock=0`）；次块 → 激光 48–95（`firingWithinBlock=1`）。
每个块携带**各自**的方位角。

---

## 6. 坐标系

参考（`ComputeCorrectedValues`，base interpreter cxx:713）：

```
X = d · cos(v) · cos(az)
Y = -d · cos(v) · sin(az)     // 注意 Y 为负
Z =  d · sin(v)
```

**SensorTransform**：LidarView 在**帧级别**施加（`vtkLidarPacketInterpreter.cxx:93-121`），
**不在**解释器内部。SF008 的穹顶通过此变换（安装朝向）翻成房间壳，在 LidarView 的传感器
配置中设置。SF.xml 的 `position_`/`orientation_` 均为 0。

---

## 7. 标定（SF.xml）

`vertCorrection_` 0–90；`LaserFireCycle_`=18；`luminousMoment_`=0(id0)/1；
`horizontalOffsetCorrection_`=0；其余修正项全为 0；`distLSB_`=0.4（legacy 解码器未使用）。
参考从该文件加载 `laser_corrections_`；SF008 插件把等效常量硬编码（未解析 SF.xml）。

---

## 8. 已应用的解释器修正（对照参考）

| # | 问题 | 修正 |
|---|------|------|
| 1 | 距离按小端 `×0.01` 读取 | 改为字节交换 + 12.4 定点 `×0.15`（匹配 `getDisFromBytes`） |
| 2 | Y 符号为 `+sin(az)` | 改为 `-sin(az)` |
| 3 | 无逐发光方位修正 | 新增 `ComputeCorrectedAzimuthDeg()` 复刻参考公式 |
| 4 | 96 线强制两块共享方位；48 线表错误（隔行采样） | 改为每块各自方位 + 48 线表 = SF.xml 前 48 行 |
| 5 | SensorTransform | 由 LidarView 框架在帧级别处理（解释器内不再施加，避免双重变换） |

> 注：#1–#4 修正点云的半径、Y 镜像与方位时序；半球穹顶本身来自垂直角 0–90° 与
> 安装朝向（#5 框架 SensorTransform 配置），并非大小端错误所致。

---

## 9. 拆帧机制与帧起始角特性（2026-08-31 实测结论）

### 9.1 拆帧机制（包级过零检测）

`PreProcessPacket` / `ProcessPacket`（vtkSenfoto008PacketInterpreter.cxx）只用 **block0 的方位角**：
`当前包 block0 az < 上一包 block0 az` 即认为方位角过零、开新帧。包内其余 7 个 block 的 az 不参与比较。

- 后果：若过零发生在上一包中间（block k 起已是新的一圈），该包 block k..7 的点会归入**旧帧**，
  拆帧边界滞后最多一包（~1.6°）。这是**业界主流做法**（Velodyne/RoboSense/Hesai 官方 ROS 驱动
  同样包级检测并接受此误差），下游 SLAM/去噪依赖逐点时间戳而非帧内 az 单调性，不受影响。
- 替代方案对比：Ouster 在包头带 `frame_id`（传感器定义帧，包边界与帧边界严格对齐，抗丢包）；
  Livox 按时间窗切帧。长期最优解是推动 SF008 固件增加帧标记。

### 9.2 帧起始角不严格为 0 —— 原始数据如此，非解析/丢包问题

对 `Examples/TestData/Senfoto-008-example.pcap`（127 MB，100801 包，456 帧）的直解验证：

| 指标 | 实测 |
|------|------|
| 包计数器字节序 | **大端 BE**（单调率 100%）；小端读单调率 0% |
| 丢包 | 仅 1 次缺口、共 2 包（0.002%），且不在帧边界 |
| 帧起始角分布 | 0–1.5°：445 帧（97.6%）；2.4–13.6°：11 帧 |
| 每包方位角跨度 | ~1.6°（每包 8 block × ~0.2°） |

两个成因：

1. **包粒度量化（正常）**：帧边界只能落在包边界上，首包 block0 az 必然散布在
   `[0, 1.6°)` 内 —— 任何过零检测实现都如此。
2. **传感器端方位角跳跃（异常，共 441 次 ≈ 每圈一次）**：原始数据中 azimuth 在**同一个包内**
   一次跳 ~10°（如 `358.8 → 7.6`），且**包计数器连续**（delta=1，无丢包）——即传感器固件本身
   没有输出 0° 附近 ~10° 的扫描。约 11 个受影响的帧起始角后移到 2.4–13.6°，且成串出现
   （#134–138、#386–390），起始角逐圈 +1.6° 后恢复，疑似固件转速/帧同步瞬时漂移。
   **此类包内 az 大跳变在 Velodyne/Ouster/Hesai 成熟产品中不是常态**（其编码器输出严格单调）。

结论：帧起始角不一致 = SF008 原始数据特性；上位机解析无误、无需改代码。
行动项：①向厂商反馈包内 az ~10° 跳变（附包计数器连续的证据）；②长期推动固件加
`frame_id`/帧同步标志。

### 9.3 实用备注

- 验证丢包用**包计数器（偏移 12，大端）**做缺口检测，不要用时间戳（时间戳是传感器内部时钟，
  不反映传输丢失）。
- Laser_id / intensity 的来源见 §5 与 `vtkSenfoto008PacketInterpreter.cxx`：laser_id 为通道
  索引推导（48 线 = channelIndex；96 线 = channelIndex / +48），intensity 取通道第 3 字节原样读出。
  若 intensity 与 laser_id 恒差 1，怀疑第 3 字节实为 1-based 通道号而非反射率（待原始包 dump 验证）。
