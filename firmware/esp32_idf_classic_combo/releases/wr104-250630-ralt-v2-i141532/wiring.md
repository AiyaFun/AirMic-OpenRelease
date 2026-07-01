# AirMic WR104 接线方式

适用固件：`wr104-250630-ralt-v2-i141532`

## 按键

按键使用内部下拉，按下为高电平。每个按键一端接 GPIO，另一端接 3V3。

| 功能 | GPIO | 接法 |
| --- | --- | --- |
| Backspace | GPIO25 | GPIO25 -> 按键 -> 3V3 |
| Enter | GPIO26 | GPIO26 -> 按键 -> 3V3 |
| Right Alt / Option | GPIO13 | GPIO13 -> 按键 -> 3V3 |

组合键：

| 功能 | 操作 |
| --- | --- |
| 重新开放配对 | Enter + 第三个键 按住约 3 秒 |
| 清除旧绑定 | Backspace + Enter 按住约 5 秒 |

## INMP441 麦克风

| INMP441 | 控制端 |
| --- | --- |
| VDD / 3V3 | 3V3 |
| GND | GND |
| SCK / BCLK | GPIO14 |
| WS / LRCL | GPIO15 |
| SD / DOUT | GPIO32 |
| L/R | GND |

注意：

- INMP441 只能接 3.3V，不要接 5V。
- 控制端与 INMP441 必须共地。
- GPIO32 已用于 INMP441 的 SD/DOUT，不再接按键。
- GPIO13 按键固定发送 `Right Alt`。
- macOS 会把 `Right Alt` 识别为右 `Option`。
- Windows 会把 GPIO13 识别为右 `Alt`，没有单独的 Win/Mac 档位。
- I2S 三根线尽量短，优先保持 `SCK=14 / WS=15 / SD=32`。

## 可选电池检测

固件默认关闭电池检测。需要启用时再接 GPIO34 分压采样：

| 连接 | 说明 |
| --- | --- |
| 电池正极 -> 100K 电阻 -> GPIO34 | 电池电压采样 |
| GPIO34 -> 100K 电阻 -> GND | 分压 |
| GPIO34 -> 0.1uF 电容 -> GND | 可选，稳定采样 |
