STM32 Smart Irrigation GUI – HAL, ILI9341, XPT2046

Small STM32F4 project that ports the core parts of my smart irrigation system into a clean STM32CubeIDE workspace.

This repository focuses on working drivers (SPI TFT + touch, software I2C, ADC, PWM, relay) and a simple GUI that can be reused as a template for future embedded projects.

Overview

The firmware runs on an STM32F4 microcontroller and provides:

SPI TFT display with a simple GUI (3 screens)

Resistive touch controller (XPT2046)

Real-time clock (DS1307) over software I2C

Temperature sensor (LM75) over software I2C

Light sensor on ADC1

Relay output for load control

Servo output with 50 Hz PWM

Soil and rain sensors are not connected in this STM32 version, so they are not shown in the UI.

Features

Three main screens:

Startup – project title and navigation buttons

Check – read time, temperature, light and test the relay + servo

Project – periodic screen that shows live time, temperature, light and placeholder text for future logic

Software I2C on PB6 / PB7 for:

DS1307 real-time clock

LM75 temperature sensor

SPI1 for:

ILI9341 TFT display

XPT2046 touch controller (shares SPI bus)

PWM servo control:

TIM4 CH3 on PB8

50 Hz, 600–2400 µs pulse mapped to 0–180 degrees

Relay output:

PB12 drives an external relay module (active high)

Light sensor:

ADC1, channel on PC0 (IN10), scaled to 0–100 % for the GUI

Debug LED on PB13 (toggles during I2C and periodic updates)

All code is written in C using STM32 HAL and tested on real hardware.

Hardware

STM32F4 development board (CubeIDE project, HSI → PLL 168 MHz)

ILI9341 320x240 TFT over SPI1

XPT2046 resistive touch controller (shared SPI1)

DS1307 RTC (I2C)

LM75 temperature sensor (I2C)

Light sensor connected to ADC1 on PC0

Servo (for example MG90S) on PB8 (TIM4_CH3)

Relay module on PB12

Common 3.3 V / 5 V supply with shared GND between MCU, sensors and servo/relay

Pin Map (core signals)
Function	MCU pin	Notes
TFT + touch SPI	SPI1 (PA5/6/7)	SCK / MISO / MOSI, CS pins in gpio.c
I2C SCL	PB6	Software I2C bit-bang
I2C SDA	PB7	Software I2C bit-bang
Light sensor (ADC)	PC0	ADC1 IN10
Servo PWM	PB8	TIM4 CH3, 50 Hz PWM
Relay control	PB12	Push-pull output to relay IN
Debug LED	PB13	Indicates I2C/sensor activity
Power, ground	VCC, GND	All modules share the same ground

For exact SPI chip-select and reset pins, see Core/Src/gpio.c and the CubeMX .ioc file.

Build and Flash

Toolchain: STM32CubeIDE

Open the .ioc file in STM32CubeIDE and let it generate the project if needed.

Build the project in Release or Debug configuration.

Flash the firmware using ST-LINK directly from CubeIDE.

Clock and timers:

System clock configured from HSI through PLL to 168 MHz.

TIM4 configured with 1 µs tick and period 19999 for 50 Hz PWM on PB8.

Repository structure
Core/
  Inc/
    main.h
    rtc_ds1307.h
    sensors_lm75.h
    i2c_sw.h
    ...
  Src/
    main.c           // GUI, touch handling, sensor refresh, servo logic
    rtc_ds1307.c     // DS1307 driver over software I2C
    sensors_lm75.c   // LM75 temperature driver
    i2c_sw.c         // Bit-banged I2C on PB6/PB7
    ...
Drivers/
  STM32F4xx_HAL_Driver/
  CMSIS/
Middlewares/
  ...
stm32_project.ioc    // STM32CubeMX configuration file
.project, .cproject  // STM32CubeIDE metadata
.gitignore
README.md
