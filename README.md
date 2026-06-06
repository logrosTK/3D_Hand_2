<p align="center">
  <h1 align="center">?? 3D_Hand_2</h1>
  <p align="center"><strong>STM32F103 · 五指灵巧手 · PWM 舵机控制</strong></p>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/MCU-STM32F103C8Tx-blue?logo=stmicroelectronics" />
  <img src="https://img.shields.io/badge/Toolchain-ARM%20GCC-009911?logo=arm" />
  <img src="https://img.shields.io/badge/Build-CMake-064F8C?logo=cmake" />
  <img src="https://img.shields.io/badge/HAL-STM32CubeMX-4B32C3" />
  <img src="https://img.shields.io/badge/License-MIT-yellow" />
</p>

---

> ?? [中文](#中文) ｜ ?? [English](#english)

---

<h2 id="中文">?? 中文</h2>

### ? 简介

基于 STM32F103C8Tx 的五指灵巧手舵机控制固件，通过 UART 串口接收文本命令，输出 5 路 50Hz PWM 驱动 SG90 舵机，实现五指抓握控制。

```text
上位机 ──UART──? STM32 ──PWM×5──? SG90 舵机 ──? ?? 灵巧手
```

### ? 功能

| 命令 | 说明 |
|------|------|
| `OPEN` / `O` | 五指全开 |
| `CLOSE` / `C` | 五指全闭 |
| `G,0.5` | 整体抓握比例 0.0~1.0 |
| `F,0.2,1.0,0.5,0.8,0.3` | 五指独立控制 |
| `PING` | 心跳测试 → `PONG` |
| `STATUS` / `S` | 查询当前舵机 PWM 值 |

? **安全特性**：500ms 无命令自动张开 · 平滑插值（±10μs/20ms）· 脉宽限幅（500~2500μs）

### ? 引脚映射

| 舵机 | 手指 | 引脚 | 定时器 |
|:---:|------|------|--------|
| 0 | 拇指 | PA0 | TIM2_CH1 |
| 1 | 食指 | PA1 | TIM2_CH2 |
| 2 | 中指 | PA2 | TIM2_CH3 |
| 3 | 无名指 | PA3 | TIM2_CH4 |
| 4 | 小指 | PA6 | TIM3_CH1 |

- **串口**：USART1 — PA9 (TX), PA10 (RX) · 115200-8N1
- **舵机供电**：5~6V 独立电源，共地（?? 禁止从 STM32 板载 5V 取电）

### ? 快速开始

```bash
# 编译
cmake --preset default && cmake --build build/Debug

# 烧录（以 st-link 为例）
st-flash write build/Debug/3D_Hand_2.bin 0x8000000

# 串口连接（115200 波特率）
picocom -b 115200 /dev/ttyUSB0
```

上电后收到 `3D_HAND_READY` 即可发送命令，每条命令以 `\n` 结尾。

### ? 项目结构

```
├── Core/Inc & Src/    # 应用代码（main.c 为核心控制逻辑）
├── Drivers/           # STM32F1 HAL 库 + CMSIS
├── cmake/             # 工具链 + CubeMX 子项目
├── CMakeLists.txt     # 顶层构建
└── 灵巧手控制程序说明.md  # 详细设计文档
```

### ? 依赖

- **CMake** ≥ 3.22
- **arm-none-eabi-gcc**（交叉编译工具链）
- **STM32CubeMX**（仅修改外设配置时）

---

<h2 id="english">?? English</h2>

### ? Overview

STM32F103C8Tx-based firmware for a five-finger dexterous hand. Receives text commands over UART and drives 5× SG90 servos via 50Hz PWM.

```text
Host ──UART──? STM32 ──PWM×5──? SG90 Servos ──? ?? Hand
```

### ? Features

| Command | Description |
|------|------|
| `OPEN` / `O` | Open all fingers |
| `CLOSE` / `C` | Close all fingers |
| `G,0.5` | Grip ratio 0.0~1.0 |
| `F,0.2,1.0,0.5,0.8,0.3` | Per-finger control |
| `PING` | Heartbeat → `PONG` |
| `STATUS` / `S` | Query current PWM values |

? **Safety**: Auto-open on 500ms timeout · Smooth interpolation (±10μs/20ms) · Pulse clamping (500~2500μs)

### ? Pin Mapping

| Servo | Finger | Pin | Timer |
|:---:|--------|------|--------|
| 0 | Thumb | PA0 | TIM2_CH1 |
| 1 | Index | PA1 | TIM2_CH2 |
| 2 | Middle | PA2 | TIM2_CH3 |
| 3 | Ring | PA3 | TIM2_CH4 |
| 4 | Pinky | PA6 | TIM3_CH1 |

- **UART**: USART1 — PA9 (TX), PA10 (RX) · 115200-8N1
- **Servo Power**: 5~6V external supply, common GND (?? do NOT use STM32 onboard 5V)

### ? Quick Start

```bash
# Build
cmake --preset default && cmake --build build/Debug

# Flash (e.g. st-link)
st-flash write build/Debug/3D_Hand_2.bin 0x8000000

# UART (115200 baud)
picocom -b 115200 /dev/ttyUSB0
```

On boot you'll see `3D_HAND_READY`. Send commands terminated with `\n`.

### ? Requirements

- **CMake** ≥ 3.22
- **arm-none-eabi-gcc** toolchain
- **STM32CubeMX** (only for reconfiguring peripherals)

