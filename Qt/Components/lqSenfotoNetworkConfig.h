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
#include <QMetaType>
#include <QObject>
#include <QString>

class QHostAddress;
class QUdpSocket;

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

  explicit lqSenfotoNetworkConfig(QObject* parent = nullptr);
  ~lqSenfotoNetworkConfig() override;

  static QByteArray BuildSetPacket(
    const QString& lidarIp, const QString& pcIp, const QString& mac, int port);
  static bool ParseResponse(const QByteArray& datagram, DeviceInfo& out);
  static bool IsValidIp(const QString& ip);
  static bool IsValidMac(const QString& mac);
  static bool IsValidPort(int port);

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

Q_DECLARE_METATYPE(lqSenfotoNetworkConfig::DeviceInfo)

#endif
