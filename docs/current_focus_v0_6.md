# AirMic V0.6 current focus

Current instruction: do not focus on battery now.

V0.6 priority is:

```text
BLE keyboard keys -> Mac
Wi-Fi microphone audio -> lightweight Mac helper -> BlackHole 2ch -> system microphone
```

Battery work is deferred. Existing battery docs and disabled battery telemetry stay in the project as future references, but they are not current acceptance blockers.

Mac status tools hide empty battery fields during the current non-battery bring-up.

## Current must-pass path

1. Compile and upload main firmware `V0.6`.
2. Pair `AirMic Keyboard` over Bluetooth.
3. Confirm keys work without Mac helper:
   - Key 1: Backspace
   - Key 2: Right Option / Dictation
   - Key 3: Enter
4. Install/start lightweight Mac helper.
5. Confirm `airmic_status.command` shows `State: receiving_audio`.
6. Select `BlackHole 2ch` as microphone input.
7. Confirm macOS Dictation receives speech.
8. Confirm diagnostics/status/dashboard tools work.

## Not required for current V0.6 validation

- Battery module.
- Battery runtime test.
- Battery ADC telemetry.
- Battery low warning.
- Final enclosure battery fit.

## Recommended current hardware power

Use stable USB-C power or known-good bench/external power for bring-up.

Do not introduce LiPo, boost modules, or charging boards until the BLE + Wi-Fi + Mac helper path is proven stable.

## Current Go decision

V0.6 can move to personal daily use when:

```text
BLE keys work
Wi-Fi audio reaches receiving_audio
BlackHole dictation works
Helper can install/uninstall
Diagnostics export works
```

V0.6 should not move to early testers until the setup can be repeated from the release package on a clean Mac.

## Mac helper role

The Mac helper is intentionally limited:

```text
Receive Wi-Fi audio
Send audio to BlackHole 2ch
Broadcast Mac-ready discovery
Record ESP32 status packets
Expose status/diagnostics/dashboard
```

It does not handle key presses in the current Wireless V1 path. Keys are BLE HID.

Legacy UDP key control is disabled by default and only available with:

```bash
python receiver.py --enable-legacy-key-control
```

Do not use the legacy mode for normal V0.6 validation.

Normal V0.6 receiver logs do not show legacy `controls=...` counters. Those counters only appear when `--enable-legacy-key-control` is used for debugging.
