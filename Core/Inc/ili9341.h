#ifndef ILI9341_HAL_SIMPLE_H
#define ILI9341_HAL_SIMPLE_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

#define ILI9341_CS_GPIO   GPIOA
#define ILI9341_CS_PIN    GPIO_PIN_4   // CS
#define ILI9341_DC_GPIO   GPIOA
#define ILI9341_DC_PIN    GPIO_PIN_3   // D/C
#define ILI9341_RST_GPIO  GPIOA
#define ILI9341_RST_PIN   GPIO_PIN_2   // RESET

// Native panel resolution (max bounds)
#define ILI9341_WIDTH_NATIVE   240
#define ILI9341_HEIGHT_NATIVE  320

// Colors (RGB565)
#define COLOR_BLACK   0x0000
#define COLOR_BLUE    0x001F
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F
#define COLOR_YELLOW  0xFFE0
#define COLOR_WHITE   0xFFFF
#define COLOR_GRAY    0x8410
#define COLOR_ORANGE  0xFD20

// Rotation presets for MADCTL (portrait/landscape)
typedef enum {
    ILI9341_ROT_0   = 0x48, // Portrait: X=0..239, Y=0..319 (MY=1,BGR=1)
    ILI9341_ROT_90  = 0x28, // Landscape: X=0..319, Y=0..239
    ILI9341_ROT_180 = 0x88, // Portrait 180
    ILI9341_ROT_270 = 0xE8  // Landscape 270
} ILI9341_Rotation;

// API
void ILI9341_Init(SPI_HandleTypeDef *hspi);
void ILI9341_SetRotation(ILI9341_Rotation rot);
uint16_t ILI9341_GetWidth(void);
uint16_t ILI9341_GetHeight(void);

void ILI9341_FillScreen(uint16_t color);
void ILI9341_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void ILI9341_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

// Text API (fixed 5x7 font, 1px spacing)
void ILI9341_DrawChar(uint16_t x, uint16_t y, char c,
                      uint16_t fg, uint16_t bg, uint8_t scale);
void ILI9341_DrawString(uint16_t x, uint16_t y, const char *str,
                        uint16_t fg, uint16_t bg, uint8_t scale);

// Backward-compat stub alias (kept for your build)
static inline void ILI9341_DrawText(uint16_t x, uint16_t y, const char *str,
                                    uint16_t color, uint16_t bg) {
    ILI9341_DrawString(x, y, str, color, bg, 1);
}

#endif
