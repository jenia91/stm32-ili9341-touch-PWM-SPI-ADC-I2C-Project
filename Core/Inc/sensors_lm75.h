#ifndef SENSORS_LM75_H
#define SENSORS_LM75_H

#include "main.h"
#include <stdint.h>

/* Default 7-bit address 0x48 (A2..A0=0) -> 8-bit on bus */
#define LM75_I2C_ADDR8    (0x48u << 1)

/* Registers */
#define LM75_REG_TEMP     0x00

/* Read temperature in Celsius */
HAL_StatusTypeDef LM75_ReadCelsius(float *out_c);

#endif /* SENSORS_LM75_H */
