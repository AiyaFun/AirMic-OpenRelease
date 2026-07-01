#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FIRMWARE_DIR="$ROOT_DIR/firmware/esp32_idf_classic_combo"
CURRENT_FW="wr104-260701-micboost-i141532"
CURRENT_PROFILE="MacAir microphone boost tuning + right alt/option key"
IDF_EXPORT_SH="${IDF_EXPORT_SH:-$HOME/esp/esp-idf/export.sh}"
PORT="${PORT:-${1:-}}"
BAUD="${AIRSHUA_BAUD:-460800}"
VERIFY_SECONDS="${AIRSHUA_VERIFY_SECONDS:-18}"
EXPECTED_FW="${AIRSHUA_EXPECTED_FW:-$CURRENT_FW}"

log() {
  printf '[airshua] %s\n' "$*"
}

fail() {
  printf '[airshua] ERROR: %s\n' "$*" >&2
  exit 1
}

usage() {
  cat <<'USAGE'
Usage:
  npm run airshua
  npm run airshua -- /dev/cu.usbserial-140
  PORT=/dev/cu.usbserial-140 npm run airshua

Environment:
  IDF_EXPORT_SH=/path/to/export.sh
  AIRSHUA_BAUD=460800
  AIRSHUA_VERIFY_SECONDS=18
  AIRSHUA_SKIP_VERIFY=1
  AIRSHUA_EXPECTED_FW=wr104-...  # default: wr104-260701-micboost-i141532
USAGE
}

find_ports() {
  find /dev -maxdepth 1 \( \
    -name 'cu.usbserial*' -o \
    -name 'cu.SLAB_USBtoUART*' -o \
    -name 'cu.wchusbserial*' -o \
    -name 'cu.usbmodem*' \
  \) -print | sort
}

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
  usage
  exit 0
fi

[[ -d "$FIRMWARE_DIR" ]] || fail "Firmware directory not found: $FIRMWARE_DIR"
[[ -f "$IDF_EXPORT_SH" ]] || fail "ESP-IDF export script not found: $IDF_EXPORT_SH"

SOURCE_FW="$(sed -n 's/^#define FIRMWARE_VERSION "\(.*\)"/\1/p' "$FIRMWARE_DIR/main/main.c" | head -n 1)"
[[ -n "$SOURCE_FW" ]] || fail "Cannot read FIRMWARE_VERSION from $FIRMWARE_DIR/main/main.c"
[[ "$SOURCE_FW" == "$EXPECTED_FW" ]] || fail "Firmware version mismatch: script expects $EXPECTED_FW, source is $SOURCE_FW"

if [[ -z "$PORT" ]]; then
  ports=()
  while IFS= read -r port; do
    ports+=("$port")
  done < <(find_ports)

  if [[ "${#ports[@]}" -eq 0 ]]; then
    fail "No ESP32 serial port found. Plug in the board and try again."
  fi

  if [[ "${#ports[@]}" -gt 1 ]]; then
    printf '[airshua] Multiple serial ports found:\n' >&2
    printf '  %s\n' "${ports[@]}" >&2
    fail "Choose one with: npm run airshua -- /dev/cu.usbserial-xxxx"
  fi

  PORT="${ports[0]}"
fi

[[ -e "$PORT" ]] || fail "Serial port not found: $PORT"

log "Using port: $PORT"
log "Firmware dir: $FIRMWARE_DIR"
log "Profile: $CURRENT_PROFILE"
log "Expected firmware: $EXPECTED_FW"
log "Expected wiring: keys GPIO25(backspace)/GPIO26(enter)/GPIO13(right alt/option); INMP441 SCK=GPIO14 WS=GPIO15 SD=GPIO32"

# shellcheck source=/dev/null
source "$IDF_EXPORT_SH" >/tmp/airmic_idf_export.log

cd "$FIRMWARE_DIR"

log "Building firmware"
idf.py build

log "Erasing flash"
idf.py -p "$PORT" -b "$BAUD" erase-flash

log "Flashing firmware"
idf.py -p "$PORT" -b "$BAUD" flash

if [[ "${AIRSHUA_SKIP_VERIFY:-0}" == "1" ]]; then
  log "Skipped boot verification"
  exit 0
fi

if [[ -n "${IDF_PYTHON_ENV_PATH:-}" && -x "$IDF_PYTHON_ENV_PATH/bin/python" ]]; then
  PYTHON_BIN="$IDF_PYTHON_ENV_PATH/bin/python"
elif command -v python3 >/dev/null 2>&1; then
  PYTHON_BIN="$(command -v python3)"
else
  log "Flash complete, but Python was not found for boot verification"
  exit 0
fi

log "Verifying boot log"
if ! "$PYTHON_BIN" - "$PORT" "$VERIFY_SECONDS" "$EXPECTED_FW" <<'PY'
import sys
import time

port = sys.argv[1]
seconds = float(sys.argv[2])
expected_fw = sys.argv[3] if len(sys.argv) > 3 else ""

try:
    import serial
except Exception as exc:
    print(f"[airshua] WARN: pyserial is unavailable: {exc}")
    sys.exit(1)

deadline = time.time() + seconds
seen_hid = False
seen_hfp = False
seen_run_ready = False
seen_fw = False
buffer = b""

try:
    ser = serial.Serial(port, 115200, timeout=0.2)
except Exception as exc:
    print(f"[airshua] WARN: cannot open serial port for verification: {exc}")
    sys.exit(1)

try:
    ser.dtr = False
    ser.rts = False
except Exception:
    pass

print(f"[airshua] Listening on {port} for {int(seconds)}s...")

try:
    while time.time() < deadline:
        chunk = ser.read(256)
        if not chunk:
            continue
        buffer += chunk
        while b"\n" in buffer:
            raw, buffer = buffer.split(b"\n", 1)
            line = raw.decode("utf-8", "replace").strip()
            if not line:
                continue
            print(line)

            if "HID init status=0 ready=yes" in line:
                seen_hid = True
            if "HFP profile ready=yes" in line:
                seen_hfp = True
            if expected_fw and expected_fw in line:
                seen_fw = True
            if "RUN fw=" in line and "hidReady=yes" in line and "hfp=yes" in line:
                seen_run_ready = True

            version_ok = (not expected_fw) or seen_fw
            if version_ok and ((seen_hid and seen_hfp) or seen_run_ready):
                print("[airshua] Boot verification passed.")
                sys.exit(0)
finally:
    ser.close()

print("[airshua] WARN: flash completed, but ready logs were not seen before timeout.")
sys.exit(1)
PY
then
  log "Flash completed, but boot verification did not finish cleanly"
fi

log "Done"
