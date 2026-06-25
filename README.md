# AirMic 纯蓝牙版公开发布

**版本**: wr1.0.4  
**发布日期**: 2026-06-26  
**作者**: Aiya FUN

本发布页面向购买者，公开可刷写固件与使用说明，不包含完整源码仓库。

## 先看文档（对外文档入口）

- [AirMic WR104 使用说明（公开版）](docs/airmic_wr104_public_guide.md)
- [AirMic WR104 详细用户说明（含配对/故障）](docs/airmic_wr104_user_guide.md)
- [全蓝牙方向说明（全流程）](docs/wroom32_ble_all_bluetooth.md)
- [按键映射说明](docs/custom_key_mapping.md)

## 一、刷入固件（终端）

### 1) 连接设备
- 用 USB 连接设备到电脑

### 2) 执行刷写

```bash
esptool.py --chip esp32 --port /dev/cu.usbserial-0001 --baud 921600 write_flash -z \
  0x1000 firmware/bootloader.bin \
  0x8000 firmware/partitions.bin \
  0x10000 firmware/airmic_ble_full.bin
```

### 3) 完成后
- 重启设备，确认设备有蓝牙广播

## 二、绑定和使用

1. 打开系统蓝牙，搜索 `AirMic WR104` 并配对。
2. 在系统输入里选择 `AirMic WR104` 作为麦克风。
3. 在输入框测试按键：
   - 左：退格（Backspace）
   - 中：右 Option
   - 右：回车（Enter）

## 三、常见问题

- 搜不到设备：重启设备后再试，或清除旧绑定重配。
- 有连接但没音频：确认输入设备已切到 `AirMic WR104`。
- 按键无响应：确认蓝牙已成功连接后重试。

## 四、发布文件

- `firmware/airmic_ble_full.bin`：主固件（合并镜像）
- `firmware/bootloader.bin`
- `firmware/partitions.bin`
- `firmware/firmware_app.elf`
- `firmware/airmic_ble_full.ino`：公开参考
- 上述文档文件（见文档入口）

## 五、版权与二次编译说明
- 公开发布包不含完整工程源码，不构成可直接复现源码级二次编译环境。
- 如需完整复现，请使用完整源码仓库（含工程配置、依赖与构建说明）。

> 本说明不展示开发板型号与开发工具链细节，按交付可执行路径提供步骤。
