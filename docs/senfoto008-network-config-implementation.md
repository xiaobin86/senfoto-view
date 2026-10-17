<!--
  功能：Senfoto008 雷达 IP / 端口配置 —— 实现文档（私有协议、组件、代码落点、测试、风险）。
  作者：acelan
  新建时间：2026-10-17
  修改时间：2026-10-17
-->

# Senfoto008 雷达 IP / 端口配置 —— 实现文档

> 设计/计划：`docs/superpowers/specs/2026-10-17-senfoto008-network-config-design.md`、
> `docs/superpowers/plans/2026-10-17-senfoto008-network-config.md`
> 分支：`feat/senfoto008-network-config`（已合入 `develop`）

---

## 1. 目标

在 SenFoToView 中提供对话框，**修改 Senfoto008 雷达自身**的网络参数（雷达 IP / PC 端 IP / MAC / 端口），
并可**读回**当前配置。仅当活动源为 Senfoto008 时可用。

> 这是**雷达硬件侧**参数（经私有 UDP 协议写进雷达），与上游 LidarView 的「PC 侧监听/转发端口」
> （`lqLidarConfigurationDialog`）是两回事。

---

## 2. 背景

上游 Kitware/LidarView **没有**任何「改雷达自身 IP/MAC/端口」的功能——只有 PC 侧
`ListeningPort` / `LocalListeningAddress` / `ForwardedIpAddress` / `MulticastAddress`。
本功能是厂商私有协议，参考 `02.Code` 的 `senfoto_dynamic_param` / `senfoto_ip_question` 移植而来。

---

## 3. 协议（私有）

- **传输**：UDP，目的端口 **8091**；本地接收 socket `bind(8091, ShareAddress)`。
- **设置（写）**：命令 `0xfc 0x60`，共 **18 字节**，发往「目标雷达地址:8091」：

  | 偏移 | 长度 | 内容 |
  |---|---|---|
  | 0 | 1 | `0xfc` |
  | 1 | 1 | `0x60` |
  | 2 | 4 | 雷达 IP（新值，4 字节） |
  | 6 | 4 | PC IP（新值，4 字节） |
  | 10 | 6 | MAC（6 字节） |
  | 16 | 2 | 端口（**小端**：`[16]=port%256`, `[17]=port/256`） |

- **读取（查）**：命令 `0xfc 0x62 0x00 0x01`（4 字节），发往「目标雷达地址:8091」。
- **应答**：头 `0xfc 0x62 0x00 0x02`，其后为 payload；关键字段（相对 payload）：

  | 偏移 | 长度 | 字段 |
  |---|---|---|
  | 6 | 4 | 雷达 IP |
  | 10 | 4 | PC IP |
  | 14 | 6 | MAC |
  | 20 | 2 | 端口（小端） |

- **校验**：IP 正则、MAC 正则（`xx-xx-xx-xx-xx-xx`）、端口 `> 1023 && < 65536`。
- **密码门**：执行写入前弹密码框，密码 **`senfoto`**。

---

## 4. 架构与组件

按本 fork「Qt 组件放 `Qt/Components`、Reaction 放 `Qt/ApplicationComponents`、菜单入口在
`Application/Client/LidarViewMainWindow.cxx`」的范式：

```
Tools「雷达 IP 配置」(LidarViewMainWindow)
  └─ lqSenfotoNetworkConfigReaction           (Qt/ApplicationComponents)  — 仅 Senfoto008 源可用
        └─ lqSenfotoNetworkConfigDialog       (Qt/Components + .ui)       — 表单 + 读取/设置
              ├─ lqSenfotoPasswordDialog      (Qt/Components + .ui)       — 密码门
              └─ lqSenfotoNetworkConfig       (Qt/Components)             — 协议：组包/解包/校验/UDP
```

---

## 5. 实现细节

### 5.1 协议类 `lqSenfotoNetworkConfig`（`Qt/Components/lqSenfotoNetworkConfig.{h,cxx}`）

- 静态纯函数（便于单测）：
  - `BuildSetPacket(lidarIp, pcIp, mac, port)` → 18 字节 `QByteArray`。
  - `ParseResponse(datagram, DeviceInfo& out)` → 校验头 `fc 62 00 02` 并回填 `{lidarIp,pcIp,mac,port}`。
  - `IsValidIp / IsValidMac / IsValidPort`（`QRegularExpression` + `anchoredPattern`）。
- 实例方法：`request(target)`（发 `fc620001`）、`apply(target, ...)`（组包发送）；`onReadyRead()` 收应答发信号
  `configReceived(DeviceInfo)` / `errorOccurred(QString)`。
- 关键实现取舍：
  - **`QRegularExpression`**（Qt6 已移除 `QRegExp`）。
  - 头文件对 `QHostAddress` **仅前置声明**（`request/apply` 用 `const QHostAddress&`），
    避免把 QtNetwork 依赖泄漏给消费者。
  - `DeviceInfo` 用 `Q_DECLARE_METATYPE`（+ 对话框 `qRegisterMetaType`）。

### 5.2 密码框 `lqSenfotoPasswordDialog`（`Qt/Components` + `.ui`）

- 密码 `senfoto`；掩码输入 + “显示密码”勾选。移植 `senfoto_ip_question`。

### 5.3 配置对话框 `lqSenfotoNetworkConfigDialog`（`Qt/Components` + `.ui`）

- 字段：**当前雷达IP(目标)**（发送目的地址，默认 `QSettings LidarView/SenfotoNetworkConfig/TargetIp`）、
  **雷达IP / 主机IP / MAC / 端口**；按钮 **读取 / 设置 / 关闭**。
- 「读取」：校验目标 IP → `request()`；收到 `configReceived` 回填四项。
- 「设置」：校验全部 → 确认框 → 密码框 → `apply()`；成功后记住目标 IP。

### 5.4 入口 Reaction `lqSenfotoNetworkConfigReaction`（`Qt/ApplicationComponents`）

- 打开对话框；`updateEnableState()` 依活动源判定：仅 `GetXMLName().startsWith("Senfoto008")` 时 `setEnabled(true)`。
- `LidarViewMainWindow.cxx` Tools 菜单加 `tr("Sensor IP Config")`。

### 5.5 构建/模块要点（本仓库 Qt6 + VTK 模块系统踩坑）

| 点 | 做法 |
|---|---|
| `QT_NO_KEYWORDS` | 用 `Q_SLOTS`/`Q_SIGNALS`，不用 `slots`/`signals` |
| QtNetwork | `lqComponents` 需 `vtk_module_link(... PRIVATE "Qt6::Network")`（`QHostAddress`/`QUdpSocket`） |
| 非 vtk 类导出 | `vtk_module_add_module(... CLASSES ...)` 收录该 `QObject` 类（同 `lqLidarConfigurationDialog`） |
| 单测接入 | 本构建 `BUILD_TESTING=OFF`，故 `Qt/Components/CMakeLists.txt` 用 `enable_testing()` + 无条件 `add_subdirectory(Testing/Cxx)`（对齐 Senfoto008 插件测试） |
| `.ui` 头注释 | 必须在 `<?xml ...?>` **之后**、`<ui>` 之前（否则 `uic` 报 “XML declaration not at start”） |

---

## 6. 文件清单（相对 `lidarview/`）

- 增：`Qt/Components/lqSenfotoNetworkConfig.{h,cxx}`
- 增：`Qt/Components/Testing/Cxx/lqSenfotoNetworkConfigTest.cxx` + `CMakeLists.txt`
- 增：`Qt/Components/lqSenfotoPasswordDialog.{h,cxx}` + `Resources/UI/lqSenfotoPasswordDialog.ui`
- 增：`Qt/Components/lqSenfotoNetworkConfigDialog.{h,cxx}` + `Resources/UI/lqSenfotoNetworkConfigDialog.ui`
- 增：`Qt/ApplicationComponents/lqSenfotoNetworkConfigReaction.{h,cxx}`
- 改：`Qt/Components/CMakeLists.txt`、`Qt/ApplicationComponents/CMakeLists.txt`
- 改：`Application/Client/LidarViewMainWindow.cxx`（Tools 入口）
- 改：`i18n/zh_CN/LidarView.ts`（`Sensor IP Config → 雷达 IP 配置`）

---

## 7. 测试

- 协议单测（无硬件）：`lqSenfotoNetworkConfigTest` 断言 18 字节包布局（雷达 IP@2、PC IP@6、MAC@10、
  端口小端@16）、应答解析回填、以及 IP/MAC/端口校验。
  ```bash
  ctest --test-dir /mnt/d/work/senfoto-view/build/superbuild/lidarview/build \
    -R lqSenfotoNetworkConfigTest --output-on-failure
  ```
- 冒烟（需硬件）：连接 Senfoto008，改 IP/端口后读回确认；用 Wireshark 抓 8091 核对报文。

---

## 8. 使用

1. 打开 Senfoto008 源 → Tools →「雷达 IP 配置」。
2. 填「当前雷达IP(目标)」+ 新参数 →「设置」（确认 + 密码 `senfoto`）；或「读取」回填当前值。

---

## 9. 风险 / 注意

- **目标地址来源**：本期由用户在对话框输入「当前雷达IP」，不做源地址自动探测。
- **接收端口占用**：`bind(8091, ShareAddress)` 可能与他进程冲突；构造时忽略 bind 失败，运行时经
  `errorOccurred` 提示。
- **密码硬编码** `senfoto`：沿用 02.Code 约定。
- 仅 Senfoto008：菜单项依活动源判定启用；不影响其它雷达。
