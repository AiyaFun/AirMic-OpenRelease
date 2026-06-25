# AirMic early prototype assembly SOP

This is the hand-assembly sequence for the first Wireless V1 prototypes.

## 1. Prepare firmware and Mac helper

1. Flash the ESP32-S3 firmware from `firmware/esp32_s3_wifi_mic/esp32_s3_wifi_mic.ino`.
2. Confirm serial output includes:

```text
AirMic Keypad ESP32-S3 Wi-Fi Mic + BLE Keyboard V0.5
BLE keyboard advertising: AirMic Keyboard
```

3. Install the Mac lightweight helper with `install_airmic_helper.command` on the test Mac.
4. Confirm `BlackHole 2ch` exists before audio testing.

## 2. Wire microphone

Use the existing verified wiring:

| JST line | ESP32-S3 | INMP441 |
| --- | --- | --- |
| JST 1 | 3V3 | VDD |
| JST 2 | GND | GND |
| JST 3 | IO4 | SCK / BCLK |
| JST 4 | IO5 | WS / LRC |
| JST 5 | IO6 | SD / DOUT |
| JST 6 | GND | L/R |

Assembly notes:

- Keep microphone wires short where possible.
- Add strain relief so the JST connector cannot pull the module off.
- Keep the microphone sound hole aligned with the enclosure opening.

## 3. Wire switches

| Function | ESP32-S3 GPIO | Wiring |
| --- | --- | --- |
| Backspace | GPIO7 | GPIO7 -> switch -> GND |
| Right Option / Dictation | GPIO8 | GPIO8 -> switch -> GND |
| Enter | GPIO9 | GPIO9 -> switch -> GND |

Assembly notes:

- The firmware uses internal pull-ups.
- One side of each switch goes to GPIO, the other side goes to GND.
- Keep the middle key physically distinct if it is the dictation key.

## 4. Wire battery power

Prototype safe path:

```text
LiPo -> charger/protection -> boost or supported board battery input -> ESP32-S3
```

Assembly notes:

- Add the physical power switch on the positive power path.
- Do not feed a bare LiPo into `3V3`.
- Do not connect USB power and external battery/boost power at the same time unless the board supports it.
- First power-on should be done with the enclosure open so heat and wiring can be checked.

## 5. Fit enclosure

1. Mount switches into the top shell.
2. Place the microphone behind the sound hole.
3. Fix ESP32-S3 board in the bottom shell.
4. Fix battery with foam or double-sided tape.
5. Route wires away from screw posts and sharp edges.
6. Close enclosure and verify no switch is stuck.

## 6. Label the device

Each prototype should have a simple label:

```text
AirMic Keypad
BLE: AirMic Keyboard
Setup Wi-Fi: AirMic-Setup-xxxx / 192.168.4.1
```

For later batches, add serial number and support QR code.

## 7. Battery first power-on

Before closing the enclosure for a battery-powered unit, complete:

```text
docs/battery_first_power_on.md
```

The unit must pass a 30-minute BLE + Wi-Fi audio run without abnormal heat or reboot before early tester handoff.

## 8. Enclosure V0 layout

Use the V0 layout guide before drilling, printing, or cutting the first enclosure:

```text
docs/enclosure_layout_v0.md
hardware/enclosure_top_plate_v0.svg
```

The SVG is a 1:1 reference template, not a final production CAD file.
