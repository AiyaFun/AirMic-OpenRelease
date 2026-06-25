# AirMic 纯蓝牙版固件公开发布

**版本**: wr1.0.1-classic-combo-kbd3  
**日期**: 2026-06-26

本仓库仅发布可运行文件与用户使用说明，不包含源码。

## 你要的一页式使用说明（对外发布）

### 1) 烧入固件（终端）

- 第一步：确认设备已连接到 USB。
- 第二步：在终端执行刷写命令（将串口改成你机器上的端口）：

```bash
esptool.py --chip esp32 --port /dev/cu.usbserial-0001 --baud 921600 write_flash -z \
  0x1000 firmware/bootloader.bin \
  0x8000 firmware/partitions.bin \
  0x10000 firmware/airmic_ble_full.bin
```

- 第三步：重启设备，确认有蓝牙广播出现。

### 2) 绑定和使用

1. 打开系统蓝牙，配对设备。
2. 在系统声音/输入设备里选中 AirMic 的蓝牙麦克风输入。
3. 在任意输入框测试按键：
   - 左：退格
   - 中：右 Option（可用于听写启动）
   - 右：回车

### 3) 常见问题（先看这里）

- 没有蓝牙广播：重启设备后再试，确认步骤 1 的刷写文件与参数无误。
- 有连接但没音频：到输入源重新选中蓝牙麦克风。
- 按键无响应：确认蓝牙已连接，重启再试。

> 本页故意不展示开发编译、开发板型号和上位机工具链细节，仅给出可执行的交付流程。

---

## 目录（文件）

- `firmware/airmic_ble_full.bin`：主固件（合并镜像）
- `firmware/bootloader.bin`
- `firmware/partitions.bin`
- `firmware/firmware_app.elf`
- `firmware/airmic_ble_full.ino`：仅作公开参考（非完整工程）
- `docs/airmic_wr104_user_guide.md`：配对/上手/故障
- `docs/wroom32_ble_all_bluetooth.md`：全蓝牙方向说明
- `docs/custom_key_mapping.md`：按键映射说明

## 你要重点知道的

- 这是“纯蓝牙”方向：键盘与麦克风都不经过桌面辅助程序。
- 本发布包不包含源代码仓库，便于外发且不泄露核心开发流程。
