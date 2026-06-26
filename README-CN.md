# Mini MCU 开发板设计

一套**从零实现、零外部依赖的 C 语言实现**，覆盖 MCU 开发板设计的全流程——最小系统启动、Bootloader 设计、调试接口、板级布局、低功耗设计、传感器扩展板以及固件基础。每个模块将电子工程/嵌入式系统理论与板级实践相结合，提供可运行的 C 代码。

## 子模块

| 子模块 | 主题 | 参考课程 |
|-----------|--------|-------------|
| [mini-stm32-minimal-system-design](mini-stm32-minimal-system-design/) | STM32 电源/时钟/复位系统、去耦 PDN、PCB 布局、EMI/EMC、热管理、板级验证 | MIT 6.002, MIT 6.004 |
| [mini-bootloader-usb-dfu-ota-design](mini-bootloader-usb-dfu-ota-design/) | USB DFU 核心、固件镜像管理、Flash 管理器、OTA 传输、加密验证、启动序列 | Berkeley EECS 149, MIT 6.S082 |
| [mini-debug-swd-jtag-interface](mini-debug-swd-jtag-interface/) | SWD 协议栈、JTAG TAP 控制器、调试端口访问、调试传输抽象 | MIT 6.004, ARM DDI 0314 |
| [mini-esp32-iot-board-layout](mini-esp32-iot-board-layout/) | 板级几何与层叠结构、PDN 电源完整性、RF 匹配与史密斯圆图、信号完整性、热设计、传输线 | Stanford EE214, MIT 6.002 |
| [mini-firmware-first-blink-hello-world](mini-firmware-first-blink-hello-world/) | Cortex-M 裸机启动、GPIO/UART/ADC/定时器驱动、看门狗、Bootloader 接口、架构兼容层 | UT Austin EE319K, MIT 6.S082 |
| [mini-low-power-coin-cell-design](mini-low-power-coin-cell-design/) | 纽扣电池模型、LDO/升压/降压-升压稳压、功耗预算分析、低功耗 MCU 睡眠模式、能量采集、电池寿命估算 | Berkeley EE290C, MIT 6.002 |
| [mini-peripheral-sensor-shield-design](mini-peripheral-sensor-shield-design/) | 传感器类型与分类、扩展板外形与总线协议、信号调理、传感器校准、数字滤波、传感器融合（卡尔曼/AHRS） | Stanford ME220, MIT 6.002 |
| [mini-power-clock-reset-bring-up](mini-power-clock-reset-bring-up/) | 电源设计、晶振设计、复位电路与监控、板级上电调试流程、信号完整性验证 | MIT 6.002, MIT 6.004 |

## 设计理念

- **零外部依赖** — 纯 C（C99/C11），仅使用 `libc` 和 `libm`
- **模块独立** — 每个目录自带 `include/`、`src/`、`tests/` 和 `examples/`
- **理论与板级对照** — 每个模块将电子工程/嵌入式教材与数据手册规范转化为可运行的 C 实现
- **实用演示** — PDN 计算器、Flash 布局工具、传感器融合管线、功耗预算表、板级验证检查器

## 构建

每个模块独立构建。进入模块目录后执行：

```bash
cd mini-stm32-minimal-system-design
make all    # 构建全部目标
make test   # 运行测试
```

需要 **GCC** 和 **GNU Make**。

## 项目结构

```
mini-mcu-dev-board-design/
├── mini-stm32-minimal-system-design/     # STM32 最小系统：电源、时钟、复位、PDN、PCB 布局、EMI/EMC、热管理、验证
├── mini-bootloader-usb-dfu-ota-design/   # USB DFU Bootloader：固件镜像、Flash 管理、OTA 传输、加密校验
├── mini-debug-swd-jtag-interface/        # SWD 协议、JTAG TAP 控制器、调试端口抽象层
├── mini-esp32-iot-board-layout/          # ESP32 IoT：板级几何、PDN、RF 匹配、信号完整性、热设计、传输线
├── mini-firmware-first-blink-hello-world/ # Cortex-M 裸机：GPIO、UART、ADC、定时器、看门狗、启动、Bootloader
├── mini-low-power-coin-cell-design/      # 纽扣电池模型、电压稳压、功耗预算、睡眠模式、能量采集
├── mini-peripheral-sensor-shield-design/ # 传感器扩展板：类型、接口、信号调理、校准、滤波、融合
└── mini-power-clock-reset-bring-up/      # 板级启动：电源、晶振、复位电路、验证
```

## 许可证

MIT
