# WROOM-32 蓝牙全链路分支方案（无 mac_receiver）

这个目录下你现在的主链路是：

- 键盘：ESP32-BLE HID 到 Mac（已完成）
- 麦克风：ESP32 通过 Wi‑Fi UDP 到 `mac_receiver.py`（需 BlackHole）

你这次提的要求是：**麦克风和键盘都走蓝牙、取消 mac 软件**。

## 先说结论（重要）

单片 ESP32-WROOM-32 目前在本项目中**不建议**直接承接“蓝牙麦克风”成品化方案：

- 现有固件是 BLE HID 键盘 + Wi‑Fi 音频。
- ESP32-WROOM-32 可以做 BLE HID，也可以做部分蓝牙音频方案，但与 macOS“蓝牙麦克风/HFP”稳定配对、低延迟对讲质量、以及与键盘共存，会有明显的不确定性。

更稳妥的 MVP 路径是：

1. 仍用 ESP32-WROOM-32 做 **BLE 键盘**（保留现有按键/配对逻辑）。
2. 加一块 **外置 Bluetooth 麦克风模块**，它直接作为 macOS 的蓝牙麦克风输入。
3. 用户端只配对两个蓝牙设备：键盘 + 麦克风。

## 为什么不建议现在就单片实现两端全蓝牙

- 目前仓库里还没有“WROOM-32 + BLE 麦克风”的稳定代码分支。
- 现有 `esp32_s3_wifi_mic.ino` 里大量流程（`WiFi`, `UDP 3333`, `MAC ready` 广播）是为 Wi‑Fi 音频链路写的。
- 要保留可维护性，建议先用“双设备分工”跑通体验，再决定是否要重写为单芯片蓝牙音频栈。

## 这次新分支里你该做什么

- `branch`: `feat/wroom32-ble-no-mac`
- 目标效果：
  - 保留蓝牙按键（无需 Mac 软件）
  - 取消 `mac_receiver.py` 作为“音频必需依赖”
  - 把麦克风迁到独立蓝牙音频设备（推荐 BM83 或 QCC 系列音频模块）

### 你的下一步工作清单

1. 先把 WROOM32 固件板子设置为 `ESP32 Dev Module`，上传当前 BLE 键盘固件（按键与配对逻辑不依赖 S3 特性）。
2. 配对 `AirMic Keyboard`。
3. 用外置麦克风模块作为蓝牙麦克风配对到 macOS。
4. 在系统设置里把输入切到那个蓝牙麦克风。
5. 键盘端仍走 `airmic_config.h` 与 setup 页面保存的按键映射。

## 自定义键盘按钮如何设置（无 Mac 助手也可用）

当前路径里仍有两个设置入口：

### 入口 A：WROOM 本机 setup 页（推荐，离线可用）

1. 上电时按住任意键约 3 秒，进入 setup 模式。
2. 连接 `AirMic-Setup-xxxx`。
3. 打开 `http://192.168.4.1`。
4. 在 `Key 1`/`Key 2`/`Key 3` 下拉里选择动作。
5. `Save` 后重启。

### 支持的按键动作（当前固件）

- `BACKSPACE`
- `RIGHT_OPTION`（等价 `DICTATION_OPTION`）
- `INPUT_KEY`
- `MIC_WINDOW`
- `ENTER`
- `ESC`
- `TAB`
- `SPACE`
- `DELETE_FORWARD`
- `CMD_BACKSPACE`
- `CMD_ENTER`
- `CMD_SPACE`
- `CTRL_SPACE`

### 默认值

- Key 1: `BACKSPACE`
- Key 2: `RIGHT_OPTION`
- Key 3: `ENTER`

### 入口 B（当前主线保留）

如果你仍在 Wi‑Fi 模式下，需要 mac 软件协同配置，可继续用 `AirMic_自定义按键和麦克风.command` 调 `~/Library/Application Support/AirMic/config.json` 并广播给 ESP32。

## 与现有主线的关系

- 这份分支是“目标重定向”文档，不会自动删掉旧文件。
- 现有 `AirMic_Setup.command`、`mac_receiver/`、`install_airmic_helper.command` 的主流程仍保留在主方向。
- 你要的“无 Mac 软件”目标先在用户流程层面取消依赖（不跑 `receiver.py`）；
  音频链路改成“外置蓝牙麦克风”由系统侧直接提供。
