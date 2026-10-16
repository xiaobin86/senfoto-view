# Senfoto008 雷达 IP / 端口配置 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 SenFoToView 中提供对话框，修改 Senfoto008 雷达自身的 IP/PC-IP/MAC/端口（私有 UDP 协议），并可读回回填；仅当活动源为 Senfoto008 时可用。

**Architecture:** 净新增纯逻辑协议类 `lqSenfotoNetworkConfig`（UDP 8091，命令 `0xfc60` 写 / `0xfc62` 读，静态组包/解包函数便于单测）+ 密码框 + 配置对话框 + Reaction（Tools 菜单入口）。全为净新增，仅菜单/JSON 有上游文件改动。

**Tech Stack:** Qt (Widgets/Core, `QUdpSocket`, `QRegExp`), CMake (`vtk_module_add_module`), ParaView 菜单（`LidarViewMainWindow.cxx`）。

**Spec:** `docs/superpowers/specs/2026-10-17-senfoto008-network-config-design.md`

> **执行时修订（2026-10-17，实际环境为 Qt6）**：
> - 校验正则用 `QRegularExpression`（+`QRegularExpression::anchoredPattern`），**不用** `QRegExp`（Qt6 已移除）。
> - `lqComponents` 需 `vtk_module_link(... PRIVATE Qt::Network)`（`QHostAddress`/`QUdpSocket` 属 QtNetwork）；头文件对 `QHostAddress` 仅前置声明，避免污染消费者。
> - 模块以 `QT_NO_KEYWORDS` 编译 → 用 `Q_SLOTS`/`Q_SIGNALS`，不用 `slots`/`signals`。
> - `.ui` 的 acelan 头注释须放在 `<?xml ...?>` **声明之后**、`<ui>` 之前。
> - 本构建 `BUILD_TESTING=OFF`，故 `Qt/Components/CMakeLists.txt` 用 `enable_testing()` + 无条件 `add_subdirectory(Testing/Cxx)`（对齐 Senfoto008 插件测试的构建方式）。

## Global Constraints

- 协议：UDP 目的端口 **8091**；写 `0xfc 0x60` + 雷达IP(4)+PC IP(4)+MAC(6)+端口(2 小端) = 18B；读 `0xfc 0x62 0x00 0x01`；应答头 `0xfc 0x62 0x00 0x02`。
- 密码：`senfoto`（沿用 02.Code）。
- 仅 Senfoto008：菜单 action 以活动源 `GetXMLName().startsWith("Senfoto008")` 判定可用性。
- 净新增文件按 `AGENTS.md` 加 acelan 文件头；被修改的上游文件不加。
- 构建两步：`ninja -C ../build/superbuild/lidarview/build` → `ninja -C ../build superbuild/lidarview`。

---

## File Structure

| 文件 | 责任 |
|---|---|
| `Qt/Components/lqSenfotoNetworkConfig.h` / `.cxx` | 协议：组「设置」包、解析应答、校验 IP/MAC/端口、收发 UDP（净新增） |
| `Qt/Components/Testing/Cxx/lqSenfotoNetworkConfigTest.cxx` + `CMakeLists.txt` | 协议单测（无硬件） |
| `Qt/Components/lqSenfotoPasswordDialog.h` / `.cxx` + `Resources/UI/lqSenfotoPasswordDialog.ui` | 密码框（净新增） |
| `Qt/Components/lqSenfotoNetworkConfigDialog.h` / `.cxx` + `Resources/UI/lqSenfotoNetworkConfigDialog.ui` | 配置对话框（净新增） |
| `Qt/ApplicationComponents/lqSenfotoNetworkConfigReaction.h` / `.cxx` | Tools 菜单入口 Reaction（净新增） |
| `Qt/Components/CMakeLists.txt` | 登记新类 + ui |
| `Qt/ApplicationComponents/CMakeLists.txt` | 登记新 Reaction |
| `Application/Client/LidarViewMainWindow.cxx` | Tools 菜单加「雷达 IP 配置」 |
| `Application/Qt/ApplicationComponents/Resources/interface_modes_config.json` | 三模式 menus 登记 |

---

### Task 1: 建分支

- [ ] **Step 1:**

```bash
cd /mnt/d/work/senfoto-view/lidarview
git checkout develop && git pull --ff-only || true
git checkout -b feat/senfoto008-network-config
```

---

### Task 2: 协议类 `lqSenfotoNetworkConfig`（TDD）

**Files:**
- Create: `Qt/Components/lqSenfotoNetworkConfig.h`
- Create: `Qt/Components/lqSenfotoNetworkConfig.cxx`
- Test: `Qt/Components/Testing/Cxx/lqSenfotoNetworkConfigTest.cxx`
- Create: `Qt/Components/Testing/Cxx/CMakeLists.txt`
- Modify: `Qt/Components/CMakeLists.txt`

**Interfaces:**
- `static QByteArray BuildSetPacket(const QString& lidarIp, const QString& pcIp, const QString& mac, int port)` → 18 字节包
- `static bool ParseResponse(const QByteArray& datagram, DeviceInfo& out)`
- `static bool IsValidIp(const QString&)` / `IsValidMac(const QString&)` / `IsValidPort(int)`
- `bool request(const QHostAddress& target)` / `bool apply(const QHostAddress& target, const QString& lidarIp, const QString& pcIp, const QString& mac, int port)`
- signal `void configReceived(const DeviceInfo& info);` / `void errorOccurred(const QString&);`

- [ ] **Step 1: 写失败测试** `Qt/Components/Testing/Cxx/lqSenfotoNetworkConfigTest.cxx`

```cpp
// ============================================================
// 功能：Senfoto008 网络配置协议的单元测试（无需硬件）。
// 作者：acelan
// 新建时间：2026-10-17
// 修改时间：2026-10-17
// ============================================================

#include "lqSenfotoNetworkConfig.h"

#include <QByteArray>
#include <iostream>

int main()
{
  // ---- 设置包布局 ----
  const QByteArray pkt = lqSenfotoNetworkConfig::BuildSetPacket(
    "192.168.1.30", "192.168.1.105", "00-0a-35-01-fe-c0", 8091);
  if (pkt.size() != 18) { std::cerr << "size=" << pkt.size() << "\n"; return 1; }
  if ((unsigned char)pkt[0] != 0xfc || (unsigned char)pkt[1] != 0x60) { std::cerr << "cmd\n"; return 1; }
  const unsigned char expIp[4] = { 192, 168, 1, 30 };
  for (int i = 0; i < 4; ++i) { if ((unsigned char)pkt[2 + i] != expIp[i]) { std::cerr << "lidarIp\n"; return 1; } }
  const unsigned char expPc[4] = { 192, 168, 1, 105 };
  for (int i = 0; i < 4; ++i) { if ((unsigned char)pkt[6 + i] != expPc[i]) { std::cerr << "pcIp\n"; return 1; } }
  const unsigned char expMac[6] = { 0x00, 0x0a, 0x35, 0x01, 0xfe, 0xc0 };
  for (int i = 0; i < 6; ++i) { if ((unsigned char)pkt[10 + i] != expMac[i]) { std::cerr << "mac\n"; return 1; } }
  if ((unsigned char)pkt[16] != (8091 % 256) || (unsigned char)pkt[17] != (8091 / 256)) { std::cerr << "port\n"; return 1; }

  // ---- 应答解析 ----
  QByteArray resp;
  resp.append((char)0xfc).append((char)0x62).append((char)0x00).append((char)0x02); // 头
  for (int i = 0; i < 30; ++i) resp.append('\0'); // payload
  resp[4 + 6] = (char)10; resp[4 + 7] = (char)0; resp[4 + 8] = (char)0; resp[4 + 9] = (char)5;  // 雷达 IP 10.0.0.5
  resp[4 + 10] = (char)192; resp[4 + 11] = (char)168; resp[4 + 12] = (char)1; resp[4 + 13] = (char)2; // PC IP
  resp[4 + 14] = (char)0xaa; resp[4 + 15] = (char)0xbb; resp[4 + 16] = (char)0xcc;
  resp[4 + 17] = (char)0xdd; resp[4 + 18] = (char)0xee; resp[4 + 19] = (char)0xff; // MAC
  resp[4 + 20] = (char)(8091 % 256); resp[4 + 21] = (char)(8091 / 256); // 端口
  lqSenfotoNetworkConfig::DeviceInfo info;
  if (!lqSenfotoNetworkConfig::ParseResponse(resp, info)) { std::cerr << "parse\n"; return 1; }
  if (info.lidarIp != "10.0.0.5" || info.pcIp != "192.168.1.2" ||
      info.mac != "aa-bb-cc-dd-ee-ff" || info.port != 8091) { std::cerr << "fields\n"; return 1; }

  // ---- 校验 ----
  if (!lqSenfotoNetworkConfig::IsValidIp("192.168.1.30")) { std::cerr << "ipv\n"; return 1; }
  if (lqSenfotoNetworkConfig::IsValidIp("999.1.1.1")) { std::cerr << "ipv2\n"; return 1; }
  if (!lqSenfotoNetworkConfig::IsValidMac("00-0a-35-01-fe-c0")) { std::cerr << "macv\n"; return 1; }
  if (lqSenfotoNetworkConfig::IsValidPort(80)) { std::cerr << "portv\n"; return 1; }
  if (!lqSenfotoNetworkConfig::IsValidPort(8091)) { std::cerr << "portv2\n"; return 1; }

  std::cout << "PASS\n";
  return 0;
}
```

- [ ] **Step 2: 测试 CMake** `Qt/Components/Testing/Cxx/CMakeLists.txt`

```cmake
# ============================================================
# 功能：Senfoto008 网络配置协议单元测试注册。
# 作者：acelan
# ============================================================

add_executable(lqSenfotoNetworkConfigTest lqSenfotoNetworkConfigTest.cxx)
target_link_libraries(lqSenfotoNetworkConfigTest
  PRIVATE
    LidarView::lqComponents
    "Qt${PARAVIEW_QT_MAJOR_VERSION}::Core")
add_test(NAME lqSenfotoNetworkConfigTest COMMAND lqSenfotoNetworkConfigTest)
```

- [ ] **Step 3: 运行测试，确认失败（红）**

`Qt/Components/CMakeLists.txt` 末尾追加（在 `vtk_module_add_module` 之后）：

```cmake
if (BUILD_TESTING)
  add_subdirectory(Testing/Cxx)
endif ()
```

```bash
cd /mnt/d/work/senfoto-view/lidarview
cmake -S ../lidarview-superbuild -B ../build >/dev/null 2>&1 || true
cmake --build ../build/superbuild/lidarview/build --target lqSenfotoNetworkConfigTest
```

Expected: FAIL — `lqSenfotoNetworkConfig.h: No such file or directory`。

- [ ] **Step 4: 头文件** `Qt/Components/lqSenfotoNetworkConfig.h`

```cpp
// ============================================================
// 功能：Senfoto008 雷达网络配置私有协议（UDP 8091，0xfc60 写 / 0xfc62 读）。
// 作者：acelan
// 新建时间：2026-10-17
// 修改时间：2026-10-17
// ============================================================

#ifndef lqSenfotoNetworkConfig_h
#define lqSenfotoNetworkConfig_h

#include "lqComponentsModule.h"

#include <QByteArray>
#include <QHostAddress>
#include <QObject>
#include <QString>

class QUdpSocket;

/**
 * @brief Encapsulates the Senfoto008 sensor network-configuration protocol
 *        (change/read the sensor's own IP, PC IP, MAC and port).
 */
class LQCOMPONENTS_EXPORT lqSenfotoNetworkConfig : public QObject
{
  Q_OBJECT

public:
  struct DeviceInfo
  {
    QString lidarIp;
    QString pcIp;
    QString mac;
    int port = 0;
  };

  static constexpr int DEFAULT_PORT = 8091;
  static constexpr const char* PASSWORD = "senfoto";

  explicit lqSenfotoNetworkConfig(QObject* parent = nullptr);
  ~lqSenfotoNetworkConfig() override;

  // Pure, testable protocol helpers.
  static QByteArray BuildSetPacket(
    const QString& lidarIp, const QString& pcIp, const QString& mac, int port);
  static bool ParseResponse(const QByteArray& datagram, DeviceInfo& out);
  static bool IsValidIp(const QString& ip);
  static bool IsValidMac(const QString& mac);
  static bool IsValidPort(int port);

  // Socket operations (send only; replies arrive via onReadyRead).
  bool request(const QHostAddress& target, int port = DEFAULT_PORT);
  bool apply(const QHostAddress& target, const QString& lidarIp, const QString& pcIp,
    const QString& mac, int port, int targetPort = DEFAULT_PORT);

Q_SIGNALS:
  void configReceived(const lqSenfotoNetworkConfig::DeviceInfo& info);
  void errorOccurred(const QString& message);

private Q_SLOTS:
  void onReadyRead();

private:
  QUdpSocket* Socket = nullptr;

  Q_DISABLE_COPY(lqSenfotoNetworkConfig)
};

#endif
```

- [ ] **Step 5: 实现** `Qt/Components/lqSenfotoNetworkConfig.cxx`

```cpp
// ============================================================
// 功能：Senfoto008 雷达网络配置私有协议实现。
// 作者：acelan
// 新建时间：2026-10-17
// 修改时间：2026-10-17
// ============================================================

#include "lqSenfotoNetworkConfig.h"

#include <QRegExp>
#include <QUdpSocket>

namespace
{
bool IpToBytes(const QString& ip, unsigned char out[4])
{
  const QStringList parts = ip.split('.');
  if (parts.size() != 4)
  {
    return false;
  }
  for (int i = 0; i < 4; ++i)
  {
    bool ok = false;
    const int v = parts[i].toInt(&ok);
    if (!ok || v < 0 || v > 255)
    {
      return false;
    }
    out[i] = static_cast<unsigned char>(v);
  }
  return true;
}

QString BytesToIp(const unsigned char* b)
{
  return QString("%1.%2.%3.%4").arg(b[0]).arg(b[1]).arg(b[2]).arg(b[3]);
}

bool MacToBytes(const QString& mac, unsigned char out[6])
{
  const QStringList parts = mac.split('-');
  if (parts.size() != 6)
  {
    return false;
  }
  for (int i = 0; i < 6; ++i)
  {
    bool ok = false;
    const uint v = parts[i].toUInt(&ok, 16);
    if (!ok || v > 0xFF)
    {
      return false;
    }
    out[i] = static_cast<unsigned char>(v);
  }
  return true;
}

QString BytesToMac(const unsigned char* b)
{
  QString mac;
  for (int i = 0; i < 6; ++i)
  {
    mac += QString("%1").arg(b[i], 2, 16, QChar('0'));
    if (i < 5)
    {
      mac += "-";
    }
  }
  return mac;
}
}

//-----------------------------------------------------------------------------
lqSenfotoNetworkConfig::lqSenfotoNetworkConfig(QObject* parent)
  : QObject(parent)
  , Socket(new QUdpSocket(this))
{
  this->Socket->bind(DEFAULT_PORT, QUdpSocket::ShareAddress);
  QObject::connect(this->Socket, &QUdpSocket::readyRead, this, &lqSenfotoNetworkConfig::onReadyRead);
}

//-----------------------------------------------------------------------------
lqSenfotoNetworkConfig::~lqSenfotoNetworkConfig() = default;

//-----------------------------------------------------------------------------
bool lqSenfotoNetworkConfig::IsValidIp(const QString& ip)
{
  QRegExp rx("((2(5[0-5]|[0-4]\\d))|[0-1]?\\d{1,2})(\\.((2(5[0-5]|[0-4]\\d))|[0-1]?\\d{1,2})){3}");
  return rx.exactMatch(ip);
}

//-----------------------------------------------------------------------------
bool lqSenfotoNetworkConfig::IsValidMac(const QString& mac)
{
  QRegExp rx("([0-9a-fA-F]{2}-[0-9a-fA-F]{2}-[0-9a-fA-F]{2}-[0-9a-fA-F]{2}-[0-9a-fA-F]{2}-[0-9a-fA-F]{2})");
  return rx.exactMatch(mac);
}

//-----------------------------------------------------------------------------
bool lqSenfotoNetworkConfig::IsValidPort(int port)
{
  return port > 1023 && port < 65536;
}

//-----------------------------------------------------------------------------
QByteArray lqSenfotoNetworkConfig::BuildSetPacket(
  const QString& lidarIp, const QString& pcIp, const QString& mac, int port)
{
  unsigned char lidar[4] = {};
  unsigned char pc[4] = {};
  unsigned char macBytes[6] = {};
  if (!IpToBytes(lidarIp, lidar) || !IpToBytes(pcIp, pc) || !MacToBytes(mac, macBytes))
  {
    return QByteArray();
  }

  QByteArray pkt(18, '\0');
  pkt[0] = static_cast<char>(0xfc);
  pkt[1] = static_cast<char>(0x60);
  for (int i = 0; i < 4; ++i)
  {
    pkt[2 + i] = static_cast<char>(lidar[i]);
    pkt[6 + i] = static_cast<char>(pc[i]);
  }
  for (int i = 0; i < 6; ++i)
  {
    pkt[10 + i] = static_cast<char>(macBytes[i]);
  }
  pkt[16] = static_cast<char>(port % 256);
  pkt[17] = static_cast<char>(port / 256);
  return pkt;
}

//-----------------------------------------------------------------------------
bool lqSenfotoNetworkConfig::ParseResponse(const QByteArray& datagram, DeviceInfo& out)
{
  if (datagram.size() < 4 + 26)
  {
    return false;
  }
  if (static_cast<unsigned char>(datagram[0]) != 0xfc ||
    static_cast<unsigned char>(datagram[1]) != 0x62 ||
    static_cast<unsigned char>(datagram[2]) != 0x00 ||
    static_cast<unsigned char>(datagram[3]) != 0x02)
  {
    return false;
  }
  const unsigned char* p = reinterpret_cast<const unsigned char*>(datagram.constData()) + 4;
  out.lidarIp = BytesToIp(p + 6);
  out.pcIp = BytesToIp(p + 10);
  out.mac = BytesToMac(p + 14);
  out.port = static_cast<int>(p[20]) | (static_cast<int>(p[21]) << 8);
  return true;
}

//-----------------------------------------------------------------------------
bool lqSenfotoNetworkConfig::request(const QHostAddress& target, int port)
{
  const char cmd[4] = { static_cast<char>(0xfc), 0x62, 0x00, 0x01 };
  const qint64 n = this->Socket->writeDatagram(cmd, 4, target, static_cast<quint16>(port));
  if (n < 0)
  {
    Q_EMIT this->errorOccurred(tr("发送读取命令失败：%1").arg(this->Socket->errorString()));
    return false;
  }
  return true;
}

//-----------------------------------------------------------------------------
bool lqSenfotoNetworkConfig::apply(const QHostAddress& target, const QString& lidarIp,
  const QString& pcIp, const QString& mac, int port, int targetPort)
{
  const QByteArray pkt = BuildSetPacket(lidarIp, pcIp, mac, port);
  if (pkt.isEmpty())
  {
    Q_EMIT this->errorOccurred(tr("参数格式错误"));
    return false;
  }
  const qint64 n = this->Socket->writeDatagram(pkt, target, static_cast<quint16>(targetPort));
  if (n < 0)
  {
    Q_EMIT this->errorOccurred(tr("发送设置命令失败：%1").arg(this->Socket->errorString()));
    return false;
  }
  return true;
}

//-----------------------------------------------------------------------------
void lqSenfotoNetworkConfig::onReadyRead()
{
  while (this->Socket->hasPendingDatagrams())
  {
    QByteArray datagram;
    datagram.resize(static_cast<int>(this->Socket->pendingDatagramSize()));
    this->Socket->readDatagram(datagram.data(), datagram.size());

    DeviceInfo info;
    if (ParseResponse(datagram, info))
    {
      Q_EMIT this->configReceived(info);
    }
  }
}
```

- [ ] **Step 6: 在 `Qt/Components/CMakeLists.txt` 登记**

`set(classes ...)` 中追加 `lqSenfotoNetworkConfig`（按字母序）。`LQCOMPONENTS_EXPORT` 宏由模块头 `lqComponentsModule.h` 生成。

- [ ] **Step 7: 运行测试，确认通过（绿）**

```bash
cmake --build ../build/superbuild/lidarview/build --target lqSenfotoNetworkConfigTest
ctest --test-dir ../build/superbuild/lidarview/build -R lqSenfotoNetworkConfigTest --output-on-failure
```

Expected: PASS。

- [ ] **Step 8: 提交**

```bash
git add Qt/Components/lqSenfotoNetworkConfig.h Qt/Components/lqSenfotoNetworkConfig.cxx \
        Qt/Components/Testing Qt/Components/CMakeLists.txt
git commit -m "feat(senfoto008): add sensor network-config protocol helper + unit test"
```

---

### Task 3: 密码框 `lqSenfotoPasswordDialog`

**Files:**
- Create: `Qt/Components/lqSenfotoPasswordDialog.h` / `.cxx`
- Create: `Qt/Components/Resources/UI/lqSenfotoPasswordDialog.ui`
- Modify: `Qt/Components/CMakeLists.txt`

- [ ] **Step 1: UI 文件** `Resources/UI/lqSenfotoPasswordDialog.ui`

```xml
<!--
  功能：Senfoto008 网络配置密码输入框。
  作者：acelan
  新建时间：2026-10-17
  修改时间：2026-10-17
-->
<?xml version="1.0" encoding="UTF-8"?>
<ui version="4.0">
 <class>lqSenfotoPasswordDialog</class>
 <widget class="QDialog" name="lqSenfotoPasswordDialog">
  <property name="windowTitle"><string>密码输入</string></property>
  <layout class="QVBoxLayout" name="verticalLayout">
   <item>
    <layout class="QHBoxLayout" name="rowLayout">
     <item><widget class="QLabel" name="labelPwd"><property name="text"><string>密码：</string></property></widget></item>
     <item><widget class="QLineEdit" name="lineEditPwd"><property name="echoMode"><enum>QLineEdit::Password</enum></property></widget></item>
     <item><widget class="QCheckBox" name="checkBoxShow"><property name="text"><string>显示密码</string></property></widget></item>
    </layout>
   </item>
   <item>
    <layout class="QHBoxLayout" name="buttonLayout">
     <item><widget class="QPushButton" name="pushButtonOk"><property name="text"><string>确定</string></property></widget></item>
     <item><widget class="QPushButton" name="pushButtonCancel"><property name="text"><string>取消</string></property></widget></item>
    </layout>
   </item>
  </layout>
 </widget>
 <resources/>
 <connections/>
</ui>
```

- [ ] **Step 2: 头文件 / 实现**

`lqSenfotoPasswordDialog.h`：

```cpp
// ============================================================
// 功能：Senfoto008 网络配置密码输入对话框。
// 作者：acelan
// 新建时间：2026-10-17
// 修改时间：2026-10-17
// ============================================================

#ifndef lqSenfotoPasswordDialog_h
#define lqSenfotoPasswordDialog_h

#include "lqComponentsModule.h"

#include <QDialog>

namespace Ui
{
class lqSenfotoPasswordDialog;
}

class LQCOMPONENTS_EXPORT lqSenfotoPasswordDialog : public QDialog
{
  Q_OBJECT

public:
  explicit lqSenfotoPasswordDialog(QWidget* parent = nullptr);
  ~lqSenfotoPasswordDialog() override;

private Q_SLOTS:
  void onShowToggled(int state);
  void onOk();
  void onCancel();

private:
  Ui::lqSenfotoPasswordDialog* Ui;

  Q_DISABLE_COPY(lqSenfotoPasswordDialog)
};

#endif
```

`lqSenfotoPasswordDialog.cxx`：

```cpp
// ============================================================
// 功能：Senfoto008 网络配置密码输入对话框实现。
// 作者：acelan
// 新建时间：2026-10-17
// 修改时间：2026-10-17
// ============================================================

#include "lqSenfotoPasswordDialog.h"
#include "ui_lqSenfotoPasswordDialog.h"

#include <QMessageBox>

//-----------------------------------------------------------------------------
lqSenfotoPasswordDialog::lqSenfotoPasswordDialog(QWidget* parent)
  : QDialog(parent)
  , Ui(new Ui::lqSenfotoPasswordDialog)
{
  this->Ui->setupUi(this);
  this->Ui->lineEditPwd->setEchoMode(QLineEdit::Password);
  this->Ui->checkBoxShow->setChecked(false);
  QObject::connect(
    this->Ui->checkBoxShow, &QCheckBox::stateChanged, this, &lqSenfotoPasswordDialog::onShowToggled);
  QObject::connect(this->Ui->pushButtonOk, &QPushButton::clicked, this, &lqSenfotoPasswordDialog::onOk);
  QObject::connect(
    this->Ui->pushButtonCancel, &QPushButton::clicked, this, &lqSenfotoPasswordDialog::onCancel);
}

//-----------------------------------------------------------------------------
lqSenfotoPasswordDialog::~lqSenfotoPasswordDialog()
{
  delete this->Ui;
}

//-----------------------------------------------------------------------------
void lqSenfotoPasswordDialog::onShowToggled(int state)
{
  this->Ui->lineEditPwd->setEchoMode(state ? QLineEdit::Normal : QLineEdit::Password);
}

//-----------------------------------------------------------------------------
void lqSenfotoPasswordDialog::onOk()
{
  if (this->Ui->lineEditPwd->text() == QStringLiteral("senfoto"))
  {
    QDialog::accept();
  }
  else
  {
    QMessageBox::information(this, tr("提示"), tr("密码不正确, 请重新输入!"));
    QDialog::reject();
  }
}

//-----------------------------------------------------------------------------
void lqSenfotoPasswordDialog::onCancel()
{
  QDialog::reject();
}
```

- [ ] **Step 3: 在 `Qt/Components/CMakeLists.txt` 登记**

`classes` 加 `lqSenfotoPasswordDialog`；`ui_files` 加 `Resources/UI/lqSenfotoPasswordDialog.ui`。

- [ ] **Step 4: 编译**

```bash
cmake --build ../build/superbuild/lidarview/build --target lqComponents
```

Expected: 成功。

- [ ] **Step 5: 提交**

```bash
git add Qt/Components/lqSenfotoPasswordDialog.h Qt/Components/lqSenfotoPasswordDialog.cxx \
        Qt/Components/Resources/UI/lqSenfotoPasswordDialog.ui Qt/Components/CMakeLists.txt
git commit -m "feat(senfoto008): add network-config password dialog"
```

---

### Task 4: 配置对话框 `lqSenfotoNetworkConfigDialog`

**Files:**
- Create: `Qt/Components/lqSenfotoNetworkConfigDialog.h` / `.cxx`
- Create: `Qt/Components/Resources/UI/lqSenfotoNetworkConfigDialog.ui`
- Modify: `Qt/Components/CMakeLists.txt`

- [ ] **Step 1: UI 文件** `Resources/UI/lqSenfotoNetworkConfigDialog.ui`

```xml
<!--
  功能：Senfoto008 雷达 IP/端口配置对话框。
  作者：acelan
  新建时间：2026-10-17
  修改时间：2026-10-17
-->
<?xml version="1.0" encoding="UTF-8"?>
<ui version="4.0">
 <class>lqSenfotoNetworkConfigDialog</class>
 <widget class="QDialog" name="lqSenfotoNetworkConfigDialog">
  <property name="windowTitle"><string>雷达 IP 配置</string></property>
  <layout class="QVBoxLayout" name="verticalLayout">
   <item>
    <layout class="QFormLayout" name="formLayout">
     <item row="0" column="0"><widget class="QLabel" name="labelTarget"><property name="text"><string>当前雷达IP(目标)：</string></property></widget></item>
     <item row="0" column="1"><widget class="QLineEdit" name="lineEditTargetIp"/></item>
     <item row="1" column="0"><widget class="QLabel" name="labelLidar"><property name="text"><string>雷达IP：</string></property></widget></item>
     <item row="1" column="1"><widget class="QLineEdit" name="lineEditLidarIp"/></item>
     <item row="2" column="0"><widget class="QLabel" name="labelPc"><property name="text"><string>主机IP：</string></property></widget></item>
     <item row="2" column="1"><widget class="QLineEdit" name="lineEditPcIp"/></item>
     <item row="3" column="0"><widget class="QLabel" name="labelMac"><property name="text"><string>MAC地址：</string></property></widget></item>
     <item row="3" column="1"><widget class="QLineEdit" name="lineEditMac"/></item>
     <item row="4" column="0"><widget class="QLabel" name="labelPort"><property name="text"><string>端口：</string></property></widget></item>
     <item row="4" column="1"><widget class="QLineEdit" name="lineEditPort"/></item>
    </layout>
   </item>
   <item>
    <layout class="QHBoxLayout" name="buttonLayout">
     <item><widget class="QPushButton" name="pushButtonGet"><property name="text"><string>读取</string></property></widget></item>
     <item><widget class="QPushButton" name="pushButtonSet"><property name="text"><string>设置</string></property></widget></item>
     <item><widget class="QPushButton" name="pushButtonClose"><property name="text"><string>关闭</string></property></widget></item>
    </layout>
   </item>
  </layout>
 </widget>
 <resources/>
 <connections/>
</ui>
```

- [ ] **Step 2: 头文件** `lqSenfotoNetworkConfigDialog.h`

```cpp
// ============================================================
// 功能：Senfoto008 雷达 IP/端口配置对话框。
// 作者：acelan
// 新建时间：2026-10-17
// 修改时间：2026-10-17
// ============================================================

#ifndef lqSenfotoNetworkConfigDialog_h
#define lqSenfotoNetworkConfigDialog_h

#include "lqComponentsModule.h"
#include "lqSenfotoNetworkConfig.h"

#include <QDialog>

namespace Ui
{
class lqSenfotoNetworkConfigDialog;
}

class LQCOMPONENTS_EXPORT lqSenfotoNetworkConfigDialog : public QDialog
{
  Q_OBJECT

public:
  explicit lqSenfotoNetworkConfigDialog(QWidget* parent = nullptr);
  ~lqSenfotoNetworkConfigDialog() override;

private Q_SLOTS:
  void onGet();
  void onSet();
  void onConfigReceived(const lqSenfotoNetworkConfig::DeviceInfo& info);
  void onError(const QString& message);

private:
  Ui::lqSenfotoNetworkConfigDialog* Ui;
  lqSenfotoNetworkConfig* Config;

  Q_DISABLE_COPY(lqSenfotoNetworkConfigDialog)
};

#endif
```

- [ ] **Step 3: 实现** `lqSenfotoNetworkConfigDialog.cxx`（含元类型注册）

```cpp
// ============================================================
// 功能：Senfoto008 雷达 IP/端口配置对话框实现。
// 作者：acelan
// 新建时间：2026-10-17
// 修改时间：2026-10-17
// ============================================================

#include "lqSenfotoNetworkConfigDialog.h"
#include "lqSenfotoNetworkConfig.h"
#include "lqSenfotoPasswordDialog.h"
#include "ui_lqSenfotoNetworkConfigDialog.h"

#include <QHostAddress>
#include <QMessageBox>
#include <QMetaType>
#include <QSettings>

Q_DECLARE_METATYPE(lqSenfotoNetworkConfig::DeviceInfo)

//-----------------------------------------------------------------------------
lqSenfotoNetworkConfigDialog::lqSenfotoNetworkConfigDialog(QWidget* parent)
  : QDialog(parent)
  , Ui(new Ui::lqSenfotoNetworkConfigDialog)
  , Config(new lqSenfotoNetworkConfig(this))
{
  qRegisterMetaType<lqSenfotoNetworkConfig::DeviceInfo>("lqSenfotoNetworkConfig::DeviceInfo");

  this->Ui->setupUi(this);
  this->Ui->lineEditTargetIp->setText(
    QSettings().value("LidarView/SenfotoNetworkConfig/TargetIp", "192.168.1.30").toString());

  QObject::connect(this->Ui->pushButtonGet, &QPushButton::clicked, this, &lqSenfotoNetworkConfigDialog::onGet);
  QObject::connect(this->Ui->pushButtonSet, &QPushButton::clicked, this, &lqSenfotoNetworkConfigDialog::onSet);
  QObject::connect(this->Ui->pushButtonClose, &QPushButton::clicked, this, &QDialog::close);
  QObject::connect(this->Config, &lqSenfotoNetworkConfig::configReceived,
    this, &lqSenfotoNetworkConfigDialog::onConfigReceived);
  QObject::connect(this->Config, &lqSenfotoNetworkConfig::errorOccurred,
    this, &lqSenfotoNetworkConfigDialog::onError);
}

//-----------------------------------------------------------------------------
lqSenfotoNetworkConfigDialog::~lqSenfotoNetworkConfigDialog()
{
  delete this->Ui;
}

//-----------------------------------------------------------------------------
void lqSenfotoNetworkConfigDialog::onGet()
{
  const QString target = this->Ui->lineEditTargetIp->text();
  if (!lqSenfotoNetworkConfig::IsValidIp(target))
  {
    QMessageBox::information(this, tr("错误"), tr("目标雷达IP地址错误"));
    return;
  }
  this->Config->request(QHostAddress(target));
}

//-----------------------------------------------------------------------------
void lqSenfotoNetworkConfigDialog::onSet()
{
  const QString target = this->Ui->lineEditTargetIp->text();
  const QString lidar = this->Ui->lineEditLidarIp->text();
  const QString pc = this->Ui->lineEditPcIp->text();
  const QString mac = this->Ui->lineEditMac->text();
  const int port = this->Ui->lineEditPort->text().toInt();

  if (!lqSenfotoNetworkConfig::IsValidIp(target) || !lqSenfotoNetworkConfig::IsValidIp(lidar) ||
    !lqSenfotoNetworkConfig::IsValidIp(pc) || !lqSenfotoNetworkConfig::IsValidMac(mac) ||
    !lqSenfotoNetworkConfig::IsValidPort(port))
  {
    QMessageBox::information(this, tr("错误"), tr("参数格式错误（IP/MAC/端口）"));
    return;
  }

  const int re = QMessageBox::question(this, tr("问题"), tr("是否更改?"),
    QMessageBox::Ok | QMessageBox::Cancel);
  if (re != QMessageBox::Ok)
  {
    return;
  }

  lqSenfotoPasswordDialog dlg(this);
  if (dlg.exec() != QDialog::Accepted)
  {
    return;
  }

  if (this->Config->apply(QHostAddress(target), lidar, pc, mac, port))
  {
    QSettings().setValue("LidarView/SenfotoNetworkConfig/TargetIp", target);
  }
}

//-----------------------------------------------------------------------------
void lqSenfotoNetworkConfigDialog::onConfigReceived(
  const lqSenfotoNetworkConfig::DeviceInfo& info)
{
  this->Ui->lineEditLidarIp->setText(info.lidarIp);
  this->Ui->lineEditPcIp->setText(info.pcIp);
  this->Ui->lineEditMac->setText(info.mac);
  this->Ui->lineEditPort->setText(QString::number(info.port));
}

//-----------------------------------------------------------------------------
void lqSenfotoNetworkConfigDialog::onError(const QString& message)
{
  QMessageBox::warning(this, tr("错误"), message);
}
```

- [ ] **Step 4: 在 `Qt/Components/CMakeLists.txt` 登记**

`classes` 加 `lqSenfotoNetworkConfigDialog`；`ui_files` 加 `Resources/UI/lqSenfotoNetworkConfigDialog.ui`。

- [ ] **Step 5: 编译**

```bash
cmake --build ../build/superbuild/lidarview/build --target lqComponents
```

Expected: 成功。

- [ ] **Step 6: 提交**

```bash
git add Qt/Components/lqSenfotoNetworkConfigDialog.h Qt/Components/lqSenfotoNetworkConfigDialog.cxx \
        Qt/Components/Resources/UI/lqSenfotoNetworkConfigDialog.ui Qt/Components/CMakeLists.txt
git commit -m "feat(senfoto008): add sensor network-config dialog"
```

---

### Task 5: Reaction + Tools 菜单入口

**Files:**
- Create: `Qt/ApplicationComponents/lqSenfotoNetworkConfigReaction.h` / `.cxx`
- Modify: `Qt/ApplicationComponents/CMakeLists.txt`
- Modify: `Application/Client/LidarViewMainWindow.cxx`
- Modify: `Application/Qt/ApplicationComponents/Resources/interface_modes_config.json`

- [ ] **Step 1: Reaction 头/实现**

`lqSenfotoNetworkConfigReaction.h`：

```cpp
// ============================================================
// 功能：Tools 菜单「雷达 IP 配置」Reaction —— 打开网络配置对话框
//       （仅活动源为 Senfoto008 时可用）。
// 作者：acelan
// 新建时间：2026-10-17
// 修改时间：2026-10-17
// ============================================================

#ifndef lqSenfotoNetworkConfigReaction_h
#define lqSenfotoNetworkConfigReaction_h

#include <pqReaction.h>

class lqSenfotoNetworkConfigReaction : public pqReaction
{
  Q_OBJECT
  typedef pqReaction Superclass;

public:
  explicit lqSenfotoNetworkConfigReaction(QAction* parent);

public slots:
  void updateEnableState() override;

protected:
  void onTriggered() override;

private:
  Q_DISABLE_COPY(lqSenfotoNetworkConfigReaction)
};

#endif
```

`lqSenfotoNetworkConfigReaction.cxx`：

```cpp
// ============================================================
// 功能：Tools 菜单「雷达 IP 配置」Reaction 实现。
// 作者：acelan
// 新建时间：2026-10-17
// 修改时间：2026-10-17
// ============================================================

#include "lqSenfotoNetworkConfigReaction.h"

#include "lqSenfotoNetworkConfigDialog.h"

#include <pqActiveObjects.h>
#include <pqPipelineSource.h>

#include <QAction>

#include <vtkSMProxy.h>

//-----------------------------------------------------------------------------
lqSenfotoNetworkConfigReaction::lqSenfotoNetworkConfigReaction(QAction* parent)
  : Superclass(parent)
{
  QObject::connect(&pqActiveObjects::instance(), &pqActiveObjects::sourceChanged, this,
    &lqSenfotoNetworkConfigReaction::updateEnableState);
  this->updateEnableState();
}

//-----------------------------------------------------------------------------
void lqSenfotoNetworkConfigReaction::updateEnableState()
{
  pqPipelineSource* src = pqActiveObjects::instance().activeSource();
  const bool isSenfoto = src && src->getProxy() &&
    QString(src->getProxy()->GetXMLName()).startsWith("Senfoto008");
  this->parentAction()->setEnabled(isSenfoto);
}

//-----------------------------------------------------------------------------
void lqSenfotoNetworkConfigReaction::onTriggered()
{
  lqSenfotoNetworkConfigDialog dlg(pqCoreUtilities::mainWidget());
  dlg.exec();
}
```

（补 include：`<pqCoreUtilities.h>`。）

- [ ] **Step 2: `Qt/ApplicationComponents/CMakeLists.txt` 登记**

`set(classes ...)` 加 `lqSenfotoNetworkConfigReaction`。

- [ ] **Step 3: Tools 菜单入口**（`Application/Client/LidarViewMainWindow.cxx`，紧接 RPM 的 action 之后）

```cpp
  QAction* actionSenfotoNetworkConfig = new QAction(tr("雷达 IP 配置"), this);
  this->Internals->menuTools->addAction(actionSenfotoNetworkConfig);
  new lqSenfotoNetworkConfigReaction(actionSenfotoNetworkConfig);
```

并 `#include "lqSenfotoNetworkConfigReaction.h"`。

- [ ] **Step 4: 界面模式登记**（`interface_modes_config.json`）

三模式 Tools 菜单白名单（若有）加入 `"actionSenfotoNetworkConfig"`；无则跳过并在提交说明标注。

- [ ] **Step 5: 编译 + 冒烟**

```bash
cmake --build ../build/superbuild/lidarview/build --target SenFoToView
```

验证：开 Senfoto008 源后 Tools →「雷达 IP 配置」可用；开非 Senfoto008 源时灰置；对话框「读取」能回填、「设置」走确认+密码流程。

- [ ] **Step 6: 提交**

```bash
git add Qt/ApplicationComponents/lqSenfotoNetworkConfigReaction.h \
        Qt/ApplicationComponents/lqSenfotoNetworkConfigReaction.cxx \
        Qt/ApplicationComponents/CMakeLists.txt \
        Application/Client/LidarViewMainWindow.cxx \
        Application/Qt/ApplicationComponents/Resources/interface_modes_config.json
git commit -m "feat(senfoto008): add Tools entry to configure sensor IP/port"
```

---

### Task 6: 端到端验证

- [ ] **Step 1: 协议单测**

```bash
ctest --test-dir ../build/superbuild/lidarview/build -R lqSenfotoNetworkConfigTest --output-on-failure
```

Expected: PASS。

- [ ] **Step 2: 完整 install 两步构建**

```bash
ninja -C ../build/superbuild/lidarview/build
ninja -C ../build superbuild/lidarview
```

Expected: 成功。

- [ ] **Step 3: 手工验证（需硬件）**

连接 Senfoto008，改 IP/端口 → 「读取」回填确认；抓包核对报文与 spec §3 一致。

---

## 注意 / 风险

- **`Q_DECLARE_METATYPE` 位置**：`DeviceInfo` 若用于跨线程队列连接需 `qRegisterMetaType`；本设计同线程连接也注册一次（已在 Task 4 Step 3）。若把 `Q_DECLARE_METATYPE` 放进头文件更整洁，实现时可调整。
- **接收端口占用**：`bind(8091, ShareAddress)` 可能与他进程冲突；构造函数忽略 bind 失败，运行时 `errorOccurred` 提示。
- **目标地址来源**：对话框内「当前雷达IP（目标）」由用户输入，默认 `QSettings` 上次值（`192.168.1.30`）。
- **仅 Senfoto008**：菜单 action 依活动源判定，无 Senfoto008 源时禁用。
