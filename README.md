# AirMic 纯蓝牙版固件公开发布（WR101/WROOM-32）

**版本**: wr1.0.1-classic-combo-kbd3  
**日期**: 2026-06-26

本仓库仅发布可运行文件与使用说明，不包含源码。

**注意：此版本为纯蓝牙方向，不含 Wi‑Fi/Mac 辅助脚本。**

## 目录

- `firmware/airmic_wr1.0.1-esp32wroom32-bluetooth.bin`：主固件（合并镜像）
- `firmware/bootloader.bin`
- `firmware/partitions.bin`
- `firmware/firmware.elf`
- `firmware/airmic_wr1.0.1-esp32wroom32-bluetooth.ino`：仅作公开参考（非完整工程）
- `docs/airmic_wr104_user_guide.md`：配对/上手/故障
- `docs/wroom32_ble_all_bluetooth.md`：全蓝牙方向说明
- `docs/custom_key_mapping.md`：按键映射说明

## 刷机说明（推荐）

1. 安装 `arduino-cli`：
   - `brew install arduino-cli`
2. 确认板子为 ESP32-WROOM-32/DA，连接 USB
3. 识别端口（例如 `/dev/cu.usbserial-0001` 或 `/dev/cu.usbmodemxxxx`）
4. 烧录（推荐 `esptool.py`）：

```bash
esptool.py --chip esp32 --port /dev/cu.usbserial-0001 --baud 921600 write_flash -z \
  0x1000 firmware/bootloader.bin \
  0x8000 firmware/partitions.bin \
  0x10000 firmware/airmic_wr1.0.1-esp32wroom32-bluetooth.bin
```

> 不同开发板 Flash 偏移可能不同：若上传后无蓝牙广播，先检查烧录起始地址。

## 使用说明

- 配对与硬件参数：见 `docs/airmic_wr104_user_guide.md`
- 需求定位（纯蓝牙版）：见 `docs/wroom32_ble_all_bluetooth.md`
- 按键定制思路：见 `docs/custom_key_mapping.md`

## 你要重点知道的

- 这是“纯蓝牙”方向：键盘与麦克风都不经过 `wifi/receiver.py`。
- Mac/PC 端不再需要安装 `AirMic` Python 助手。
- 本发布包不包含源代码仓库，便于外发且不泄露核心开发流程。
