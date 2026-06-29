# AirMic WR104 ESP-IDF Classic Combo

This is the active firmware project for AirMic WR104.

It exposes one Classic Bluetooth device named `AirMic WR104` with:

- HFP/HSP microphone input from INMP441 over I2S
- Classic Bluetooth HID keyboard input
- No Wi-Fi receiver path
- No separate BLE keyboard device

## Version

```text
wr104-2026-06-29-new-board
```

## Pins

Keys use internal pulldown and are active high:

| Function | GPIO |
| --- | --- |
| Backspace | GPIO32 |
| Enter | GPIO27 |
| Right Option | GPIO12 |

INMP441:

| Signal | GPIO |
| --- | --- |
| BCLK | GPIO17 |
| WS / LRCL | GPIO4 |
| DOUT | GPIO0 |
| VDD | 3V3 |
| GND | GND |
| L/R | GND |

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
AirMic WR104 ESP-IDF Classic combo wr104-2026-06-29-new-board
HID init status=0 ready=yes
No bonded host. Pair AirMic WR104 from macOS/Windows Bluetooth settings.
```
