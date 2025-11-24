Smart Irrigation GUI (STM32F4 + ILI9341 + Touch)

Small STM32F4 project that ports the core parts of my smart irrigation system into a clean STM32CubeIDE workspace.

The focus here is a working GUI, SPI TFT + touch, software I2C, ADC, relay and servo PWM that can be reused as a template for future embedded projects.

Overview

The firmware runs on an STM32F4 microcontroller and provides:

SPI TFT display with a simple touch GUI (3 screens)

Software I2C for DS1307 real-time clock and LM75 temperature sensor

ADC input for a light sensor

PWM output for a servo motor

Digital output for a relay module

Debug LED for quick visual feedback while reading sensors

All code is written in C using STM32 HAL and tested directly on hardware.

GUI and Screens

The UI is divided into three screens:

Startup

Project title

Three navigation buttons: Check, Setup, Project

Check

Buttons to read:

Time from DS1307

Temperature from LM75

Light percentage from ADC

Relay test button:

Toggles a relay output

When the relay is ON, the servo starts sweeping between 0 and 180 degrees

Setup

Simple configuration screen with three rows:

Hour

Minute

Temperature threshold

Each row has a numeric value box and a “+” button

Long press on a button repeats the increment

Project

Periodic refresh once per second

Shows:

Current time (RTC or user-set software time)

Temperature and threshold

Light percentage

Placeholder line for future irrigation logic

Peripherals and Interfaces
Communication Interfaces
Interface	Purpose	Implementation
SPI1	TFT display + touch controller	HAL SPI (blocking)
I2C (SW)	DS1307 RTC, LM75 temperature	Bit-banged on PB6/PB7
ADC1	Light sensor	Single channel on PC0
PWM	Servo control	TIM4 CH3 on PB8
GPIO	Relay and debug LED	PB12, PB13 outputs
Hardware Summary

STM32F4 development board (HSI to PLL at 168 MHz)

ILI9341 320x240 TFT display over SPI1

XPT2046 resistive touch controller (shares SPI1)

DS1307 real-time clock (I2C)

LM75 temperature sensor (I2C)

Light sensor connected to ADC1 on PC0 (IN10)

Servo motor (for example MG90S) on PB8 driven by 50 Hz PWM

Relay module on PB12

Common power supply and shared GND between MCU, sensors, servo and relay

Pin Map (core signals)
Function	MCU pin	Notes
TFT SCK / MISO / MOSI	PA5 / PA6 / PA7	SPI1 bus shared with XPT2046
TFT / Touch CS	See gpio.c	Chip-select pins configured in CubeMX / gpio.c
I2C SCL (SW)	PB6	Software I2C bit-bang
I2C SDA (SW)	PB7	Software I2C bit-bang
Light sensor	PC0	ADC1 IN10, scaled to 0–100%
Servo PWM	PB8	TIM4 CH3, 50 Hz, 600–2400 µs pulse width
Relay control	PB12	Push-pull output to relay module input (active high)
Debug LED	PB13	Toggles during I2C / periodic refresh
Power / GND	VCC, GND	All modules share the same ground

For exact chip-select and reset pins of the display and touch controller, see Core/Src/gpio.c and the CubeMX .ioc file.

Timing and PWM

System clock is configured from HSI through PLL to 168 MHz.

TIM4 is configured with:

1 µs tick (Prescaler = 83)

Period = 19999 for a 20 ms frame (50 Hz PWM)

Servo mapping:

0 degrees → 600 µs

180 degrees → 2400 µs
The code linearly maps 0–180 degrees to this pulse width range and writes it into TIM4 CH3 compare register.

Build and Flash

Toolchain: STM32CubeIDE

Steps:

Clone this repository.

Open the project in STM32CubeIDE using the existing .ioc file.

Build the project in Debug or Release configuration.

Flash the firmware to the board using ST-LINK.

Open a serial terminal or use the on-board debug LED to observe activity.

No RTOS is used, the main loop is fully event-driven with HAL and small delays to limit CPU usage.

Repository Structure
Core/
  Inc/
    main.h
    rtc_ds1307.h
    sensors_lm75.h
    i2c_sw.h
    ...
  Src/
    main.c           // GUI, touch handling, sensors, relay and servo logic
    rtc_ds1307.c     // DS1307 driver over software I2C
    sensors_lm75.c   // LM75 temperature driver
    i2c_sw.c         // Bit-banged I2C implementation
    spi.c, adc.c, tim.c, gpio.c
Drivers/
  STM32F4xx_HAL_Driver/
  CMSIS/
Middlewares/
  ...
stm32_project.ioc    // STM32CubeMX configuration
.gitignore
README.md
