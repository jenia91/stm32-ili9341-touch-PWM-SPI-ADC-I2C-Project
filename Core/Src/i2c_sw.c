#include "i2c_sw.h"

/* ---- PB6=SCL, PB7=SDA ---- */
#define SW_SCL_GPIO   GPIOB
#define SW_SCL_PIN    GPIO_PIN_6
#define SW_SDA_GPIO   GPIOB
#define SW_SDA_PIN    GPIO_PIN_7

/* Open-Drain helpers: SET = release via pull-up */
static inline void SCL_HI(void){ HAL_GPIO_WritePin(SW_SCL_GPIO, SW_SCL_PIN, GPIO_PIN_SET); }
static inline void SCL_LO(void){ HAL_GPIO_WritePin(SW_SCL_GPIO, SW_SCL_PIN, GPIO_PIN_RESET); }
static inline void SDA_HI(void){ HAL_GPIO_WritePin(SW_SDA_GPIO, SW_SDA_PIN, GPIO_PIN_SET); }
static inline void SDA_LO(void){ HAL_GPIO_WritePin(SW_SDA_GPIO, SW_SDA_PIN, GPIO_PIN_RESET); }

static inline GPIO_PinState SDA_RD(void){ return HAL_GPIO_ReadPin(SW_SDA_GPIO, SW_SDA_PIN); }
static inline GPIO_PinState SCL_RD(void){ return HAL_GPIO_ReadPin(SW_SCL_GPIO, SW_SCL_PIN); }

/* precise µs delay — DWT->CYCCNT */
static inline void delay_us(uint32_t us){
  const uint32_t ticks = (SystemCoreClock/1000000U)*us;
  uint32_t start = DWT->CYCCNT;
  while ((DWT->CYCCNT - start) < ticks) { __NOP(); }
}

/* Wait for SCL release (clock stretching), timeout in µs */
static inline int scl_release_wait(uint32_t tout_us){
  SCL_HI();
  uint32_t start = DWT->CYCCNT, ticks = (SystemCoreClock/1000000U)*tout_us;
  while (SCL_RD()==GPIO_PIN_RESET){
    if ((DWT->CYCCNT - start) > ticks) return 0;
  }
  return 1;
}

static void START(void){
  SDA_HI(); SCL_HI(); delay_us(4);
  SDA_LO(); delay_us(4);
  SCL_LO(); delay_us(4);
}

static void STOP(void){
  SDA_LO(); delay_us(4);
  (void)scl_release_wait(200);
  delay_us(4);
  SDA_HI(); delay_us(4);
}

/* Write byte; return 1 on ACK */
static int WR(uint8_t b){
  for (int i=7;i>=0;i--){
    (b & (1U<<i)) ? SDA_HI() : SDA_LO();
    delay_us(2);
    if (!scl_release_wait(200)) return 0;
    delay_us(3);
    SCL_LO(); delay_us(2);
  }
  /* ACK bit from slave */
  SDA_HI(); delay_us(2);                /* release */
  if (!scl_release_wait(200)) return 0;
  int ack = (SDA_RD()==GPIO_PIN_RESET);
  delay_us(3);
  SCL_LO(); delay_us(2);
  return ack;
}

/* Read byte; ack=1 -> send ACK, ack=0 -> NACK */
static uint8_t RD(int ack){
  uint8_t v=0;
  SDA_HI();                              /* release for slave drive */
  for (int i=7;i>=0;i--){
    if (!scl_release_wait(200)) break;
    delay_us(2);
    if (SDA_RD()==GPIO_PIN_SET) v |= (uint8_t)(1U<<i);
    SCL_LO(); delay_us(2);
  }
  if (ack) SDA_LO(); else SDA_HI();
  delay_us(2);
  (void)scl_release_wait(200);
  delay_us(3);
  SCL_LO(); delay_us(2);
  SDA_HI();
  return v;
}

void SWI2C_Init_PB6_PB7(void){
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* Enable DWT CYCCNT */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;

  GPIO_InitTypeDef g = {0};
  g.Mode  = GPIO_MODE_OUTPUT_OD;
  g.Pull  = GPIO_PULLUP;                /* ext. 4.7k to 3.3V recommended */
  g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

  g.Pin = SW_SCL_PIN; HAL_GPIO_Init(SW_SCL_GPIO, &g);
  g.Pin = SW_SDA_PIN; HAL_GPIO_Init(SW_SDA_GPIO, &g);

  SCL_HI(); SDA_HI(); delay_us(5);      /* idle-high */
}

/* UM10204 §3.1.16: up to 9 clocks if SDA stuck low */
void SWI2C_BusClear(void){
  if (SDA_RD()==GPIO_PIN_RESET){
    for (int i=0;i<9;i++){
      SCL_LO(); delay_us(5);
      SCL_HI(); delay_us(5);
      if (SDA_RD()==GPIO_PIN_SET) break;
    }
  }
  STOP();
}

HAL_StatusTypeDef SWI2C_Mem_Read(uint8_t addr8, uint8_t mem, uint8_t *data, uint16_t len){
  START();
  if (!WR((uint8_t)(addr8 & ~1U))) { STOP(); return HAL_ERROR; }
  if (!WR(mem))                    { STOP(); return HAL_ERROR; }
  START();
  if (!WR((uint8_t)(addr8 | 1U)))  { STOP(); return HAL_ERROR; }
  for (uint16_t i=0;i<len;i++){
    data[i] = RD((i < (len-1)) ? 1 : 0);
  }
  STOP();
  return HAL_OK;
}

HAL_StatusTypeDef SWI2C_Mem_Write(uint8_t addr8, uint8_t mem, const uint8_t *data, uint16_t len){
  START();
  if (!WR((uint8_t)(addr8 & ~1U))) { STOP(); return HAL_ERROR; }
  if (!WR(mem))                    { STOP(); return HAL_ERROR; }
  for (uint16_t i=0;i<len;i++){
    if (!WR(data[i]))              { STOP(); return HAL_ERROR; }
  }
  STOP();
  return HAL_OK;
}

uint8_t SWI2C_Scan_One(uint8_t addr7){
  uint8_t ok=0;
  START();
  if (WR((uint8_t)(addr7<<1))) ok=1;
  STOP();
  return ok;
}
