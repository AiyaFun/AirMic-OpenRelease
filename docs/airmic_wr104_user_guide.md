# AirMic WR104 用户说明

适用固件：`wr1.0.4`

蓝牙名称：`AirMic WR104`

版本命名规则：

- `wr1.0.4` 对应蓝牙名称 `AirMic WR104`
- `wr1.0.5` 对应蓝牙名称 `AirMic WR105`
- 后续只改固件 patch 版本号，蓝牙名称会跟随生成

## 1. 支持的开发板

推荐使用经典 ESP32：

- ESP32-WROOM-32
- ESP32-WROOM-32D
- ESP32-WROOM-32E
- ESP32-WROOM-DA

不适用：

- ESP32-C3
- ESP32-C6
- ESP32-S2

原因：本项目使用 Classic Bluetooth HFP 麦克风和 Classic Bluetooth HID 键盘。

## 2. 按键接线

`wr1.0.4` 已经把按键从旧的 `GPIO16 / GPIO17 / GPIO5` 改成支持 deep sleep 唤醒的 RTC GPIO。

新版接线：

| 功能 | ESP32 GPIO | 接法 |
| --- | --- | --- |
| Backspace | GPIO32 | 按键一端接 GPIO32，另一端接 3V3 |
| Enter | GPIO27 | 按键一端接 GPIO27，另一端接 3V3 |
| 右 Option | GPIO14 | 按键一端接 GPIO14，另一端接 3V3 |

说明：

- 三个按键都使用内部下拉。
- 按下时 GPIO 被拉到 3V3。
- 三个按键都可以从 deep sleep 唤醒设备。
- 旧版 `GPIO5` 已移除，避免 ESP32 启动绑定位风险。
- 如果你是从旧版升级，需要把按键另一端从 GND 改接到 3V3。

## 3. INMP441 麦克风接线

| INMP441 | ESP32 |
| --- | --- |
| VDD / 3V3 | 3V3 |
| GND | GND |
| SCK / BCLK | GPIO26 |
| WS / LRCL | GPIO25 |
| SD / DOUT | GPIO33 |
| L/R | GND |

注意：

- INMP441 只能接 3.3V，不要接 5V。
- ESP32 和 INMP441 必须共地。
- I2S 三根线尽量短。

## 4. 电池与电源

开发板调试推荐：

`3.7V 锂电池 -> 5V 升压充电模块 -> ESP32 开发板 USB / 5V / VIN`

不要这样接：

- 不要把 3.7V 锂电池直接接 ESP32 开发板 5V/VIN。
- 不要把 3.7V 锂电池直接接 ESP32 3V3。
- 不要把 5V 接到 INMP441。

如果是裸 ESP32-WROOM-32 模组：

- 使用稳定 3.3V 电源。
- 峰值电流建议至少 500mA，最好 700mA 以上。

## 5. 低电量检测接线

固件已经预留低电量检测，但默认关闭，避免没有接线时误判。

启用前需要接：

| 连接 | 说明 |
| --- | --- |
| 电池正极 -> 100K 电阻 -> GPIO34 | 电池电压采样 |
| GPIO34 -> 100K 电阻 -> GND | 分压 |
| GPIO34 -> 0.1uF 电容 -> GND | 可选，稳定采样 |

接好后，把源码中：

```c
#define BATTERY_MONITOR_ENABLED 0
```

改成：

```c
#define BATTERY_MONITOR_ENABLED 1
```

低电量策略：

- 低于 3.5V：串口日志提示，按键时也会提示低电量。
- 低于 3.3V：断开蓝牙并进入 deep sleep，保护电池。

## 6. 省电策略

`wr1.0.4` 的省电策略：

- 10 分钟无音频/无按键：关闭 INMP441/I2S 采样，进入麦克风轻待机。
- 60 分钟无连接：降低蓝牙后台重连频率。
- 2 小时无连接：进入 deep sleep。
- deep sleep 后按任意按键唤醒，设备会重启并恢复蓝牙逻辑。

## 7. 蓝牙配对

首次使用：

1. 给 ESP32 上电。
2. 电脑或手机蓝牙里搜索 `AirMic WR104`。
3. 点击连接。

重新开放配对：

1. 同时按住 `Enter + 右 Option` 约 3 秒。
2. 重新搜索 `AirMic WR104`。

清除旧绑定：

1. 同时按住 `Backspace + Enter` 约 5 秒。
2. 设备会清除绑定并重启。
3. 重启后重新配对。

## 8. Mac 使用

连接后，系统里会出现：

- 键盘设备：`AirMic WR104`
- 音频输入设备：通常也显示为 `AirMic WR104` 或蓝牙耳机/免提输入

如果没有声音输入：

1. 打开系统设置。
2. 进入声音。
3. 输入设备选择 `AirMic WR104`。
4. 确认浏览器或 App 有麦克风权限。

## 9. Windows 使用

连接后请检查：

1. 设置 -> 蓝牙和设备，确认 `AirMic WR104` 已连接。
2. 设置 -> 系统 -> 声音 -> 输入，选择 `AirMic WR104`。
3. 如果只连接了键盘，没有麦克风，删除设备后重新配对。

## 10. 常见问题

搜不到设备：

- 确认设备刚上电 60 秒内可配对。
- 或长按 `Enter + 右 Option` 3 秒重新开放配对。
- 如果换电脑使用，建议长按 `Backspace + Enter` 5 秒清除旧绑定。

键盘没有反应：

- 确认新版按键已经改接到 `GPIO32 / GPIO27 / GPIO14`。
- 确认按键另一端接 3V3。
- 确认电脑蓝牙里不是只连接了音频，没有连接键盘。

麦克风没有声音：

- 确认 INMP441 的 VDD 接 3V3。
- 确认 BCLK=GPIO26，WS=GPIO25，DOUT=GPIO33。
- 确认系统输入设备选择 `AirMic WR104`。

设备休眠后不见了：

- 2 小时无连接后会进入 deep sleep。
- 按任意按键唤醒，等待几秒后重新连接。

刷机后还显示旧名字：

- 删除电脑蓝牙里旧的 `AirMic WR101`。
- 清除设备绑定：`Backspace + Enter` 长按 5 秒。
- 重新搜索 `AirMic WR104`。
