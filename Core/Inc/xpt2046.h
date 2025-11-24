#ifndef XPT2046_H
#define XPT2046_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* === Pin mapping (edit to your board) === */
#define XPT_CS_GPIO     GPIOA
#define XPT_CS_PIN      GPIO_PIN_1      /* T_CS = PA1 */
#define XPT_IRQ_GPIO    GPIOA
#define XPT_IRQ_PIN     GPIO_PIN_0      /* T_IRQ = PA0 (LOW when pressed) */

/* Touch point (screen coordinates after mapping) */
typedef struct {
    uint16_t x;
    uint16_t y;
    uint8_t  pressed;    /* 1 = touch detected (IRQ low) */
} XPT_TouchPoint;

/* API */
void XPT_Init(SPI_HandleTypeDef *hspi, uint8_t rot_deg,
              uint16_t screen_w, uint16_t screen_h);

/* Set raw calibration (read once from 4 corners and update here). */
void XPT_SetCalibration(int32_t x_min, int32_t x_max,
                        int32_t y_min, int32_t y_max);

/* Read mapped point (returns 1 if pressed and valid, fills tp) */
uint8_t XPT_GetPoint(XPT_TouchPoint *tp);

/* Optional: read raw 12-bit averages (no mapping) */
uint8_t XPT_ReadRaw(uint16_t *raw_x, uint16_t *raw_y);

/* Extended: mapped + raw in one call (for on-screen debug) */
uint8_t XPT_GetPointWithRaw(XPT_TouchPoint *tp, uint16_t *raw_x, uint16_t *raw_y);

#endif
