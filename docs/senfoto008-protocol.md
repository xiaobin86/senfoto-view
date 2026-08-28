<!--
  功能：SenFoToView 新增功能文档 —— Senfoto008 雷达线协议（Wire Protocol）参考。
  作者：acelan
  新建时间：2026-08-28
  修改时间：2026-08-28
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

- **头部 (42 B)**：magic `00 00 53 46`（偏移 0）；包序号（偏移 12）；时间戳（10 B，偏移 20）；
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
