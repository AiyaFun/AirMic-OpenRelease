# AirMic Keypad quick start card

## What this does

AirMic Keypad is a wireless voice-input keypad for Mac.

- The three keys connect as a Bluetooth keyboard.
- The microphone sends audio over Wi-Fi to the Mac helper.
- macOS uses `BlackHole 2ch` as the microphone input.

## First setup

1. Install `BlackHole 2ch`.
2. Double-click `install_airmic_helper.command`.
3. Turn on AirMic Keypad.
4. Open Mac Bluetooth settings and connect `AirMic Keyboard`.
5. If a Wi-Fi setup hotspot appears, connect to `AirMic-Setup-xxxx` and open:

```text
http://192.168.4.1
```

6. Enter your Wi-Fi name and password.
7. In macOS dictation microphone settings, choose:

```text
BlackHole 2ch
```

## Keys

| Key | Action |
| --- | --- |
| Left | Backspace |
| Middle | Right Option / Dictation |
| Right | Enter |

## If microphone does not work

Check these first:

- Mac and AirMic are on the same Wi-Fi.
- VPN is off for the first test.
- `BlackHole 2ch` is selected as microphone input.
- The Mac helper is installed and running.

## Reset Wi-Fi

Hold the middle key while powering on for 3 seconds. AirMic will clear saved Wi-Fi and start setup mode again.

## Change key actions

To change key actions, enter setup mode and open `http://192.168.4.1`.

Supported actions include:

```text
Backspace, Right Option, Enter, Esc, Tab, Space,
Forward Delete, Command+Backspace, Command+Enter,
Command+Space, Control+Space
```

## If macOS blocks a command file

Right-click the `.command` file and choose `Open`, then confirm in the macOS dialog.

More details: `docs/macos_permissions.md`.

## Device operations

For Wi-Fi reset, key mapping, Bluetooth pairing, and battery operation, see:

```text
docs/device_operations.md
```
