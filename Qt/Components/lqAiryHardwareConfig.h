// ============================================================
// 功能：Airy（SF-Q96A）雷达硬件参数 HTTP 协议封装：
//       读取 GET /setting_data.json，写入 POST /Parameter_Setting.html。
// 作者：acelan
// 新建时间：2026-10-20
// 修改时间：2026-10-20
// ============================================================

#ifndef lqAiryHardwareConfig_h
#define lqAiryHardwareConfig_h

#include "lqComponentsModule.h"

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantMap>

class LQCOMPONENTS_EXPORT lqAiryHardwareConfig : public QObject
{
  Q_OBJECT

public:
  struct DeviceInfo
  {
    QString serial;
    QString mac;
    QString mainboardFw;
    QString baseboardFw;
    QString appFw;
    QString motorFw;
    QString cfgVersion;
    QString model;
    int lines = 0;
  };

  explicit lqAiryHardwareConfig(QObject* parent = nullptr);
  ~lqAiryHardwareConfig() override = default;

  static QUrl SettingsUrl(const QString& lidarIp);
  static QUrl SaveUrl(const QString& lidarIp);
  static QUrl DeviceDataUrl(const QString& lidarIp);

  /// Listen on the DIFOP port for one packet and parse device information.
  bool fetchDeviceInfo(const QString& lidarIp, DeviceInfo& info, QString& error);

  /// GET /setting_data.json and return all parameters.
  bool fetch(const QString& lidarIp, QVariantMap& values, QString& error);
  /// Merge the given values over the current device values and POST them.
  bool apply(const QString& lidarIp, const QVariantMap& values, QString& error);

private:
  Q_DISABLE_COPY(lqAiryHardwareConfig)
};

#endif
