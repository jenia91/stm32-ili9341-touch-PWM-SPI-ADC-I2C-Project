#pragma once
#include "main.h"
#include <stdint.h>
#include <stddef.h>

extern SPI_HandleTypeDef hspi1;

#define ILI9341_WIDTH   240
#define ILI9341_HEIGHT  320

// ---- PIN MAP (התאם אם חיברת אחרת) ----
#define ILI9341_CS_Port   GPIOA
#define ILI9341_CS_Pin    GPIO_PIN_4
#define ILI9341_DC_Port   GPIOA
#define ILI9341_DC_Pin    GPIO_PIN_2
#define ILI9341_RST_Port  GPIOA
#define ILI9341_RST_Pin   GPIO_PIN_3
// ---------------------------------------

void ILI9341_Init(void);
void ILI9341_FillScreen(uint16_t color);
void ILI9341_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void ILI9341_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
