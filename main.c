/*
 * File:   FinalProject_Steps.c
 * Author: Vladimir lihatchov 322017252 lichvladimr@gmail.com
 */

#define FCY 4000000UL // For __delay_ms
#include <libpic30.h> // For __delay_ms()
#include <xc.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>          // for sqrtf
#include "System/system.h" // For SYSTEM_Initialize()
#include "System/delay.h"  // For DELAY_milliseconds() if needed
#include "oledDriver/oledC.h"
#include "oledDriver/oledC_colors.h"
#include "oledDriver/oledC_shapes.h"
#include "i2cDriver/i2c1_driver.h"


/* ----------------- I2C / Accelerometer Defines ----------------- */
// Example I2C address for ADXL345 in write mode:
#define ACCEL_WRITE_ADDRESS 0x3A
// Register addresses for X, Y, Z:
#define ACCEL_REG_DATAX0 0x32
#define ACCEL_REG_DATAY0 0x34
#define ACCEL_REG_DATAZ0 0x36

// Simple struct for raw accel data:
typedef struct
{
    int16_t x;
    int16_t y;
    int16_t z;
} ACCEL_DATA_t;

/* ----------------- Clock / Date Structures ----------------- */
typedef struct
{
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    uint8_t day;
    uint8_t month;
} ClockTime;

static const uint8_t daysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/* ----------------- Globals ----------------- */
ClockTime currentTime = {.hours = 12, .minutes = 0, .seconds = 0, .day = 7, .month = 3};
static bool wasAboveThreshold = false; // Step detection
static uint16_t stepCount = 0;         // Step counter

/* --------------- Function Prototypes --------------- */
void User_Initialize(void);
void Timer_Initialize(void);
void Timer1_Interrupt_Initialize(void);

// Accelerometer
void readAccelData(ACCEL_DATA_t *accel);

// Clock & Date
void incrementTime(ClockTime *time);
void drawClock(ClockTime *time);

// Steps
void detectStep(void);
void drawSteps(void);

/* Utility for 2-digit string */
static void twoDigitString(uint8_t val, char *buffer);

/* --------------- Timer ISR --------------- */
void __attribute__((__interrupt__, auto_psv)) _T1Interrupt(void)
{
    // Each second:
    incrementTime(&currentTime);
    IFS0bits.T1IF = 0;
}

/* --------------- main() --------------- */
int main(void)
{
    // 1) System + Peripherals
    SYSTEM_Initialize();
    User_Initialize();

    // 2) Timer Setup
    Timer_Initialize();
    Timer1_Interrupt_Initialize();

    // 3) Clear OLED
    oledC_clearScreen();

    // Main Loop
    while (1)
    {
        // A) Step detection
        detectStep();

        // B) Draw clock & date
        drawClock(&currentTime);

        // C) Draw step count
        drawSteps();

        // Slow down to ~10 Hz
        __delay_ms(100);
    }
    return 1;
}

/* ---------------- Implementation ---------------- */

/* 1) Pins, ADC config, etc. */
void User_Initialize(void)
{
    // For example, set LED pins as output, Switches as input
    TRISA &= ~(1 << 8); // LED1 as output
    TRISA &= ~(1 << 9); // LED2 as output
    TRISA |= (1 << 11); // S1 as input
    TRISA |= (1 << 12); // S2 as input

    // Example ADC config from your original code:
    TRISB |= (1 << 12);
    ANSB = 0;
    AD1CON1 = 0;
    AD1CON2 = 0;
    AD1CON3 = 0b001000011111111;
    AD1CHS = 0;
    AD1CHS |= (1 << 3);
    AD1CON1 |= (1 << 15);
}

/* 2) Timer ~1 second intervals (Fcy=4MHz, prescaler=256 => PR1=15625) */
void Timer_Initialize(void)
{
    TMR1 = 0;
    PR1 = 15625;
    T1CONbits.TON = 0;
    T1CONbits.TCKPS = 3; // 1:256
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

/* ---------------- Accelerometer ---------------- */
void readAccelData(ACCEL_DATA_t *accel)
{
    /*
      You must have i2cReadSlaveRegister(...) in your I2C driver.
      E.g.:

      void i2cReadSlaveRegister(uint8_t slaveAddr, uint8_t reg, unsigned char* dataPtr)
      {
          // i2c code to do start, write address, write reg, repeated start, read data
      }
    */

    // read X
    i2cReadSlaveRegister(ACCEL_WRITE_ADDRESS, ACCEL_REG_DATAX0, (unsigned char *)&accel->x);
    // read Y
    i2cReadSlaveRegister(ACCEL_WRITE_ADDRESS, ACCEL_REG_DATAY0, (unsigned char *)&accel->y);
    // read Z
    i2cReadSlaveRegister(ACCEL_WRITE_ADDRESS, ACCEL_REG_DATAZ0, (unsigned char *)&accel->z);
}

/* Step detection via threshold (~1.2g) */
void detectStep(void)
{
    ACCEL_DATA_t data;
    readAccelData(&data);

    // e.g. ±2g => ~4 mg per LSB
    float ax = data.x * 4.0f;
    float ay = data.y * 4.0f;
    float az = data.z * 4.0f;

    float mag = sqrtf(ax * ax + ay * ay + az * az);
    float threshold = 1200.0f; // ~1.2g in mg

    bool above = (mag > threshold);

    // Rising edge detection
    if (above && !wasAboveThreshold)
    {
        stepCount++;
    }
    wasAboveThreshold = above;
}

/* Displays stepCount at top-left corner */
void drawSteps(void)
{
    static char oldStepStr[6] = "";
    char newStepStr[6];
    sprintf(newStepStr, "%u", stepCount);

    // Only redraw if changed
    if (strcmp(oldStepStr, newStepStr) != 0)
    {
        // Erase old text (draw black)
        oledC_DrawString(0, 0, 1, 1, (uint8_t *)oldStepStr, OLEDC_COLOR_BLACK);
        // Draw new text
        oledC_DrawString(0, 0, 1, 1, (uint8_t *)newStepStr, OLEDC_COLOR_WHITE);
        strcpy(oldStepStr, newStepStr);
    }
}

/* ---------------- Clock & Date ---------------- */
void incrementTime(ClockTime *time)
{
    time->seconds++;
    if (time->seconds >= 60)
    {
        time->seconds = 0;
        time->minutes++;
        if (time->minutes >= 60)
        {
            time->minutes = 0;
            time->hours++;
            if (time->hours >= 24)
            {
                time->hours = 0;
                // new day
                time->day++;
                uint8_t mIndex = time->month - 1;
                if (time->day > daysInMonth[mIndex])
                {
                    time->day = 1;
                    time->month++;
                    if (time->month > 12)
                        time->month = 1; // no year logic
                }
            }
        }
    }
}

static void twoDigitString(uint8_t val, char *buffer)
{
    buffer[0] = (val / 10) + '0';
    buffer[1] = (val % 10) + '0';
    buffer[2] = '\0';
}

void drawClock(ClockTime *time)
{
    // We'll store the old strings to minimize flicker
    static char oldTimeString[9] = "";
    static char oldDateString[6] = "";

    // Build new time string "HH:MM:SS"
    char newTimeString[9];
    char buff[3];

    twoDigitString(time->hours, buff);
    sprintf(newTimeString, "%s:", buff);
    twoDigitString(time->minutes, buff);
    strcat(newTimeString, buff);
    strcat(newTimeString, ":");
    twoDigitString(time->seconds, buff);
    strcat(newTimeString, buff);

    // Overwrite old if changed
    if (strcmp(oldTimeString, newTimeString) != 0)
    {
        // Erase old time
        oledC_DrawString(8, 45, 2, 2, (uint8_t *)oldTimeString, OLEDC_COLOR_BLACK);
        // Draw new time
        oledC_DrawString(8, 45, 2, 2, (uint8_t *)newTimeString, OLEDC_COLOR_WHITE);
        strcpy(oldTimeString, newTimeString);
    }

    // Build new date "DD/MM"
    char newDateString[6];
    twoDigitString(time->day, buff);
    sprintf(newDateString, "%s/", buff);
    twoDigitString(time->month, buff);
    strcat(newDateString, buff);

    // If date changed, erase old, draw new
    if (strcmp(oldDateString, newDateString) != 0)
    {
        oledC_DrawString(65, 85, 1, 1, (uint8_t *)oldDateString, OLEDC_COLOR_BLACK);
        oledC_DrawString(65, 85, 1, 1, (uint8_t *)newDateString, OLEDC_COLOR_WHITE);
        strcpy(oldDateString, newDateString);
    }
}
