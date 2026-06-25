# AirMic custom key mapping plan

Current Wireless V1 behavior:

```text
Key 1 -> Backspace
Key 2 -> Right Option
Key 3 -> Enter
```

The active firmware now keeps product constants in:

```text
firmware/esp32_s3_wifi_mic/airmic_config.h
```

## Current simple customization

For the current prototype, edit these values and re-upload firmware:

```cpp
static const uint8_t KEY_PINS[] = {7, 8, 9};
static const char* const KEY_COMMANDS[] = {"BACKSPACE", "RIGHT_OPTION", "ENTER"};
```

Supported commands in the current firmware:

| Command | HID behavior |
| --- | --- |
| `BACKSPACE` | Backspace/Delete |
| `RIGHT_OPTION` | Right Option single press |
| `DICTATION_OPTION` | Same as Right Option compatibility name |
| `ENTER` | Enter/Return |

## Product customization direction

Do not make normal users edit firmware. For a sellable version, use this staged path.

### Stage 1: fixed presets

Expose a small number of presets in firmware:

```text
Preset A: Backspace / Right Option / Enter
Preset B: Escape / Right Option / Enter
Preset C: Command+Backspace / Right Option / Enter
```

A long press during boot can cycle presets. This avoids a Mac UI too early.

### Stage 2: setup portal mapping

Add key mapping to the ESP32 setup page:

```text
http://192.168.4.1
```

Store choices in NVS with `Preferences`:

```text
key1=BACKSPACE
key2=RIGHT_OPTION
key3=ENTER
```

This keeps customization inside the device and works even when the Mac helper is not running.

### Stage 3: Mac helper UI

A later lightweight menu bar UI can send config commands to ESP32 over UDP or HTTP:

```text
Mac helper -> ESP32 config endpoint -> Preferences/NVS -> BLE key behavior
```

The Mac helper should not be required for daily key presses. It should only configure the device.

## Future HID expansion

To support more shortcuts, add HID command types:

```text
single key: ENTER, ESC, BACKSPACE
modifier only: RIGHT_OPTION
modifier combo: CMD+BACKSPACE, CMD+ENTER, CTRL+SPACE
media/system key: later if needed
```

Recommended internal representation:

```cpp
struct KeyAction {
  const char* name;
  uint8_t modifier;
  uint8_t key;
};
```

This keeps the BLE report format simple and avoids AppleScript/Accessibility permissions.

## Implemented in firmware

The firmware now supports saving the three key mappings through the ESP32 setup portal.

Setup page:

```text
http://192.168.4.1
```

The page includes:

```text
Key 1: Backspace / Right Option / Enter
Key 2: Backspace / Right Option / Enter
Key 3: Backspace / Right Option / Enter
```

Saved values are stored in ESP32 NVS under the `airmic` namespace:

```text
key1
key2
key3
```

Default values remain:

```text
key1=BACKSPACE
key2=RIGHT_OPTION
key3=ENTER
```

To change mappings on a real device:

1. Hold key 2 while powering on for 3 seconds to enter setup mode.
2. Connect to `AirMic-Setup-xxxx`.
3. Open `http://192.168.4.1`.
4. Select Wi-Fi and key mappings.
5. Save and restart.

The keys still work as BLE HID keyboard events. The Mac helper is not required for daily key presses.

## Supported preset actions in V0.6

The setup portal now supports a larger preset action list. These actions are still sent directly as BLE HID keyboard reports.

| Command | macOS behavior |
| --- | --- |
| `BACKSPACE` | Backspace / Delete backward |
| `RIGHT_OPTION` | Right Option single press, useful for Dictation shortcut |
| `DICTATION_OPTION` | Compatibility alias for Right Option |
| `ENTER` | Enter / Return |
| `ESC` | Escape |
| `TAB` | Tab |
| `SPACE` | Space |
| `DELETE_FORWARD` | Forward Delete |
| `CMD_BACKSPACE` | Command + Backspace |
| `CMD_ENTER` | Command + Enter |
| `CMD_SPACE` | Command + Space |
| `CTRL_SPACE` | Control + Space |

Default mapping is unchanged:

```text
Key 1 = BACKSPACE
Key 2 = RIGHT_OPTION
Key 3 = ENTER
```

Recommended early product mapping:

```text
Left key: BACKSPACE
Middle key: RIGHT_OPTION
Right key: ENTER
```

Only use the other presets after the basic three-key workflow is stable.
