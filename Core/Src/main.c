/* ============================= FILE HEADER ============================= */
/*
 * File   : main.c
 * Target : STM32F4 + ILI9341 + XPT2046 + DS1307 + LM75
 *
 * Features:
 * - I2C (software bit-bang on PB6/PB7) for DS1307 + LM75
 * - SPI for ILI9341 TFT + XPT2046 touch
 * - ADC (PC0 / IN10) for light sensor
 * - PWM (TIM4 CH3 / PB8) for MG90S servo
 * - Relay control on PB12
 * - GUI with 3 screens: STARTUP, CHECK, SETUP, PROJECT
 * - Time editing in SETUP and commit back to DS1307
 */

#include "main.h"                  // Core HAL definitions and project-level declarations
#include "spi.h"                   // SPI peripheral configuration header
#include "gpio.h"                  // GPIO initialization utilities
#include "adc.h"                   // ADC peripheral configuration header
#include "tim.h"                   // Timer peripheral configuration header

#include "ili9341.h"               // ILI9341 TFT driver API
#include "xpt2046.h"               // XPT2046 touch controller driver API
#include "i2c_sw.h"                // Software I2C bit-bang interface
#include "rtc_ds1307.h"            // DS1307 RTC driver
#include "sensors_lm75.h"          // LM75 temperature sensor driver

#include <string.h>                 // Standard string utilities
#include <stdio.h>                  // Standard formatted I/O utilities

/* ============================= UI ENUMS ================================ */

typedef enum {
    UI_STARTUP = 0,                 // Startup screen shown after boot
    UI_CHECK,                       // Check screen for quick sensor queries
    UI_SETUP,                       // Setup screen for adjusting parameters
    UI_PROJECT                      // Project screen for main logic display
} UIState;                          // Tracks which UI screen is active

typedef enum {
    SH_NONE = 0,                    // No setup button is active
    SH_HOUR_PLUS,                   // Hour increment button active
    SH_MIN_PLUS,                    // Minute increment button active
    SH_TTH_PLUS                     // Temperature threshold increment button active
} SetupHit;                         // Indicates which setup control is engaged

typedef enum {
    ROW_NONE = 0,                   // Touch not inside any setup row
    ROW_HOUR,                       // Hour row selected
    ROW_MIN,                        // Minute row selected
    ROW_TTH                         // Temperature threshold row selected
} SetupRow;                         // Describes which row corresponds to a touch

/* ============================= UI GEOMETRY ============================= */

#define SCR_W  320                  // Screen width in pixels
#define SCR_H  240                  // Screen height in pixels

/* Top navigation bar */
#define NAV_Y   8                   // Y offset for navigation bar
#define NAV_H   36                  // Height of navigation bar
#define NAV_GAP 6                   // Gap between navigation buttons
#define NAV_W   96                  // Width of each navigation button
#define BTN_CHECK_X  8              // X coordinate for "Check" button
#define BTN_SETUP_X  (BTN_CHECK_X + NAV_W + NAV_GAP) // X coordinate for "Setup" button
#define BTN_PROJ_X   (BTN_SETUP_X + NAV_W + NAV_GAP) // X coordinate for "Project" button

/* Content frame */
#define AREA_X   6                  // X coordinate for main content area
#define AREA_Y   (NAV_Y + NAV_H + 6) // Y coordinate for main content area
#define AREA_W   (SCR_W - 12)       // Width of main content area
#define AREA_H   (SCR_H - AREA_Y - 6) // Height of main content area

/* CHECK screen buttons (2x2 layout) */
#define SBTN_W   90                 // Width of small buttons on CHECK screen
#define SBTN_H   36                 // Height of small buttons on CHECK screen
#define SBTN_GX  12                 // Horizontal gap between small buttons
#define SBTN_GY  12                 // Vertical gap between small buttons

#define SBTN_T1_X   (AREA_X + 0*SBTN_W + 0*SBTN_GX) // X coordinate for first column buttons
#define SBTN_T2_X   (AREA_X + 1*SBTN_W + 1*SBTN_GX) // X coordinate for second column buttons

#define SBTN_ROW1_Y (AREA_Y + 8)    // Y coordinate for first row of buttons
#define SBTN_ROW2_Y (SBTN_ROW1_Y + SBTN_H + SBTN_GY) // Y coordinate for second row of buttons

#define RES_X    (AREA_X + 8)       // X coordinate for result text on CHECK screen
#define RES_Y    (AREA_Y + AREA_H - 28) // Y coordinate for result text on CHECK screen

/* SETUP screen layout */
#define VAL_W    56                 // Width of value display boxes
#define VAL_H    30                 // Height of value display boxes
#define VAL_X    (AREA_X + 128)     // X coordinate of value boxes
#define VAL1_Y   (AREA_Y + 6)       // Y coordinate for hour value box
#define VAL2_Y   (AREA_Y + 56)      // Y coordinate for minute value box
#define VAL3_Y   (AREA_Y + 106)     // Y coordinate for temperature threshold box

#define UBTN_W   48                 // Width of increment buttons
#define UBTN_H   36                 // Height of increment buttons
#define UBTN_X   (VAL_X + VAL_W + 8) // X coordinate for increment buttons

/* Auto-repeat timings */
#define REPEAT_DELAY_MS  400        // Delay before auto-repeat starts when holding a button
#define REPEAT_RATE_MS   100        // Interval between auto-repeat increments

/* ========================== INLINE HELPERS ============================= */

static inline uint8_t in_rect(uint16_t x, uint16_t y,
                              uint16_t rx, uint16_t ry,
                              uint16_t rw, uint16_t rh)
{
    return (x >= rx && x < rx + rw && y >= ry && y < ry + rh); // Check if a point lies inside a rectangle
}

/* ============================ UI STATE ================================= */

static UIState  ui_state      = UI_STARTUP; // Current UI screen
static uint8_t  was_down      = 0;          // Flag indicating previous touch state
static uint16_t last_x        = 0;          // Last touch X coordinate
static uint16_t last_y        = 0;          // Last touch Y coordinate
static uint8_t  topbar_down   = 0;          // Flag indicating touch started on top bar

static SetupHit  setup_active = SH_NONE;    // Active setup control for auto-repeat
static uint32_t  setup_t0     = 0;          // Timestamp when touch began
static uint32_t  setup_tlast  = 0;          // Timestamp of last auto-repeat action

/* ============================= LIVE VALUES ============================= */

static int   light_pct       = 0;      /* 0..100% mapped from ADC */ // Current light percentage
static float temp_c          = 26.5f;  /* LM75 temperature */         // Latest temperature reading
static int   hour            = 12;     // Current hour value
static int   minute          = 34;     // Current minute value
static int   second          = 56;     // Current second value
static int   temp_threshold  = 27;     /* User threshold */           // Temperature threshold set by user

static uint8_t  relay_on      = 0;     // Relay state indicator
static uint32_t proj_t0       = 0;     // Timestamp for project screen refresh
static uint8_t  time_from_rtc = 1;     /* 1=RTC, 0=software tick */  // Flag showing if time comes from RTC
static uint8_t  time_dirty    = 0;     /* 1=needs write to DS1307 */ // Flag showing pending RTC write

/* SERVO state */
static uint8_t  servo_enable  = 0;     // Servo sweep enable flag
static int      servo_angle   = 0;     // Current servo angle
static int8_t   servo_dir     = 1;     // Servo sweep direction
static uint32_t servo_t0_ms   = 0;     // Timestamp for servo updates

/* ============================= PROTOTYPES ============================== */

void SystemClock_Config(void);         // Forward declaration for clock configuration
static void DrawFrame(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t c); // Draw rectangle outline
static void DrawButton(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                       uint16_t bg, uint16_t fg, const char *label, uint8_t scale); // Draw UI button
static void UI_DrawTopBar(void);       // Render top navigation bar
static void UI_DrawStartup(void);      // Render startup screen
static void UI_DrawCheck(void);        // Render check screen
static void UI_DrawSetup(void);        // Render setup screen
static void UI_DrawProject(void);      // Render project screen
static void UI_ShowResult(const char *line); // Show result text on check screen

static void Setup_PrintHour(void);     // Print hour value in setup UI
static void Setup_PrintMin(void);      // Print minute value in setup UI
static void Setup_PrintTempTh(void);   // Print temperature threshold in setup UI
static SetupRow  setup_row_from_y(uint16_t y); // Map Y coordinate to setup row
static SetupHit  setup_hit_test(uint16_t x, uint16_t y); // Determine which setup control is hit
static void      setup_apply(SetupHit h); // Apply change based on setup control
static void      Setup_CommitTimeToRTC(void); // Write updated time to DS1307

static void SERVO_SetAngle(int angle_deg); // Set servo angle via PWM

static void REFRESH_Time_From_DS1307(void); // Update time variables from RTC
static void REFRESH_Temp_From_LM75(void);   // Update temperature from LM75
static void REFRESH_Light_From_ADC(void);   // Update light percentage from ADC

static void handle_touch_topbar(uint16_t x, uint16_t y); // Handle touches on navigation bar
static void handle_touch_check(uint16_t x, uint16_t y);  // Handle touches on check screen
static void handle_touch_setup_release(uint16_t x, uint16_t y); // Handle release on setup screen
static void handle_touch_project(uint16_t x, uint16_t y); // Handle touches on project screen

/* ============================ SENSOR HELPERS =========================== */

static void REFRESH_Time_From_DS1307(void)
{
    DS1307_Time t;                             // Structure to hold RTC time
    if (DS1307_ReadTime(&t) == HAL_OK) {       // Attempt to read time from RTC
        hour   = (int)t.hours;                 // Copy hours to local variable
        minute = (int)t.minutes;               // Copy minutes to local variable
        second = (int)t.seconds;               // Copy seconds to local variable
        time_from_rtc = 1;                     // Mark that time now comes from RTC
    }
}

static void REFRESH_Temp_From_LM75(void)
{
    float c = 0.0f;                            // Temporary variable for temperature
    if (LM75_ReadCelsius(&c) == HAL_OK) {      // Attempt to read temperature
        temp_c = c;                            // Store temperature if read succeeded
    }
}

/* Light sensor on ADC1 / PC0 / IN10, 12-bit, mapped to 0..100% */
static void REFRESH_Light_From_ADC(void)
{
    HAL_StatusTypeDef st;                      // Status variable for ADC operations
    uint32_t raw = 0U;                         // Raw ADC sample holder
    uint32_t pct = 0U;                         // Non-inverted percentage placeholder
    uint32_t lpct = 0U;                        // Inverted and scaled light percentage

    st = HAL_ADC_Start(&hadc1);                // Start ADC conversion
    if (st != HAL_OK) return;                  // Abort if start failed

    st = HAL_ADC_PollForConversion(&hadc1, 5); // Wait briefly for conversion
    if (st == HAL_OK) {                        // Proceed if conversion succeeded
        raw = HAL_ADC_GetValue(&hadc1);        // Read raw ADC value
        if (raw > 4095U) raw = 4095U;          // Clamp value to 12-bit maximum

        pct  = (raw * 100U) / 4095U;           // Convert to percentage (0..100%)
        lpct = 100U - pct;                     // Invert so more light equals higher percentage

        if (lpct <= 10U) {                     // Apply dead zone for low light
            lpct = 0U;                         // Set to zero inside dead zone
        } else {
            lpct = (lpct - 10U) * 100U / 90U;  // Scale remaining range to 0..100
        }

        light_pct = (int)lpct;                 // Store computed light percentage
    }

    HAL_ADC_Stop(&hadc1);                      // Stop ADC conversion
}

/* =========================== SERVO HELPER ============================== */
/* TIM4 configured to 1 MHz tick (Prescaler=83), Period=19999 → 50 Hz.   */
/* 0..180 deg → 600..2400 us pulse width for MG90S servo.                */
static void SERVO_SetAngle(int angle_deg)
{
    if (angle_deg < 0)   angle_deg = 0;        // Clamp angle to minimum
    if (angle_deg > 180) angle_deg = 180;      // Clamp angle to maximum

    uint32_t pulse_us = 600U + ((uint32_t)angle_deg * (2400U - 600U)) / 180U; // Map angle to pulse width
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3, pulse_us); // Update PWM duty cycle
}

/* ============================ UI PRIMITIVES ============================ */

static void DrawFrame(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t c)
{
    ILI9341_FillRect(x,         y,          w, 2, c); // Top border of frame
    ILI9341_FillRect(x,         y + h - 2,  w, 2, c); // Bottom border of frame
    ILI9341_FillRect(x,         y,          2, h, c); // Left border of frame
    ILI9341_FillRect(x + w - 2, y,          2, h, c); // Right border of frame
}

static uint16_t center_for_box(uint16_t box_x, uint16_t box_w,
                               const char *s, uint8_t scale)
{
    uint16_t tw = (uint16_t)(6 * scale * (uint16_t)strlen(s)); // Compute text width
    return (uint16_t)(box_x + (box_w - tw) / 2);               // Return centered X coordinate
}

static void DrawButton(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                       uint16_t bg, uint16_t fg, const char *label, uint8_t scale)
{
    ILI9341_FillRect(x, y, w, h, bg);            // Fill button background
    DrawFrame(x, y, w, h, fg);                   // Draw button border

    uint16_t tw = (uint16_t)(6 * scale * (uint16_t)strlen(label)); // Calculate label width
    uint16_t th = (uint16_t)(8 * scale);         // Calculate label height
    uint16_t tx = x + (w - tw) / 2;              // Center label horizontally
    uint16_t ty = y + (h - th) / 2;              // Center label vertically

    ILI9341_DrawString(tx, ty, label, fg, bg, scale); // Render button label
}

/* ============================ TOP BAR & STARTUP ======================== */

static void UI_DrawTopBar(void)
{
    ILI9341_FillRect(0, 0, SCR_W, NAV_Y + NAV_H + 2, COLOR_BLUE); // Paint top area background

    DrawButton(BTN_CHECK_X, NAV_Y, NAV_W, NAV_H,
               COLOR_YELLOW, COLOR_BLACK, "Check", 2); // Draw "Check" navigation button
    DrawButton(BTN_SETUP_X, NAV_Y, NAV_W, NAV_H,
               COLOR_YELLOW, COLOR_BLACK, "Setup", 2); // Draw "Setup" navigation button
    DrawButton(BTN_PROJ_X,  NAV_Y, NAV_W, NAV_H,
               COLOR_YELLOW, COLOR_BLACK, "Project", 2); // Draw "Project" navigation button

    DrawFrame(AREA_X, AREA_Y, AREA_W, AREA_H, COLOR_WHITE); // Outline main content area
}

static void UI_DrawStartup(void)
{
    ILI9341_SetRotation(ILI9341_ROT_90);        // Set screen rotation for landscape mode
    ILI9341_FillScreen(COLOR_BLACK);            // Clear screen with black background
    UI_DrawTopBar();                            // Draw navigation bar and frame

    ILI9341_DrawString(AREA_X + 20, AREA_Y + 20,
                       "Smart Irrigation System",
                       COLOR_CYAN, COLOR_BLACK, 2); // Show project title
    ILI9341_DrawString(AREA_X + 20, AREA_Y + 50,
                       "Ivgeni Goriatchev",
                       COLOR_WHITE, COLOR_BLACK, 2); // Show author name
    ILI9341_DrawString(AREA_X + 20, AREA_Y + 90,
                       "Tap any top button",
                       COLOR_GRAY, COLOR_BLACK, 2); // Prompt user to interact
}

/* ============================ CHECK SCREEN ============================= */

static void UI_ShowResult(const char *line)
{
    ILI9341_FillRect(AREA_X + 4, RES_Y - 2, AREA_W - 8, 22, COLOR_BLACK); // Clear previous result area
    ILI9341_DrawString(RES_X, RES_Y, line, COLOR_WHITE, COLOR_BLACK, 2);  // Display result text
}

static void UI_DrawCheck(void)
{
    ILI9341_SetRotation(ILI9341_ROT_90);        // Ensure correct rotation
    ILI9341_FillScreen(COLOR_BLACK);            // Clear display
    UI_DrawTopBar();                            // Draw navigation bar and frame

    /* Row 1: Time, Temp */
    DrawButton(SBTN_T1_X, SBTN_ROW1_Y, SBTN_W, SBTN_H,
               COLOR_GREEN, COLOR_WHITE, "Time", 2); // Button to read time
    DrawButton(SBTN_T2_X, SBTN_ROW1_Y, SBTN_W, SBTN_H,
               COLOR_GREEN, COLOR_WHITE, "Temp", 2); // Button to read temperature

    /* Row 2: Light, Relay */
    DrawButton(SBTN_T1_X, SBTN_ROW2_Y, SBTN_W, SBTN_H,
               COLOR_GREEN, COLOR_WHITE, "Light", 2); // Button to read light sensor
    DrawButton(SBTN_T2_X, SBTN_ROW2_Y, SBTN_W, SBTN_H,
               COLOR_GREEN, COLOR_WHITE, "Relay", 2); // Button to toggle relay

    char line[24];                               // Buffer for relay status text
    snprintf(line, sizeof(line), "Relay: %s", relay_on ? "ON" : "OFF"); // Format relay state string
    ILI9341_DrawString(AREA_X + 10, RES_Y - 30,
                       line, COLOR_WHITE, COLOR_BLACK, 2); // Show relay status above results

    UI_ShowResult("Result:");                    // Initialize result area text
}

/* ============================ SETUP SCREEN ============================= */

static void Setup_PrintHour(void)
{
    char buf[8];                                 // Buffer for formatted hour
    snprintf(buf, sizeof(buf), "%02d", hour);   // Convert hour to two-digit string

    ILI9341_FillRect(VAL_X + 2, VAL1_Y + 2, VAL_W - 4, VAL_H - 4, COLOR_BLUE); // Clear hour box interior
    uint16_t tx = center_for_box(VAL_X, VAL_W, buf, 2); // Calculate centered X position
    ILI9341_DrawString(tx, VAL1_Y + 8, buf, COLOR_WHITE, COLOR_BLUE, 2); // Render hour value
}

static void Setup_PrintMin(void)
{
    char buf[8];                                 // Buffer for formatted minute
    snprintf(buf, sizeof(buf), "%02d", minute); // Convert minute to two-digit string

    ILI9341_FillRect(VAL_X + 2, VAL2_Y + 2, VAL_W - 4, VAL_H - 4, COLOR_BLUE); // Clear minute box interior
    uint16_t tx = center_for_box(VAL_X, VAL_W, buf, 2); // Calculate centered X position
    ILI9341_DrawString(tx, VAL2_Y + 8, buf, COLOR_WHITE, COLOR_BLUE, 2); // Render minute value
}

static void Setup_PrintTempTh(void)
{
    char buf[8];                                 // Buffer for formatted threshold
    snprintf(buf, sizeof(buf), "%02d", temp_threshold); // Convert threshold to two-digit string

    ILI9341_FillRect(VAL_X + 2, VAL3_Y + 2, VAL_W - 4, VAL_H - 4, COLOR_BLUE); // Clear threshold box interior
    uint16_t tx = center_for_box(VAL_X, VAL_W, buf, 2); // Calculate centered X position
    ILI9341_DrawString(tx, VAL3_Y + 8, buf, COLOR_WHITE, COLOR_BLUE, 2); // Render threshold value
}

static SetupRow setup_row_from_y(uint16_t y)
{
    const uint16_t PAD_TOP = 4;                   // Padding before each row
    const uint16_t PAD_BOT = 6;                   // Padding after hour/min rows
    const uint16_t PAD_BOT_TTH = 20;              // Additional padding for temperature row

    if (y >= (VAL3_Y - PAD_TOP) && y < (VAL3_Y + UBTN_H + PAD_BOT_TTH)) // Check temperature row bounds
        return ROW_TTH;                           // Touch is in temperature row
    if (y >= (VAL2_Y - PAD_TOP) && y < (VAL2_Y + UBTN_H + PAD_BOT))     // Check minute row bounds
        return ROW_MIN;                           // Touch is in minute row
    if (y >= (VAL1_Y - PAD_TOP) && y < (VAL1_Y + UBTN_H + PAD_BOT))     // Check hour row bounds
        return ROW_HOUR;                          // Touch is in hour row

    return ROW_NONE;                              // Touch is outside editable rows
}

static SetupHit setup_hit_test(uint16_t x, uint16_t y)
{
    SetupRow r = setup_row_from_y(y);             // Determine which row is touched
    if (r == ROW_NONE) return SH_NONE;            // Abort if touch outside rows

    uint16_t ry = 0;                              // Variable for row Y coordinate
    switch (r) {                                  // Select Y position based on row
        case ROW_HOUR: ry = VAL1_Y; break;        // Use hour row Y coordinate
        case ROW_MIN:  ry = VAL2_Y; break;        // Use minute row Y coordinate
        case ROW_TTH:  ry = VAL3_Y; break;        // Use temperature row Y coordinate
        default:       return SH_NONE;            // Fallback safety
    }

    if (r == ROW_TTH) {                           // Special handling for threshold row
        if (in_rect(x, y, VAL_X, ry, (UBTN_X + UBTN_W) - VAL_X, UBTN_H)) // Check within combined box
            return SH_TTH_PLUS;                   // Touch targets threshold increment
        return SH_NONE;                           // Otherwise nothing active
    }

    if (in_rect(x, y, UBTN_X, ry, UBTN_W, UBTN_H)) { // Check if increment button touched
        return (r == ROW_HOUR) ? SH_HOUR_PLUS : SH_MIN_PLUS; // Return matching control
    }

    return SH_NONE;                               // Default to no control hit
}

static void setup_apply(SetupHit h)
{
    switch (h) {                                  // Act based on active control
        case SH_HOUR_PLUS:
            time_from_rtc = 0;                    // Use software time after manual edit
            time_dirty    = 1;                    // Mark time as needing RTC commit
            second        = 0;                    // Reset seconds when hour changes
            if (++hour >= 24) hour = 0;           // Wrap hour after 23
            Setup_PrintHour();                    // Refresh hour display
            break;

        case SH_MIN_PLUS:
            time_from_rtc = 0;                    // Switch to software time after edit
            time_dirty    = 1;                    // Mark time dirty for RTC write
            second        = 0;                    // Reset seconds when minute changes
            if (++minute >= 60) minute = 0;       // Wrap minute after 59
            Setup_PrintMin();                     // Refresh minute display
            break;

        case SH_TTH_PLUS:
            if (++temp_threshold > 35) temp_threshold = 20; // Cycle threshold within range
            Setup_PrintTempTh();                  // Refresh threshold display
            break;

        default:
            break;                                // No action for SH_NONE
    }
}

/* Commit edited time to DS1307 and go back to RTC mode */
static void Setup_CommitTimeToRTC(void)
{
    if (!time_dirty) return;                      // Exit if no pending edits

    DS1307_Time t;                                // Temporary structure for RTC write
    t.hours   = (uint8_t)hour;                    // Populate hours
    t.minutes = (uint8_t)minute;                  // Populate minutes
    t.seconds = (uint8_t)second;                  // Populate seconds

    if (DS1307_WriteTime(&t) == HAL_OK) {         // Attempt to write time to RTC
        time_from_rtc = 1;                        // Switch back to RTC time on success
    }

    time_dirty = 0;                               // Clear dirty flag regardless of result
}

static void UI_DrawSetup(void)
{
    ILI9341_SetRotation(ILI9341_ROT_90);          // Ensure display rotation is correct
    ILI9341_FillScreen(COLOR_BLACK);              // Clear the screen
    UI_DrawTopBar();                              // Draw navigation bar and frame

    ILI9341_DrawString(AREA_X + 10, AREA_Y + 10,
                       "Hour", COLOR_GREEN, COLOR_BLACK, 2); // Label for hour row
    ILI9341_DrawString(AREA_X + 10, AREA_Y + 50,
                       "Min",  COLOR_GREEN, COLOR_BLACK, 2); // Label for minute row
    ILI9341_DrawString(AREA_X + 10, AREA_Y + 100,
                       "Temp Th", COLOR_GREEN, COLOR_BLACK, 2); // Label for threshold row

    DrawButton(VAL_X, VAL1_Y, VAL_W, VAL_H, COLOR_BLUE, COLOR_WHITE, " ", 2); // Hour value placeholder
    DrawButton(VAL_X, VAL2_Y, VAL_W, VAL_H, COLOR_BLUE, COLOR_WHITE, " ", 2); // Minute value placeholder
    DrawButton(VAL_X, VAL3_Y, VAL_W, VAL_H, COLOR_BLUE, COLOR_WHITE, " ", 2); // Threshold value placeholder

    DrawButton(UBTN_X, VAL1_Y, UBTN_W, UBTN_H,
               COLOR_YELLOW, COLOR_BLACK, "+", 2); // Hour increment button
    DrawButton(UBTN_X, VAL2_Y, UBTN_W, UBTN_H,
               COLOR_YELLOW, COLOR_BLACK, "+", 2); // Minute increment button
    DrawButton(UBTN_X, VAL3_Y, UBTN_W, UBTN_H,
               COLOR_YELLOW, COLOR_BLACK, "+", 2); // Threshold increment button

    Setup_PrintHour();                            // Render current hour value
    Setup_PrintMin();                             // Render current minute value
    Setup_PrintTempTh();                          // Render current threshold value
}

/* ============================ PROJECT SCREEN =========================== */

static void UI_DrawProject(void)
{
    ILI9341_SetRotation(ILI9341_ROT_90);          // Ensure display rotation is correct
    ILI9341_FillScreen(COLOR_BLACK);              // Clear the screen
    UI_DrawTopBar();                              // Draw navigation bar and frame

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET); // Turn on debug LED during sensor reads
    REFRESH_Temp_From_LM75();                     // Update temperature reading
    REFRESH_Light_From_ADC();                     // Update light sensor reading
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET); // Turn off debug LED after reads

    char line[64];                                // Buffer for formatted strings

    snprintf(line, sizeof(line),
             "Time: %02d:%02d:%02d", hour, minute, second); // Format current time string
    ILI9341_DrawString(AREA_X + 10, AREA_Y + 12,
                       line, COLOR_WHITE, COLOR_BLACK, 2); // Display current time

    int t100 = (int)(temp_c * 100 + 0.5f);        // Convert temperature to integer hundredths
    snprintf(line, sizeof(line),
             "Temp: %d.%02d C (Th=%d)",
             t100 / 100, t100 % 100, temp_threshold); // Format temperature with threshold
    ILI9341_DrawString(AREA_X + 10, AREA_Y + 42,
                       line, COLOR_WHITE, COLOR_BLACK, 2); // Display temperature data

    snprintf(line, sizeof(line),
             "Light=%d%%", light_pct);           // Format light percentage string
    ILI9341_DrawString(AREA_X + 10, AREA_Y + 72,
                       line, COLOR_WHITE, COLOR_BLACK, 2); // Display light reading

    ILI9341_DrawString(AREA_X + 10, AREA_Y + 100,
                       "Logic will run here...",
                       COLOR_GRAY, COLOR_BLACK, 2); // Placeholder text for project logic
}

/* ============================ TOUCH HANDLERS =========================== */

static void handle_touch_topbar(uint16_t x, uint16_t y)
{
    if (in_rect(x, y, BTN_CHECK_X, NAV_Y, NAV_W, NAV_H)) { // Check if "Check" button pressed
        if (ui_state == UI_SETUP) {            // If leaving setup, commit time edits
            Setup_CommitTimeToRTC();           // Write pending time to RTC
        }
        ui_state = UI_CHECK;                   // Switch state to CHECK
        UI_DrawCheck();                        // Redraw CHECK screen
    }
    else if (in_rect(x, y, BTN_SETUP_X, NAV_Y, NAV_W, NAV_H)) { // Check if "Setup" pressed
        ui_state = UI_SETUP;                   // Switch state to SETUP
        UI_DrawSetup();                        // Redraw SETUP screen
    }
    else if (in_rect(x, y, BTN_PROJ_X, NAV_Y, NAV_W, NAV_H)) { // Check if "Project" pressed
        if (ui_state == UI_SETUP) {            // Commit edits if leaving SETUP
            Setup_CommitTimeToRTC();           // Save time changes to RTC
        }
        ui_state = UI_PROJECT;                 // Switch state to PROJECT
        UI_DrawProject();                      // Redraw PROJECT screen
        proj_t0 = HAL_GetTick();               // Reset project refresh timer
    }
}

static void handle_touch_check(uint16_t x, uint16_t y)
{
    char buf[40];                              // Buffer for result text

    /* Time button */
    if (in_rect(x, y, SBTN_T1_X, SBTN_ROW1_Y, SBTN_W, SBTN_H)) { // Touch on time button
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET); // Turn on debug LED
        REFRESH_Time_From_DS1307();            // Read current time from RTC
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET); // Turn off debug LED

        snprintf(buf, sizeof(buf),
                 "Time: %02d:%02d:%02d", hour, minute, second); // Format time string
        UI_ShowResult(buf);                    // Display formatted time
    }
    /* Temp button */
    else if (in_rect(x, y, SBTN_T2_X, SBTN_ROW1_Y, SBTN_W, SBTN_H)) { // Touch on temperature button
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET); // Turn on debug LED
        REFRESH_Temp_From_LM75();               // Read temperature from sensor
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET); // Turn off debug LED

        int t10 = (int)(temp_c * 10 + 0.5f);    // Convert temperature to tenths
        snprintf(buf, sizeof(buf),
                 "Temp: %d.%d C", t10 / 10, t10 % 10); // Format temperature string
        UI_ShowResult(buf);                      // Display temperature result
    }
    /* Light button */
    else if (in_rect(x, y, SBTN_T1_X, SBTN_ROW2_Y, SBTN_W, SBTN_H)) { // Touch on light button
        REFRESH_Light_From_ADC();               // Read light level
        snprintf(buf, sizeof(buf),
                 "Light: %d%%", light_pct);    // Format light string
        UI_ShowResult(buf);                     // Display light result
    }
    /* Relay button */
    else if (in_rect(x, y, SBTN_T2_X, SBTN_ROW2_Y, SBTN_W, SBTN_H)) { // Touch on relay button
        relay_on ^= 1;                          // Toggle relay state variable

        snprintf(buf, sizeof(buf),
                 "Relay: %s", relay_on ? "ON" : "OFF"); // Format relay status text
        ILI9341_FillRect(AREA_X + 10, RES_Y - 30, 160, 16, COLOR_BLACK); // Clear relay status area
        ILI9341_DrawString(AREA_X + 10, RES_Y - 30,
                           buf, COLOR_WHITE, COLOR_BLACK, 2); // Show updated relay status

        if (relay_on) {                         // Actions when turning relay on
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET); // Energize relay output
            servo_enable = 1;                   // Enable servo sweep
            servo_angle  = 0;                   // Reset servo angle
            servo_dir    = 1;                   // Start sweeping forward
            servo_t0_ms  = HAL_GetTick();       // Capture time for servo scheduling
            SERVO_SetAngle(servo_angle);        // Apply initial servo position
        } else {                                // Actions when turning relay off
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET); // De-energize relay
            servo_enable = 0;                   // Disable servo movement
        }
    }
}

static void handle_touch_setup_release(uint16_t x, uint16_t y)
{
    (void)x;                                    // Suppress unused parameter warning
    (void)y;                                    // Suppress unused parameter warning
    /* No special release action in SETUP */     // Placeholder comment for clarity
}

static void handle_touch_project(uint16_t x, uint16_t y)
{
    (void)x;                                    // Suppress unused parameter warning
    (void)y;                                    // Suppress unused parameter warning
    /* Reserved for future project interactions */ // Placeholder for future logic
}

/* =============================== MAIN ================================== */

int main(void)
{
    HAL_Init();                                  // Initialize the HAL library
    SystemClock_Config();                        // Configure system clocks

    MX_GPIO_Init();                              // Initialize GPIO peripheral
    MX_SPI1_Init();                              // Initialize SPI1 peripheral
    MX_ADC1_Init();                              // Initialize ADC1 peripheral
    MX_TIM4_Init();                              // Initialize TIM4 peripheral

    SWI2C_Init_PB6_PB7();                        // Initialize software I2C on PB6/PB7
    SWI2C_BusClear();                            // Clear I2C bus state

    __HAL_RCC_GPIOB_CLK_ENABLE();                // Enable clock for GPIOB

    GPIO_InitTypeDef GPIO_InitStruct = {0};      // GPIO configuration structure

    /* PB13: debug LED */
    GPIO_InitStruct.Pin   = GPIO_PIN_13;         // Select pin PB13
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP; // Configure as push-pull output
    GPIO_InitStruct.Pull  = GPIO_NOPULL;         // No pull-up or pull-down
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW; // Low speed output
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);      // Apply configuration to PB13

    /* PB12: relay control */
    GPIO_InitStruct.Pin = GPIO_PIN_12;           // Select pin PB12
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);      // Apply configuration to PB12

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET); // Ensure debug LED is off
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET); // Ensure relay is off

    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3);    // Start PWM generation on TIM4 channel 3
    SERVO_SetAngle(0);                           // Set servo to initial position

    DS1307_StartIfHalted();                      // Start RTC oscillator if it was halted

    ILI9341_Init(&hspi1);                        // Initialize TFT display driver
    ILI9341_SetRotation(ILI9341_ROT_90);         // Set display rotation

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET); // Turn on debug LED for sensor reads
    REFRESH_Time_From_DS1307();                  // Fetch current time from RTC
    REFRESH_Temp_From_LM75();                    // Fetch initial temperature
    REFRESH_Light_From_ADC();                    // Fetch initial light level
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET); // Turn off debug LED after reads

    UI_DrawStartup();                            // Draw startup screen

    XPT_Init(&hspi1, 90, 320, 240);              // Initialize touch controller
    XPT_SetCalibration(350, 3683, 350, 3802);    // Apply touch calibration values

    while (1) {                                  // Main application loop
        XPT_TouchPoint tp;                       // Structure to hold touch data

        if (XPT_GetPoint(&tp)) {                 // Check if touch is detected
            if (!was_down) {                     // If touch has just begun
                was_down   = 1;                  // Mark touch as active
                last_x     = tp.x;               // Store current X coordinate
                last_y     = tp.y;               // Store current Y coordinate

                topbar_down =
                    in_rect(tp.x, tp.y, BTN_CHECK_X, NAV_Y, NAV_W, NAV_H) || // Touch within "Check" button
                    in_rect(tp.x, tp.y, BTN_SETUP_X, NAV_Y, NAV_W, NAV_H) || // Touch within "Setup" button
                    in_rect(tp.x, tp.y, BTN_PROJ_X,  NAV_Y, NAV_W, NAV_H);   // Touch within "Project" button

                if (ui_state == UI_SETUP && !topbar_down) { // If on SETUP screen and not on top bar
                    setup_active = setup_hit_test(tp.x, tp.y); // Determine which control is pressed
                    if (setup_active != SH_NONE) {             // If a control is active
                        setup_t0    = HAL_GetTick();           // Record initial press time
                        setup_tlast = setup_t0;                // Initialize last repeat time
                        setup_apply(setup_active);             // Apply immediate change
                    }
                }
            } else {                            // Touch is continuing
                last_x = tp.x;                  // Update last X coordinate
                last_y = tp.y;                  // Update last Y coordinate

                if (ui_state == UI_SETUP && setup_active != SH_NONE) { // Handle auto-repeat on SETUP screen
                    if (setup_hit_test(tp.x, tp.y) == setup_active) { // Confirm finger still on control
                        uint32_t now = HAL_GetTick();           // Get current time
                        if ((now - setup_t0)   >= REPEAT_DELAY_MS &&
                            (now - setup_tlast) >= REPEAT_RATE_MS) { // Check repeat timing
                            setup_tlast = now;                  // Update last repeat time
                            setup_apply(setup_active);          // Apply repeated increment
                        }
                    }
                }
            }
        } else {                                // No touch currently detected
            if (was_down) {                     // If touch was previously active
                uint16_t x = last_x;            // Capture last touch X coordinate
                uint16_t y = last_y;            // Capture last touch Y coordinate

                if (topbar_down) {              // If touch started on navigation bar
                    handle_touch_topbar(x, y);  // Process navigation touch
                } else {                        // Otherwise process according to active screen
                    switch (ui_state) {
                        case UI_STARTUP:
                            break;              // No action on startup screen release
                        case UI_CHECK:
                            handle_touch_check(x, y); // Handle check screen release
                            break;
                        case UI_SETUP:
                            handle_touch_setup_release(x, y); // Handle setup release
                            break;
                        case UI_PROJECT:
                            handle_touch_project(x, y); // Handle project screen release
                            break;
                    }
                }

                was_down     = 0;               // Reset touch active flag
                topbar_down  = 0;               // Clear navigation touch flag
                setup_active = SH_NONE;         // Clear active setup control
            }
        }

        /* PROJECT screen periodic refresh */
        if (ui_state == UI_PROJECT) {           // Execute periodic updates on project screen
            uint32_t now = HAL_GetTick();       // Capture current system tick
            if (now - proj_t0 >= 1000U) {       // Check if one second elapsed
                proj_t0 = now;                  // Reset project refresh timer

                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET); // Turn on debug LED during refresh

                DS1307_Time t;                  // Temporary structure for RTC reads
                float c = 0.0f;                 // Temporary temperature variable
                HAL_StatusTypeDef st1 = HAL_OK; // Status for RTC read
                HAL_StatusTypeDef st2;          // Status for temperature read

                if (time_from_rtc) {            // If using RTC as time source
                    st1 = DS1307_ReadTime(&t);  // Attempt to read time from RTC
                    if (st1 == HAL_OK) {        // If read successful
                        hour   = t.hours;       // Update hour
                        minute = t.minutes;     // Update minute
                        second = t.seconds;     // Update second
                    }
                } else {                        // If using software timekeeping
                    second++;                   // Increment seconds
                    if (second >= 60) {         // Handle minute rollover
                        second = 0;             // Reset seconds
                        minute++;               // Increment minutes
                        if (minute >= 60) {     // Handle hour rollover
                            minute = 0;         // Reset minutes
                            hour++;             // Increment hours
                            if (hour >= 24) hour = 0; // Wrap hours after 23
                        }
                    }
                }

                st2 = LM75_ReadCelsius(&c);     // Read temperature from LM75
                if (st2 == HAL_OK) {            // If temperature read succeeded
                    temp_c = c;                 // Update stored temperature
                }

                REFRESH_Light_From_ADC();       // Update light reading

                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET); // Turn off debug LED after refresh

                char line[64];                  // Buffer for display strings

                ILI9341_FillRect(AREA_X + 10, AREA_Y + 12, 220, 16, COLOR_BLACK); // Clear time display area
                if (time_from_rtc && (st1 != HAL_OK)) { // Check for RTC read failure
                    snprintf(line, sizeof(line),
                             "Time: --:--:-- (I2C FAIL)"); // Show error message
                } else {
                    snprintf(line, sizeof(line),
                             "Time: %02d:%02d:%02d", hour, minute, second); // Show current time
                }
                ILI9341_DrawString(AREA_X + 10, AREA_Y + 12,
                                   line, COLOR_WHITE, COLOR_BLACK, 2); // Display time string

                ILI9341_FillRect(AREA_X + 10, AREA_Y + 42, 260, 16, COLOR_BLACK); // Clear temperature area
                if (st2 == HAL_OK) {            // If temperature read succeeded
                    int t100 = (int)(temp_c * 100 + 0.5f); // Convert to hundredths
                    snprintf(line, sizeof(line),
                             "Temp: %d.%02d C (Th=%d)",
                             t100 / 100, t100 % 100, temp_threshold); // Format temperature string
                } else {
                    snprintf(line, sizeof(line),
                             "Temp: --.- C (I2C FAIL)"); // Show temperature read error
                }
                ILI9341_DrawString(AREA_X + 10, AREA_Y + 42,
                                   line, COLOR_WHITE, COLOR_BLACK, 2); // Display temperature string

                ILI9341_FillRect(AREA_X + 10, AREA_Y + 72, 180, 16, COLOR_BLACK); // Clear light display area
                snprintf(line, sizeof(line),
                         "Light=%d%%", light_pct); // Format light percentage
                ILI9341_DrawString(AREA_X + 10, AREA_Y + 72,
                                   line, COLOR_WHITE, COLOR_BLACK, 2); // Display light percentage
            }
        }

        /* Non-blocking servo sweep while relay ON */
        if (servo_enable) {                      // Only update servo when enabled
            uint32_t now = HAL_GetTick();       // Get current tick
            if (now - servo_t0_ms >= 20U) {     // Check if update interval elapsed
                servo_t0_ms = now;              // Refresh servo timer
                servo_angle += (int)servo_dir * 5; // Step servo angle by 5 degrees
                if (servo_angle >= 180) {       // If reached upper limit
                    servo_angle = 180;          // Clamp to max
                    servo_dir   = -1;           // Reverse direction
                } else if (servo_angle <= 0) {  // If reached lower limit
                    servo_angle = 0;            // Clamp to min
                    servo_dir   = 1;            // Reverse direction
                }
                SERVO_SetAngle(servo_angle);    // Update servo position
            }
        }

        HAL_Delay(8);                            // Small delay to pace loop
    }
}

/* ========================= CLOCK CONFIGURATION ========================= */

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};  // Structure for oscillator configuration
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};  // Structure for clock configuration

    __HAL_RCC_PWR_CLK_ENABLE();                  // Enable power control clock
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1); // Configure voltage scaling

    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI; // Use internal HSI oscillator
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;             // Turn on HSI
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT; // Use default calibration
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;             // Enable PLL
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSI;      // Use HSI as PLL source
    RCC_OscInitStruct.PLL.PLLM            = 8;                      // PLLM divisor
    RCC_OscInitStruct.PLL.PLLN            = 168;                    // PLLN multiplier
    RCC_OscInitStruct.PLL.PLLP            = RCC_PLLP_DIV2;          // PLLP divisor
    RCC_OscInitStruct.PLL.PLLQ            = 4;                      // PLLQ divisor
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { // Apply oscillator configuration
        Error_Handler();                         // Handle configuration error
    }

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK  |
                                       RCC_CLOCKTYPE_SYSCLK|
                                       RCC_CLOCKTYPE_PCLK1 |
                                       RCC_CLOCKTYPE_PCLK2; // Specify clocks to configure
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK; // Use PLL as system clock
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;    // Set AHB prescaler
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4; /* 42 MHz */ // Set APB1 prescaler
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2; /* 84 MHz */ // Set APB2 prescaler

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) { // Apply clock configuration
        Error_Handler();                         // Handle configuration error
    }
}

/* ============================ ERROR HANDLER ============================ */

void Error_Handler(void)
{
    __disable_irq();                             // Disable interrupts to enter safe state
    while (1) {                                  // Infinite loop to indicate fatal error
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;                                  // Suppress unused parameter warning
    (void)line;                                  // Suppress unused parameter warning
}
#endif
