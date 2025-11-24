#include "sensors_lm75.h"
#include "i2c_sw.h"

/* LM75: 9-bit two's complement, left-justified; 0.125°C/LSB */
HAL_StatusTypeDef LM75_ReadCelsius(float *out_c){
  uint8_t buf[2] = {0};
  HAL_StatusTypeDef st = SWI2C_Mem_Read(LM75_I2C_ADDR8, LM75_REG_TEMP, buf, 2);
  if (st != HAL_OK) return st;

  int16_t raw = (int16_t)((buf[0] << 8) | buf[1]);
  raw >>= 5;                                 /* align 9-bit; keeps sign */
  if (raw & 0x0400) raw |= 0xF800;           /* sign-extend (safety) */
  *out_c = (float)raw * 0.125f;
  return HAL_OK;
}
