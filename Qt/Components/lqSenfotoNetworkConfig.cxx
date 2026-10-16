// ============================================================
// 功能：Senfoto008 雷达网络配置私有协议实现。
// 作者：acelan
// 新建时间：2026-10-17
// 修改时间：2026-10-17
// ============================================================

#include "lqSenfotoNetworkConfig.h"

#include <QRegularExpression>
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
    const int value = parts[i].toInt(&ok);
    if (!ok || value < 0 || value > 255)
    {
      return false;
    }
    out[i] = static_cast<unsigned char>(value);
  }
  return true;
}

QString BytesToIp(const unsigned char* bytes)
{
  return QString("%1.%2.%3.%4").arg(bytes[0]).arg(bytes[1]).arg(bytes[2]).arg(bytes[3]);
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
    const uint value = parts[i].toUInt(&ok, 16);
    if (!ok || value > 0xFF)
    {
      return false;
    }
    out[i] = static_cast<unsigned char>(value);
  }
  return true;
}

QString BytesToMac(const unsigned char* bytes)
{
  QString mac;
  for (int i = 0; i < 6; ++i)
  {
    mac += QString("%1").arg(bytes[i], 2, 16, QChar('0'));
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
  QObject::connect(
    this->Socket, &QUdpSocket::readyRead, this, &lqSenfotoNetworkConfig::onReadyRead);
}

//-----------------------------------------------------------------------------
lqSenfotoNetworkConfig::~lqSenfotoNetworkConfig() = default;

//-----------------------------------------------------------------------------
bool lqSenfotoNetworkConfig::IsValidIp(const QString& ip)
{
  static const QRegularExpression rx(QRegularExpression::anchoredPattern(
    "((2(5[0-5]|[0-4]\\d))|[0-1]?\\d{1,2})(\\.((2(5[0-5]|[0-4]\\d))|[0-1]?\\d{1,2})){3}"));
  return rx.match(ip).hasMatch();
}

//-----------------------------------------------------------------------------
bool lqSenfotoNetworkConfig::IsValidMac(const QString& mac)
{
  static const QRegularExpression rx(QRegularExpression::anchoredPattern(
    "([0-9a-fA-F]{2}-[0-9a-fA-F]{2}-[0-9a-fA-F]{2}-[0-9a-fA-F]{2}-[0-9a-fA-F]{2}-[0-9a-fA-F]{2})"));
  return rx.match(mac).hasMatch();
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

  QByteArray packet(18, '\0');
  packet[0] = static_cast<char>(0xfc);
  packet[1] = static_cast<char>(0x60);
  for (int i = 0; i < 4; ++i)
  {
    packet[2 + i] = static_cast<char>(lidar[i]);
    packet[6 + i] = static_cast<char>(pc[i]);
  }
  for (int i = 0; i < 6; ++i)
  {
    packet[10 + i] = static_cast<char>(macBytes[i]);
  }
  packet[16] = static_cast<char>(port % 256);
  packet[17] = static_cast<char>(port / 256);
  return packet;
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
  const unsigned char* payload =
    reinterpret_cast<const unsigned char*>(datagram.constData()) + 4;
  out.lidarIp = BytesToIp(payload + 6);
  out.pcIp = BytesToIp(payload + 10);
  out.mac = BytesToMac(payload + 14);
  out.port = static_cast<int>(payload[20]) | (static_cast<int>(payload[21]) << 8);
  return true;
}

//-----------------------------------------------------------------------------
bool lqSenfotoNetworkConfig::request(const QHostAddress& target, int port)
{
  const char cmd[4] = { static_cast<char>(0xfc), 0x62, 0x00, 0x01 };
  const qint64 written = this->Socket->writeDatagram(cmd, 4, target, static_cast<quint16>(port));
  if (written < 0)
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
  const QByteArray packet = BuildSetPacket(lidarIp, pcIp, mac, port);
  if (packet.isEmpty())
  {
    Q_EMIT this->errorOccurred(tr("参数格式错误"));
    return false;
  }
  const qint64 written =
    this->Socket->writeDatagram(packet, target, static_cast<quint16>(targetPort));
  if (written < 0)
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
