// ============================================================
// 功能：Airy（SF-Q96A）雷达硬件参数 HTTP 协议实现。
// 作者：acelan
// 新建时间：2026-10-20
// 修改时间：2026-10-20
// ============================================================

#include "lqAiryHardwareConfig.h"

#include <QElapsedTimer>
#include <QDebug>
#include <QEventLoop>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QNetworkProxyQuery>
#include <QNetworkRequest>
#include <QStringList>
#include <QUdpSocket>
#include <QUrlQuery>

#include <cstring>

namespace
{
QString DescribeNetworkError(QNetworkReply::NetworkError code, const QString& fallback)
{
  switch (code)
  {
    case QNetworkReply::ConnectionRefusedError:
      return QStringLiteral("Connection refused (nothing listening on that address/port)");
    case QNetworkReply::RemoteHostClosedError:
      return QStringLiteral("Connection closed by the device");
    case QNetworkReply::HostNotFoundError:
      return QStringLiteral("Host not found (wrong IP?)");
    case QNetworkReply::TimeoutError:
      return QStringLiteral("Network timeout");
    case QNetworkReply::OperationCanceledError:
      return QStringLiteral("Operation canceled");
    case QNetworkReply::NetworkSessionFailedError:
    case QNetworkReply::TemporaryNetworkFailureError:
      return QStringLiteral("Network unreachable (no route to that subnet?)");
    case QNetworkReply::ProxyConnectionRefusedError:
    case QNetworkReply::ProxyConnectionClosedError:
    case QNetworkReply::ProxyNotFoundError:
    case QNetworkReply::ProxyTimeoutError:
      return QStringLiteral("Proxy error (check system proxy settings)");
    default:
      return fallback;
  }
}

bool ExecRequest(QNetworkAccessManager& nam, const QNetworkRequest& request,
  const QByteArray& body, QByteArray& output, QString& error)
{
  const QList<QNetworkProxy> systemProxies =
    QNetworkProxyFactory::systemProxyForQuery(QNetworkProxyQuery(request.url()));
  qDebug() << "[AiryHTTP] sending" << (body.isEmpty() ? "GET" : "POST")
           << request.url().toString() << "body" << body.size()
           << "systemProxy(type/host/port)" << (systemProxies.isEmpty() ? -1 : int(systemProxies[0].type()))
           << (systemProxies.isEmpty() ? QString() : systemProxies[0].hostName())
           << (systemProxies.isEmpty() ? -1 : systemProxies[0].port());

  QEventLoop loop;
  QNetworkReply* reply = body.isEmpty() ? nam.get(request) : nam.post(request, body);
  QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

  loop.exec();

  const QNetworkReply::NetworkError replyError = reply->error();
  output = reply->readAll();
  if (replyError != QNetworkReply::NoError)
  {
    error = DescribeNetworkError(replyError, reply->errorString());
  }
  const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  qDebug() << "[AiryHTTP] reply" << (body.isEmpty() ? "GET" : "POST") << request.url().toString()
           << "httpStatus" << httpStatus
           << "bytes" << output.size()
           << "errorCode" << int(replyError)
           << "error" << reply->errorString();
  if (replyError != QNetworkReply::NoError || (httpStatus != 0 && httpStatus != 200))
  {
    qDebug() << "[AiryHTTP] body-snippet" << output.left(200);
  }
  if (replyError != QNetworkReply::NoError)
  {
    reply->deleteLater();
    return false;
  }
  reply->deleteLater();
  return true;
}
}

//-----------------------------------------------------------------------------
lqAiryHardwareConfig::lqAiryHardwareConfig(QObject* parent)
  : QObject(parent)
{
}

//-----------------------------------------------------------------------------
QUrl lqAiryHardwareConfig::SettingsUrl(const QString& lidarIp)
{
  return QUrl(QStringLiteral("http://%1/setting_data.json").arg(lidarIp));
}

//-----------------------------------------------------------------------------
QUrl lqAiryHardwareConfig::DeviceDataUrl(const QString& lidarIp)
{
  return QUrl(QStringLiteral("http://%1/device_data.json").arg(lidarIp));
}

QUrl lqAiryHardwareConfig::SaveUrl(const QString& lidarIp)
{
  return QUrl(QStringLiteral("http://%1/Parameter_Setting.html").arg(lidarIp));
}

//-----------------------------------------------------------------------------
bool lqAiryHardwareConfig::fetch(const QString& lidarIp, QVariantMap& values, QString& error)
{
  qDebug() << "[AiryHTTP]" << QStringLiteral("GET %1").arg(SettingsUrl(lidarIp).toString());
  QNetworkAccessManager nam;
  // Talk to the LiDAR directly: never route LAN traffic through a system proxy.
  nam.setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
  const QNetworkRequest request(SettingsUrl(lidarIp));
  QByteArray body;
  if (!ExecRequest(nam, request, QByteArray(), body, error))
  {
    qDebug() << "[AiryHTTP]" << QStringLiteral("  FAILED: %1").arg(error);
    return false;
  }
  qDebug() << "[AiryHTTP]" << QStringLiteral("  OK (%1 bytes)").arg(body.size());
  const QJsonDocument document = QJsonDocument::fromJson(body);
  if (!document.isObject())
  {
    error = QStringLiteral("Unexpected response from /setting_data.json");
    return false;
  }
  values = document.object().toVariantMap();
  return true;
}

//-----------------------------------------------------------------------------
bool lqAiryHardwareConfig::apply(const QString& lidarIp, const QVariantMap& values, QString& error)
{
  // Submit the full parameter set: start from the device's current values so
  // every unspecified field keeps its setting.
  QVariantMap parameters;
  if (!this->fetch(lidarIp, parameters, error))
  {
    return false;
  }
  for (auto it = values.constBegin(); it != values.constEnd(); ++it)
  {
    parameters.insert(it.key(), it.value());
  }

  // Replicate the browser's "General Setting" form submission exactly: fixed
  // field order plus the hidden/button fields (tabs-two, save_param, mode2).
  static const char* const kFields[] = {
    "tabs-two", "SrcIp", "SrcMask", "SrcGateWay", "DstIp", "MPort", "DPort", "ReMode", "FovS",
    "FovE", "SynSrc", "PDomNum", "RTPdelay", "NoLeap", "SyTOut", "UToL", "LToU", "OpM",
    "PhaLockSet", "GPSBRate", "ImuCtrl", "IPort", "ImuOutR", "AccRg", "GyroRg", "Lpf",
    "ResDefault", "save_param", "RefEn", "TFilLvl", "BlkRnDis", "Rn", "Blk", "FSAgl",
    "DZ10cm", "C81858993", "GapFill", "RanM", "RnFgSwt", "L48", "mode2"
  };
  QUrlQuery query;
  for (const char* field : kFields)
  {
    const QString key = QLatin1String(field);
    QString value;
    if (key == QLatin1String("tabs-two"))
    {
      value = QStringLiteral("on");
    }
    else if (key == QLatin1String("save_param"))
    {
      value = QStringLiteral("Save");
    }
    else if (key == QLatin1String("mode2"))
    {
      value = QStringLiteral("Mode2");
    }
    else
    {
      value = parameters.value(key).toString();
    }
    if (!value.isEmpty())
    {
      query.addQueryItem(key, value);
    }
  }

  QNetworkAccessManager nam;
  // Talk to the LiDAR directly: never route LAN traffic through a system proxy.
  nam.setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
  QNetworkRequest request(SaveUrl(lidarIp));

  // Match the browser request observed in the capture exactly.
  request.setHeader(QNetworkRequest::ContentTypeHeader,
    QStringLiteral("application/x-www-form-urlencoded"));
  request.setRawHeader("User-Agent",
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
    "Chrome/155.0.0.0 Safari/537.36 Edg/155.0.0.0");
  request.setRawHeader("Accept",
    "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,"
    "image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7");
  request.setRawHeader("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6");
  request.setRawHeader("Accept-Encoding", "gzip, deflate");
  request.setRawHeader("Origin", QStringLiteral("http://%1").arg(lidarIp).toUtf8());
  request.setRawHeader("Referer",
    QStringLiteral("http://%1/Parameter_Setting.html").arg(lidarIp).toUtf8());
  request.setRawHeader("Upgrade-Insecure-Requests", "1");
  request.setRawHeader("Cache-Control", "max-age=0");
  request.setRawHeader("Connection", "keep-alive");
  qDebug() << "[AiryHTTP]" << 
    QStringLiteral("POST %1 (%2 fields)").arg(SaveUrl(lidarIp).toString()).arg(query.queryItems().size());
  QByteArray response;
  const bool posted =
    ExecRequest(nam, request, query.toString(QUrl::FullyEncoded).toLatin1(), response, error);
  qDebug() << "[AiryHTTP]"
           << (posted ? QStringLiteral("  replied (%1 bytes)").arg(response.size())
                      : QStringLiteral("  no reply: %1").arg(error));
  return posted;
}

namespace
{
QString PrintableOrHex(const unsigned char* data, int length)
{
  bool printable = true;
  for (int i = 0; i < length; ++i)
  {
    const unsigned char c = data[i];
    if (c != 0 && (c < 0x20 || c > 0x7E))
    {
      printable = false;
      break;
    }
  }
  if (!printable)
  {
    QString hex;
    for (int i = 0; i < length; ++i)
    {
      hex += QStringLiteral("%1").arg(data[i], 2, 16, QLatin1Char('0')).toUpper();
    }
    return hex;
  }
  int count = 0;
  while (count < length && data[count] != 0)
  {
    ++count;
  }
  return QString::fromLatin1(reinterpret_cast<const char*>(data), count).trimmed();
}
}

//-----------------------------------------------------------------------------
bool lqAiryHardwareConfig::fetchDeviceInfo(
  const QString& lidarIp, DeviceInfo& info, QString& error)
{
  QNetworkAccessManager nam;
  // Talk to the LiDAR directly: never route LAN traffic through a system proxy.
  nam.setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
  const QNetworkRequest request(DeviceDataUrl(lidarIp));
  QByteArray body;
  if (!ExecRequest(nam, request, QByteArray(), body, error))
  {
    qDebug() << "[AiryHTTP]" << QStringLiteral("  FAILED: %1").arg(error);
    return false;
  }
  qDebug() << "[AiryHTTP]" << QStringLiteral("  OK (%1 bytes)").arg(body.size());
  const QJsonDocument document = QJsonDocument::fromJson(body);
  if (!document.isObject())
  {
    error = QStringLiteral("Unexpected response from /device_data.json");
    return false;
  }
  const QJsonObject object = document.object();
  info.mainboardFw = object.value("top_v").toString();
  info.baseboardFw = object.value("bot_v").toString();
  info.appFw = object.value("bot_soft_v").toString();
  info.motorFw = object.value("motor_v").toString();
  info.cfgVersion = object.value("cfg_v").toString();
  info.serial = object.value("sn").toString();
  info.mac = object.value("mac_addr").toString();
  info.model = object.value("lidar_model").toString();
  info.lines = info.model.contains("Airy", Qt::CaseInsensitive) ? 96 : 0;
  return true;
}
