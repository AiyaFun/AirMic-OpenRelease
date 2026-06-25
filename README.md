# AirMic Keypad 固件公开发布（Wireless V1）

**版本**: wr1.0.1
**日期**: 2026-06-26

本仓库仅发布可运行文件与使用说明，不包含源码。

## 目录

- `firmware/airmic_wr1.0.1_esp32s3_wifi_mic.bin`：主固件（合并镜像）
- `firmware/bootloader.bin`
- `firmware/partitions.bin`
- `firmware/firmware.elf`
- `docs/`：使用说明和排障文档

## 烧录说明（推荐）

1. 安装 `arduino-cli`：
   - `brew install arduino-cli`
2. 连接 ESP32-S3 且确认端口（例如 `/dev/cu.usbmodem101`）。
3. 用 `arduino-cli` 指定分区/bootloader 写入：

```bash
esptool.py --chip esp32s3 --port /dev/cu.usbmodem101 --baud 921600 write_flash -z \
  0x1000 firmware/bootloader.bin \
  0x8000 firmware/partitions.bin \
  0x10000 firmware/airmic_wr1.0.1_esp32s3_wifi_mic.bin
```

> 不同开发板可能起始地址有差异，按开发板工具链要求微调。

你也可以用 Arduino IDE 通过 `Firmware` 菜单上传 `airmic_wr1.0.1_esp32s3_wifi_mic.bin` 对应的项目源码（不建议公开源码）。

## 购买后首次刷机（给普通用户）

如果你是第一次拿到设备，请按下面顺序做：

1. 先确认设备通过 USB 连接到 Mac。
2. 在系统串口里确认设备端口（形如 `/dev/cu.usbmodemxxxx`）。
3. 按上述 `esptool.py` 命令刷写 `bootloader.bin`、`partitions.bin` 与 `airmic_wr1.0.1_esp32s3_wifi_mic.bin`。
4. 重新上电，等待出现 `Setup AP: AirMic-Setup-xxxx`。
5. 用手机连接 Wi-Fi 热点 `AirMic-Setup-xxxx`，打开 `http://192.168.4.1` 完成配网。
6. 回到 Mac，安装并运行 `AirMic` 轻量助手，选择 `BlackHole 2ch` 验证听写。

建议第一次刷完立即保存一次设备端口和日志截图，后续遇到问题可直接反馈给支持。

## 二次编译与自定义键位（高级用户）

如果你有固件源码（本发布只放二进制，不包含源码），可以按以下方式自定义键位后自行编译：

1. 获取源码（开发者分支或源码包）。
2. 编辑 `airmic_config.h` 中的按键映射参数（当前主线用 GPIO7/8/9）。
3. 用与你当前硬件一致的板型编译：

```bash
arduino-cli compile --fqbn esp32:esp32:esp32s3 firmware/esp32_s3_wifi_mic/esp32_s3_wifi_mic.ino
```

4. 按本说明中的 `烧录说明` 刷写生成的 `*.bin`。
5. 烧写后打开串口确认 `BLE keyboard advertising: AirMic Keyboard`，再做配网与按键测试。

> 提示：如仅改键位，建议只验证 3 个默认动作（Backspace/Right Option/Enter），确保 BLE 发布链路正常。

## Mac 端验证（摘要）

- 安装 `BlackHole 2ch`
- 打开 `AirMic-Setup.command` 并完成配网
- 蓝牙连接 `AirMic Keyboard`
- 在听写设置里选择 `BlackHole 2ch`

## 常见问题

见 `docs/troubleshooting.md`。
