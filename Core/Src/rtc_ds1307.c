#include "rtc_ds1307.h"
#include "i2c_sw.h"

/* BCD helpers */
static uint8_t bcd_to_dec(uint8_t v){ return (uint8_t)(((v >> 4) * 10u) + (v & 0x0Fu)); }
static uint8_t dec_to_bcd(uint8_t v){ return (uint8_t)(((v / 10u) << 4) | (v % 10u)); }

HAL_StatusTypeDef DS1307_ReadReg(uint8_t reg, uint8_t *data, uint16_t len){
  return SWI2C_Mem_Read(DS1307_I2C_ADDR8, reg, data, len);
}
HAL_StatusTypeDef DS1307_WriteReg(uint8_t reg, const uint8_t *data, uint16_t len){
  return SWI2C_Mem_Write(DS1307_I2C_ADDR8, reg, data, len);
}

HAL_StatusTypeDef DS1307_ReadTime(DS1307_Time *t){
  uint8_t buf[3];
  HAL_StatusTypeDef st = DS1307_ReadReg(DS1307_REG_SECONDS, buf, 3);
  if (st != HAL_OK) return st;
  buf[0] &= 0x7F;                           /* CH=0 */
  t->seconds = bcd_to_dec(buf[0]);
  t->minutes = bcd_to_dec(buf[1]);
  t->hours   = bcd_to_dec((uint8_t)(buf[2] & 0x3Fu)); /* 24h */
  return HAL_OK;
}

HAL_StatusTypeDef DS1307_WriteTime(const DS1307_Time *t){
  uint8_t buf[3];
  buf[0] = dec_to_bcd((uint8_t)(t->seconds & 0x7Fu)); /* CH=0 */
  buf[1] = dec_to_bcd(t->minutes);
  buf[2] = dec_to_bcd(t->hours);
  return DS1307_WriteReg(DS1307_REG_SECONDS, buf, 3);
}

HAL_StatusTypeDef DS1307_ReadSeconds(uint8_t *sec){
  uint8_t b; HAL_StatusTypeDef st = DS1307_ReadReg(DS1307_REG_SECONDS, &b, 1);
  if (st != HAL_OK) return st;
  *sec = bcd_to_dec((uint8_t)(b & 0x7F));
  return HAL_OK;
}
HAL_StatusTypeDef DS1307_ReadMinutes(uint8_t *min){
  uint8_t b; HAL_StatusTypeDef st = DS1307_ReadReg(DS1307_REG_MINUTES, &b, 1);
  if (st != HAL_OK) return st;
  *min = bcd_to_dec(b);
  return HAL_OK;
}
HAL_StatusTypeDef DS1307_ReadHours(uint8_t *hour){
  uint8_t b; HAL_StatusTypeDef st = DS1307_ReadReg(DS1307_REG_HOURS, &b, 1);
  if (st != HAL_OK) return st;
  *hour = bcd_to_dec((uint8_t)(b & 0x3F));
  return HAL_OK;
}

void DS1307_StartIfHalted(void){
  uint8_t sec;
  if (DS1307_ReadReg(DS1307_REG_SECONDS, &sec, 1) == HAL_OK){
    if (sec & 0x80u){ sec &= 0x7Fu; (void)DS1307_WriteReg(DS1307_REG_SECONDS, &sec, 1); }
  }
}
