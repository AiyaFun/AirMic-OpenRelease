# AirMic ESP32-S3 Arduino bring-up guide

This guide is for flashing and testing the Wireless V1 firmware:

```text
ESP32-S3 -> BLE keyboard + Wi-Fi microphone sender
```

Firmware file:

```text
/Users/jiang/Documents/AirMic Keypad/firmware/esp32_s3_wifi_mic/esp32_s3_wifi_mic.ino
```

## 1. Arduino IDE board settings

Use these as the first attempt:

| Setting | Value |
| --- | --- |
| Board | `ESP32S3 Dev Module`（当前主线）或 `ESP32 Dev Module`（WROOM-32） |
| USB CDC On Boot | Enabled |
| Upload Mode | UART0 / Hardware CDC, depending on the board option shown |
| USB Mode | Hardware CDC and JTAG, if available |
| Flash Size | Match your board, commonly 8MB or 16MB |
| PSRAM | Enabled if your board has PSRAM, otherwise Disabled |
| Partition Scheme | Default or Huge APP |
| Upload Speed | 460800 first, lower to 115200 if unstable |
| Core Debug Level | None |

If the exact option names differ, keep the spirit:

Select board:

- `ESP32S3 Dev Module` for the current Wi-Fi+helper path
- `ESP32 Dev Module` for WROOM-32 keyboard-only BLE experiments

Enable USB serial/CDC
Use a normal app partition
Do not use debug-console as upload port

## 2. Correct serial port

Use the USB modem style port, for example:

```text
/dev/cu.usbmodem101
```

Avoid this for upload:

```text
/dev/cu.debug-console
```

If multiple ports appear, unplug the board, check which port disappears, then plug it back in.

## 3. Upload sequence

Normal upload:

1. Close Serial Monitor.
2. Select the correct `/dev/cu.usbmodem...` port.
3. Click Upload.
4. Open Serial Monitor after upload completes.
5. Set baud rate to `115200`.

If upload fails with no serial data:

1. Hold `BOOT`.
2. Click Upload.
3. When Arduino shows `Connecting...`, keep holding `BOOT` for 2-3 seconds.
4. Release `BOOT` after upload starts.
5. If needed, tap `RST/EN` once while holding `BOOT`.

Expected successful upload tail:

```text
Hash of data verified.
Hard resetting via RTS pin...
```

## 4. Expected serial output

After reset, Serial Monitor should show:

```text
=== AirMic Keypad ESP32-S3 Wi-Fi Mic + BLE Keyboard V0.6 ===
BLE keyboard advertising: AirMic Keyboard
```

If no Wi-Fi is saved, it should enter setup mode:

```text
Setup AP: AirMic-Setup-xxxx
Setup URL: http://192.168.4.1
```

After Wi-Fi and Mac helper are ready:

```text
Wi-Fi connected, IP: ...
Mac receiver discovered: ...:3333
seq=... bytesRead=... sentBytes=... fail=0 ble=...
```

If BLE keyboard is connected:

```text
BLE keyboard connected.
```

When keys are pressed:

```text
[KEY] BACKSPACE -> BLE Backspace
[KEY] RIGHT_OPTION -> BLE Right Option
[KEY] ENTER -> BLE Enter
```

## 5. Common compile errors

### `BLEHIDDevice.h: No such file or directory`

Likely cause:

```text
Arduino ESP32 core BLE library is missing, disabled, or version layout differs.
```

Action:

- Confirm you selected an ESP32-S3 board from the Espressif ESP32 package.
- Update or reinstall the `esp32` board package in Arduino Boards Manager.
- Send the complete compile log to Codex for firmware adaptation.

### BLE symbol or BLESecurity errors

Likely cause:

```text
Your installed ESP32 Arduino core uses a slightly different BLE API.
```

Action:

- Do not rewrite the project manually.
- Send the full error block.
- The firmware can be adapted to your installed BLE API.

### I2S field errors

Examples:

```text
I2S_MCLK_MULTIPLE_DEFAULT was not declared
field order does not match declaration order
```

The current firmware avoids the previous known issue by using explicit I2S config assignment and `I2S_MCLK_MULTIPLE_256`.

If another I2S field error appears, send the full error block.

## 6. First firmware test order

Do not test everything at once. Use this order:

1. Compile.
2. Upload.
3. Serial shows BLE advertising.
4. macOS Bluetooth sees `AirMic Keyboard`.
5. Pair BLE keyboard.
6. Test keys in Notes without running Mac helper.
7. Run Mac helper.
8. Confirm Wi-Fi audio packets.
9. Select `BlackHole 2ch` and test dictation.

This separation matters because it tells us whether a problem is BLE, Wi-Fi, Mac helper, or BlackHole.

## 7. Optional smoke test first

If the full firmware gives BLE compile errors, or if you want a smaller first check, compile this sketch first:

```text
firmware/ble_keyboard_smoke_test/ble_keyboard_smoke_test.ino
```

It only tests BLE keyboard behavior on GPIO7/8/9.

See:

```text
docs/firmware_smoke_tests.md
```
