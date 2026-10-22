<!--
  功能：记录 Airy(SF-Q96A) 雷达 Web API 在 Windows / Linux 上的差异问题
        （Qt 头名小写 vs lwIP 大小写敏感），含定位过程、证据与正确做法。
  作者：acelan
  新建时间：2026-10-22
  修改时间：2026-10-22
-->

# Airy (SF-Q96A) HTTP 配置：Windows / Linux 差异问题记录

## 1. 问题现象

同一个上位机程序（同一份代码）：

| 平台 | 结果 |
|---|---|
| Ubuntu | 修改回波模式 / IP **成功** |
| Windows | POST 后设备不生效，回读仍是旧值 |

设备侧表现：`POST /Parameter_Setting.html` **没有 HTTP 响应**，客户端收到
`RemoteHostClosedError`（"连接已关闭"）。而 `GET /setting_data.json`、
`GET /device_data.json` 在两个平台都正常（200）。

## 2. 定位过程

1. **浏览器抓包对比**（Windows `261021.pcap` / Ubuntu `200to202.pcap`）：
   两者对 `POST /Parameter_Setting.html` 的请求**完全一致**（同一端点、
   `Content-Type: application/x-www-form-urlencoded`、同样 433 字节 body、
   同样字段顺序）。→ 排除"浏览器平台差异"，也说明端点正确。
2. **上位机自身抓包**（Windows，`test11111.pcap`，过滤 `tcp port 80`）：
   抓到上位机实际发出的 POST，发现 **所有 header 名都是小写**：

   ```
   POST /Parameter_Setting.html HTTP/1.1
   host: 192.168.1.202
   content-type: application/x-www-form-urlencoded
   content-length: 433
   ...
   ```

   而浏览器发的是首字母大写：

   ```
   Host: 192.168.1.200
   Content-Type: application/x-www-form-urlencoded
   Content-Length: 433
   ```

## 3. 根因

- **Qt (`QNetworkAccessManager`) 在 Windows 上会把自定义 header 名规范化成小写**；
  Linux 上则保留设定的大小写。
- 设备的 HTTP 服务是 **lwIP httpd**（响应头为 `HTTP/1.0`，实现非常精简），
  对 header 名**大小写敏感**：认不出小写的 `content-length` / `content-type`
  → 不读取/不解析 body → **直接关闭连接、配置不生效**。
- `GET` 请求没有 body，不依赖 `Content-Length`，所以小写头不影响 → 表现为
  "能刷新、不能保存"，且只有 Windows 出问题。

## 4. 解决方式（已实现）

保存请求不再使用 `QNetworkAccessManager`，改为 **`QTcpSocket` 直接发送原始报文**，
header 名严格按浏览器大小写：

```
POST /Parameter_Setting.html HTTP/1.1
Host: <雷达IP>
Content-Type: application/x-www-form-urlencoded
Content-Length: <n>
User-Agent: Mozilla/5.0 ...
Accept: text/html,...
Accept-Language: zh-CN,...
Accept-Encoding: gzip, deflate
Origin: http://<雷达IP>
Referer: http://<雷达IP>/Parameter_Setting.html
Upgrade-Insecure-Requests: 1
Cache-Control: max-age=0
Connection: keep-alive

<urlencoded body>
```

要点：
1. header 名**首字母大写**、顺序与浏览器一致；
2. body 为浏览器的 **41 个字段固定顺序**（含 `tabs-two=on`、`save_param=Save`、`mode2=Mode2`）；
3. 发送后**保持约 1s 再 `disconnectFromHost()`**（优雅 FIN，避免 Windows 上销毁
   `QNetworkAccessManager` 造成的 RST 让 lwIP 丢弃未处理完的请求）；
4. 设备**从不回包**（浏览器亦然），故用「1s 后回读 `setting_data.json` 比对」判定成功。

## 5. 其他已确认的事实（避免再次踩坑）

| 事项 | 结论 |
|---|---|
| `POST /Parameter_Setting.html` | **就是正确端点**；设备 apply 后直接断开、不返回响应 |
| `/cgi-bin/param_setting.cgi` | **不存在**（GET/POST 均 404）。来源只是 HTML 第一行的 `saved from url=(...)cgi-bin/param_setting.cgi` 注释，属误导 |
| 设备 HTTP 版本 | 响应为 `HTTP/1.0` |
| `setting_data.json` vs `device_data.json` | 前者=可写参数（表单/保存用），后者=只读身份（SN/MAC/固件/型号） |
| 保存 body 必须包含 | `tabs-two`、`save_param=Save`、`mode2=Mode2`；多发额外字段（`MVEn/DVEn/PVEn`、`RotaSpe`、`MotorR`、`MSideEn`、`msop_vlan` 等）会被设备拒绝 |
| 通用页范围 | 只含 41 字段；`RotaSpe/MotorR/MSideEn/VLAN` 等属于其它 Tab（对应 `save_param_performance` / `save_param_pulse`） |
| 系统代理 | Windows 上系统代理/梯子会劫持 `192.168.x` 请求 → 必须 `QNetworkProxy::NoProxy` |
| 改 IP 后 | 设备立刻切网并断开；验证需对新 IP 做**短超时探测**（2s）再回读，避免卡系统 TCP 超时 |

## 6. 后续约束

- **任何带 body 的请求**（其它 Tab 的保存：`save_param_performance` /
  `save_param_pulse`）都必须沿用**裸 TCP + 首字母大写头**，不得使用
  `QNetworkAccessManager::post()`，否则会在 Windows 上重现本问题。
- GET 类请求可继续使用 `QNetworkAccessManager`（当前工作正常）。
