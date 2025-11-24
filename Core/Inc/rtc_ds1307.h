#ifndef RTC_DS1307_H
#define RTC_DS1307_H

#include "main.h"
#include <stdint.h>

/* 7-bit base address 0x68 -> 8-bit on bus */
#define DS1307_I2C_ADDR8       (0x68u << 1)

/* Registers */
#define DS1307_REG_SECONDS     0x00
#define DS1307_REG_MINUTES     0x01
#define DS1307_REG_HOURS       0x02
#define DS1307_REG_DAY         0x03
#define DS1307_REG_DATE        0x04
#define DS1307_REG_MONTH       0x05
#define DS1307_REG_YEAR        0x06
#define DS1307_REG_CONTROL     0x07

typedef struct {
  uint8_t seconds;
  uint8_t minutes;
  uint8_t hours;   /* 0–23 (assume 24h) */
} DS1307_Time;

/* Raw read/write */
HAL_StatusTypeDef DS1307_ReadReg (uint8_t reg, uint8_t *data, uint16_t len);
HAL_StatusTypeDef DS1307_WriteReg(uint8_t reg, const uint8_t *data, uint16_t len);

/* Time API */
HAL_StatusTypeDef DS1307_ReadTime (DS1307_Time *t);
HAL_StatusTypeDef DS1307_WriteTime(const DS1307_Time *t);

/* Singles (optional) */
HAL_StatusTypeDef DS1307_ReadSeconds(uint8_t *sec);
HAL_StatusTypeDef DS1307_ReadMinutes(uint8_t *min);
HAL_StatusTypeDef DS1307_ReadHours  (uint8_t *hour);

/* Ensure oscillator runs (clear CH bit if set) */
void DS1307_StartIfHalted(void);

#endif /* RTC_DS1307_H */
