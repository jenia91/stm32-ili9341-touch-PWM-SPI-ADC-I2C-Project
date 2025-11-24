# Smart Irrigation GUI – STM32F4 (ILI9341 + Touch)

<!-- Optional: add a project photo/screenshot and update the src URL -->
<!--
<img width="900" alt="STM32F4 GUI Prototype"
     src="https://github.com/user-attachments/assets/xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" />
-->

---

## Overview

STM32CubeIDE project that ports the core parts of my smart irrigation system  
into a clean **STM32F4** workspace.

The firmware runs on an STM32F4 and provides:

- **SPI TFT GUI** with 3 screens (Startup / Check / Project)
- **Touch input** via XPT2046 over the same SPI bus
- **Software I²C** on PB6/PB7 for:
  - DS1307 real-time clock (RTC)
  - LM75 temperature sensor
- **ADC1** input (PC0) for light sensor
- **PWM** on TIM4 CH3 (PB8) to drive a servo (0–180° sweep)
- **Relay output** on PB12
- **Debug LED** on PB13 to show I²C/sensor activity

All code is written in C using **STM32 HAL** and tested directly on hardware  
(ILI9341 TFT + XPT2046 touch + DS1307 + LM75 + light sensor + relay + servo).

---

## Features

- **Three-screen GUI (landscape 320×240)**  
  - **Startup** – project title + 3 navigation buttons  
  - **Check** – read Time / Temp / Light, plus Relay test button  
  - **Project** – periodic 1 Hz update with:
    - Current time (RTC or software time if user set it)
    - Temperature and threshold
    - Light percentage
    - Placeholder line for future irrigation logic

- **Touch-friendly Setup screen**
  - Rows for **Hour**, **Minute**, **Temperature threshold**
  - Numeric value boxes + `"+"` button per row
  - **Long press = auto-repeat** for faster changes
  - Option to switch between **RTC time** and **software time**

- **Relay + Servo test logic**
  - Tapping **Relay** button toggles PB12 (active-high relay module)
  - When relay is ON:
    - Servo on PB8 sweeps continuously **0 ↔ 180°**
    - Non-blocking sweep in the main loop (no delay inside PWM)

- **Sensor refresh indicators**
  - PB13 debug LED turns ON while reading RTC/LM75/light
  - Provides quick visual feedback during sensor I/O

---

## Peripherals & Communication

### Communication interfaces

| Interface | Purpose                           | Implementation                  |
|----------|------------------------------------|---------------------------------|
| **SPI1** | ILI9341 TFT + XPT2046 touch       | HAL SPI (blocking transfers)   |
| **I²C (SW)** | DS1307 RTC, LM75 temperature | Bit-banged on PB6/PB7 (`i2c_sw`) |
| **ADC1** | Light sensor on PC0 (IN10)        | Single-channel polling         |
| **PWM**  | Servo control on PB8              | TIM4 CH3, 50 Hz, 1 µs tick     |
| **GPIO** | Relay + debug LED                 | PB12, PB13 push-pull outputs   |

### Hardware summary

- **MCU**: STM32F405 (HSI → PLL at 168 MHz)
- **Display**: ILI9341 320×240 TFT over SPI1
- **Touch**: XPT2046 resistive controller (shares SPI1)
- **RTC**: DS1307 over software I²C
- **Temperature**: LM75 over software I²C
- **Light sensor**: analog input to **ADC1 IN10 (PC0)**
- **Servo**: e.g. MG90S on PB8 (TIM4 CH3, 50 Hz PWM)
- **Relay**: module driven from PB12 (active-high input)
- **Debug LED**: PB13
- **Power**: common 5 V supply with **shared GND** between MCU, sensors, relay and servo

---

## Pin Map (core signals)

| Function                  | MCU pin(s)       | Notes                                       |
|--------------------------|------------------|---------------------------------------------|
| TFT SCK / MISO / MOSI    | PA5 / PA6 / PA7  | SPI1 bus (shared with XPT2046)             |
| TFT / Touch CS           | see `gpio.c`     | Chip-select pins configured in CubeMX      |
| I²C SCL (software)       | PB6              | Bit-banged I²C clock                        |
| I²C SDA (software)       | PB7              | Bit-banged I²C data                         |
| Light sensor             | PC0              | ADC1 IN10, mapped to 0–100% light          |
| Servo PWM                | PB8              | TIM4 CH3, 50 Hz, 600–2400 µs pulse width    |
| Relay control            | PB12             | Push-pull output → relay input (active-high)|
| Debug LED                | PB13             | Toggles during I²C / periodic refresh      |
| Power / GND              | VCC, GND         | All modules share the same ground          |

For exact CS/RESET pins of display and touch, see `Core/Src/gpio.c`  
and the CubeMX `.ioc` file.

---

## Timing & PWM

- **System clock**: HSI → PLL → **168 MHz SYSCLK**
- **TIM4 configuration**:
  - Prescaler = 83 → **1 µs tick**
  - Period = 19999 → **20 ms frame (50 Hz PWM)**
- **Servo mapping (PB8 / TIM4 CH3)**:
  - 0°  → 600 µs
  - 180° → 2400 µs  
  The code maps **0–180° → 600–2400 µs** and writes the value directly  
  into the TIM4 CH3 compare register via `SERVO_SetAngle()`.

---

## Build & Flash

- **Toolchain**: STM32CubeIDE

### Steps

1. Clone this repository:
   ```bash
   git clone https://github.com/jenia91/stm32-ili9341-touch-PWM-SPI-ADC-I2C-Project.git
Open the project in STM32CubeIDE using
Ivgeni_Goriatchev_STM32_Project.ioc.

Let CubeIDE load the existing configuration and code.

Build the project in Debug or Release configuration.

Flash the firmware to the STM32F4 board using ST-LINK.

Power the board with the TFT, touch, sensors, relay and servo connected.

Use the GUI:

Check screen to test RTC/LM75/light/relay

Setup screen to adjust time and temperature threshold

Project screen to watch the periodic updates

No RTOS is used – the main loop is event-driven with small delays
to limit CPU usage.

Quick Links
Main GUI / logic → Core/Src/main.c

DS1307 RTC driver (SW I²C) → Core/Src/rtc_ds1307.c

LM75 temperature driver → Core/Src/sensors_lm75.c

Software I²C bit-bang → Core/Src/i2c_sw.c

GPIO, SPI, ADC, TIM init → Core/Src/gpio.c, Core/Src/spi.c, Core/Src/adc.c, Core/Src/tim.c

Public headers → Core/Inc/

CubeMX configuration → Ivgeni_Goriatchev_STM32_Project.ioc
## Repo map
```
Core/
 ├─ Inc/
 │   ├─ main.h             # Global defines, prototypes
 │   ├─ rtc_ds1307.h       # DS1307 RTC interface (SW I2C)
 │   ├─ sensors_lm75.h     # LM75 temperature interface
 │   ├─ i2c_sw.h           # Software I2C on PB6/PB7
 │   └─ ...                # HAL config / IRQ headers
 │
 └─ Src/
     ├─ main.c             # GUI, touch handling, sensors, relay + servo logic
     ├─ rtc_ds1307.c       # DS1307 driver over software I2C
     ├─ sensors_lm75.c     # LM75 temperature driver
     ├─ i2c_sw.c           # Bit-banged I2C implementation
     ├─ spi.c, adc.c       # SPI1, ADC1 init
     ├─ tim.c, gpio.c      # TIM4 PWM, GPIO and pin mapping
     └─ ...                # HAL MSP / IRQ sources

Drivers/
 ├─ STM32F4xx_HAL_Driver/  # HAL drivers
 ├─ CMSIS/                 # CMSIS device + core
 └─ Middlewares/           # (if used by CubeMX)

Ivgeni_Goriatchev_STM32_Project.ioc  # STM32CubeMX configuration
.gitignore
LICENSE
```
