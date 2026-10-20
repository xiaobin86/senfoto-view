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
#include <QNetworkRequest>
#include <QStringList>
#include <QUdpSocket>
#include <QUrlQuery>

#include <cstring>

namespace
{
bool ExecRequest(QNetworkAccessManager& nam, const QNetworkRequest& request,
  const QByteArray& body, QByteArray& output, QString& error)
{
  qDebug() << "[AiryHTTP] sending" << (body.isEmpty() ? "GET" : "POST")
           << request.url().toString() << "body" << body.size();

  QEventLoop loop;
  QNetworkReply* reply = body.isEmpty() ? nam.get(request) : nam.post(request, body);
  QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

  // QNetworkReply has no default timeout; abort unresponsive requests.
  QTimer timer;
  timer.setSingleShot(true);
  QObject::connect(&timer, &QTimer::timeout, reply, &QNetworkReply::abort);
  timer.start(8000);

  loop.exec();
  timer.stop();

  const QNetworkReply::NetworkError replyError = reply->error();
  output = reply->readAll();
  qDebug() << "[AiryHTTP]" << (body.isEmpty() ? "GET" : "POST") << request.url().toString()
           << "status" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()
           << "bytes" << output.size()
           << (replyError == QNetworkReply::NoError ? "" : reply->errorString().toUtf8().constData());
  if (replyError != QNetworkReply::NoError)
  {
    error = reply->errorString();
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
  QNetworkAccessManager nam;
  const QNetworkRequest request(SettingsUrl(lidarIp));
  QByteArray body;
  if (!ExecRequest(nam, request, QByteArray(), body, error))
  {
    return false;
  }
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

  QUrlQuery query;
  for (auto it = parameters.constBegin(); it != parameters.constEnd(); ++it)
  {
    query.addQueryItem(it.key(), it.value().toString());
  }
  query.addQueryItem(QStringLiteral("save_param"), QStringLiteral("Save"));

  QNetworkAccessManager nam;
  QNetworkRequest request(SaveUrl(lidarIp));
  request.setHeader(QNetworkRequest::ContentTypeHeader,
    QStringLiteral("application/x-www-form-urlencoded"));
  QByteArray response;
  return ExecRequest(
    nam, request, query.toString(QUrl::FullyEncoded).toLatin1(), response, error);
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
  const QNetworkRequest request(DeviceDataUrl(lidarIp));
  QByteArray body;
  if (!ExecRequest(nam, request, QByteArray(), body, error))
  {
    return false;
  }
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
