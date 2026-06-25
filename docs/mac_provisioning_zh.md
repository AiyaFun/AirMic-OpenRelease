# AirMic Mac 助手配网

可以把 ESP32 配网放到 Mac 助手里做，但最稳的轻量方案不是让 Mac 自动切 Wi-Fi，而是：

1. 用户先手动连接 ESP32 的临时 Wi-Fi：`AirMic-Setup-xxxx`。
2. 双击 `AirMic_配网.command`。
3. Mac 助手向 `http://192.168.4.1/save` 提交 Wi-Fi 名称、密码和当前按键配置。
4. ESP32 保存后重启，连接目标 Wi-Fi。
5. Mac 切回正常 Wi-Fi，AirMic 助手通过 UDP ready 广播让 ESP32 自动发现 Mac。

## 为什么不完全自动切 Wi-Fi

macOS 对 Wi-Fi、钥匙串、网络切换有权限限制。开发阶段强行自动化会带来这些问题：

- 不同 Mac 的 Wi-Fi 设备名可能不是固定的。
- 自动加入开放 AP 可能被系统拦截或提示。
- 读取当前 Wi-Fi 密码需要钥匙串权限，不适合作为轻量助手默认行为。
- 自动切回原 Wi-Fi 的可靠性不如用户手动确认。

所以当前产品化建议是：

- 轻量助手负责填写和提交配置。
- 用户负责在 macOS Wi-Fi 菜单中选择 `AirMic-Setup-xxxx` 和切回正常 Wi-Fi。

## 使用入口

项目根目录双击：

```text
AirMic_配网.command
```

## 配网前提

ESP32 需要处在 setup mode：

- 第一次刷固件后，没有保存 Wi-Fi 时会自动进入。
- 或者开机时按住 Key 2 / GPIO8 约 3 秒，清空 Wi-Fi 并进入 setup mode。

Mac 需要先连接：

```text
AirMic-Setup-xxxx
```

然后运行配网入口。

## 提交内容

配网工具会提交：

- Wi-Fi SSID
- Wi-Fi password
- Key 1 配置
- Key 2 配置
- Key 3 配置

按键配置来自：

```text
~/Library/Application Support/AirMic/config.json
```

如果没有配置文件，默认是：

```text
key1=BACKSPACE
key2=RIGHT_OPTION
key3=ENTER
```
