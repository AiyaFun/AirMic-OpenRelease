// AirMic WR101 Bluetooth firmware
// Board: ESP32-WROOM-32 / ESP32-WROOM-DA
// Audio: INMP441 -> ESP32 I2S -> Classic Bluetooth HFP/HSP SCO mic
// Keys: GPIO16/GPIO17/GPIO5 -> Classic Bluetooth HID keyboard

#include <Arduino.h>
#include "esp32-hal-alloc-bt-classic-mem.h"
#include "esp32-hal-bt.h"
#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_hidd.h"
#include "esp_hf_client_api.h"
#include "esp_hf_client_legacy_api.h"
#include "driver/i2s.h"
#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>

#include <inttypes.h>
#include <string.h>

static const char* FIRMWARE_VERSION = "wr1.0.4";
static const char* BT_DEVICE_NAME = "AirMic WR101";
static const char* BOND_RESET_MARKER = "wr101-kbd-one3";

static const int I2S_BCLK = 26;
static const int I2S_WS = 25;
static const int I2S_SD = 33;
static const i2s_port_t I2S_PORT = I2S_NUM_0;

static const uint32_t CVSD_RATE_HZ = 8000;
static const uint32_t MSBC_RATE_HZ = 16000;
static const size_t CVSD_FRAME_SAMPLES = 30;
static const size_t MSBC_FRAME_SAMPLES = 120;
static const size_t RING_FRAMES = 8;
static const uint32_t HFP_RECONNECT_MS = 5000;
static const uint32_t SCO_RETRY_MS = 3000;
static const uint32_t STATUS_MS = 3000;
static const uint8_t MAX_BONDED_DEVICES = 8;
static const int MIC_GAIN = 2;

static const uint8_t KEY_PINS[] = {16, 17, 5};
static const char* KEY_COMMANDS[] = {"BACKSPACE", "ENTER", "RIGHT_OPTION"};
static const uint8_t KEY_COUNT = sizeof(KEY_PINS) / sizeof(KEY_PINS[0]);
static const uint32_t DEBOUNCE_MS = 25;
static const uint16_t KEY_HOLD_MS = 45;
static const uint8_t HID_KEY_ENTER = 0x28;
static const uint8_t HID_KEY_BACKSPACE = 0x2A;
static const uint8_t HID_MOD_RIGHT_ALT = 0x40;
static const uint8_t HID_REPORT_ID_KEYBOARD = 0;

struct KeyDebounceState {
  uint8_t pin;
  int lastRaw;
  int stableState;
  uint32_t lastChangeMs;
  bool armed;
};

static KeyDebounceState keyStates[KEY_COUNT];
static BLEHIDDevice* bleHid = nullptr;
static BLECharacteristic* bleKeyboardInput = nullptr;
static BLECharacteristic* bleKeyboardOutput = nullptr;
static BLEAdvertising* bleAdvertising = nullptr;
static bool bleKeyboardConnected = false;

static uint8_t HID_REPORT_MAP[] = {
  0x05, 0x01, 0x09, 0x06, 0xA1, 0x01,
  0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00,
  0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
  0x95, 0x01, 0x75, 0x08, 0x81, 0x01, 0x95, 0x05,
  0x75, 0x01, 0x05, 0x08, 0x19, 0x01, 0x29, 0x05,
  0x91, 0x02, 0x95, 0x01, 0x75, 0x03, 0x91, 0x01,
  0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65,
  0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00,
  0xC0
};

static esp_hid_raw_report_map_t hidReportMap;
static esp_hid_device_config_t hidDeviceConfig;
static esp_hidd_dev_t* hidDev = nullptr;

volatile bool hfpReady = false;
volatile bool hidReady = false;
volatile bool classicKeyboardConnected = false;
volatile bool slcConnected = false;
volatile bool audioConnected = false;
volatile bool audioConnecting = false;
volatile bool haveRemoteBda = false;
volatile bool msbcActive = false;
volatile bool i2sRunning = false;
volatile bool micTaskRunning = false;

esp_bd_addr_t remoteBda = {0};
StreamBufferHandle_t micRing = nullptr;
TaskHandle_t micTask = nullptr;

uint32_t lastHfpConnectMs = 0;
uint32_t lastScoConnectMs = 0;
uint32_t lastStatusMs = 0;
uint32_t micFrames = 0;
uint32_t micDrops = 0;
int16_t micPeak = 0;

static int32_t i2sRaw[MSBC_FRAME_SAMPLES];
static int16_t pcmFrame[MSBC_FRAME_SAMPLES];

void logEspCall(const char* what, esp_err_t err) {
  if (err != ESP_OK) {
    Serial.printf("%s: %s (0x%04" PRIX32 ")\n", what, esp_err_to_name(err), (uint32_t)err);
  }
}

void printBda(const esp_bd_addr_t bda);

void clearClassicBondsOnce() {
  Preferences prefs;
  prefs.begin("airmic", false);
  String marker = prefs.getString("bond_reset", "");
  if (marker == BOND_RESET_MARKER) {
    prefs.end();
    return;
  }

  int bondedCount = esp_bt_gap_get_bond_device_num();
  Serial.printf("[BT] refreshing pair cache for HID keyboard, old bonds=%d\n", bondedCount);
  if (bondedCount > MAX_BONDED_DEVICES) bondedCount = MAX_BONDED_DEVICES;

  if (bondedCount > 0) {
    esp_bd_addr_t bonded[MAX_BONDED_DEVICES];
    int count = bondedCount;
    esp_err_t err = esp_bt_gap_get_bond_device_list(&count, bonded);
    if (err == ESP_OK) {
      for (int i = 0; i < count; ++i) {
        Serial.print("[BT] remove old bond ");
        printBda(bonded[i]);
        Serial.println();
        logEspCall("esp_bt_gap_remove_bond_device", esp_bt_gap_remove_bond_device(bonded[i]));
        delay(80);
      }
    } else {
      logEspCall("esp_bt_gap_get_bond_device_list", err);
    }
  }

  haveRemoteBda = false;
  prefs.putString("bond_reset", BOND_RESET_MARKER);
  prefs.end();
}

class KeyboardServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*) override {
    bleKeyboardConnected = true;
    Serial.println("[BLE KEY] connected");
  }

  void onDisconnect(BLEServer*) override {
    bleKeyboardConnected = false;
    Serial.println("[BLE KEY] disconnected");
    BLEDevice::startAdvertising();
  }
};

class KeyboardSecurityCallbacks : public BLESecurityCallbacks {
  bool onSecurityRequest() {
    return true;
  }

#if defined(CONFIG_BLUEDROID_ENABLED)
  void onAuthenticationComplete(esp_ble_auth_cmpl_t authComplete) {
    if (authComplete.success) {
      Serial.println("[BLE KEY] pairing successful");
    } else {
      Serial.print("[BLE KEY] pairing failed: ");
      Serial.println(authComplete.fail_reason);
    }
  }
#endif

  uint32_t onPassKeyRequest() {
    return 0;
  }

  void onPassKeyNotify(uint32_t passKey) {
    (void)passKey;
  }

  bool onConfirmPIN(uint32_t passKey) {
    (void)passKey;
    return true;
  }
};

void setupKeys() {
  for (uint8_t i = 0; i < KEY_COUNT; ++i) {
    pinMode(KEY_PINS[i], INPUT_PULLUP);
    int raw = digitalRead(KEY_PINS[i]);
    keyStates[i] = {KEY_PINS[i], raw, raw, millis(), raw == HIGH};
  }
}

void sendClassicKeyReport(uint8_t modifier, uint8_t key) {
  uint8_t report[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  report[0] = modifier;
  report[2] = key;
  logEspCall("esp_hidd_dev_input_set", esp_hidd_dev_input_set(
    hidDev,
    0,
    HID_REPORT_ID_KEYBOARD,
    report,
    sizeof(report)
  ));
}

void releaseClassicKeys() {
  if (!hidDev || (!classicKeyboardConnected && !esp_hidd_dev_connected(hidDev))) return;
  sendClassicKeyReport(0, 0);
}

void tapClassicKey(uint8_t modifier, uint8_t key) {
  bool hidConnectedNow = hidDev && (classicKeyboardConnected || esp_hidd_dev_connected(hidDev));
  if (!hidConnectedNow) {
    Serial.println("[KEY] ignored, Classic HID keyboard not connected");
    return;
  }

  classicKeyboardConnected = true;
  sendClassicKeyReport(modifier, key);
  delay(KEY_HOLD_MS);
  releaseClassicKeys();
}

void releaseBleKeys() {
  if (!bleKeyboardInput) return;
  uint8_t report[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  bleKeyboardInput->setValue(report, sizeof(report));
  bleKeyboardInput->notify();
}

void tapBleKey(uint8_t modifier, uint8_t key) {
  if (!bleKeyboardConnected || !bleKeyboardInput) {
    Serial.println("[KEY] ignored, BLE keyboard not connected");
    return;
  }

  uint8_t report[8] = {modifier, 0, key, 0, 0, 0, 0, 0};
  bleKeyboardInput->setValue(report, sizeof(report));
  bleKeyboardInput->notify();
  delay(KEY_HOLD_MS);
  releaseBleKeys();
}

void handleKeyCommand(const char* command) {
  Serial.print("[KEY] ");
  Serial.println(command);
  if (strcmp(command, "BACKSPACE") == 0) {
    tapClassicKey(0, HID_KEY_BACKSPACE);
  } else if (strcmp(command, "ENTER") == 0) {
    tapClassicKey(0, HID_KEY_ENTER);
  } else if (strcmp(command, "RIGHT_OPTION") == 0) {
    tapClassicKey(HID_MOD_RIGHT_ALT, 0);
  }
}

void handleKeys() {
  uint32_t now = millis();
  for (uint8_t i = 0; i < KEY_COUNT; ++i) {
    KeyDebounceState& state = keyStates[i];
    int raw = digitalRead(state.pin);

    if (raw != state.lastRaw) {
      state.lastRaw = raw;
      state.lastChangeMs = now;
    }

    if (raw != state.stableState && now - state.lastChangeMs >= DEBOUNCE_MS) {
      state.stableState = raw;
      if (state.stableState == LOW && state.armed) {
        state.armed = false;
        handleKeyCommand(KEY_COMMANDS[i]);
      } else if (state.stableState == HIGH) {
        state.armed = true;
      }
    }
  }
}

void setupBleKeyboard() {
  BLEDevice::init(BT_DEVICE_NAME);
  BLEDevice::setMTU(247);

  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(new KeyboardServerCallbacks());

  bleHid = new BLEHIDDevice(server);
  bleKeyboardInput = bleHid->inputReport(HID_REPORT_ID_KEYBOARD);
  bleKeyboardOutput = bleHid->outputReport(HID_REPORT_ID_KEYBOARD);
  bleKeyboardInput->setAccessPermissions(ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE);
  bleKeyboardOutput->setAccessPermissions(ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE);
  uint8_t ledState = 0;
  bleKeyboardOutput->setValue(&ledState, 1);

  bleHid->manufacturer()->setValue("AirMic");
  bleHid->pnp(0x02, 0x1209, 0xA11C, 0x0101);
  bleHid->hidInfo(0x00, 0x03);
  bleHid->reportMap(HID_REPORT_MAP, sizeof(HID_REPORT_MAP));
  bleHid->setBatteryLevel(100);
  bleHid->startServices();

  BLESecurity* security = new BLESecurity();
  security->setCapability(ESP_IO_CAP_NONE);
  security->setAuthenticationMode(ESP_LE_AUTH_BOND);
  security->setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_NO_MITM);
  security->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  security->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  BLEDevice::setSecurityCallbacks(new KeyboardSecurityCallbacks());

  bleAdvertising = BLEDevice::getAdvertising();
  bleAdvertising->setAppearance(0x03C1);
  bleAdvertising->addServiceUUID(bleHid->hidService()->getUUID());
  bleAdvertising->setScanResponse(true);
  bleAdvertising->setMinPreferred(0x06);
  bleAdvertising->setMaxPreferred(0x12);

  esp_bd_addr_t bleAddress = {0xD2, 0x10, 0xA1, 0x7D, 0x26, 0xBF};
  bleAdvertising->setDeviceAddress(bleAddress, BLE_ADDR_TYPE_RANDOM);

  BLEAdvertisementData scanResponseData;
  scanResponseData.setName(BT_DEVICE_NAME);
  bleAdvertising->setScanResponseData(scanResponseData);

  BLEDevice::startAdvertising();
  Serial.print("[BLE KEY] advertising as ");
  Serial.println(BT_DEVICE_NAME);
}

size_t scoFrameSamples(bool msbc) {
  return msbc ? MSBC_FRAME_SAMPLES : CVSD_FRAME_SAMPLES;
}

size_t scoFrameBytes(bool msbc) {
  return scoFrameSamples(msbc) * sizeof(int16_t);
}

void printBda(const esp_bd_addr_t bda) {
  Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X", bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
}

void rememberBda(const esp_bd_addr_t bda) {
  memcpy(remoteBda, bda, ESP_BD_ADDR_LEN);
  haveRemoteBda = true;
}

int16_t convertInmp441Sample(int32_t sample) {
  int32_t v = (sample >> 14) * MIC_GAIN;
  if (v > 32767) return 32767;
  if (v < -32768) return -32768;
  return (int16_t)v;
}

const char* hfpConnStateName(esp_hf_client_connection_state_t s) {
  switch (s) {
    case ESP_HF_CLIENT_CONNECTION_STATE_DISCONNECTED: return "DISCONNECTED";
    case ESP_HF_CLIENT_CONNECTION_STATE_CONNECTING: return "CONNECTING";
    case ESP_HF_CLIENT_CONNECTION_STATE_CONNECTED: return "RFCOMM_CONNECTED";
    case ESP_HF_CLIENT_CONNECTION_STATE_SLC_CONNECTED: return "SLC_CONNECTED";
    case ESP_HF_CLIENT_CONNECTION_STATE_DISCONNECTING: return "DISCONNECTING";
  }
  return "UNKNOWN";
}

const char* hfpAudioStateName(esp_hf_client_audio_state_t s) {
  switch (s) {
    case ESP_HF_CLIENT_AUDIO_STATE_DISCONNECTED: return "DISCONNECTED";
    case ESP_HF_CLIENT_AUDIO_STATE_CONNECTING: return "CONNECTING";
    case ESP_HF_CLIENT_AUDIO_STATE_CONNECTED: return "CONNECTED_CVSD";
    case ESP_HF_CLIENT_AUDIO_STATE_CONNECTED_MSBC: return "CONNECTED_MSBC";
  }
  return "UNKNOWN";
}

void startI2S(bool msbc) {
  if (i2sRunning) return;

  i2s_driver_uninstall(I2S_PORT);

  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate = msbc ? MSBC_RATE_HZ : CVSD_RATE_HZ;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 4;
  cfg.dma_buf_len = scoFrameSamples(msbc);
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = false;
  cfg.fixed_mclk = 0;

  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_PIN_NO_CHANGE;
  pins.bck_io_num = I2S_BCLK;
  pins.ws_io_num = I2S_WS;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = I2S_SD;

  esp_err_t err = i2s_driver_install(I2S_PORT, &cfg, 0, nullptr);
  if (err != ESP_OK) {
    logEspCall("i2s_driver_install", err);
    return;
  }

  err = i2s_set_pin(I2S_PORT, &pins);
  if (err != ESP_OK) {
    logEspCall("i2s_set_pin", err);
    i2s_driver_uninstall(I2S_PORT);
    return;
  }

  i2s_zero_dma_buffer(I2S_PORT);
  i2s_start(I2S_PORT);
  i2sRunning = true;
  Serial.printf("[I2S] started INMP441 fs=%" PRIu32 " Hz BCLK=%d WS=%d DOUT=%d\n", cfg.sample_rate, I2S_BCLK, I2S_WS, I2S_SD);
}

void stopI2S() {
  if (!i2sRunning) return;
  i2sRunning = false;
  delay(20);
  i2s_stop(I2S_PORT);
  i2s_driver_uninstall(I2S_PORT);
  Serial.println("[I2S] stopped");
}

void micCaptureTask(void*) {
  const bool taskMsbc = msbcActive;
  const size_t samples = scoFrameSamples(taskMsbc);
  const size_t rawBytes = samples * sizeof(int32_t);
  const size_t pcmBytes = samples * sizeof(int16_t);

  while (micTaskRunning) {
    size_t bytesRead = 0;
    esp_err_t err = i2s_read(I2S_PORT, i2sRaw, rawBytes, &bytesRead, pdMS_TO_TICKS(20));
    if (!micTaskRunning) break;
    if (err != ESP_OK || bytesRead < rawBytes) continue;

    int16_t peak = 0;
    for (size_t i = 0; i < samples; ++i) {
      int16_t s = convertInmp441Sample(i2sRaw[i]);
      pcmFrame[i] = s;
      int32_t absS = s < 0 ? -(int32_t)s : (int32_t)s;
      if (absS > peak) peak = absS > 32767 ? 32767 : (int16_t)absS;
    }
    if (peak > micPeak) micPeak = peak;

    if (!micRing || xStreamBufferSpacesAvailable(micRing) < pcmBytes) {
      micDrops++;
      continue;
    }

    size_t sent = xStreamBufferSend(micRing, pcmFrame, pcmBytes, 0);
    if (sent == pcmBytes) {
      micFrames++;
      esp_hf_client_outgoing_data_ready();
    } else {
      micDrops++;
    }
  }

  micTask = nullptr;
  vTaskDelete(nullptr);
}

void startAudioPath(bool msbc) {
  if (micTask || micRing) return;

  msbcActive = msbc;
  startI2S(msbc);
  if (!i2sRunning) return;

  const size_t frameBytes = scoFrameBytes(msbc);
  micRing = xStreamBufferCreate(frameBytes * RING_FRAMES, frameBytes);
  if (!micRing) {
    Serial.println("[HFP] xStreamBufferCreate failed");
    stopI2S();
    return;
  }

  micFrames = 0;
  micDrops = 0;
  micPeak = 0;
  micTaskRunning = true;
  if (xTaskCreate(micCaptureTask, "hfp_mic", 4096, nullptr, 3, &micTask) != pdPASS) {
    Serial.println("[HFP] xTaskCreate micCaptureTask failed");
    micTaskRunning = false;
    vStreamBufferDelete(micRing);
    micRing = nullptr;
    stopI2S();
  }
}

void stopAudioPath() {
  micTaskRunning = false;
  while (micTask) delay(1);

  if (micRing) {
    vStreamBufferDelete(micRing);
    micRing = nullptr;
  }
  stopI2S();
}

uint32_t hfOutgoingDataCb(uint8_t* buf, uint32_t len) {
  if (!buf || len == 0) return 0;
  if (!micRing || xStreamBufferBytesAvailable(micRing) < len) {
    memset(buf, 0, len);
    return len;
  }

  size_t got = xStreamBufferReceive(micRing, buf, len, 0);
  if (got < len) {
    memset(buf + got, 0, len - got);
  }
  return len;
}

void hfIncomingDataCb(const uint8_t* buf, uint32_t len) {
  (void)buf;
  (void)len;
  // AirMic WR101 is microphone-first. Incoming speaker audio from the computer is ignored.
}

void tryConnectHfp(const char* reason) {
  if (!hfpReady || !haveRemoteBda || slcConnected) return;
  uint32_t now = millis();
  if (now - lastHfpConnectMs < HFP_RECONNECT_MS) return;
  lastHfpConnectMs = now;
  Serial.printf("[HFP] connect (%s) to ", reason);
  printBda(remoteBda);
  Serial.println();
  logEspCall("esp_hf_client_connect", esp_hf_client_connect(remoteBda));
}

void tryConnectAudio(const char* reason) {
  if (!haveRemoteBda || !slcConnected || audioConnected || audioConnecting) return;
  uint32_t now = millis();
  if (now - lastScoConnectMs < SCO_RETRY_MS) return;
  lastScoConnectMs = now;
  audioConnecting = true;
  Serial.printf("[HFP] open SCO audio (%s)\n", reason);
  logEspCall("esp_hf_client_connect_audio", esp_hf_client_connect_audio(remoteBda));
}

void connectFirstBondedDevice() {
  int bondedCount = esp_bt_gap_get_bond_device_num();
  if (bondedCount <= 0) {
    Serial.println("[BT] no bonded computer yet. Pair AirMic WR101 from macOS/Windows Bluetooth settings.");
    return;
  }

  if (bondedCount > MAX_BONDED_DEVICES) bondedCount = MAX_BONDED_DEVICES;
  esp_bd_addr_t bonded[MAX_BONDED_DEVICES];
  int count = bondedCount;
  esp_err_t err = esp_bt_gap_get_bond_device_list(&count, bonded);
  if (err != ESP_OK || count <= 0) {
    logEspCall("esp_bt_gap_get_bond_device_list", err);
    return;
  }

  rememberBda(bonded[0]);
  Serial.print("[BT] bonded computer found: ");
  printBda(remoteBda);
  Serial.println();
  tryConnectHfp("bonded device");
}

void hidDeviceCallback(void* handlerArg, esp_event_base_t eventBase, int32_t eventId, void* eventData) {
  (void)handlerArg;
  (void)eventBase;
  esp_hidd_event_t event = (esp_hidd_event_t)eventId;
  esp_hidd_event_data_t* param = (esp_hidd_event_data_t*)eventData;

  switch (event) {
    case ESP_HIDD_START_EVENT:
      hidReady = (param->start.status == ESP_OK);
      Serial.printf("[HID] profile ready=%s status=%s\n", hidReady ? "yes" : "no", esp_err_to_name(param->start.status));
      break;

    case ESP_HIDD_CONNECT_EVENT:
      classicKeyboardConnected = (param->connect.status == ESP_OK);
      Serial.printf("[HID] keyboard connected=%s status=%s\n", classicKeyboardConnected ? "yes" : "no", esp_err_to_name(param->connect.status));
      break;

    case ESP_HIDD_DISCONNECT_EVENT:
      classicKeyboardConnected = false;
      Serial.printf("[HID] keyboard disconnected status=%s reason=%d\n", esp_err_to_name(param->disconnect.status), param->disconnect.reason);
      break;

    case ESP_HIDD_PROTOCOL_MODE_EVENT:
      Serial.printf("[HID] protocol mode=%u map=%u\n", param->protocol_mode.protocol_mode, param->protocol_mode.map_index);
      break;

    case ESP_HIDD_OUTPUT_EVENT:
      Serial.printf("[HID] output report id=%u len=%u map=%u\n", param->output.report_id, param->output.length, param->output.map_index);
      break;

    case ESP_HIDD_STOP_EVENT:
      hidReady = false;
      classicKeyboardConnected = false;
      Serial.printf("[HID] profile stopped status=%s\n", esp_err_to_name(param->stop.status));
      break;

    default:
      break;
  }
}

void setupClassicHidKeyboard() {
  memset(&hidReportMap, 0, sizeof(hidReportMap));
  memset(&hidDeviceConfig, 0, sizeof(hidDeviceConfig));

  hidReportMap.data = HID_REPORT_MAP;
  hidReportMap.len = sizeof(HID_REPORT_MAP);

  hidDeviceConfig.vendor_id = 0x1209;
  hidDeviceConfig.product_id = 0xA11C;
  hidDeviceConfig.version = 0x0101;
  hidDeviceConfig.device_name = BT_DEVICE_NAME;
  hidDeviceConfig.manufacturer_name = "AirMic";
  hidDeviceConfig.serial_number = "WR101";
  hidDeviceConfig.report_maps = &hidReportMap;
  hidDeviceConfig.report_maps_len = 1;

  logEspCall("esp_hidd_dev_init(BT)", esp_hidd_dev_init(&hidDeviceConfig, ESP_HID_TRANSPORT_BT, hidDeviceCallback, &hidDev));
}

void gapCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param) {
  switch (event) {
    case ESP_BT_GAP_AUTH_CMPL_EVT:
      if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
        rememberBda(param->auth_cmpl.bda);
        Serial.print("[BT] pairing complete: ");
        printBda(param->auth_cmpl.bda);
        Serial.printf(" (%s)\n", param->auth_cmpl.device_name);
        tryConnectHfp("pairing complete");
      } else {
        Serial.printf("[BT] pairing failed status=%d\n", param->auth_cmpl.stat);
      }
      break;

    case ESP_BT_GAP_CFM_REQ_EVT:
      rememberBda(param->cfm_req.bda);
      Serial.printf("[BT] SSP confirm %06" PRIu32 " for ", param->cfm_req.num_val);
      printBda(param->cfm_req.bda);
      Serial.println();
      logEspCall("esp_bt_gap_ssp_confirm_reply", esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true));
      break;

    case ESP_BT_GAP_PIN_REQ_EVT: {
      esp_bt_pin_code_t pin = {'0', '0', '0', '0'};
      rememberBda(param->pin_req.bda);
      Serial.print("[BT] legacy PIN requested for ");
      printBda(param->pin_req.bda);
      Serial.println(" -> 0000");
      logEspCall("esp_bt_gap_pin_reply", esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin));
      break;
    }

    default:
      break;
  }
}

void hfClientEventCb(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t* param) {
  switch (event) {
    case ESP_HF_CLIENT_PROF_STATE_EVT:
      hfpReady = (param->prof_stat.state == ESP_HF_INIT_SUCCESS || param->prof_stat.state == ESP_HF_INIT_ALREADY);
      Serial.printf("[HFP] profile ready=%s\n", hfpReady ? "yes" : "no");
      if (hfpReady) connectFirstBondedDevice();
      break;

    case ESP_HF_CLIENT_CONNECTION_STATE_EVT:
      rememberBda(param->conn_stat.remote_bda);
      slcConnected = (param->conn_stat.state == ESP_HF_CLIENT_CONNECTION_STATE_SLC_CONNECTED);
      if (param->conn_stat.state == ESP_HF_CLIENT_CONNECTION_STATE_DISCONNECTED) {
        slcConnected = false;
        audioConnected = false;
        audioConnecting = false;
        msbcActive = false;
        stopAudioPath();
      }
      Serial.printf("[HFP] connection=%s ", hfpConnStateName(param->conn_stat.state));
      printBda(param->conn_stat.remote_bda);
      Serial.println();
      if (slcConnected) {
        logEspCall("esp_hf_client_send_nrec", esp_hf_client_send_nrec());
        logEspCall("esp_hf_client_volume_update(mic)", esp_hf_client_volume_update(ESP_HF_VOLUME_CONTROL_TARGET_MIC, 15));
        tryConnectAudio("SLC connected");
      }
      break;

    case ESP_HF_CLIENT_AUDIO_STATE_EVT: {
      esp_hf_client_audio_state_t state = param->audio_stat.state;
      audioConnecting = (state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTING);
      audioConnected = (state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED || state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED_MSBC);
      bool stateMsbc = (state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED_MSBC);
      Serial.printf("[HFP] audio=%s\n", hfpAudioStateName(state));
      if (audioConnected) {
        audioConnecting = false;
        logEspCall("esp_hf_client_volume_update(mic)", esp_hf_client_volume_update(ESP_HF_VOLUME_CONTROL_TARGET_MIC, 15));
        Serial.printf("[HFP] SCO mic active codec=%s\n", stateMsbc ? "mSBC 16k" : "CVSD 8k");
        startAudioPath(stateMsbc);
      } else if (!audioConnecting) {
        msbcActive = false;
        stopAudioPath();
      }
      break;
    }

    case ESP_HF_CLIENT_CIND_CALL_EVT:
      Serial.printf("[HFP] call status=%d\n", param->call.status);
      break;

    case ESP_HF_CLIENT_CIND_CALL_SETUP_EVT:
      Serial.printf("[HFP] call setup=%d\n", param->call_setup.status);
      break;

    case ESP_HF_CLIENT_BVRA_EVT:
      Serial.printf("[HFP] voice recognition state=%d\n", param->bvra.value);
      break;

    default:
      break;
  }
}

void setupClassicBtHfp() {
  if (!btStarted()) {
    Serial.println("[BT] starting Classic Bluetooth controller");
    if (!btStartMode(BT_MODE_CLASSIC_BT)) {
      Serial.println("[BT] btStartMode failed");
    }
  }

  esp_bluedroid_status_t state = esp_bluedroid_get_status();
  if (state == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
    logEspCall("esp_bluedroid_init", esp_bluedroid_init());
  }

  state = esp_bluedroid_get_status();
  if (state != ESP_BLUEDROID_STATUS_ENABLED) {
    logEspCall("esp_bluedroid_enable", esp_bluedroid_enable());
  }

  logEspCall("esp_bt_gap_register_callback", esp_bt_gap_register_callback(gapCallback));
  clearClassicBondsOnce();

  esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_NONE;
  logEspCall("esp_bt_gap_set_security_param", esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE, &iocap, sizeof(iocap)));

  esp_bt_pin_code_t pin = {0};
  logEspCall("esp_bt_gap_set_pin", esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_VARIABLE, 0, pin));

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
  logEspCall("esp_bt_gap_set_device_name", esp_bt_gap_set_device_name(BT_DEVICE_NAME));
#else
  logEspCall("esp_bt_dev_set_device_name", esp_bt_dev_set_device_name(BT_DEVICE_NAME));
#endif

  esp_bt_cod_t cod = {};
  cod.major = ESP_BT_COD_MAJOR_DEV_PERIPHERAL;
  cod.minor = ESP_BT_COD_MINOR_PERIPHERAL_KEYBOARD;
  cod.service = ESP_BT_COD_SRVC_AUDIO | ESP_BT_COD_SRVC_TELEPHONY | ESP_BT_COD_SRVC_CAPTURING;
  logEspCall("esp_bt_gap_set_cod", esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_ALL));
  logEspCall("esp_bt_gap_set_scan_mode", esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE));
  setupClassicHidKeyboard();
  logEspCall("esp_bredr_sco_datapath_set", esp_bredr_sco_datapath_set(ESP_SCO_DATA_PATH_HCI));
  logEspCall("esp_hf_client_register_data_callback", esp_hf_client_register_data_callback(hfIncomingDataCb, hfOutgoingDataCb));
  logEspCall("esp_hf_client_register_callback", esp_hf_client_register_callback(hfClientEventCb));
  logEspCall("esp_hf_client_init", esp_hf_client_init());
}

void printStatus() {
  uint32_t now = millis();
  if (now - lastStatusMs < STATUS_MS) return;
  lastStatusMs = now;

  Serial.print("[RUN] fw=");
  Serial.print(FIRMWARE_VERSION);
  Serial.print(" hfpReady=");
  Serial.print(hfpReady ? "yes" : "no");
  Serial.print(" hidReady=");
  Serial.print(hidReady ? "yes" : "no");
  Serial.print(" hid=");
  Serial.print(classicKeyboardConnected ? "yes" : "no");
  Serial.print(" slc=");
  Serial.print(slcConnected ? "yes" : "no");
  Serial.print(" sco=");
  Serial.print(audioConnected ? "yes" : (audioConnecting ? "connecting" : "no"));
  Serial.print(" i2s=");
  Serial.print(i2sRunning ? "yes" : "no");
  Serial.print(" peak=");
  Serial.print(micPeak);
  Serial.print(" frames=");
  Serial.print(micFrames);
  Serial.print(" drops=");
  Serial.println(micDrops);
  micPeak = 0;
}

void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println();
  Serial.print("AirMic WR101 HFP microphone firmware ");
  Serial.println(FIRMWARE_VERSION);
  Serial.println("Mode: Classic Bluetooth HFP/HSP microphone + Classic HID keyboard, no BLE, no Wi-Fi");
  Serial.println("Pair as one device: AirMic WR101");
  Serial.println("INMP441: BCLK=GPIO26 WS=GPIO25 DOUT=GPIO33 L/R=GND");

  setupKeys();
  setupClassicBtHfp();
  Serial.println("[BT] ready. Pair from macOS/Windows Bluetooth settings.");
}

void loop() {
  handleKeys();
  tryConnectHfp("background retry");
  tryConnectAudio("background SCO retry");
  printStatus();
  delay(20);
}
