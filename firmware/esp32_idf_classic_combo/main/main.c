// AirMic WR104 ESP-IDF Classic Bluetooth combo experiment
// Board: ESP32-WROOM-32 / ESP32-WROOM-DA
// Goal: one Classic Bluetooth device name with HFP microphone + Classic HID keyboard.
// Firmware version: wr104-260701-micboost-i141532

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2s.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_gap_bt_api.h"
#include "esp_hf_client_api.h"
#include "esp_hf_client_legacy_api.h"
#include "esp_hidd_api.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"

#define FIRMWARE_VERSION "wr104-260701-micboost-i141532"
#define BT_DEVICE_NAME "AirMic WR104"

#define I2S_BCLK_GPIO 14
#define I2S_WS_GPIO 15
#define I2S_SD_GPIO 32
#define I2S_PORT I2S_NUM_0

#define CVSD_RATE_HZ 8000
#define MSBC_RATE_HZ 16000
#define CVSD_FRAME_SAMPLES 30
#define MSBC_FRAME_SAMPLES 120
#define RING_FRAMES 8
#define MIC_GAIN 2

#define LOG_FULL_BDA 0
#define MIC_ONLY_TEST_ENABLED 0
#define MSM3526_STEREO_SCAN_ENABLED 0
#define HFP_TEST_TONE_ENABLED 0
#define HFP_TEST_TONE_HZ 1000
#define HFP_TEST_TONE_AMPLITUDE 9000
#define HFP_REQUEST_DISABLE_NREC 0
#define AUDIO_DSP_ENABLED 0
#define HFP_AUTO_OPEN_SCO 1
#define RAW_I2S_DIAG_ENABLED 0
#define MACAIR_TARGET_AVG_CVSD 2600
#define MACAIR_TARGET_AVG_MSBC 3600
#define MACAIR_AGC_MIN_Q8 256
#define MACAIR_AGC_MAX_Q8 2048
#define MACAIR_NOISE_HOLD_AVG 12
#define MACAIR_NOISE_FLOOR_MIN 8
#define MACAIR_NOISE_FLOOR_MAX 600
#define MACAIR_NOISE_GATE_MULTIPLIER 1
#define MACAIR_NOISE_GATE_OFFSET 18
#define MACAIR_SPEECH_LOW_ATTENUATION_Q8 256
#define MACAIR_SILENCE_ATTENUATION_Q8 224
#define MACAIR_PEAK_LIMIT 28000
#define MACAIR_SOFT_LIMIT 22000

#define PAIRING_WINDOW_MS 60000
#define PAIRING_REOPEN_HOLD_MS 3000
#define PAIRING_CLEAR_HOLD_MS 5000

#define HFP_RECONNECT_MS 5000
#define SCO_RETRY_MS 3000
#define HID_RECONNECT_MS 5000
#define LOW_ACTIVITY_RECONNECT_MS 60000
#define STATUS_MS 3000
#define KEY_DEBOUNCE_MS 60
#define KEY_HOLD_MS 45

#define AUDIO_IDLE_STANDBY_MS (10UL * 60UL * 1000UL)
#define NO_CONNECTION_LOW_ACTIVITY_MS (60UL * 60UL * 1000UL)
#define NO_CONNECTION_LIGHT_SLEEP_MS (2UL * 60UL * 60UL * 1000UL)
#define AUDIO_ACTIVITY_PEAK_THRESHOLD 1200

#define BATTERY_MONITOR_ENABLED 0
#define BATTERY_ADC_CHANNEL ADC_CHANNEL_6
#define BATTERY_ADC_GPIO 34
#define BATTERY_ADC_RAW_PRESENT_MIN 80
#define BATTERY_DIVIDER_MULTIPLIER_X100 200
#define BATTERY_LOW_MV 3500
#define BATTERY_CRITICAL_MV 3300
#define BATTERY_CHECK_MS 30000
#define BATTERY_LOW_LOG_MS 60000
#define CRITICAL_BATTERY_RECHECK_US (10ULL * 60ULL * 1000000ULL)

#define HID_REPORT_ID_KEYBOARD 1
#define HID_KEY_ENTER 0x28
#define HID_KEY_BACKSPACE 0x2A
#define HID_MOD_RIGHT_ALT 0x40

#define HPF_ALPHA_CVSD_Q15 30780
#define HPF_ALPHA_MSBC_Q15 31756
#define NOISE_FLOOR_MIN 180
#define NOISE_FLOOR_MAX 5000
#define NOISE_GATE_MULTIPLIER 3
#define NOISE_GATE_OFFSET 120
#define NOISE_GATE_ATTENUATION_Q8 48
#define AGC_TARGET_AVG 4200
#define AGC_MIN_Q8 128
#define AGC_MAX_Q8 768
#define LIMITER_ABS 28000

static const char *TAG = "AirMicIDF";
static const gpio_num_t KEY_PINS[] = {GPIO_NUM_25, GPIO_NUM_26, GPIO_NUM_13};
static const char *KEY_NAMES[] = {"BACKSPACE", "ENTER", "RIGHT_ALT_OPTION"};
static const uint8_t KEY_COUNT = sizeof(KEY_PINS) / sizeof(KEY_PINS[0]);

static const uint8_t HID_REPORT_MAP[] = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, HID_REPORT_ID_KEYBOARD,
    0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00,
    0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x08, 0x81, 0x01, 0x95, 0x05,
    0x75, 0x01, 0x05, 0x08, 0x19, 0x01, 0x29, 0x05,
    0x91, 0x02, 0x95, 0x01, 0x75, 0x03, 0x91, 0x01,
    0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65,
    0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00,
    0xC0,
};

struct key_state_t {
    gpio_num_t pin;
    int last_raw;
    int stable_state;
    int64_t last_change_ms;
    bool armed;
};

static struct key_state_t key_states[3];

static volatile bool hfp_ready = false;
static volatile bool slc_connected = false;
static volatile bool audio_connected = false;
static volatile bool audio_connecting = false;
static volatile bool have_remote_bda = false;
static volatile bool msbc_active = false;
static volatile bool i2s_running = false;
static volatile bool mic_task_running = false;
static volatile bool hid_ready = false;
static volatile bool hid_connected = false;
static volatile bool pairing_discoverable = false;

static esp_bd_addr_t remote_bda = {0};
static StreamBufferHandle_t mic_ring = NULL;
static TaskHandle_t mic_task = NULL;
static uint32_t last_hfp_connect_ms = 0;
static uint32_t last_sco_connect_ms = 0;
static uint32_t last_hid_connect_ms = 0;
static uint32_t last_status_ms = 0;
static uint32_t last_key_activity_ms = 0;
static uint32_t last_audio_activity_ms = 0;
static uint32_t last_connected_ms = 0;
static uint32_t last_battery_check_ms = 0;
static uint32_t last_battery_low_log_ms = 0;
static uint32_t pairing_started_ms = 0;
static uint32_t combo_pair_started_ms = 0;
static uint32_t combo_clear_started_ms = 0;
static uint32_t mic_frames = 0;
static uint32_t mic_drops = 0;
static int16_t mic_peak = 0;
static bool combo_pair_fired = false;
static bool combo_clear_fired = false;
static bool mic_light_standby = false;
static bool bt_low_activity = false;
static bool battery_monitor_ready = false;
static bool battery_low = false;
static bool battery_critical = false;
static int battery_mv = -1;
static int32_t hpf_prev_x = 0;
static int32_t hpf_prev_y = 0;
static int32_t noise_floor = NOISE_FLOOR_MIN;
static int32_t agc_gain_q8 = 256;

#if BATTERY_MONITOR_ENABLED
static adc_oneshot_unit_handle_t battery_adc_handle = NULL;
#endif

static int32_t i2s_raw[MSBC_FRAME_SAMPLES * (MSM3526_STEREO_SCAN_ENABLED ? 2 : 1)];
static int16_t pcm_frame[MSBC_FRAME_SAMPLES];

static void exit_mic_light_standby(const char *reason);
static void exit_bt_low_activity(const char *reason);
static void close_pairing_window(const char *reason);
static void connect_first_bonded_device(void);

static uint32_t millis_now(void) {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static void log_esp(const char *what, esp_err_t err) {
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s failed: %s (0x%04" PRIX32 ")", what, esp_err_to_name(err), (uint32_t)err);
    } else {
        ESP_LOGI(TAG, "%s ok", what);
    }
}

static void print_bda(const esp_bd_addr_t bda, char *out, size_t out_len) {
#if LOG_FULL_BDA
    snprintf(out, out_len, "%02X:%02X:%02X:%02X:%02X:%02X", bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
#else
    snprintf(out, out_len, "**:**:**:**:%02X:%02X", bda[4], bda[5]);
#endif
}

static void remember_bda(const esp_bd_addr_t bda) {
    memcpy(remote_bda, bda, ESP_BD_ADDR_LEN);
    have_remote_bda = true;
}

static const char *key_name(uint8_t index) {
    if (index < KEY_COUNT) return KEY_NAMES[index];
    return "UNKNOWN";
}

static size_t sco_frame_samples(bool msbc) {
    return msbc ? MSBC_FRAME_SAMPLES : CVSD_FRAME_SAMPLES;
}

static size_t sco_frame_bytes(bool msbc) {
    return sco_frame_samples(msbc) * sizeof(int16_t);
}

static int16_t convert_i2s_mic_sample(int32_t sample) {
    int32_t v = (sample >> 16) * MIC_GAIN;
    if (v > 26000) v = 26000 + ((v - 26000) >> 2);
    if (v < -26000) v = -26000 + ((v + 26000) >> 2);
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return (int16_t)v;
}

#if RAW_I2S_DIAG_ENABLED
typedef struct {
    int32_t raw_min;
    int32_t raw_max;
    int32_t raw_peak;
    int32_t p8;
    int32_t p12;
    int32_t p14;
    int32_t p16;
    bool inited;
} raw_i2s_stats_t;
#endif

#if RAW_I2S_DIAG_ENABLED || MSM3526_STEREO_SCAN_ENABLED
static int32_t abs32_safe(int32_t v) {
    if (v == INT32_MIN) return INT32_MAX;
    return v < 0 ? -v : v;
}
#endif

#if RAW_I2S_DIAG_ENABLED
static void raw_i2s_stats_update(raw_i2s_stats_t *stats, int32_t raw) {
    if (!stats->inited) {
        stats->raw_min = raw;
        stats->raw_max = raw;
        stats->inited = true;
    }
    if (raw < stats->raw_min) stats->raw_min = raw;
    if (raw > stats->raw_max) stats->raw_max = raw;
    int32_t a = abs32_safe(raw);
    if (a > stats->raw_peak) stats->raw_peak = a;
    int32_t a8 = abs32_safe(raw >> 8);
    int32_t a12 = abs32_safe(raw >> 12);
    int32_t a14 = abs32_safe(raw >> 14);
    int32_t a16 = abs32_safe(raw >> 16);
    if (a8 > stats->p8) stats->p8 = a8;
    if (a12 > stats->p12) stats->p12 = a12;
    if (a14 > stats->p14) stats->p14 = a14;
    if (a16 > stats->p16) stats->p16 = a16;
}

static void raw_i2s_stats_reset(raw_i2s_stats_t *stats) {
    memset(stats, 0, sizeof(*stats));
}
#endif

static int16_t clamp16(int32_t v) {
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return (int16_t)v;
}

static void reset_audio_dsp(void) {
    hpf_prev_x = 0;
    hpf_prev_y = 0;
    noise_floor = MACAIR_NOISE_FLOOR_MIN;
    agc_gain_q8 = 512;
}

static int16_t highpass_sample(int16_t sample, bool msbc) {
    int32_t alpha = msbc ? HPF_ALPHA_MSBC_Q15 : HPF_ALPHA_CVSD_Q15;
    int32_t y = (int32_t)sample - hpf_prev_x + ((alpha * hpf_prev_y) >> 15);
    hpf_prev_x = sample;
    hpf_prev_y = y;
    return clamp16(y);
}

static void process_audio_frame(int16_t *samples, size_t count, bool msbc, int16_t *out_peak) {
    (void)msbc;
    if (!samples || count == 0) {
        if (out_peak) *out_peak = 0;
        return;
    }

#if !AUDIO_DSP_ENABLED
    {
    int64_t abs_sum = 0;
    int32_t raw_peak = 0;
    for (size_t i = 0; i < count; ++i) {
        samples[i] = highpass_sample(samples[i], msbc);
        int32_t a = samples[i] < 0 ? -(int32_t)samples[i] : (int32_t)samples[i];
        abs_sum += a;
        if (a > raw_peak) raw_peak = a;
    }

    int32_t avg_abs = (int32_t)(abs_sum / (int64_t)count);
    if (avg_abs < MACAIR_NOISE_FLOOR_MAX) {
        int32_t candidate = avg_abs < MACAIR_NOISE_FLOOR_MIN ? MACAIR_NOISE_FLOOR_MIN : avg_abs;
        if (candidate < noise_floor) {
            noise_floor = (noise_floor * 3 + candidate) / 4;
        } else if (candidate < noise_floor * 2) {
            noise_floor = (noise_floor * 31 + candidate) / 32;
        }
        if (noise_floor < MACAIR_NOISE_FLOOR_MIN) noise_floor = MACAIR_NOISE_FLOOR_MIN;
        if (noise_floor > MACAIR_NOISE_FLOOR_MAX) noise_floor = MACAIR_NOISE_FLOOR_MAX;
    }

    int32_t gate_threshold = noise_floor * MACAIR_NOISE_GATE_MULTIPLIER + MACAIR_NOISE_GATE_OFFSET;
    bool speech_like = avg_abs >= MACAIR_NOISE_HOLD_AVG &&
                       (avg_abs > gate_threshold || raw_peak > gate_threshold * 4);
    int32_t target_gain_q8 = 384;
    if (speech_like && avg_abs > 0) {
        int32_t target_avg = msbc ? MACAIR_TARGET_AVG_MSBC : MACAIR_TARGET_AVG_CVSD;
        target_gain_q8 = (target_avg * 256) / avg_abs;
        if (raw_peak > 0) {
            int32_t peak_limited_q8 = (MACAIR_PEAK_LIMIT * 256) / raw_peak;
            if (target_gain_q8 > peak_limited_q8) target_gain_q8 = peak_limited_q8;
        }
        if (target_gain_q8 < MACAIR_AGC_MIN_Q8) target_gain_q8 = MACAIR_AGC_MIN_Q8;
        if (target_gain_q8 > MACAIR_AGC_MAX_Q8) target_gain_q8 = MACAIR_AGC_MAX_Q8;
    }

    if (target_gain_q8 > agc_gain_q8) {
        agc_gain_q8 = (agc_gain_q8 * 7 + target_gain_q8) / 8;
    } else {
        agc_gain_q8 = (agc_gain_q8 * 3 + target_gain_q8) / 4;
    }

    int32_t final_peak = 0;
    for (size_t i = 0; i < count; ++i) {
        int32_t v = samples[i];
        int32_t a = v < 0 ? -v : v;
        if (a < gate_threshold) {
            int32_t attenuation_q8 = speech_like ? MACAIR_SPEECH_LOW_ATTENUATION_Q8 : MACAIR_SILENCE_ATTENUATION_Q8;
            v = (v * attenuation_q8) >> 8;
        }
        v = (v * agc_gain_q8) >> 8;
        if (v > MACAIR_SOFT_LIMIT) v = MACAIR_SOFT_LIMIT + ((v - MACAIR_SOFT_LIMIT) >> 3);
        if (v < -MACAIR_SOFT_LIMIT) v = -MACAIR_SOFT_LIMIT + ((v + MACAIR_SOFT_LIMIT) >> 3);
        samples[i] = clamp16(v);
        a = samples[i] < 0 ? -(int32_t)samples[i] : (int32_t)samples[i];
        if (a > final_peak) final_peak = a;
    }

    if (out_peak) *out_peak = (int16_t)(final_peak > 32767 ? 32767 : final_peak);
    if (final_peak >= AUDIO_ACTIVITY_PEAK_THRESHOLD) {
        last_audio_activity_ms = millis_now();
    }
    return;
    }
#endif

    int64_t abs_sum = 0;
    int32_t frame_peak = 0;
    for (size_t i = 0; i < count; ++i) {
        int16_t filtered = highpass_sample(samples[i], msbc);
        samples[i] = filtered;
        int32_t a = filtered < 0 ? -(int32_t)filtered : (int32_t)filtered;
        abs_sum += a;
        if (a > frame_peak) frame_peak = a;
    }

    int32_t avg_abs = (int32_t)(abs_sum / (int64_t)count);
    if (avg_abs < NOISE_FLOOR_MAX) {
        int32_t candidate = avg_abs < NOISE_FLOOR_MIN ? NOISE_FLOOR_MIN : avg_abs;
        if (candidate < noise_floor) {
            noise_floor = (noise_floor * 7 + candidate) / 8;
        } else if (candidate < noise_floor * 2) {
            noise_floor = (noise_floor * 31 + candidate) / 32;
        }
        if (noise_floor < NOISE_FLOOR_MIN) noise_floor = NOISE_FLOOR_MIN;
        if (noise_floor > NOISE_FLOOR_MAX) noise_floor = NOISE_FLOOR_MAX;
    }

    int32_t gate_threshold = noise_floor * NOISE_GATE_MULTIPLIER + NOISE_GATE_OFFSET;
    bool speech_like = avg_abs > gate_threshold || frame_peak > gate_threshold * 3;
    int32_t target_gain_q8 = agc_gain_q8;
    if (speech_like && avg_abs > 0) {
        target_gain_q8 = (AGC_TARGET_AVG * 256) / avg_abs;
        if (target_gain_q8 < AGC_MIN_Q8) target_gain_q8 = AGC_MIN_Q8;
        if (target_gain_q8 > AGC_MAX_Q8) target_gain_q8 = AGC_MAX_Q8;
    } else {
        target_gain_q8 = agc_gain_q8 > 256 ? 256 : agc_gain_q8;
    }
    agc_gain_q8 = (agc_gain_q8 * 7 + target_gain_q8) / 8;

    int32_t final_peak = 0;
    for (size_t i = 0; i < count; ++i) {
        int32_t s = samples[i];
        int32_t a = s < 0 ? -s : s;
        if (!speech_like || a < gate_threshold) {
            s = (s * NOISE_GATE_ATTENUATION_Q8) >> 8;
        }
        s = (s * agc_gain_q8) >> 8;
        if (s > LIMITER_ABS) s = LIMITER_ABS;
        if (s < -LIMITER_ABS) s = -LIMITER_ABS;
        samples[i] = (int16_t)s;
        a = s < 0 ? -s : s;
        if (a > final_peak) final_peak = a;
    }

	    if (out_peak) *out_peak = (int16_t)(final_peak > 32767 ? 32767 : final_peak);
	    if (final_peak >= AUDIO_ACTIVITY_PEAK_THRESHOLD) {
	        last_audio_activity_ms = millis_now();
	    }
	}

static const char *hfp_conn_state_name(esp_hf_client_connection_state_t s) {
    switch (s) {
        case ESP_HF_CLIENT_CONNECTION_STATE_DISCONNECTED: return "DISCONNECTED";
        case ESP_HF_CLIENT_CONNECTION_STATE_CONNECTING: return "CONNECTING";
        case ESP_HF_CLIENT_CONNECTION_STATE_CONNECTED: return "RFCOMM_CONNECTED";
        case ESP_HF_CLIENT_CONNECTION_STATE_SLC_CONNECTED: return "SLC_CONNECTED";
        case ESP_HF_CLIENT_CONNECTION_STATE_DISCONNECTING: return "DISCONNECTING";
        default: return "UNKNOWN";
    }
}

static const char *hfp_audio_state_name(esp_hf_client_audio_state_t s) {
    switch (s) {
        case ESP_HF_CLIENT_AUDIO_STATE_DISCONNECTED: return "DISCONNECTED";
        case ESP_HF_CLIENT_AUDIO_STATE_CONNECTING: return "CONNECTING";
        case ESP_HF_CLIENT_AUDIO_STATE_CONNECTED: return "CONNECTED_CVSD";
        case ESP_HF_CLIENT_AUDIO_STATE_CONNECTED_MSBC: return "CONNECTED_MSBC";
        default: return "UNKNOWN";
    }
}

static void start_i2s(bool msbc) {
    if (i2s_running) return;
    i2s_driver_uninstall(I2S_PORT);

    i2s_config_t cfg = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX,
        .sample_rate = msbc ? MSBC_RATE_HZ : CVSD_RATE_HZ,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
#if MSM3526_STEREO_SCAN_ENABLED
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
#else
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
#endif
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = (int)sco_frame_samples(msbc),
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0,
    };
    i2s_pin_config_t pins = {
        .mck_io_num = I2S_PIN_NO_CHANGE,
        .bck_io_num = I2S_BCLK_GPIO,
        .ws_io_num = I2S_WS_GPIO,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SD_GPIO,
    };

    esp_err_t err = i2s_driver_install(I2S_PORT, &cfg, 0, NULL);
    if (err != ESP_OK) {
        log_esp("i2s_driver_install", err);
        return;
    }
    err = i2s_set_pin(I2S_PORT, &pins);
    if (err != ESP_OK) {
        log_esp("i2s_set_pin", err);
        i2s_driver_uninstall(I2S_PORT);
        return;
    }
    i2s_zero_dma_buffer(I2S_PORT);
    i2s_start(I2S_PORT);
    i2s_running = true;
    ESP_LOGI(TAG, "INMP441 I2S started fs=%d SCK/BCLK=%d WS=%d SD=%d stereoScan=%s",
             cfg.sample_rate,
             I2S_BCLK_GPIO,
             I2S_WS_GPIO,
             I2S_SD_GPIO,
             MSM3526_STEREO_SCAN_ENABLED ? "yes" : "no");
}

static void stop_i2s(void) {
    if (!i2s_running) return;
    i2s_running = false;
    vTaskDelay(pdMS_TO_TICKS(20));
    i2s_stop(I2S_PORT);
    i2s_driver_uninstall(I2S_PORT);
    ESP_LOGI(TAG, "I2S stopped");
}

static void mic_capture_task(void *arg) {
    bool task_msbc = ((uintptr_t)arg) != 0;
    size_t samples = sco_frame_samples(task_msbc);
    size_t raw_words = samples * (MSM3526_STEREO_SCAN_ENABLED ? 2 : 1);
    size_t raw_bytes = raw_words * sizeof(int32_t);
    size_t pcm_bytes = samples * sizeof(int16_t);

    while (mic_task_running) {
        size_t bytes_read = 0;
        esp_err_t err = i2s_read(I2S_PORT, i2s_raw, raw_bytes, &bytes_read, pdMS_TO_TICKS(20));
        if (!mic_task_running) break;
        if (err != ESP_OK || bytes_read < raw_bytes) continue;

#if MSM3526_STEREO_SCAN_ENABLED
        int64_t left_energy = 0;
        int64_t right_energy = 0;
        for (size_t i = 0; i < samples; ++i) {
            left_energy += abs32_safe(i2s_raw[i * 2] >> 16);
            right_energy += abs32_safe(i2s_raw[i * 2 + 1] >> 16);
        }
        bool use_right = right_energy > left_energy;
#endif

#if RAW_I2S_DIAG_ENABLED
        {
            static uint32_t diag_last_ms = 0;
            static uint32_t diag_frames = 0;
#if MSM3526_STEREO_SCAN_ENABLED
            static raw_i2s_stats_t diag_left = {0};
            static raw_i2s_stats_t diag_right = {0};
            static uint32_t diag_use_left = 0;
            static uint32_t diag_use_right = 0;
#else
            static raw_i2s_stats_t diag_mono = {0};
#endif

            for (size_t i = 0; i < samples; ++i) {
#if MSM3526_STEREO_SCAN_ENABLED
                raw_i2s_stats_update(&diag_left, i2s_raw[i * 2]);
                raw_i2s_stats_update(&diag_right, i2s_raw[i * 2 + 1]);
#else
                int32_t raw = i2s_raw[i];
                raw_i2s_stats_update(&diag_mono, raw);
#endif
            }
            diag_frames++;
#if MSM3526_STEREO_SCAN_ENABLED
            if (use_right) {
                diag_use_right++;
            } else {
                diag_use_left++;
            }
#endif

            uint32_t now = millis_now();
            if (diag_last_ms == 0) diag_last_ms = now;
            if (now - diag_last_ms >= 1000) {
#if MSM3526_STEREO_SCAN_ENABLED
                ESP_LOGI(TAG,
                         "RAW_I2S_MSM3526 frames=%lu choose=L:%lu/R:%lu L[min=%ld max=%ld peak=%ld p16=%ld] R[min=%ld max=%ld peak=%ld p16=%ld]",
                         (unsigned long)diag_frames,
                         (unsigned long)diag_use_left,
                         (unsigned long)diag_use_right,
                         (long)diag_left.raw_min,
                         (long)diag_left.raw_max,
                         (long)diag_left.raw_peak,
                         (long)diag_left.p16,
                         (long)diag_right.raw_min,
                         (long)diag_right.raw_max,
                         (long)diag_right.raw_peak,
                         (long)diag_right.p16);
                diag_frames = 0;
                diag_use_left = 0;
                diag_use_right = 0;
                raw_i2s_stats_reset(&diag_left);
                raw_i2s_stats_reset(&diag_right);
#else
                ESP_LOGI(TAG,
                         "RAW_I2S ch=LEFT frames=%lu rawMin=%ld rawMax=%ld rawPeak=%ld p8=%ld p12=%ld p14=%ld p16=%ld",
                         (unsigned long)diag_frames,
                         (long)diag_mono.raw_min,
                         (long)diag_mono.raw_max,
                         (long)diag_mono.raw_peak,
                         (long)diag_mono.p8,
                         (long)diag_mono.p12,
                         (long)diag_mono.p14,
                         (long)diag_mono.p16);
                diag_frames = 0;
                raw_i2s_stats_reset(&diag_mono);
#endif
                diag_last_ms = now;
            }
        }
#endif

        for (size_t i = 0; i < samples; ++i) {
#if MSM3526_STEREO_SCAN_ENABLED
            int32_t raw = use_right ? i2s_raw[i * 2 + 1] : i2s_raw[i * 2];
            pcm_frame[i] = convert_i2s_mic_sample(raw);
#else
            pcm_frame[i] = convert_i2s_mic_sample(i2s_raw[i]);
#endif
        }
        int16_t peak = 0;
        process_audio_frame(pcm_frame, samples, task_msbc, &peak);
        if (peak > mic_peak) mic_peak = peak;

#if HFP_TEST_TONE_ENABLED
        mic_frames++;
        esp_hf_client_outgoing_data_ready();
        continue;
#endif

        if (!mic_ring || xStreamBufferSpacesAvailable(mic_ring) < pcm_bytes) {
            mic_drops++;
            continue;
        }
        size_t sent = xStreamBufferSend(mic_ring, pcm_frame, pcm_bytes, 0);
        if (sent == pcm_bytes) {
            mic_frames++;
            esp_hf_client_outgoing_data_ready();
        } else {
            mic_drops++;
        }
    }

    mic_task = NULL;
    vTaskDelete(NULL);
}

static void start_audio_path(bool msbc) {
    if (mic_task || mic_ring) return;
    msbc_active = msbc;
    mic_light_standby = false;
    last_audio_activity_ms = millis_now();
    start_i2s(msbc);
    if (!i2s_running) return;

    size_t frame_bytes = sco_frame_bytes(msbc);
    mic_ring = xStreamBufferCreate(frame_bytes * RING_FRAMES, frame_bytes);
    if (!mic_ring) {
        ESP_LOGE(TAG, "xStreamBufferCreate failed");
        stop_i2s();
        return;
    }
    mic_frames = 0;
    mic_drops = 0;
    mic_peak = 0;
    reset_audio_dsp();
    mic_task_running = true;
    if (xTaskCreate(mic_capture_task, "hfp_mic", 4096, (void *)(uintptr_t)msbc, 5, &mic_task) != pdPASS) {
        ESP_LOGE(TAG, "xTaskCreate mic_capture_task failed");
        mic_task_running = false;
        vStreamBufferDelete(mic_ring);
        mic_ring = NULL;
        stop_i2s();
    }
}

static void stop_audio_path(void) {
    mic_task_running = false;
    while (mic_task) vTaskDelay(pdMS_TO_TICKS(1));
    if (mic_ring) {
        vStreamBufferDelete(mic_ring);
        mic_ring = NULL;
    }
    stop_i2s();
}

static bool any_bt_connected(void) {
#if MIC_ONLY_TEST_ENABLED
    return slc_connected || audio_connected || audio_connecting;
#else
    return hid_connected || slc_connected || audio_connected || audio_connecting;
#endif
}

static void note_connected_activity(void) {
    last_connected_ms = millis_now();
    if (bt_low_activity) {
        exit_bt_low_activity("connection active");
    }
}

static void enter_mic_light_standby(const char *reason) {
    if (mic_light_standby || !audio_connected || !mic_task_running) return;
    mic_light_standby = true;
    ESP_LOGI(TAG, "Light standby: stopping INMP441 sampling after idle (%s)", reason);
    stop_audio_path();
}

static void exit_mic_light_standby(const char *reason) {
    if (!mic_light_standby) return;
    mic_light_standby = false;
    last_audio_activity_ms = millis_now();
    ESP_LOGI(TAG, "Light standby exit: restoring microphone path (%s)", reason);
    if (audio_connected) {
        start_audio_path(msbc_active);
    }
}

static void enter_bt_low_activity(const char *reason) {
    if (bt_low_activity || any_bt_connected() || pairing_discoverable) return;
    bt_low_activity = true;
    ESP_LOGI(TAG, "Bluetooth low-activity mode after no connection (%s)", reason);
    log_esp("esp_bt_gap_set_scan_mode(low activity)", esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_NON_DISCOVERABLE));
}

static void exit_bt_low_activity(const char *reason) {
    if (!bt_low_activity) return;
    bt_low_activity = false;
    last_hfp_connect_ms = 0;
    last_hid_connect_ms = 0;
    last_sco_connect_ms = 0;
    last_connected_ms = millis_now();
    ESP_LOGI(TAG, "Bluetooth low-activity exit (%s)", reason);
    log_esp("esp_bt_gap_set_scan_mode(active hidden)", esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_NON_DISCOVERABLE));
}

static void note_key_activity(void) {
    last_key_activity_ms = millis_now();
    if (mic_light_standby) {
        exit_mic_light_standby("key press");
    }
    if (bt_low_activity) {
        exit_bt_low_activity("key press");
        connect_first_bonded_device();
    }
}

static bool any_key_pressed_raw(void) {
    for (uint8_t i = 0; i < KEY_COUNT; ++i) {
        if (gpio_get_level(KEY_PINS[i]) == 1) return true;
    }
    return false;
}

static void enter_button_wake_light_sleep(const char *reason) {
    if (any_bt_connected()) return;
    ESP_LOGW(TAG, "Entering button-wake light sleep (%s). Press any key to wake.", reason);
    close_pairing_window("button-wake sleep");
    stop_audio_path();
    log_esp("esp_bt_gap_set_scan_mode(sleep)", esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE));

    for (uint8_t i = 0; i < KEY_COUNT; ++i) {
        gpio_wakeup_enable(KEY_PINS[i], GPIO_INTR_HIGH_LEVEL);
    }
    log_esp("esp_sleep_enable_gpio_wakeup", esp_sleep_enable_gpio_wakeup());

    esp_err_t err = esp_light_sleep_start();
    if (err != ESP_OK) {
        log_esp("esp_light_sleep_start", err);
    }

    for (uint8_t i = 0; i < KEY_COUNT; ++i) {
        gpio_wakeup_disable(KEY_PINS[i]);
    }
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);

    uint32_t now = millis_now();
    last_key_activity_ms = now;
    last_audio_activity_ms = now;
    last_connected_ms = now;
    bt_low_activity = false;
    mic_light_standby = false;
    ESP_LOGI(TAG, "Woke from light sleep, wake_cause=%d", esp_sleep_get_wakeup_cause());
    log_esp("esp_bt_gap_set_scan_mode(wake)", esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_NON_DISCOVERABLE));
    connect_first_bonded_device();
}

static void enter_critical_battery_sleep(void) {
    ESP_LOGE(TAG, "Battery critical: %dmV <= %dmV. Disconnecting Bluetooth and sleeping to protect battery.",
             battery_mv, BATTERY_CRITICAL_MV);
    stop_audio_path();
    if (have_remote_bda) {
        if (audio_connected || audio_connecting) {
            log_esp("esp_hf_client_disconnect_audio", esp_hf_client_disconnect_audio(remote_bda));
        }
        if (slc_connected) {
            log_esp("esp_hf_client_disconnect", esp_hf_client_disconnect(remote_bda));
        }
    }
    if (hid_connected) {
        log_esp("esp_bt_hid_device_disconnect", esp_bt_hid_device_disconnect());
    }
    vTaskDelay(pdMS_TO_TICKS(500));
    log_esp("esp_bt_gap_set_scan_mode(critical sleep)", esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE));
    log_esp("esp_sleep_enable_timer_wakeup", esp_sleep_enable_timer_wakeup(CRITICAL_BATTERY_RECHECK_US));
    esp_deep_sleep_start();
}

static void setup_battery_monitor(void) {
#if MIC_ONLY_TEST_ENABLED
    battery_monitor_ready = false;
    ESP_LOGI(TAG, "MIC_ONLY_TEST: battery monitor disabled");
    return;
#endif
#if BATTERY_MONITOR_ENABLED
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    esp_err_t err = adc_oneshot_new_unit(&init_cfg, &battery_adc_handle);
    if (err != ESP_OK) {
        log_esp("adc_oneshot_new_unit", err);
        return;
    }
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    err = adc_oneshot_config_channel(battery_adc_handle, BATTERY_ADC_CHANNEL, &chan_cfg);
    if (err != ESP_OK) {
        log_esp("adc_oneshot_config_channel", err);
        return;
    }
    battery_monitor_ready = true;
    last_battery_check_ms = millis_now() - BATTERY_CHECK_MS;
    ESP_LOGI(TAG, "Battery monitor enabled on GPIO%d. Divider multiplier=%d.%02d",
             BATTERY_ADC_GPIO,
             BATTERY_DIVIDER_MULTIPLIER_X100 / 100,
             BATTERY_DIVIDER_MULTIPLIER_X100 % 100);
#else
    battery_monitor_ready = false;
    ESP_LOGI(TAG, "Battery monitor disabled. Wire battery divider to GPIO34 and set BATTERY_MONITOR_ENABLED=1 to enable.");
#endif
}

static int read_battery_mv(void) {
#if BATTERY_MONITOR_ENABLED
    if (!battery_monitor_ready || !battery_adc_handle) return -1;
    int raw = 0;
    esp_err_t err = adc_oneshot_read(battery_adc_handle, BATTERY_ADC_CHANNEL, &raw);
    if (err != ESP_OK) {
        log_esp("adc_oneshot_read", err);
        return -1;
    }
    if (raw < BATTERY_ADC_RAW_PRESENT_MIN) return -1;
    int sense_mv = (raw * 3300) / 4095;
    return (sense_mv * BATTERY_DIVIDER_MULTIPLIER_X100) / 100;
#else
    return -1;
#endif
}

static void service_battery(void) {
#if MIC_ONLY_TEST_ENABLED
    return;
#endif
    uint32_t now = millis_now();
    if (now - last_battery_check_ms < BATTERY_CHECK_MS) return;
    last_battery_check_ms = now;

    int mv = read_battery_mv();
    if (mv <= 0) {
        battery_mv = -1;
        battery_low = false;
        battery_critical = false;
        return;
    }

    battery_mv = mv;
    battery_low = mv <= BATTERY_LOW_MV;
    battery_critical = mv <= BATTERY_CRITICAL_MV;

    if (battery_critical) {
        enter_critical_battery_sleep();
    } else if (battery_low && now - last_battery_low_log_ms >= BATTERY_LOW_LOG_MS) {
        last_battery_low_log_ms = now;
        ESP_LOGW(TAG, "LOW BATTERY: %dmV <= %dmV. Keys still work; recharge soon.", battery_mv, BATTERY_LOW_MV);
    }
}

static void service_power_management(void) {
#if MIC_ONLY_TEST_ENABLED
    return;
#endif
    uint32_t now = millis_now();
    if (any_bt_connected()) {
        last_connected_ms = now;
    }

    uint32_t last_activity_ms = last_key_activity_ms;
    if (last_audio_activity_ms > last_activity_ms) last_activity_ms = last_audio_activity_ms;

    if (audio_connected && mic_task_running && !mic_light_standby &&
        now - last_activity_ms >= AUDIO_IDLE_STANDBY_MS) {
        enter_mic_light_standby("10min no audio/key");
    }

    if (!any_bt_connected() && !pairing_discoverable) {
        uint32_t no_connection_ms = now - last_connected_ms;
        if (no_connection_ms >= NO_CONNECTION_LIGHT_SLEEP_MS) {
            enter_button_wake_light_sleep("2h no connection");
        } else if (no_connection_ms >= NO_CONNECTION_LOW_ACTIVITY_MS) {
            enter_bt_low_activity("60min no connection");
        }
    }
}

static uint32_t hf_outgoing_data_cb(uint8_t *buf, uint32_t len) {
    if (!buf || len == 0) return 0;
#if HFP_TEST_TONE_ENABLED
    static uint32_t tone_phase = 0;
    uint32_t sample_rate = msbc_active ? MSBC_RATE_HZ : CVSD_RATE_HZ;
    uint32_t half_period = sample_rate / (HFP_TEST_TONE_HZ * 2);
    if (half_period == 0) half_period = 1;

    int16_t *pcm = (int16_t *)buf;
    uint32_t samples = len / sizeof(int16_t);
    for (uint32_t i = 0; i < samples; ++i) {
        pcm[i] = ((tone_phase / half_period) & 1U) ? HFP_TEST_TONE_AMPLITUDE : -HFP_TEST_TONE_AMPLITUDE;
        tone_phase++;
    }
    if (len & 1U) buf[len - 1] = 0;
    if (mic_peak < HFP_TEST_TONE_AMPLITUDE) mic_peak = HFP_TEST_TONE_AMPLITUDE;
    return len;
#endif
    if (!mic_ring || xStreamBufferBytesAvailable(mic_ring) < len) {
        memset(buf, 0, len);
        return len;
    }
    size_t got = xStreamBufferReceive(mic_ring, buf, len, 0);
    if (got < len) memset(buf + got, 0, len - got);
    return len;
}

static void hf_incoming_data_cb(const uint8_t *buf, uint32_t len) {
    (void)buf;
    (void)len;
}

static void try_connect_hfp(const char *reason) {
    if (pairing_discoverable) return;
    if (!hfp_ready || !have_remote_bda || slc_connected) return;
    uint32_t now = millis_now();
    uint32_t reconnect_ms = bt_low_activity ? LOW_ACTIVITY_RECONNECT_MS : HFP_RECONNECT_MS;
    if (now - last_hfp_connect_ms < reconnect_ms) return;
    last_hfp_connect_ms = now;
    char bda[18];
    print_bda(remote_bda, bda, sizeof(bda));
    ESP_LOGI(TAG, "HFP connect (%s) to %s", reason, bda);
    log_esp("esp_hf_client_connect", esp_hf_client_connect(remote_bda));
}

static void try_connect_hid(const char *reason) {
#if MIC_ONLY_TEST_ENABLED
    (void)reason;
    return;
#endif
    if (pairing_discoverable) return;
    if (!hid_ready || !have_remote_bda || hid_connected) return;
    uint32_t now = millis_now();
    uint32_t reconnect_ms = bt_low_activity ? LOW_ACTIVITY_RECONNECT_MS : HID_RECONNECT_MS;
    if (now - last_hid_connect_ms < reconnect_ms) return;
    last_hid_connect_ms = now;
    char bda[18];
    print_bda(remote_bda, bda, sizeof(bda));
    ESP_LOGI(TAG, "HID connect (%s) to %s", reason, bda);
    log_esp("esp_bt_hid_device_connect", esp_bt_hid_device_connect(remote_bda));
}

static void try_connect_audio(const char *reason) {
    if (!have_remote_bda || !slc_connected || audio_connected || audio_connecting) return;
    uint32_t now = millis_now();
    if (now - last_sco_connect_ms < SCO_RETRY_MS) return;
    last_sco_connect_ms = now;
    audio_connecting = true;
    ESP_LOGI(TAG, "open SCO audio (%s)", reason);
    log_esp("esp_hf_client_connect_audio", esp_hf_client_connect_audio(remote_bda));
}

static bool bda_equal(const esp_bd_addr_t a, const esp_bd_addr_t b) {
    return memcmp(a, b, ESP_BD_ADDR_LEN) == 0;
}

static void open_pairing_window(const char *reason) {
    pairing_discoverable = true;
    pairing_started_ms = millis_now();
    ESP_LOGI(TAG, "Pairing window opened for %" PRIu32 " ms (%s)", (uint32_t)PAIRING_WINDOW_MS, reason);
    log_esp("esp_bt_gap_set_scan_mode(pairing)", esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE));
}

static void close_pairing_window(const char *reason) {
    if (!pairing_discoverable) return;
    pairing_discoverable = false;
    ESP_LOGI(TAG, "Pairing window closed (%s)", reason);
    log_esp("esp_bt_gap_set_scan_mode(hidden)", esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_NON_DISCOVERABLE));
}

static void enforce_single_bond(const esp_bd_addr_t keep) {
    int count = esp_bt_gap_get_bond_device_num();
    if (count <= 1) return;
    if (count > 8) count = 8;
    esp_bd_addr_t bonded[8];
    int request_count = count;
    esp_err_t err = esp_bt_gap_get_bond_device_list(&request_count, bonded);
    if (err != ESP_OK) {
        log_esp("esp_bt_gap_get_bond_device_list", err);
        return;
    }
    for (int i = 0; i < request_count; ++i) {
        if (!bda_equal(bonded[i], keep)) {
            char bda[18];
            print_bda(bonded[i], bda, sizeof(bda));
            ESP_LOGI(TAG, "Removing extra bonded host %s", bda);
            log_esp("esp_bt_gap_remove_bond_device", esp_bt_gap_remove_bond_device(bonded[i]));
        }
    }
}

static void clear_all_bonds_and_restart(void) {
    int count = esp_bt_gap_get_bond_device_num();
    ESP_LOGW(TAG, "Clearing %" PRIu32 " bonded host(s), then restarting", (uint32_t)(count < 0 ? 0 : count));
    if (count > 8) count = 8;
    if (count > 0) {
        esp_bd_addr_t bonded[8];
        int request_count = count;
        if (esp_bt_gap_get_bond_device_list(&request_count, bonded) == ESP_OK) {
            for (int i = 0; i < request_count; ++i) {
                log_esp("esp_bt_gap_remove_bond_device", esp_bt_gap_remove_bond_device(bonded[i]));
            }
        }
    }
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
}

static void service_pairing_window(void) {
    if (!pairing_discoverable) return;
    uint32_t now = millis_now();
    if (now - pairing_started_ms >= PAIRING_WINDOW_MS) {
        close_pairing_window("timeout");
    }
}

static bool handle_pairing_shortcuts(void) {
    bool key0 = gpio_get_level(KEY_PINS[0]) == 1;
    bool key1 = gpio_get_level(KEY_PINS[1]) == 1;
    bool key2 = gpio_get_level(KEY_PINS[2]) == 1;
    bool clear_combo = key0 && key1;
    bool pair_combo = key1 && key2;
    uint32_t now = millis_now();

    if (clear_combo) {
        if (combo_clear_started_ms == 0) combo_clear_started_ms = now;
        if (!combo_clear_fired && now - combo_clear_started_ms >= PAIRING_CLEAR_HOLD_MS) {
            combo_clear_fired = true;
            clear_all_bonds_and_restart();
        }
        return true;
    }
    combo_clear_started_ms = 0;
    combo_clear_fired = false;

    if (pair_combo) {
        if (combo_pair_started_ms == 0) combo_pair_started_ms = now;
        if (!combo_pair_fired && now - combo_pair_started_ms >= PAIRING_REOPEN_HOLD_MS) {
            combo_pair_fired = true;
            open_pairing_window("key combo");
        }
        return true;
    }
    combo_pair_started_ms = 0;
    combo_pair_fired = false;

    return false;
}

static void send_hid_report(uint8_t modifier, uint8_t key) {
    if (!hid_connected) {
        ESP_LOGW(TAG, "KEY ignored, HID not connected");
        try_connect_hid("key pressed");
        return;
    }
    uint8_t report[8] = {modifier, 0, key, 0, 0, 0, 0, 0};
    log_esp("esp_bt_hid_device_send_report(press)", esp_bt_hid_device_send_report(
        ESP_HIDD_REPORT_TYPE_INTRDATA, HID_REPORT_ID_KEYBOARD, sizeof(report), report));
    vTaskDelay(pdMS_TO_TICKS(KEY_HOLD_MS));
    memset(report, 0, sizeof(report));
    log_esp("esp_bt_hid_device_send_report(release)", esp_bt_hid_device_send_report(
        ESP_HIDD_REPORT_TYPE_INTRDATA, HID_REPORT_ID_KEYBOARD, sizeof(report), report));
}

static void handle_key_command(uint8_t index) {
    note_key_activity();
    ESP_LOGI(TAG, "KEY %s", key_name(index));
    if (battery_low) {
        ESP_LOGW(TAG, "LOW BATTERY key notice: key=%s battery=%dmV threshold=%dmV",
                 key_name(index), battery_mv, BATTERY_LOW_MV);
    }
    if (index == 0) {
        send_hid_report(0, HID_KEY_BACKSPACE);
    } else if (index == 1) {
        send_hid_report(0, HID_KEY_ENTER);
    } else {
        send_hid_report(HID_MOD_RIGHT_ALT, 0);
    }
}

static void setup_keys(void) {
#if MIC_ONLY_TEST_ENABLED
    ESP_LOGI(TAG, "MIC_ONLY_TEST: keys disabled");
    return;
#endif
    for (uint8_t i = 0; i < KEY_COUNT; ++i) {
        gpio_config_t cfg = {
            .pin_bit_mask = 1ULL << KEY_PINS[i],
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_ENABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&cfg);
        int raw = gpio_get_level(KEY_PINS[i]);
        key_states[i].pin = KEY_PINS[i];
        key_states[i].last_raw = raw;
        key_states[i].stable_state = raw;
        key_states[i].last_change_ms = millis_now();
        key_states[i].armed = raw == 0;
        ESP_LOGI(TAG, "KEY_INIT pin=%d raw=%d", KEY_PINS[i], raw);
    }
}

static void handle_keys(void) {
#if MIC_ONLY_TEST_ENABLED
    return;
#endif
    if (any_key_pressed_raw()) {
        note_key_activity();
    }
    if (handle_pairing_shortcuts()) return;

    int64_t now = millis_now();
    for (uint8_t i = 0; i < KEY_COUNT; ++i) {
        struct key_state_t *state = &key_states[i];
        int raw = gpio_get_level(state->pin);
        if (raw != state->last_raw) {
            state->last_raw = raw;
            state->last_change_ms = now;
        }
        if (raw != state->stable_state && now - state->last_change_ms >= KEY_DEBOUNCE_MS) {
            state->stable_state = raw;
            if (state->stable_state == 1 && state->armed) {
                state->armed = false;
                handle_key_command(i);
            } else if (state->stable_state == 0) {
                state->armed = true;
            }
        }
    }
}

static void connect_first_bonded_device(void) {
    int count = esp_bt_gap_get_bond_device_num();
    if (count <= 0) {
        ESP_LOGI(TAG, "No bonded host. Pair %s from macOS/Windows Bluetooth settings.", BT_DEVICE_NAME);
        open_pairing_window("no bonded host");
        return;
    }
    if (count > 8) count = 8;
    esp_bd_addr_t bonded[8];
    int request_count = count;
    esp_err_t err = esp_bt_gap_get_bond_device_list(&request_count, bonded);
    if (err != ESP_OK || request_count <= 0) {
        log_esp("esp_bt_gap_get_bond_device_list", err);
        return;
    }
    remember_bda(bonded[0]);
    char bda[18];
    print_bda(remote_bda, bda, sizeof(bda));
    ESP_LOGI(TAG, "bonded host found: %s", bda);
    close_pairing_window("bonded host");
    try_connect_hid("bonded host");
    try_connect_hfp("bonded host");
}

static void hid_callback(esp_hidd_cb_event_t event, esp_hidd_cb_param_t *param) {
    switch (event) {
        case ESP_HIDD_INIT_EVT:
            hid_ready = (param->init.status == ESP_HIDD_SUCCESS);
            ESP_LOGI(TAG, "HID init status=%d ready=%s", param->init.status, hid_ready ? "yes" : "no");
            if (hid_ready) {
                esp_hidd_app_param_t app = {
                    .name = BT_DEVICE_NAME,
                    .description = "AirMic WR104 Keyboard",
                    .provider = "AirMic",
                    .subclass = ESP_HID_CLASS_KBD,
                    .desc_list = (uint8_t *)HID_REPORT_MAP,
                    .desc_list_len = sizeof(HID_REPORT_MAP),
                };
                esp_hidd_qos_param_t qos = {0};
                log_esp("esp_bt_hid_device_register_app", esp_bt_hid_device_register_app(&app, &qos, &qos));
            }
            break;
        case ESP_HIDD_REGISTER_APP_EVT:
            ESP_LOGI(TAG, "HID app registered status=%d in_use=%s", param->register_app.status, param->register_app.in_use ? "yes" : "no");
            if (param->register_app.status == ESP_HIDD_SUCCESS && param->register_app.in_use) {
                remember_bda(param->register_app.bd_addr);
                ESP_LOGI(TAG, "HID virtual cable host remembered; waiting for host initiated connection");
            }
            break;
        case ESP_HIDD_OPEN_EVT:
            hid_connected = (param->open.status == ESP_HIDD_SUCCESS && param->open.conn_status == ESP_HIDD_CONN_STATE_CONNECTED);
            remember_bda(param->open.bd_addr);
            ESP_LOGI(TAG, "HID open status=%d state=%d connected=%s", param->open.status, param->open.conn_status, hid_connected ? "yes" : "no");
            if (hid_connected) note_connected_activity();
            break;
        case ESP_HIDD_CLOSE_EVT:
            hid_connected = false;
            ESP_LOGI(TAG, "HID close status=%d state=%d", param->close.status, param->close.conn_status);
            break;
        case ESP_HIDD_GET_REPORT_EVT: {
            uint8_t report[8] = {0};
            ESP_LOGI(TAG, "HID get report type=%d id=%u size=%u", param->get_report.report_type, param->get_report.report_id, param->get_report.buffer_size);
            esp_bt_hid_device_send_report(ESP_HIDD_REPORT_TYPE_INPUT, HID_REPORT_ID_KEYBOARD, sizeof(report), report);
            break;
        }
        case ESP_HIDD_SET_PROTOCOL_EVT:
            ESP_LOGI(TAG, "HID protocol mode=%d", param->set_protocol.protocol_mode);
            break;
        case ESP_HIDD_SEND_REPORT_EVT:
            if (param->send_report.status != ESP_HIDD_SUCCESS) {
                ESP_LOGW(TAG, "HID send report status=%d reason=%u", param->send_report.status, param->send_report.reason);
            }
            break;
        default:
            break;
    }
}

static void gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    switch (event) {
        case ESP_BT_GAP_AUTH_CMPL_EVT:
            if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
                remember_bda(param->auth_cmpl.bda);
#if LOG_FULL_BDA
                ESP_LOGI(TAG, "pairing complete: %s", param->auth_cmpl.device_name);
#else
                ESP_LOGI(TAG, "pairing complete");
#endif
                enforce_single_bond(param->auth_cmpl.bda);
                close_pairing_window("pairing complete");
                try_connect_hid("pairing complete");
                try_connect_hfp("pairing complete");
            } else {
                ESP_LOGW(TAG, "pairing failed status=%d", param->auth_cmpl.stat);
            }
            break;
        case ESP_BT_GAP_CFM_REQ_EVT:
            remember_bda(param->cfm_req.bda);
            ESP_LOGI(TAG, "SSP confirm %06" PRIu32, param->cfm_req.num_val);
            esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
            break;
        case ESP_BT_GAP_PIN_REQ_EVT: {
            esp_bt_pin_code_t pin = {'0', '0', '0', '0'};
            remember_bda(param->pin_req.bda);
            ESP_LOGI(TAG, "legacy PIN requested -> 0000");
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin);
            break;
        }
        default:
            break;
    }
}

static void hf_client_event_cb(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *param) {
    switch (event) {
        case ESP_HF_CLIENT_PROF_STATE_EVT:
            hfp_ready = (param->prof_stat.state == ESP_HF_INIT_SUCCESS || param->prof_stat.state == ESP_HF_INIT_ALREADY);
            ESP_LOGI(TAG, "HFP profile ready=%s", hfp_ready ? "yes" : "no");
            if (hfp_ready) connect_first_bonded_device();
            break;
        case ESP_HF_CLIENT_CONNECTION_STATE_EVT:
            remember_bda(param->conn_stat.remote_bda);
            slc_connected = (param->conn_stat.state == ESP_HF_CLIENT_CONNECTION_STATE_SLC_CONNECTED);
            if (param->conn_stat.state == ESP_HF_CLIENT_CONNECTION_STATE_DISCONNECTED) {
                slc_connected = false;
                audio_connected = false;
                audio_connecting = false;
                msbc_active = false;
                stop_audio_path();
            }
            ESP_LOGI(TAG, "HFP connection=%s", hfp_conn_state_name(param->conn_stat.state));
            if (slc_connected) {
                note_connected_activity();
#if HFP_REQUEST_DISABLE_NREC
                esp_hf_client_send_nrec();
#endif
                esp_hf_client_volume_update(ESP_HF_VOLUME_CONTROL_TARGET_MIC, 15);
#if HFP_AUTO_OPEN_SCO
                try_connect_audio("SLC connected");
#endif
            }
            break;
        case ESP_HF_CLIENT_AUDIO_STATE_EVT: {
            esp_hf_client_audio_state_t state = param->audio_stat.state;
            audio_connecting = (state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTING);
            audio_connected = (state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED || state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED_MSBC);
            bool state_msbc = (state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED_MSBC);
            ESP_LOGI(TAG, "HFP audio=%s", hfp_audio_state_name(state));
            if (audio_connected) {
                audio_connecting = false;
                note_connected_activity();
                esp_hf_client_volume_update(ESP_HF_VOLUME_CONTROL_TARGET_MIC, 15);
                start_audio_path(state_msbc);
            } else if (!audio_connecting) {
                msbc_active = false;
                mic_light_standby = false;
                stop_audio_path();
            }
            break;
        }
        default:
            break;
    }
}

static void setup_bluetooth(void) {
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    log_esp("esp_bt_controller_init", esp_bt_controller_init(&bt_cfg));
    log_esp("esp_bt_controller_enable", esp_bt_controller_enable(bt_cfg.mode));
    log_esp("esp_bluedroid_init", esp_bluedroid_init());
    log_esp("esp_bluedroid_enable", esp_bluedroid_enable());

    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_NONE;
    esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE, &iocap, sizeof(iocap));
    esp_bt_pin_code_t pin = {0};
    esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_VARIABLE, 0, pin);
    esp_bt_dev_set_device_name(BT_DEVICE_NAME);

    esp_bt_cod_t cod = {0};
#if MIC_ONLY_TEST_ENABLED
    cod.major = ESP_BT_COD_MAJOR_DEV_AV;
    cod.minor = 0;
#else
    cod.major = ESP_BT_COD_MAJOR_DEV_PERIPHERAL;
    cod.minor = ESP_BT_COD_MINOR_PERIPHERAL_KEYBOARD;
#endif
    cod.service = ESP_BT_COD_SRVC_AUDIO | ESP_BT_COD_SRVC_TELEPHONY | ESP_BT_COD_SRVC_CAPTURING;
    esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_ALL);
    esp_bt_gap_register_callback(gap_callback);
    log_esp("esp_bt_gap_set_scan_mode(initial hidden)", esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_NON_DISCOVERABLE));
    log_esp("esp_bt_sleep_enable", esp_bt_sleep_enable());

    log_esp("esp_bredr_sco_datapath_set", esp_bredr_sco_datapath_set(ESP_SCO_DATA_PATH_HCI));

#if MIC_ONLY_TEST_ENABLED
    hid_ready = false;
    hid_connected = false;
    ESP_LOGI(TAG, "MIC_ONLY_TEST: HID service disabled");
#else
    log_esp("esp_bt_hid_device_register_callback", esp_bt_hid_device_register_callback(hid_callback));
    log_esp("esp_bt_hid_device_init", esp_bt_hid_device_init());
#endif

    log_esp("esp_hf_client_register_data_callback", esp_hf_client_register_data_callback(hf_incoming_data_cb, hf_outgoing_data_cb));
    log_esp("esp_hf_client_register_callback", esp_hf_client_register_callback(hf_client_event_cb));
    log_esp("esp_hf_client_init", esp_hf_client_init());
}

static void print_status(void) {
    uint32_t now = millis_now();
    if (now - last_status_ms < STATUS_MS) return;
    last_status_ms = now;
    ESP_LOGI(TAG, "RUN fw=%s pair=%s hfp=%s slc=%s sco=%s hidReady=%s hid=%s i2s=%s micStandby=%s btLow=%s batt=%dmV battLow=%s peak=%d noise=%" PRId32 " agc_q8=%" PRId32 " frames=%" PRIu32 " drops=%" PRIu32,
             FIRMWARE_VERSION,
             pairing_discoverable ? "open" : "closed",
             hfp_ready ? "yes" : "no",
             slc_connected ? "yes" : "no",
             audio_connected ? "yes" : (audio_connecting ? "connecting" : "no"),
             hid_ready ? "yes" : "no",
             hid_connected ? "yes" : "no",
             i2s_running ? "yes" : "no",
             mic_light_standby ? "yes" : "no",
             bt_low_activity ? "yes" : "no",
             battery_mv,
             battery_low ? "yes" : "no",
             mic_peak,
             noise_floor,
             agc_gain_q8,
             mic_frames,
             mic_drops);
    mic_peak = 0;
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "AirMic WR104 ESP-IDF Classic combo %s", FIRMWARE_VERSION);
#if MIC_ONLY_TEST_ENABLED
    ESP_LOGI(TAG, "MIC_ONLY_TEST: only HFP microphone is active; HID, keys, battery and power management are disabled");
    ESP_LOGI(TAG, "MIC_ONLY_TEST: INMP441 SCK/BCLK=%d WS=%d SD=%d RAW_I2S_DIAG=on stereoScan=%s",
             I2S_BCLK_GPIO,
             I2S_WS_GPIO,
             I2S_SD_GPIO,
             MSM3526_STEREO_SCAN_ENABLED ? "yes" : "no");
#if HFP_TEST_TONE_ENABLED
    ESP_LOGI(TAG, "HFP_TEST_TONE: outgoing microphone stream is synthetic %dHz amplitude=%d",
             HFP_TEST_TONE_HZ, HFP_TEST_TONE_AMPLITUDE);
#endif
#else
    ESP_LOGI(TAG, "Pins: key%d/backspace key%d/enter key%d/right_alt_option; INMP441 SCK/BCLK=%d WS=%d SD=%d",
             KEY_PINS[0],
             KEY_PINS[1],
             KEY_PINS[2],
             I2S_BCLK_GPIO,
             I2S_WS_GPIO,
             I2S_SD_GPIO);
#endif
    uint32_t boot_ms = millis_now();
    last_key_activity_ms = boot_ms;
    last_audio_activity_ms = boot_ms;
    last_connected_ms = boot_ms;
    setup_keys();
    setup_battery_monitor();
    setup_bluetooth();

    while (true) {
        handle_keys();
        service_pairing_window();
        try_connect_hid("background retry");
        try_connect_hfp("background retry");
#if HFP_AUTO_OPEN_SCO
    try_connect_audio("background SCO retry");
#endif
        service_battery();
        service_power_management();
        print_status();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
