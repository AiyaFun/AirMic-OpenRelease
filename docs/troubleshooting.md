# 常见问题排障（V0.1）

按下面顺序排查，避免盲调。

## 1) 没声音

1. 确认 ESP32 供电与 GND 正确
2. 串口是否持续打印发送日志
3. Mac 端 `receiver.py` 是否在运行
4. 确认音频输出设备是否选择了 `BlackHole 2ch`
5. macOS 听写里是否选对麦克风来源

## 2) 丢包严重

1. ESP32 与 Mac 是否在同一 Wi-Fi，且无隔离（公司/校园网络常会阻挡 UDP）
2. 路由器是否启用了 AP 隔离、客户端隔离
3. 检查家里路由器是否稳定、Wi-Fi 信道是否过于拥挤
4. 先减少距离再测试，确保两端信号强度好
5. 观察日志是否有 seq 跳变：
   - `seq gap` 频繁出现通常是网络层丢包

## 3) 找不到 BlackHole

1. 确认已用 `brew install --cask blackhole-2ch` 安装
2. 重启电脑后再试
3. 打开“音频 MIDI 设置”确认设备出现
4. 脚本没自动选中时，使用 `--device` 参数传入名字：
   ```bash
   python receiver.py --device blackhole
   ```

## 4) ESP32 连不上 Wi-Fi

1. 确认 `WIFI_SSID/WIFI_PASS` 无误
2. 确认路由器关闭 MAC 过滤
3. 检查是否是 5G-only/6G-only（ESP32 某些模块不支持）
4. 确保串口输出里出现：
   - `Wi-Fi connected, IP: ...`

## 5) INMP441 声道反了

1. `L/R` 一般建议接 GND（左声道）
2. 如果音色异常，尝试将 `L/R` 改悬空或 3.3V 对比测试（仅用于排查）
3. 一般最终以“稳定有声 + 识别率好”为准

## 6) 音量太小

1. 先确认 INMP441 与麦克风孔离嘴部距离
2. 排除是否接了错误 GPIO（当前固件是 `IO4=SCK/BCLK`、`IO5=WS/LRC`、`IO6=SD/DOUT`）
3. 在麦克风附近说话、减少环境噪声
4. 以后会加增益控制，V0.1 先不做

## 7) 爆音

1. 检查麦克风没直接对着风扇、风口、振动件
2. 检查 `I2S_WS` 与 `I2S_BCLK` 是否接反
3. 固件未加降噪，距离过近、发音过大可能出现削波

## 8) Mac IP 变了

1. Mac 切换网络后 IPv4 地址会变
2. 需要同步改 `firmware/...ino` 中 `macIP`
3. 改完重新烧录即可

## 9) 公司/校园 Wi-Fi 隔离

1. 这种网络常见“无线客户端不能互访”
2. 现象：ESP32 串口显示已发，但 Mac 长时间收不到
3. 解决方式：
   - 换热点（手机/路由器个人热点）
   - 或后续上配网页与局域网发现后再优化协议

## 10) 还有报错没法继续

1. 先把最近 20 行串口日志和 `receiver.py` 终端输出发来
2. 标注：
   - Mac 型号/系统版本
   - ESP32 开发板型号
   - 路由器品牌与 Wi-Fi 名称
   - 你是否在同网段/同 SSID

## 11) 按键没触发 / 触发错乱

1. 确认按键 GPIO 接法：
   - 按键1 `GPIO7` -> 按键 -> GND
   - 按键2 `GPIO8` -> 按键 -> GND
   - 按键3 `GPIO9` -> 按键 -> GND
2. 确认固件里 `controlPort = 3334` 与 `receiver.py --control-port 3334` 一致
3. 固件串口有没有输出 `Key event: BACKSPACE` / `DICTATION_OPTION` / `ENTER`
   `DICTATION_OPTION` 在 Mac 端会映射为单击右 Option。
4. Mac 端是否有 ` [CTRL]` 日志
5. 检查 `系统设置 -> 隐私与安全性 -> 辅助功能` 是否已允许当前终端 / Python 可控制电脑
6. 长按按键只应触发一次；如果多次触发，检查串口是否疯狂抖动，需提高 DEBOUNCE_MS（固件里可改）

## 12) 接收端触发快捷键失败

1. 是否有 `osascript` 权限提示
2. 在终端直接手工测试：
   ```bash
   osascript -e 'tell application "System Events" to key code 53'
   ```
3. 如果没反应，先到“辅助功能”加权限，再重启 `receiver.py`
