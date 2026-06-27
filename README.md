# AirMic WR104 ESP-IDF Classic Combo

**当前唯一保留版本**：`wr104-2026-06-27-stable`（WR104 2026-06-27 稳定版）  
**蓝牙名称**：`AirMic WR104`  
**方案**：一个 Classic Bluetooth 设备同时提供 HFP/HSP 麦克风和 Classic HID 键盘。

本仓库已收敛到 ESP-IDF 项目方案，不再保留其他临时固件、旧链路或旧发布包。

## 固件位置

```text
firmware/esp32_idf_classic_combo/
```

当前构建产物：

```text
firmware/esp32_idf_classic_combo/build/bootloader/bootloader.bin
firmware/esp32_idf_classic_combo/build/partition_table/partition-table.bin
firmware/esp32_idf_classic_combo/build/airmic_wr104_classic_combo.bin
```

## 刷机

```bash
cd firmware/esp32_idf_classic_combo
python3 -m esptool --chip esp32 --port /dev/cu.usbserial-1140 --baud 460800 erase-flash
python3 -m esptool --chip esp32 --port /dev/cu.usbserial-1140 --baud 460800 --before default-reset --after hard-reset write-flash \
  --flash-mode dio --flash-size 4MB --flash-freq 40m \
  0x1000 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0x10000 build/airmic_wr104_classic_combo.bin
```

## 重新构建

```bash
source ~/esp/esp-idf/export.sh
cd firmware/esp32_idf_classic_combo
idf.py set-target esp32
idf.py build
```

## 接线

按键：

| 功能 | 引脚 | 触发 |
| --- | --- | --- |
| Backspace | GPIO32 | 按下接 3V3 |
| Enter | GPIO27 | 按下接 3V3 |
| 右 Option | GPIO14 | 按下接 3V3 |

麦克风：

| INMP441 | 控制端 |
| --- | --- |
| VDD | 3V3 |
| GND | GND |
| SCK / BCLK | GPIO26 |
| WS / LRCL | GPIO25 |
| SD / DOUT | GPIO33 |
| L/R | GND |

## 文档

- [WR104 公开说明](docs/airmic_wr104_public_guide.md)
- [WR104 用户说明](docs/airmic_wr104_user_guide.md)
