#include "xpt2046.h"

/* ====== Internal: SPI handle & pins ====== */
static SPI_HandleTypeDef *tp_spi = NULL;

static inline void TCS_LOW(void){  HAL_GPIO_WritePin(XPT_CS_GPIO, XPT_CS_PIN, GPIO_PIN_RESET); }
static inline void TCS_HIGH(void){ HAL_GPIO_WritePin(XPT_CS_GPIO, XPT_CS_PIN, GPIO_PIN_SET);   }
static inline uint8_t PENIRQ(void){return (uint8_t)HAL_GPIO_ReadPin(XPT_IRQ_GPIO, XPT_IRQ_PIN);} /* LOW when pressed */

/* ====== XPT2046 command bytes (12-bit differential) ====== */
#define CMD_X   0xD0  /* 1101 0000 : X position */
#define CMD_Y   0x90  /* 1001 0000 : Y position */

/* ====== Calibration & rotation ====== */
static int32_t cal_x_min = 350, cal_x_max = 3800;
static int32_t cal_y_min = 350, cal_y_max = 3800;

/* Output (displayed) size and rotation */
static uint16_t out_w = 320, out_h = 240;  /* MUST match UI after rotation */
static uint16_t rot   = 90;                /* 0/90/180/270 */

void XPT_SetCalibration(int32_t x_min, int32_t x_max,
                        int32_t y_min, int32_t y_max){
    cal_x_min = x_min; cal_x_max = x_max;
    cal_y_min = y_min; cal_y_max = y_max;
}

void XPT_Init(SPI_HandleTypeDef *hspi, uint8_t rot_deg,
              uint16_t screen_w, uint16_t screen_h){
    tp_spi = hspi;
    rot    = rot_deg;
    out_w  = screen_w;   /* displayed (rotated) width */
    out_h  = screen_h;   /* displayed (rotated) height */

    /* Ensure CS high when idle */
    TCS_HIGH();
    /* Pins config in CubeMX:
       - XPT_CS as Output PP, no pull
       - XPT_IRQ as Input with Pull-Up
       - SPI mode 0 (CPOL=0, CPHA=0) shared with TFT */
}

/* ====== Low-level: read 12-bit value for a given command ====== */
static uint16_t read12(uint8_t cmd){
    uint8_t tx[3] = {cmd, 0x00, 0x00};
    uint8_t rx[3] = {0};

    /* discard-first trick: toggle CS and make an extra dummy read improves stability */
    TCS_LOW();
    HAL_SPI_Transmit(tp_spi, &tx[0], 1, HAL_MAX_DELAY);
    HAL_SPI_TransmitReceive(tp_spi, &tx[1], &rx[1], 2, HAL_MAX_DELAY);
    TCS_HIGH();

    /* 12-bit packed: [rx1:8][rx2: high 4 bits][low 4 bits don't care] */
    return (uint16_t)((rx[1] << 5) | (rx[2] >> 3));
}

/* ====== Public: read raw averaged X/Y (no mapping) ====== */
uint8_t XPT_ReadRaw(uint16_t *raw_x, uint16_t *raw_y){
    if(PENIRQ() == GPIO_PIN_SET) return 0; /* not pressed (IRQ is low when pressed) */

    const uint8_t N = 6; /* small average for noise reduction */
    uint32_t sx = 0, sy = 0;

    /* First dummy cycle (recommended) */
    (void)read12(CMD_Y);
    (void)read12(CMD_X);

    for(uint8_t i=0;i<N;i++){
        uint16_t ry = read12(CMD_Y);
        uint16_t rx = read12(CMD_X);
        sx += rx; sy += ry;
    }

    if(raw_x) *raw_x = (uint16_t)(sx / N);
    if(raw_y) *raw_y = (uint16_t)(sy / N);
    return 1;
}

/* ====== Map raw -> screen directly in displayed space ====== */
static uint8_t map_to_screen(uint16_t rx, uint16_t ry, uint16_t *sx, uint16_t *sy){
    if(cal_x_max <= cal_x_min || cal_y_max <= cal_y_min) return 0;

    /* normalize 0..1 */
    float nx = (float)((int32_t)rx - cal_x_min) / (float)(cal_x_max - cal_x_min);
    float ny = (float)((int32_t)ry - cal_y_min) / (float)(cal_y_max - cal_y_min);
    if (nx < 0.f) { nx = 0.f; }
    if (nx > 1.f) { nx = 1.f; }

    if (ny < 0.f) { ny = 0.f; }
    if (ny > 1.f) { ny = 1.f; }

    /* apply rotation – ROT_90: sx=y; sy=x (no inversion) */
    switch(rot){
        case 0:
            *sx = (uint16_t)(nx * (out_w  - 1));
            *sy = (uint16_t)(ny * (out_h  - 1));
            break;
        case 90:
            *sx = (uint16_t)(ny * (out_w  - 1));
            *sy = (uint16_t)(nx * (out_h  - 1));
            break;
        case 180:
            *sx = (uint16_t)((1.f - nx) * (out_w  - 1));
            *sy = (uint16_t)((1.f - ny) * (out_h  - 1));
            break;
        case 270:
            *sx = (uint16_t)((1.f - ny) * (out_w  - 1));
            *sy = (uint16_t)((1.f - nx) * (out_h  - 1));
            break;
        default:
            *sx = (uint16_t)(nx * (out_w  - 1));
            *sy = (uint16_t)(ny * (out_h  - 1));
            break;
    }
    return 1;
}

/* ====== Public: mapped point ====== */
uint8_t XPT_GetPoint(XPT_TouchPoint *tp){
    if(!tp) return 0;
    tp->pressed = 0;

    uint16_t rx=0, ry=0;
    if(!XPT_ReadRaw(&rx,&ry)) return 0;

    uint16_t sx=0, sy=0;
    if(!map_to_screen(rx, ry, &sx, &sy)) return 0;

    tp->x = sx;
    tp->y = sy;
    tp->pressed = 1;
    return 1;
}

/* ====== Public: mapped + raw in one call ====== */
uint8_t XPT_GetPointWithRaw(XPT_TouchPoint *tp, uint16_t *raw_x, uint16_t *raw_y){
    if(!tp) return 0;
    tp->pressed = 0;

    uint16_t rx=0, ry=0;
    if(!XPT_ReadRaw(&rx,&ry)) return 0;

    uint16_t sx=0, sy=0;
    if(!map_to_screen(rx, ry, &sx, &sy)) return 0;

    if(raw_x) *raw_x = rx;
    if(raw_y) *raw_y = ry;

    tp->x = sx;
    tp->y = sy;
    tp->pressed = 1;
    return 1;
}
