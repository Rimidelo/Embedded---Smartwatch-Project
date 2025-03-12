/*
 * File:   FinalProject_Steps.c
 * Author: Vladimir lihatchov 322017252 lichvladimr@gmail.com
 *
 * Description:
 *   - Displays clock and date (unchanged)
 *   - Displays a step counter near the top-left (offset to the right of the foot icon)
 *   - Uses custom 16×16 foot icon bitmaps (monochrome; white drawn on black)
 *   - Shows animated feet only when movement is detected.
 *     If no movement is detected for 10 seconds, the foot icon area is cleared.
 *   - Uses the Accel_i2c API to read the ADXL345.
 */

#define FCY 4000000UL // For __delay_ms()
#include <libpic30.h> // For __delay_ms()
#include <xc.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// System and OLED drivers
#include "System/system.h"
#include "System/delay.h"
#include "oledDriver/oledC.h"
#include "oledDriver/oledC_colors.h"
#include "oledDriver/oledC_shapes.h"

// I2C and Accelerometer API
#include "Accel_i2c.h"

// ---------------- ADXL345 Defines ----------------
#define WRITE_ADDRESS 0x3A // 8-bit write address for ADXL345
#define REG_POWER_CTL 0x2D
#define REG_DATAX0 0x32
#define REG_DATAY0 0x34
#define REG_DATAZ0 0x36
#define MEASURE_MODE 0x08

typedef struct
{
    int16_t x;
    int16_t y;
    int16_t z;
} ACCEL_DATA_t;

// ---------------- Clock/Date Structure ----------------
typedef struct
{
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    uint8_t day;
    uint8_t month;
} ClockTime;

static const uint8_t daysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

// ---------------- Globals ----------------
ClockTime currentTime = {8, 24, 35, 24, 1}; // Example initial time/date
static bool wasAboveThreshold = false;
static bool movementDetected = false; // true if dynamic acceleration > threshold
static uint16_t stepCount = 0;
static bool footToggle = false;       // toggles each second (used for animation)
static uint8_t inactivityCounter = 0; // seconds with no movement

// Define a baseline for gravity (adjust based on your sensor; for many ADXL345 setups zero-g might be near 0)
const float baselineGravity = 0.0f;

// ---------------- Custom Foot Bitmaps (16x16) ----------------
// Each bitmap is defined as 16 rows, with each row a 16-bit value.
// A set bit means "draw white pixel".
static const uint16_t foot1Bitmap[16] = {
    0x7800, // row0:  0 1 1 1 1 0 0 0 0 0 0 0 0 0 0 0
    0xF800, // row1:  1 1 1 1 1 0 0 0 0 0 0 0 0 0 0 0
    0xFC00, // row2:  1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0
    0xFC00, // row3:  same as row2
    0xFC00, // row4:  same as row2
    0x7C1E, // row5:  0 1 1 1 1 1 0 0 0 0 0 1 1 1 1 0
    0x783E, // row6:  0 1 1 1 1 0 0 0 0 0 1 1 1 1 1 0
    0x047F, // row7:  0 0 0 0 0 1 0 0 0 1 1 1 1 1 1 1
    0x3F9F, // row8:  0 0 1 1 1 1 1 0 0 1 1 1 1 1 1 1
    0x1F3E, // row9:  0 0 0 1 1 1 1 0 0 1 1 1 1 1 1 0
    0x0C3E, // row10: 0 0 0 0 1 1 0 0 0 0 1 1 1 1 1 0
    0x003E, // row11: 0 0 0 0 0 0 0 0 0 0 1 1 1 1 1 0
    0x0004, // row12: 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0
    0x00F0, // row13: 0 0 0 0 0 0 0 0 0 1 1 1 1 0 0 0
    0x01F0, // row14: 0 0 0 0 0 0 0 0 1 1 1 1 1 0 0 0
    0x00E0  // row15: 0 0 0 0 0 0 0 0 0 1 1 1 0 0 0 0
};

static const uint16_t foot2Bitmap[16] = {
    0x001E, // row0:  0 0 0 0 0 0 0 0 0 0 0 1 1 1 1 0
    0x003F, // row1:  0 0 0 0 0 0 0 0 0 0 1 1 1 1 1 1
    0x003F, // row2:  same as row1
    0x007F, // row3:  0 0 0 0 0 0 0 0 0 1 1 1 1 1 1 1
    0x003F, // row4:  0 0 0 0 0 0 0 0 0 0 1 1 1 1 1 1
    0x383E, // row5:  0 0 1 1 1 0 0 0 0 0 1 1 1 1 1 0
    0x7C1E, // row6:  0 1 1 1 1 1 0 0 0 0 0 1 1 1 1 0
    0x7E10, // row7:  0 1 1 1 1 1 1 0 0 0 1 0 0 0 0 0
    0x7E7C, // row8:  0 1 1 1 1 1 1 0 0 1 1 1 1 1 0 0
    0x7E78, // row9:  0 1 1 1 1 1 1 0 0 1 1 1 1 0 0 0
    0x7C30, // row10: 0 1 1 1 1 1 0 0 0 0 1 1 0 0 0 0
    0x3C00, // row11: 0 0 1 1 1 1 0 0 0 0 0 0 0 0 0 0
    0x2000, // row12: 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0
    0x1E00, // row13: 0 0 0 1 1 1 1 0 0 0 0 0 0 0 0 0
    0x1F00, // row14: 0 0 0 1 1 1 1 1 0 0 0 0 0 0 0 0
    0x0E00  // row15: 0 0 0 0 1 1 1 0 0 0 0 0 0 0 0 0
};

// -----------------------------------------------------------------
// Helper function to draw a 16x16 monochrome bitmap at (x,y).
// For each row, each bit (MSB = leftmost) that is set causes a white pixel to be drawn.
// -----------------------------------------------------------------
void drawFootIcon(uint8_t x, uint8_t y, const uint16_t *bitmap, uint8_t width, uint8_t height)
{
    for (uint8_t row = 0; row < height; row++)
    {
        uint16_t rowData = bitmap[row];
        for (uint8_t col = 0; col < width; col++)
        {
            // Check the (width-1-col)th bit; if set, draw white pixel.
            if (rowData & (1 << (width - 1 - col)))
            {
                oledC_DrawPoint(x + col, y + row, OLEDC_COLOR_WHITE);
            }
        }
    }
}

/* -------------- Prototypes -------------- */
void User_Initialize(void);
void Timer_Initialize(void);
void Timer1_Interrupt_Initialize(void);

void initAccelerometer(void);
void readAccelData(ACCEL_DATA_t *accel);

void detectStep(void);
void drawSteps(void);

void incrementTime(ClockTime *time);
void drawClock(ClockTime *time);

static void twoDigitString(uint8_t val, char *buffer);

/* ---------------- Timer1 ISR ----------------
   - Increments clock time (once per second)
   - Toggles footToggle for animation
   - Updates inactivityCounter: increments if no movement, resets if movement detected.
*/
void __attribute__((__interrupt__, auto_psv)) _T1Interrupt(void)
{
    incrementTime(&currentTime);
    footToggle = !footToggle;

    if (!movementDetected)
        inactivityCounter++;
    else
        inactivityCounter = 0;

    IFS0bits.T1IF = 0;
}

/* ========== main() ========== */
int main(void)
{
    SYSTEM_Initialize();
    User_Initialize();

    // Initialize accelerometer in measure mode.
    initAccelerometer();

    Timer_Initialize();
    Timer1_Interrupt_Initialize();

    // Setup OLED.
    oledC_setup();
    oledC_clearScreen();

    while (1)
    {
        // Step detection (updates movementDetected and stepCount).
        detectStep();

        // Draw clock & date.
        drawClock(&currentTime);

        // Draw step counter near top-left (offset at (12,2)).
        drawSteps();

        // Clear the foot icon region (e.g., (0,0) to (15,15)).
        oledC_DrawRectangle(0, 0, 15, 15, OLEDC_COLOR_BLACK);

        // Draw foot icons only if movement is detected and inactivity < 10 sec.
        if (inactivityCounter < 10 && movementDetected)
        {
            if (footToggle)
                drawFootIcon(0, 0, foot1Bitmap, 16, 16);
            else
                drawFootIcon(0, 0, foot2Bitmap, 16, 16);
        }

        __delay_ms(100); // Refresh ~10 Hz.
    }
    return 1;
}

/* ------------- Implementation ------------- */

/* User initialization: set pins and ADC as needed */
void User_Initialize(void)
{
    TRISA &= ~(1 << 8); // LED1 output.
    TRISA &= ~(1 << 9); // LED2 output.
    TRISA |= (1 << 11); // S1 input.
    TRISA |= (1 << 12); // S2 input.

    TRISB |= (1 << 12);
    ANSB = 0;

    AD1CON1 = 0;
    AD1CON2 = 0;
    AD1CON3 = 0b001000011111111;
    AD1CHS = 0;
    AD1CHS |= (1 << 3);
    AD1CON1 |= (1 << 15);
}

/* Timer1 initialization: ~1-second intervals (PR1 = 15625 for Fcy=4MHz with 1:256 prescaler) */
void Timer_Initialize(void)
{
    TMR1 = 0;
    PR1 = 15625;
    T1CONbits.TCKPS = 3;
    T1CONbits.TCS = 0;
    T1CONbits.TGATE = 0;
    T1CONbits.TON = 1;
}

void Timer1_Interrupt_Initialize(void)
{
    IPC0bits.T1IP = 5;
    IFS0bits.T1IF = 0;
    IEC0bits.T1IE = 1;
}

/* Initialize ADXL345: set device to measurement mode by writing MEASURE_MODE (0x08)
   to POWER_CTL (0x2D) register.
*/
void initAccelerometer(void)
{
    I2Cerror err = i2cWriteSlave(WRITE_ADDRESS, REG_POWER_CTL, MEASURE_MODE);
    if (err != OK)
    {
        // For debugging, you might print an error or blink an LED.
        printf("Accelerometer init error: %d\r\n", (int)err);
    }
}

/* Read acceleration data from ADXL345 using Accel_i2c API. */
void readAccelData(ACCEL_DATA_t *accel)
{
    i2cReadSlaveRegister(WRITE_ADDRESS, REG_DATAX0, (unsigned char *)&accel->x);
    i2cReadSlaveRegister(WRITE_ADDRESS, REG_DATAY0, (unsigned char *)&accel->y);
    i2cReadSlaveRegister(WRITE_ADDRESS, REG_DATAZ0, (unsigned char *)&accel->z);
}

/* Step detection:
   - Reads accelerometer values.
   - Converts raw data to mg (assuming 4 mg/LSB).
   - Computes magnitude.
   - Subtracts baselineGravity (set to 0.0f here; adjust if needed).
   - If dynamic acceleration > threshold (200 mg), then marks movement.
   - Increments stepCount on a rising edge.
*/
void detectStep(void)
{
    ACCEL_DATA_t data;
    readAccelData(&data);

    float ax = data.x * 4.0f;
    float ay = data.y * 4.0f;
    float az = data.z * 4.0f;
    float mag = sqrtf(ax * ax + ay * ay + az * az);

    float dynamic = fabsf(mag - baselineGravity);
    float threshold = 200.0f; // Adjust threshold as needed.
    bool above = (dynamic > threshold);
    movementDetected = above;

    if (above && !wasAboveThreshold)
    {
        stepCount++;
        printf("Step detected! Count=%u\r\n", stepCount); // Debug print.
    }
    wasAboveThreshold = above;
}

/* Draw the step counter at (12,2). */
void drawSteps(void)
{
    static char oldStr[6] = "";
    char newStr[6];
    sprintf(newStr, "%u", stepCount);
    if (strcmp(oldStr, newStr) != 0)
    {
        oledC_DrawString(12, 2, 1, 1, (uint8_t *)oldStr, OLEDC_COLOR_BLACK);
        oledC_DrawString(12, 2, 1, 1, (uint8_t *)newStr, OLEDC_COLOR_WHITE);
        strcpy(oldStr, newStr);
    }
}

/* Increment clock time with rollover. Also (re)updates inactivityCounter.
   (In this version, inactivityCounter is updated in the Timer1 ISR.)
*/
void incrementTime(ClockTime *t)
{
    t->seconds++;
    if (t->seconds >= 60)
    {
        t->seconds = 0;
        t->minutes++;
        if (t->minutes >= 60)
        {
            t->minutes = 0;
            t->hours++;
            if (t->hours >= 24)
            {
                t->hours = 0;
                t->day++;
                uint8_t idx = t->month - 1;
                if (t->day > daysInMonth[idx])
                {
                    t->day = 1;
                    t->month++;
                    if (t->month > 12)
                        t->month = 1;
                }
            }
        }
    }
}

/* Helper: Convert a number (0-99) to a two-digit string. */
static void twoDigitString(uint8_t val, char *buffer)
{
    buffer[0] = (val / 10) + '0';
    buffer[1] = (val % 10) + '0';
    buffer[2] = '\0';
}

/* Draw clock (HH:MM:SS) at (8,45) and date (DD/MM) at (65,85). */
void drawClock(ClockTime *time)
{
    static char oldTime[9] = "";
    static char oldDate[6] = "";
    char newTime[9];
    char buff[3];

    twoDigitString(time->hours, buff);
    sprintf(newTime, "%s:", buff);
    twoDigitString(time->minutes, buff);
    strcat(newTime, buff);
    strcat(newTime, ":");
    twoDigitString(time->seconds, buff);
    strcat(newTime, buff);

    if (strcmp(oldTime, newTime) != 0)
    {
        oledC_DrawString(8, 45, 2, 2, (uint8_t *)oldTime, OLEDC_COLOR_BLACK);
        oledC_DrawString(8, 45, 2, 2, (uint8_t *)newTime, OLEDC_COLOR_WHITE);
        strcpy(oldTime, newTime);
    }

    char newDate[6];
    twoDigitString(time->day, buff);
    sprintf(newDate, "%s/", buff);
    twoDigitString(time->month, buff);
    strcat(newDate, buff);

    if (strcmp(oldDate, newDate) != 0)
    {
        oledC_DrawString(65, 85, 1, 1, (uint8_t *)oldDate, OLEDC_COLOR_BLACK);
        oledC_DrawString(65, 85, 1, 1, (uint8_t *)newDate, OLEDC_COLOR_WHITE);
        strcpy(oldDate, newDate);
    }
}
