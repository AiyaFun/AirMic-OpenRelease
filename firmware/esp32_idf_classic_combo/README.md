# AirMic WR104 ESP-IDF Classic Combo

This is the active firmware project for AirMic WR104.

It exposes one Classic Bluetooth device named `AirMic WR104` with:

- HFP/HSP microphone input from INMP441 over I2S
- Classic Bluetooth HID keyboard input
- No Wi-Fi receiver path
- No separate BLE keyboard device

## Version

```text
wr104-260701-micboost-i141532
```

## Pins

Keys use internal pulldown and are active high:

| Function | GPIO |
| --- | --- |
| Backspace | GPIO25 |
| Enter | GPIO26 |
| Right Alt / Option | GPIO13 |

The third key sends one fixed HID modifier:

- HID modifier: `Right Alt`.
- macOS shows this as right `Option`.
- Windows shows this as right `Alt`.
- No separate Win/Mac mode is used.

INMP441:

| Signal | GPIO |
| --- | --- |
| SCK / BCLK | GPIO14 |
| WS / LRCL | GPIO15 |
| SD / DOUT | GPIO32 |
| VDD | 3V3 |
| GND | GND |
| L/R | GND |

Audio profile:

- MacAir microphone boost tuning.
- Raw I2S diagnostic logging is off for normal use.
- Voice path uses stronger front-end gain, slow AGC, lighter noise gating, and soft limiting.

## Build

```bash
source ~/esp/esp-idf/export.sh
idf.py set-target esp32
idf.py build
```

## Flash

```bash
python3 -m esptool --chip esp32 --port /dev/cu.usbserial-1140 --baud 460800 erase-flash
python3 -m esptool --chip esp32 --port /dev/cu.usbserial-1140 --baud 460800 --before default-reset --after hard-reset write-flash \
  --flash-mode dio --flash-size 4MB --flash-freq 40m \
  0x1000 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0x10000 build/airmic_wr104_classic_combo.bin
```

Expected boot log:

```text
AirMic WR104 ESP-IDF Classic combo wr104-260701-micboost-i141532
HID init status=0 ready=yes
No bonded host. Pair AirMic WR104 from macOS/Windows Bluetooth settings.
```
