// ============================================================
// 功能：Senfoto008 雷达网络配置协议单元测试（无需硬件）。
// 作者：acelan
// 新建时间：2026-10-17
// 修改时间：2026-10-17
// ============================================================

#include "lqSenfotoNetworkConfig.h"

#include <QByteArray>
#include <iostream>

int main()
{
  const QByteArray pkt = lqSenfotoNetworkConfig::BuildSetPacket(
    "192.168.1.30", "192.168.1.105", "00-0a-35-01-fe-c0", 8091);
  if (pkt.size() != 18)
  {
    std::cerr << "size=" << pkt.size() << "\n";
    return 1;
  }
  if (static_cast<unsigned char>(pkt[0]) != 0xfc || static_cast<unsigned char>(pkt[1]) != 0x60)
  {
    std::cerr << "cmd\n";
    return 1;
  }
  const unsigned char expIp[4] = { 192, 168, 1, 30 };
  for (int i = 0; i < 4; ++i)
  {
    if (static_cast<unsigned char>(pkt[2 + i]) != expIp[i])
    {
      std::cerr << "lidarIp\n";
      return 1;
    }
  }
  const unsigned char expPc[4] = { 192, 168, 1, 105 };
  for (int i = 0; i < 4; ++i)
  {
    if (static_cast<unsigned char>(pkt[6 + i]) != expPc[i])
    {
      std::cerr << "pcIp\n";
      return 1;
    }
  }
  const unsigned char expMac[6] = { 0x00, 0x0a, 0x35, 0x01, 0xfe, 0xc0 };
  for (int i = 0; i < 6; ++i)
  {
    if (static_cast<unsigned char>(pkt[10 + i]) != expMac[i])
    {
      std::cerr << "mac\n";
      return 1;
    }
  }
  if (static_cast<unsigned char>(pkt[16]) != (8091 % 256) ||
    static_cast<unsigned char>(pkt[17]) != (8091 / 256))
  {
    std::cerr << "port\n";
    return 1;
  }

  QByteArray resp;
  resp.append(static_cast<char>(0xfc)).append(static_cast<char>(0x62)).append(static_cast<char>(0x00)).append(static_cast<char>(0x02));
  for (int i = 0; i < 30; ++i)
  {
    resp.append('\0');
  }
  resp[4 + 6] = static_cast<char>(10);
  resp[4 + 7] = static_cast<char>(0);
  resp[4 + 8] = static_cast<char>(0);
  resp[4 + 9] = static_cast<char>(5);
  resp[4 + 10] = static_cast<char>(192);
  resp[4 + 11] = static_cast<char>(168);
  resp[4 + 12] = static_cast<char>(1);
  resp[4 + 13] = static_cast<char>(2);
  resp[4 + 14] = static_cast<char>(0xaa);
  resp[4 + 15] = static_cast<char>(0xbb);
  resp[4 + 16] = static_cast<char>(0xcc);
  resp[4 + 17] = static_cast<char>(0xdd);
  resp[4 + 18] = static_cast<char>(0xee);
  resp[4 + 19] = static_cast<char>(0xff);
  resp[4 + 20] = static_cast<char>(8091 % 256);
  resp[4 + 21] = static_cast<char>(8091 / 256);

  lqSenfotoNetworkConfig::DeviceInfo info;
  if (!lqSenfotoNetworkConfig::ParseResponse(resp, info))
  {
    std::cerr << "parse\n";
    return 1;
  }
  if (info.lidarIp != "10.0.0.5" || info.pcIp != "192.168.1.2" ||
    info.mac != "aa-bb-cc-dd-ee-ff" || info.port != 8091)
  {
    std::cerr << "fields\n";
    return 1;
  }

  if (!lqSenfotoNetworkConfig::IsValidIp("192.168.1.30"))
  {
    std::cerr << "ipv\n";
    return 1;
  }
  if (lqSenfotoNetworkConfig::IsValidIp("999.1.1.1"))
  {
    std::cerr << "ipv2\n";
    return 1;
  }
  if (!lqSenfotoNetworkConfig::IsValidMac("00-0a-35-01-fe-c0"))
  {
    std::cerr << "macv\n";
    return 1;
  }
  if (lqSenfotoNetworkConfig::IsValidPort(80))
  {
    std::cerr << "portv\n";
    return 1;
  }
  if (!lqSenfotoNetworkConfig::IsValidPort(8091))
  {
    std::cerr << "portv2\n";
    return 1;
  }

  std::cout << "PASS\n";
  return 0;
}
