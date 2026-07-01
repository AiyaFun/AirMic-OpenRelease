# AirMic WR104 ESP-IDF Classic Combo

**当前唯一保留版本**：`wr104-260701-micboost-i141532`（MacAir 参考麦克风增强版；第三个键固定为 Right Alt）  
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

快捷方式：

```bash
npm run airshua
```

如果电脑上有多个串口，可以手动指定：

```bash
npm run airshua -- /dev/cu.usbserial-140
```

手动方式：

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
| Backspace | GPIO25 | 按下接 3V3 |
| Enter | GPIO26 | 按下接 3V3 |
| 第三个键 | GPIO13 | 按下接 3V3 |

麦克风：

| INMP441 | 控制端 |
| --- | --- |
| VDD | 3V3 |
| GND | GND |
| SCK / BCLK | GPIO14 |
| WS / LRCL | GPIO15 |
| SD / DOUT | GPIO32 |
| L/R | GND |

说明：

- 三个按键使用内部下拉，按下为高电平。
- 第三个键固定发送 `Right Alt`。
- 在 macOS 上，`Right Alt` 会识别为右 `Option`。
- 在 Windows 上，第三个键就是右 `Alt`，没有单独的 Win/Mac 档位。
- INMP441 只能接 3.3V，控制端与麦克风必须共地。
- GPIO32 已用于 INMP441 的 SD/DOUT，不再接按键。
- I2S 三根线尽量短，先按 `SCK=14 / WS=15 / SD=32` 保持不变。
- 麦克风采用 MacAir 参考增强调音：更高前级增益、更开放 AGC、轻噪声门和软限幅，保留明显输入电平波动。

## 文档

- [WR104 公开说明](docs/airmic_wr104_public_guide.md)
- [WR104 用户说明](docs/airmic_wr104_user_guide.md)
