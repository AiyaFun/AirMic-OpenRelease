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

## Mac 端验证（摘要）

- 安装 `BlackHole 2ch`
- 打开 `AirMic-Setup.command` 并完成配网
- 蓝牙连接 `AirMic Keyboard`
- 在听写设置里选择 `BlackHole 2ch`

## 常见问题

见 `docs/troubleshooting.md`。
