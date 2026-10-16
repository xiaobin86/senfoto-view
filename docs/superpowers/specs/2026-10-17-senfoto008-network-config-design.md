# Senfoto008 雷达 IP / 端口配置 — 设计文档

- 日期：2026-10-17
- 分支：`feat/senfoto008-network-config`（基于 `develop`）
- 依据：`02.Code` 的 `senfoto_dynamic_param`（「更改 IP」对话框）+ `senfoto_ip_question`（密码框）私有协议实现

## 1. 目标

在 SenFoToView 中提供一个对话框，用于**修改 Senfoto008 雷达自身的网络参数**（雷达 IP、PC 端 IP、MAC、端口），并可**读回**当前配置回填显示。仅在 Senfoto008 源存在时可用。

> 注意：这是**雷达硬件侧**的网络参数（通过私有 UDP 协议写进雷达），与上游 LidarView 的「PC 侧监听端口 / 转发」配置（`lqLidarConfigurationDialog`）是两回事。

## 2. 研究结论

上游 Kitware/LidarView **没有**任何「修改雷达自身 IP / MAC / 端口」的功能：

- 上游只有 PC 侧网络参数：`lqLidarConfigurationDialog` + stream XML 属性 `ListeningPort` / `LocalListeningAddress` / `ForwardedIpAddress` / `MulticastAddress` / `IsForwarding`（`LidarCore/Plugin/IO/Interpreters/BaseStream.xml`、`PacketReceiver.xml`）。
- 本功能属于**厂商私有协议**，必须自研。`02.Code` 已有可用实现（`senfoto_dynamic_param.cpp` / `senfoto_ip_question.cpp`），本设计将其移植并按本 fork 的「新增自定义面板」范式落地（见 `docs/senfoto008-data-flow-and-features.md` §4.3）。

## 3. 协议（来自 `02.Code/senfoto_dynamic_param.cpp`）

- **传输**：UDP，目的端口 **8091**；本地接收 socket `bind(8091, ShareAddress)` 接收应答。
- **设置（写）**：命令 `0xfc 0x60`，共 **18 字节**，发往「目标雷达地址:8091」：

  | 偏移 | 长度 | 内容 |
  |---|---|---|
  | 0 | 1 | `0xfc`（命令头） |
  | 1 | 1 | `0x60`（设置网络参数） |
  | 2 | 4 | 雷达 IP（新值，4 字节） |
  | 6 | 4 | PC IP（新值，4 字节） |
  | 10 | 6 | MAC 地址（6 字节） |
  | 16 | 2 | 端口（**小端**：`[16]=port%256`, `[17]=port/256`） |

- **读取（查）**：命令 `0xfc 0x62 0x00 0x01`（4 字节），发往「目标雷达地址:8091」。
- **应答解析**：收到后校验前 4 字节为 `0xfc 0x62 0x00 0x02`；其后（原实现 `data = raw+4`）逐字段解析：

  | 偏移(相对 data) | 长度 | 字段 |
  |---|---|---|
  | 0 | 1 | `number_dis`（200/400/FB 型号区分） |
  | 1 | 2 | `lidar_number`（线数） |
  | 3 | 1 | `lidar_line` |
  | 4 | 2 | `distance`（量程） |
  | 6 | 4 | `src_ip`（雷达 IP） |
  | 10 | 4 | `dst_ip`（PC IP） |
  | 14 | 6 | `mac` |
  | 20 | 2 | `port` |
  | 22 | 1 | `gain` |
  | 23 | 1 | `clock` |
  | 24 | 1 | `eeprom_top` |
  | 25 | 1 | `flash_top` |
  | 26 | 1 | `fpga` |
  | 27 | 1 | `scale_mutiple` |
  | 28 | 1 | `eeprom_bot` |
  | 29 | 1 | `flash_bot` |
  | 32–38 | 7 | 生产日期（top y/m/d、bot y/m/d）、电机结构（长包 `MAX_DATA_SIZE` 才有） |

  > 具体显示字段取舍见 §4.4（本期只回填与「IP/端口」相关的字段，其余可选）。

- **校验**：
  - IP：`QRegExp("((2(5[0-5]|[0-4]\\d))|[0-1]?\\d{1,2})(\\.(...)){3}")`。
  - MAC：`QRegExp("([0-9a-fA-F]{2}-){5}[0-9a-fA-F]{2}")`。
  - 端口：`port > 1023 && port < 65536`。
- **密码门**：执行写入前弹密码框，密码硬编码 `senfoto`（沿用 02.Code）。

## 4. 架构与落点

按本 fork 的 Qt 组件组织（`Qt/Components/` 放对话框与控件，Reaction 放 `Qt/ApplicationComponents/`，菜单入口在 `Application/Client/LidarViewMainWindow.cxx`）。

### 4.1 协议封装（净新增）

`Qt/Components/lqSenfotoNetworkConfig.{h,cxx}`：纯逻辑类（可选：一个 `QObject` 或静态工具）。
- 常量：`0xfc60` / `0xfc62`、端口 `8091`。
- 方法：
  - `bool SendConfig(const QHostAddress& target, const QByteArray& newLidarIp, const QByteArray& newPcIp, const QByteArray& mac, quint16 port)` — 组 18 字节包并发送。
  - `bool RequestConfig(const QHostAddress& target)` — 发读取命令。
  - 通过信号/回调把解析出的字段回传（`readyRead` 解析头 `0xfc620002`）。
- 复用 `QUdpSocket`（与 `senfoto_dynamic_param` 一致，`bind(8091, ShareAddress)`）。
- 校验函数：IP / MAC / 端口（移植正则）。

### 4.2 配置对话框（净新增）

`Qt/Components/lqSenfotoNetworkConfigDialog.{h,cxx}` + `Resources/UI/lqSenfotoNetworkConfigDialog.ui`：
- 字段：
  - **目标地址（当前雷达 IP）** —— 发送命令的目的地址（默认取 `QSettings` 上次值）；
  - **雷达 IP**、**PC IP**、**MAC**、**端口** —— 待写入的新值。
- 按钮：「读取」（发 `0xfc62` 并回填）、「设置」（校验 → 密码框 → 发 `0xfc60`）、「关闭」。
- 输入校验失败用 `QMessageBox` 提示（移植原逻辑）。
- 回填：解析应答后把 IP/端口写入对应字段（其余诊断字段本期可仅展示或忽略，见 §4.4）。

### 4.3 密码框（净新增）

`Qt/Components/lqSenfotoPasswordDialog.{h,cxx}` + `.ui`：移植 `senfoto_ip_question`（掩码输入 + 显示勾选，密码 `senfoto`，错误提示）。

### 4.4 入口 Reaction（净新增）+ 菜单（改上游文件）

`Qt/ApplicationComponents/lqSenfotoNetworkConfigReaction.{h,cxx}`：
- 构造式绑定 `QAction`，`triggered` → 打开 `lqSenfotoNetworkConfigDialog`（非模态或模态均可，建议模态）。
- 打开前探测当前活动源；**仅当活动源 XML 名以 `Senfoto008` 开头**时可用。

`Application/Client/LidarViewMainWindow.cxx`：在 Tools 菜单构造处（现 `:211-214`）新增 action「雷达 IP 配置」并 `new lqSenfotoNetworkConfigReaction(action)`。
`Application/Qt/ApplicationComponents/Resources/interface_modes_config.json`：三模式的 `menus` 登记该 action。

> 「目标地址（当前雷达 IP）」由用户在对话框内填写（沿用 02.Code 做法：不做源地址自动探测）。若后续要从源自动探测，可读 `vtkStream` 的接收端地址（本期不做，列入风险）。

## 5. 文件改动清单

**净新增（加 acelan 文件头，见 `AGENTS.md`）**
- `Qt/Components/lqSenfotoNetworkConfig.h` / `.cxx`
- `Qt/Components/lqSenfotoNetworkConfigDialog.h` / `.cxx`
- `Qt/Components/Resources/UI/lqSenfotoNetworkConfigDialog.ui`
- `Qt/Components/lqSenfotoPasswordDialog.h` / `.cxx`
- `Qt/Components/Resources/UI/lqSenfotoPasswordDialog.ui`
- `Qt/ApplicationComponents/lqSenfotoNetworkConfigReaction.h` / `.cxx`

**修改（上游/既有文件，不加文件头）**
- `Qt/Components/CMakeLists.txt`（登记新类 + ui）
- `Qt/ApplicationComponents/CMakeLists.txt`（登记新 Reaction）
- `Application/Client/LidarViewMainWindow.cxx`（Tools 菜单入口）
- `Application/Qt/ApplicationComponents/Resources/interface_modes_config.json`（模式登记）

## 6. 测试

- **协议单元测试（无硬件可跑）**：对 `lqSenfotoNetworkConfig::BuildSetPacket(...)` 断言输出恰好 18 字节且各字段偏移/字节序正确（雷达 IP @2、PC IP @6、MAC @10、端口小端 @16）；对 `ParseResponse(0xfc 0x62 0x00 0x02 + payload)` 断言回填的 IP/MAC/端口正确。
- **校验**：非法 IP/MAC/端口被拒。
- **冒烟（需硬件）**：连接 Senfoto008，改 IP/端口后读回确认；用抓包确认报文与 §3 一致。
- **构建**（superbuild 两步）：`ninja -C ../build/superbuild/lidarview/build` → `ninja -C ../build superbuild/lidarview`。

## 7. 风险 / 注意

- **密码硬编码**：`senfoto` 为 02.Code 既有约定；如安全要求提高需改为可配置（本期沿用）。
- **目标地址来源**：本期由用户输入「当前雷达 IP」。若雷达被改到未知 IP，需先用厂商工具或抓包找回——不在本期范围。
- **接收端口占用**：`bind(8091, ShareAddress)` 可能与其它占用 8091 的进程冲突（02.Code 注释提到「受 vtkStream::StartMessage() 影响，接收不到」）；实现时需处理 bind 失败与共享地址。
- **仅 Senfoto008**：菜单 action 以活动源 XML 名前缀判定；无 Senfoto008 源时禁用/隐藏。
- **不影响上游语义**：本功能全为净新增，仅菜单/JSON 有上游文件改动。
