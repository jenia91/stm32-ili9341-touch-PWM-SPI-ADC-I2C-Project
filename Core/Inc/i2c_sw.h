#ifndef I2C_SW_H
#define I2C_SW_H

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bit-bang I²C on PB6=SCL, PB7=SDA (Open-Drain + Pull-Up 3.3V) */
void SWI2C_Init_PB6_PB7(void);
void SWI2C_BusClear(void);

/* 8-bit address API (7-bit<<1) — compatible with HAL-style addr8 */
HAL_StatusTypeDef SWI2C_Mem_Read (uint8_t addr8, uint8_t mem, uint8_t *data, uint16_t len);
HAL_StatusTypeDef SWI2C_Mem_Write(uint8_t addr8, uint8_t mem, const uint8_t *data, uint16_t len);

/* Scanner: probe a single 7-bit address; returns 1 on ACK */
uint8_t SWI2C_Scan_One(uint8_t addr7);

#ifdef __cplusplus
}
#endif
#endif /* I2C_SW_H */
