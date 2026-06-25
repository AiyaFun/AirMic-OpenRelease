# AirMic 纯蓝牙版公开发布包（公开发放）

## 说明信息
- **产品**：AirMic WR104（纯蓝牙）
- **版本**：`wr1.0.4`
- **发布日期**：2026-06-26
- **作者**：**Aiya FUN**

本发布包仅用于外发交付，包含可刷写固件、必要说明与校验文件，不包含完整源码仓库。

---

## 一、刷写固件（终端）

### 1. 准备
- 用 USB 连接设备到电脑  
- 安装 `esptool.py`（若已可直接执行可跳过）

### 2. 刷写命令
请将串口改为你当前机器上的端口后执行：

```bash
esptool.py --chip <你的芯片型号> --port /dev/cu.usbserial-0001 --baud 921600 write_flash -z \
  0x1000 firmware/bootloader.bin \
  0x8000 firmware/partitions.bin \
  0x10000 firmware/airmic_ble_full.bin
```

### 3. 完成后
- 让设备重启  
- 确认出现蓝牙广播后，再执行绑定

---

## 二、绑定与使用

### 1) 蓝牙绑定
1. 打开系统蓝牙  
2. 搜索并连接 `AirMic WR104`

### 2) 语音输入设备选择
- 在系统输入设备里选择 `AirMic WR104`

### 3) 按键测试
- 左键：Backspace  
- 中键：右 Option（常用于语音输入热键）  
- 右键：Enter

---

## 三、常见问题
- **搜不到广播或连不上**：重启设备，再按配对说明重试  
- **有蓝牙但没有麦克风输入**：确认系统输入设备已切换到 AirMic  
- **按键无反应**：确认蓝牙连接成功后重试，必要时重启设备  
- **换电脑后配对异常**：清除旧绑定后再配对

---

## 四、发布内容
- `firmware/airmic_ble_full.bin`
- `firmware/bootloader.bin`
- `firmware/partitions.bin`
- `firmware/firmware_app.elf`
- `firmware/airmic_ble_full.ino`（公开参考）
- `docs/airmic_wr104_user_guide.md`
- `docs/wroom32_ble_all_bluetooth.md`
- `docs/custom_key_mapping.md`

---

## 五、版权与二次编译说明
- 本发布包不含完整工程源码，不构成可直接复现源码级开发环境。  
- 如需完整复现与二次编译，请使用包含工程配置、源文件与依赖约束的完整源仓库。

> 发布说明不包含开发板型号与开发工具链信息，保留了可交付的用户操作路径。

---

## 六、AirMic WR104 使用说明（公开版）

AirMic WR104 通过蓝牙提供两项核心能力：蓝牙麦克风输入 + 蓝牙键盘快捷输入（Mac/Windows 都可用）。

### 核心能力
- 可直接用作麦克风：在系统输入中选择 `AirMic WR104`
- 可直接用作键盘：Backspace / 右 Option / Enter 即可测试
- 语音输入前建议确认麦克风权限与输入设备已切换到 AirMic

### 可以做什么
- 语音输入：与系统或第三方语音输入法配合使用
- 快捷键输入：用按键完成退格、回车、辅助键操作
- 多设备：可在不同电脑之间重新配对使用

### 常见使用流程
1. 先按上文完成刷写并重启设备  
2. 进入蓝牙，配对 `AirMic WR104`  
3. 在系统里把输入源切到 `AirMic WR104`  
4. 打开需要输入的文本框进行语音输入或按键测试

### 常见问题
- 搜不到设备：重启设备并重试，或清除旧绑定后重配  
- 只有部分功能可见：确认系统输入源与麦克风权限  
- 设备在另一台电脑用后异常：清除旧绑定后重新配对

### 常见问题
- 搜不到设备：重启设备并重试，或清除旧绑定后重配  
- 只有部分功能可见：确认系统输入源与麦克风权限  
- 设备在另一台电脑用后异常：清除旧绑定后重新配对

完整说明见：[airmic_wr104_public_guide.md](/Users/jiang/Documents/AirMic%20Keypad/docs/airmic_wr104_public_guide.md)

---

**作者**：Aiya FUN  
**版本**：wr1.0.4  
**发布日期**：2026-06-26
