#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "System/system.h"
#include "System/delay.h"
#include "oledDriver/oledC.h"
#include "oledDriver/oledC_colors.h"
#include "oledDriver/oledC_shapes.h"
#include "Accel_i2c.h"
#include <libpic30.h>
#include <xc.h>

// ---------------- Defines ----------------
#define WRITE_ADDRESS 0x3A
#define REG_POWER_CTL 0x2D
#define REG_DATAX0 0x32
#define REG_DATAY0 0x34
#define REG_DATAZ0 0x36
#define MEASURE_MODE 0x08
#define STEP_THRESHOLD 600.0f

typedef struct
{
    int16_t x, y, z;
} ACCEL_DATA_t;

// ---------------- Globals ----------------
static bool wasAboveThreshold = false;
static bool movementDetected = false;
static uint16_t stepCount = 0;
static uint8_t inactivityCounter = 0;
const float baselineGravity = 1024.0f;

// Use a 60-second history for steps per minute calculation
#define HISTORY_SIZE 60                          // One slot per second (60 seconds total)
static uint8_t stepsHistory[HISTORY_SIZE] = {0}; // Holds steps for each of the last 60 seconds
static uint8_t currentSecondIndex = 0;           // Points to the current second slot

// For smoothing the displayed pace (steps per minute)
static float displayedPace = 0.0f;

// A global seconds counter updated every Timer1 interrupt
static uint32_t globalSeconds = 0;

typedef struct
{
    uint8_t hours, minutes, seconds, day, month;
} ClockTime;
static const uint8_t daysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
ClockTime currentTime = {8, 24, 35, 24, 1};

static bool footToggle = false;

// ---------------- Foot Bitmaps (16×16) ----------------
static const uint16_t foot1Bitmap[16] = {
    0x7800, 0xF800, 0xFC00, 0xFC00,
    0xFC00, 0x7C1E, 0x783E, 0x047F,
    0x3F9F, 0x1F3E, 0x0C3E, 0x003E,
    0x0004, 0x00F0, 0x01F0, 0x00E0};
static const uint16_t foot2Bitmap[16] = {
    0x001E, 0x003F, 0x003F, 0x007F,
    0x003F, 0x383E, 0x7C1E, 0x7E10,
    0x7E7C, 0x7E78, 0x7C30, 0x3C00,
    0x2000, 0x1E00, 0x1F00, 0x0E00};

// ---------------- Functions ----------------
void errorStop(char *msg)
{
    oledC_DrawString(0, 20, 1, 1, (uint8_t *)msg, OLEDC_COLOR_DARKRED);
    printf("Error: %s\n", msg);
    for (;;)
        ;
}

int16_t readAxis(uint8_t regAddress)
{
    uint8_t lowByte, highByte;
    int retries = 3;
    for (int i = 0; i < retries; i++)
    {
        if (i2cReadSlaveRegister(WRITE_ADDRESS, regAddress, &lowByte) == OK)
            break;
        if (i == retries - 1)
            errorStop("I2C Read Error (LSB)");
        DELAY_milliseconds(10);
    }
    for (int i = 0; i < retries; i++)
    {
        if (i2cReadSlaveRegister(WRITE_ADDRESS, regAddress + 1, &highByte) == OK)
            break;
        if (i == retries - 1)
            errorStop("I2C Read Error (MSB)");
        DELAY_milliseconds(10);
    }
    return ((int16_t)highByte << 8) | lowByte;
}

void initAccelerometer(void)
{
    I2Cerror err;
    uint8_t deviceId = 0;
    for (int i = 0; i < 3; i++)
    {
        err = i2cReadSlaveRegister(WRITE_ADDRESS, 0x00, &deviceId);
        if (err == OK && deviceId == 0xE5)
            break;
        if (i == 2)
            errorStop("I2C Error or Wrong Device ID");
        DELAY_milliseconds(10);
    }
    for (int i = 0; i < 3; i++)
    {
        err = i2cWriteSlave(WRITE_ADDRESS, REG_POWER_CTL, MEASURE_MODE);
        if (err == OK)
            break;
        if (i == 2)
            errorStop("Accel Power Error");
        DELAY_milliseconds(10);
    }
    for (int i = 0; i < 3; i++)
    {
        err = i2cWriteSlave(WRITE_ADDRESS, 0x31, 0x0B);
        if (err == OK)
            break;
        if (i == 2)
            errorStop("Accel Data Format Error");
        DELAY_milliseconds(10);
    }
}

void detectStep(void)
{
    ACCEL_DATA_t accel;
    accel.x = readAxis(REG_DATAX0);
    accel.y = readAxis(REG_DATAY0);
    accel.z = readAxis(REG_DATAZ0);

    float ax = accel.x * 4.0f;
    float ay = accel.y * 4.0f;
    float az = accel.z * 4.0f;
    float mag = sqrtf(ax * ax + ay * ay + az * az);
    float dynamic = fabsf(mag - baselineGravity);
    bool above = (dynamic > STEP_THRESHOLD);
    movementDetected = above;

    if (above && !wasAboveThreshold)
    {
        stepCount++; // (Keep total count if needed)
        // Record one step in the current second's slot.
        stepsHistory[currentSecondIndex]++;
        printf("Step detected! Count=%u\n", stepCount);
    }
    wasAboveThreshold = above;
}

void drawSteps(void)
{
    uint16_t sum = 0;
    for (int i = 0; i < HISTORY_SIZE; i++)
    {
        sum += stepsHistory[i];
    }
    // Raw pace is the total steps in the last 60 seconds (i.e., steps per minute)
    uint16_t rawPace = sum;

    // Update displayedPace once per second using globalSeconds.
    static uint32_t lastUpdateSecond = 0;
    if (globalSeconds != lastUpdateSecond)
    {
        if (rawPace > displayedPace)
        {
            // Increase gradually: 1 unit per second
            displayedPace += 1.0f;
            if (displayedPace > rawPace)
                displayedPace = rawPace;
        }
        else if (rawPace < displayedPace)
        {
            // Drop faster: 2 units per second (adjust this value if needed)
            displayedPace -= 2.0f;
            if (displayedPace < rawPace)
                displayedPace = rawPace;
        }
        lastUpdateSecond = globalSeconds;
    }

    uint16_t pace = (uint16_t)(displayedPace + 0.5f);

    static char oldStr[6] = "";
    char newStr[6];
    if (pace == 0)
    {
        if (oldStr[0] != '\0')
        {
            oledC_DrawString(80, 2, 1, 1, (uint8_t *)oldStr, OLEDC_COLOR_BLACK);
            oldStr[0] = '\0';
        }
        return;
    }
    sprintf(newStr, "%u", pace);
    if (strcmp(oldStr, newStr) != 0)
    {
        oledC_DrawString(80, 2, 1, 1, (uint8_t *)oldStr, OLEDC_COLOR_BLACK);
        oledC_DrawString(80, 2, 1, 1, (uint8_t *)newStr, OLEDC_COLOR_WHITE);
        strcpy(oldStr, newStr);
    }
}

static void twoDigitString(uint8_t val, char *buffer)
{
    buffer[0] = (val / 10) + '0';
    buffer[1] = (val % 10) + '0';
    buffer[2] = '\0';
}

void incrementTime(ClockTime *t)
{
    t->seconds++;
    if (t->seconds >= 60)
    {
        t->seconds = 0;
        t->minutes++;
    }
    if (t->minutes >= 60)
    {
        t->minutes = 0;
        t->hours++;
    }
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

void drawClock(ClockTime *time)
{
    static char oldTime[9] = "", oldDate[6] = "";
    char newTime[9], buff[3], newDate[6];
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

void drawFootIcon(uint8_t x, uint8_t y, const uint16_t *bitmap, uint8_t width, uint8_t height)
{
    for (uint8_t row = 0; row < height; row++)
        for (uint8_t col = 0; col < width; col++)
            if (bitmap[row] & (1 << (width - 1 - col)))
                oledC_DrawPoint(x + col, y + row, OLEDC_COLOR_WHITE);
}

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

void User_Initialize(void)
{
    TRISA &= ~(1 << 8 | 1 << 9);
    TRISA |= (1 << 11 | 1 << 12);
    TRISB |= (1 << 12);
    ANSB = 0;
    AD1CON1 = 0;
    AD1CON2 = 0;
    AD1CON3 = 0b001000011111111;
    AD1CHS = 0;
    AD1CHS |= (1 << 3);
    AD1CON1 |= (1 << 15);
}

void __attribute__((__interrupt__, auto_psv)) _T1Interrupt(void)
{
    incrementTime(&currentTime);
    footToggle = !footToggle;
    globalSeconds++; // Update global seconds counter

    // Update inactivity counter: if no movement is detected, increment it.
    if (!movementDetected)
    {
        inactivityCounter++;
        // After 2 seconds of inactivity, clear history and force pace to 0.
        if (inactivityCounter >= 4)
        {
            for (int i = 0; i < HISTORY_SIZE; i++)
            {
                stepsHistory[i] = 0;
            }
            displayedPace = 0.0f;
        }
    }
    else
    {
        // Reset inactivity counter if movement is detected.
        inactivityCounter = 0;
    }

    // Advance the circular buffer index and clear that slot.
    currentSecondIndex = (currentSecondIndex + 1) % HISTORY_SIZE;
    stepsHistory[currentSecondIndex] = 0;

    IFS0bits.T1IF = 0;
}

int main(void)
{
    int rc;
    uint8_t deviceId = 0;
    SYSTEM_Initialize();
    User_Initialize();
    oledC_setBackground(OLEDC_COLOR_BLACK);
    oledC_clearScreen();
    i2c1_open();
    for (int i = 0; i < 3; i++)
    {
        rc = i2cReadSlaveRegister(WRITE_ADDRESS, 0x00, &deviceId);
        if (rc == OK && deviceId == 0xE5)
            break;
        if (i == 2)
            errorStop("I2C Error or Wrong Device ID");
        DELAY_milliseconds(10);
    }
    initAccelerometer();
    Timer_Initialize();
    Timer1_Interrupt_Initialize();
    for (;;)
    {
        detectStep();
        drawSteps();
        drawClock(&currentTime);

        // Clear the foot icon area.
        oledC_DrawRectangle(0, 0, 15, 15, OLEDC_COLOR_BLACK);

        // Compute the sum of steps in the history buffer.
        uint16_t sum = 0;
        for (int i = 0; i < HISTORY_SIZE; i++)
        {
            sum += stepsHistory[i];
        }

        // Show the foot animation if any steps occurred in the last 60 seconds.
        if (sum > 0)
            drawFootIcon(0, 0, footToggle ? foot1Bitmap : foot2Bitmap, 16, 16);

        DELAY_milliseconds(100);
    }

    return 0;
}
