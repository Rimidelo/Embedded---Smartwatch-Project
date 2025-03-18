/*
 * File:   Final Project - Smart Watch
 * Author: Vladimir Lihatchov - 322017252 - lichvladimr@gmail.com
 */

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

// ---------------- Button Pin Defines ----------------
#define S1_PORT PORTAbits.RA0
#define S2_PORT PORTAbits.RA1
#define S1_TRIS TRISAbits.TRISA0
#define S2_TRIS TRISAbits.TRISA1

// ---------------- Defines ----------------
#define WRITE_ADDRESS 0x3A
#define REG_POWER_CTL 0x2D
#define REG_DATAX0 0x32
#define REG_DATAY0 0x34
#define REG_DATAZ0 0x36
#define MEASURE_MODE 0x08
#define HISTORY_SIZE 60
#define STEP_THRESHOLD 900.0f
#define GRAPH_WIDTH 90
#define GRAPH_HEIGHT 100

// ---------------- Type and Globals for Steps Graph ----------------
uint8_t instantStepRate[GRAPH_WIDTH] = {0}; // Holds instantaneous step rates (steps per minute)
uint32_t lastStepTime = 0;                  // Time (in seconds) when the last step occurred

// ---------------- Type and Globals for Set Time ----------------
typedef struct
{
    uint8_t hours;
    uint8_t minutes;
} TimeSetting;

TimeSetting setClock = {8, 24};
// 0 means hours selected; 1 means minutes selected.
uint8_t timeSelection = 0;

// ---------------- Type and Globals for Set Date ----------------
typedef struct
{
    uint8_t day;
    uint8_t month;
} DateSetting;

DateSetting setDate = {24, 1}; // initialize with currentTime.day and currentTime.month
// 0 means day is selected; 1 means month is selected.
uint8_t dateSelection = 0;

typedef struct
{
    int16_t x, y, z;
} ACCEL_DATA_t;

// ---------------- Globals for Pedometer & Clock ----------------
static bool wasAboveThreshold = false;
static bool movementDetected = false;
static uint16_t stepCount = 0;
const float baselineGravity = 1024.0f;
static uint8_t stepsHistory[HISTORY_SIZE] = {0};
static uint8_t currentSecondIndex = 0;
// For smoothing the displayed pace
static float displayedPace = 0.0f;
// Global seconds counter (updated every Timer1 interrupt)
static uint32_t globalSeconds = 0;
bool is12HourFormat = false;
bool inTimeFormatSubpage = false;
bool inTimeSetSubpage = false;
// For the sub-page selection: 0 => “12H”, 1 => “24H”
uint8_t timeFormatSelectedIndex = 0;
static bool footToggle = false;
bool forceClockRedraw = false;
typedef struct
{
    uint8_t hours, minutes, seconds, day, month;
} ClockTime;
static const uint8_t daysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
ClockTime currentTime = {8, 24, 35, 24, 1};
// ---------------- Function declaration to avoid implicit warnings ----------------
void updateMenuClock(void);
void drawTimeFormatSubpage(void);
void drawSetTimeMenuBase(void);
void drawSetTimeStatus(void);
void handleSetTimeInput(void);
bool detectTiltForSave(void);
void drawSetDateStatus(void);
extern bool inMenu;
extern void drawMenu(void);
bool graphActive = false; // Indicates if we are currently in the graph screen

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

// ---------------- Functions for Pedometer, Clock, etc. ----------------
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

    // If crossing threshold from below => new step
    if (above && !wasAboveThreshold)
    {
        stepCount++;
        stepsHistory[currentSecondIndex]++;
        printf("Step detected! Count=%u\n", stepCount);
    }

    wasAboveThreshold = above;
}

void drawSteps(void)
{
    static char oldStr[6] = "";
    char newStr[6];

    // If displayedPace is effectively zero, erase the old text and return.
    if (displayedPace <= 0.5f)
    {
        if (oldStr[0] != '\0')
        {
            oledC_DrawString(80, 2, 1, 1, (uint8_t *)oldStr, OLEDC_COLOR_BLACK);
            oldStr[0] = '\0';
        }
        return;
    }

    // Convert the current float pace to an integer string (e.g. "12", "45", etc.)
    sprintf(newStr, "%u", (uint16_t)(displayedPace + 0.5f));

    // Only redraw if the displayed string actually changed
    if (strcmp(oldStr, newStr) != 0)
    {
        // Erase the old text
        oledC_DrawString(80, 2, 1, 1, (uint8_t *)oldStr, OLEDC_COLOR_BLACK);

        // Draw the new text
        oledC_DrawString(80, 2, 1, 1, (uint8_t *)newStr, OLEDC_COLOR_WHITE);

        // Save the newStr for next comparison
        strcpy(oldStr, newStr);
    }
}

static void twoDigitString(uint8_t val, char *buffer)
{
    buffer[0] = (val / 10) + '0';
    buffer[1] = (val % 10) + '0';
    buffer[2] = '\0';
}

void updateDate(ClockTime *t)
{
    t->day++;                                 // Increment the day
    uint8_t days = daysInMonth[t->month - 1]; // Get max days for current month
    if (t->day > days)
    {
        t->day = 1;
        t->month++;
        if (t->month > 12)
            t->month = 1;
    }
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
        updateDate(t);
    }
}

void drawClock(ClockTime *time)
{
    static char oldTime[9] = "";
    static char oldDate[6] = "";
    static bool oldWas12H = false;
    static bool oldPM = false;

    if (forceClockRedraw)
    {
        oldTime[0] = '\0';
        oldDate[0] = '\0';
        oldWas12H = !is12HourFormat;
        oldPM = false;
        forceClockRedraw = false;
    }

    char newTime[9];
    char buff[3];
    char newDateStr[6];
    bool pm = false;

    uint8_t displayHrs = time->hours;
    if (is12HourFormat)
    {
        if (displayHrs == 0)
        {
            displayHrs = 12; // midnight is 12 AM
        }
        else if (displayHrs >= 12)
        {
            pm = true;
            if (displayHrs > 12)
                displayHrs -= 12;
        }
    }

    twoDigitString(displayHrs, buff);
    sprintf(newTime, "%s:", buff);
    twoDigitString(time->minutes, buff);
    strcat(newTime, buff);
    strcat(newTime, ":");
    twoDigitString(time->seconds, buff);
    strcat(newTime, buff);

    // If there is any change in time or format, clear old areas and redraw.
    if (strcmp(oldTime, newTime) != 0 || oldWas12H != is12HourFormat || oldPM != pm)
    {
        oledC_DrawString(8, 45, 2, 2, (uint8_t *)oldTime, OLEDC_COLOR_BLACK);
        oledC_DrawRectangle(50, 45, 80, 60, OLEDC_COLOR_BLACK);
        oledC_DrawString(8, 45, 2, 2, (uint8_t *)newTime, OLEDC_COLOR_WHITE);
        if (is12HourFormat)
        {
            if (pm)
                oledC_DrawString(0, 85, 1, 1, (uint8_t *)"PM", OLEDC_COLOR_WHITE);
            else
                oledC_DrawString(0, 85, 1, 1, (uint8_t *)"AM", OLEDC_COLOR_WHITE);
        }

        strcpy(oldTime, newTime);
        oldWas12H = is12HourFormat;
        oldPM = pm;
    }

    twoDigitString(time->day, buff);
    sprintf(newDateStr, "%s/", buff);
    twoDigitString(time->month, buff);
    strcat(newDateStr, buff);

    if (strcmp(oldDate, newDateStr) != 0)
    {
        oledC_DrawString(65, 85, 1, 1, (uint8_t *)oldDate, OLEDC_COLOR_BLACK);
        oledC_DrawString(65, 85, 1, 1, (uint8_t *)newDateStr, OLEDC_COLOR_WHITE);
        strcpy(oldDate, newDateStr);
    }
}

void drawFootIcon(uint8_t x, uint8_t y, const uint16_t *bitmap, uint8_t width, uint8_t height)
{
    for (uint8_t row = 0; row < height; row++)
        for (uint8_t col = 0; col < width; col++)
            if (bitmap[row] & (1 << (width - 1 - col)))
                oledC_DrawPoint(x + col, y + row, OLEDC_COLOR_WHITE);
}

// ---------------- 12H/24H SYSTEM ---------------- //
void handleTimeFormatSelection(void)
{
    inTimeFormatSubpage = true;
    timeFormatSelectedIndex = (is12HourFormat ? 0 : 1);

    drawTimeFormatSubpage();

    bool s1WasPressed = false;
    bool s2WasPressed = false;

    while (inTimeFormatSubpage)
    {
        bool s1State = (PORTAbits.RA11 == 0); // Confirmation button
        bool s2State = (PORTAbits.RA12 == 0); // Navigation button

        // S2 cycles the selection on a rising edge.
        if (s2State && !s2WasPressed)
        {
            timeFormatSelectedIndex = (timeFormatSelectedIndex + 1) % 2;
            drawTimeFormatSubpage();
        }

        // S1 confirms the current selection.
        if (s1State && !s1WasPressed)
        {
            is12HourFormat = (timeFormatSelectedIndex == 0); // index 0 = 12H, 1 = 24H
            inTimeFormatSubpage = false;
            break;
        }

        s1WasPressed = s1State;
        s2WasPressed = s2State;

        DELAY_milliseconds(100); // Adjust as needed.
    }
}

void drawTimeFormatSubpage(void)
{
    oledC_clearScreen();
    // Display a title (optional)
    oledC_DrawString(10, 5, 1, 1, (uint8_t *)"Choose Format:", OLEDC_COLOR_WHITE);

    // Draw "12H" item at y=25.
    if (timeFormatSelectedIndex == 0)
    {
        oledC_DrawRectangle(8, 23, 60, 35, OLEDC_COLOR_WHITE);
        oledC_DrawString(10, 25, 1, 1, (uint8_t *)"12H", OLEDC_COLOR_BLACK);
    }
    else
    {
        oledC_DrawString(10, 25, 1, 1, (uint8_t *)"12H", OLEDC_COLOR_WHITE);
    }

    // Draw "24H" item at y=40.
    if (timeFormatSelectedIndex == 1)
    {
        oledC_DrawRectangle(8, 38, 60, 50, OLEDC_COLOR_WHITE);
        oledC_DrawString(10, 40, 1, 1, (uint8_t *)"24H", OLEDC_COLOR_BLACK);
    }
    else
    {
        oledC_DrawString(10, 40, 1, 1, (uint8_t *)"24H", OLEDC_COLOR_WHITE);
    }
}

// ---------------- SET TIME SYSTEM ---------------- //
void drawSetTimeMenuBase(void)
{
    oledC_clearScreen();

    // Clear any mini clock area.
    oledC_DrawRectangle(30, 2, 115, 10, OLEDC_COLOR_BLACK);

    // Display header.
    oledC_DrawString(6, 10, 2, 2, (uint8_t *)"Set Time", OLEDC_COLOR_WHITE);

    if (timeSelection == 0)
    {
        // Hours selected: draw hours box with white border and black fill.
        oledC_DrawRectangle(8, 40, 44, 64, OLEDC_COLOR_WHITE);
        oledC_DrawRectangle(10, 42, 42, 62, OLEDC_COLOR_BLACK);

        // Draw minute box completely black (shifted left).
        oledC_DrawRectangle(50, 40, 86, 64, OLEDC_COLOR_BLACK);
        oledC_DrawRectangle(52, 42, 84, 62, OLEDC_COLOR_BLACK);
    }
    else
    {
        // Minutes selected: draw minute box with white border and black fill.
        oledC_DrawRectangle(8, 40, 44, 64, OLEDC_COLOR_BLACK);
        oledC_DrawRectangle(10, 42, 42, 62, OLEDC_COLOR_BLACK);

        oledC_DrawRectangle(50, 40, 86, 64, OLEDC_COLOR_WHITE);
        oledC_DrawRectangle(52, 42, 84, 62, OLEDC_COLOR_BLACK);
    }

    drawSetTimeStatus();
}

// Update the displayed numbers (hours and minutes).
void drawSetTimeStatus(void)
{
    char buf[3];

    // Clear the region where the hour value is drawn.
    oledC_DrawRectangle(15, 46, 43, 62, OLEDC_COLOR_BLACK);
    sprintf(buf, "%02d", setClock.hours);
    oledC_DrawString(15, 46, 2, 2, (uint8_t *)buf, OLEDC_COLOR_WHITE);

    // Clear the region where the minute value is drawn.
    oledC_DrawRectangle(55, 46, 83, 62, OLEDC_COLOR_BLACK);
    sprintf(buf, "%02d", setClock.minutes);
    oledC_DrawString(55, 46, 2, 2, (uint8_t *)buf, OLEDC_COLOR_WHITE);
}

// Handle input for the Set Time page using RA11 (S1) and RA12 (S2).
void handleSetTimeInput(void)
{
    // Using the same structure as in your 12H/24H page.
    static bool s1WasPressed = false;
    static bool s2WasPressed = false;

    // Read buttons using RA11 and RA12.
    bool s1State = (PORTAbits.RA11 == 0);
    bool s2State = (PORTAbits.RA12 == 0);

    // If both buttons are pressed (new press), toggle the selected field.
    if (s1State && s2State && !s1WasPressed && !s2WasPressed)
    {
        timeSelection = !timeSelection;
        drawSetTimeMenuBase();
    }
    // If S1 is pressed alone, increase the selected value.
    else if (s1State && !s1WasPressed && !(s1State && s2State))
    {
        if (timeSelection == 0)
            setClock.hours = (setClock.hours + 1) % 24;
        else
            setClock.minutes = (setClock.minutes + 1) % 60;
        drawSetTimeStatus();
    }
    // If S2 is pressed alone, decrease the selected value.
    else if (s2State && !s2WasPressed && !(s1State && s2State))
    {
        if (timeSelection == 0)
            setClock.hours = (setClock.hours == 0) ? 23 : setClock.hours - 1;
        else
            setClock.minutes = (setClock.minutes == 0) ? 59 : setClock.minutes - 1;
        drawSetTimeStatus();
    }

    s1WasPressed = s1State;
    s2WasPressed = s2State;
}
bool detectTiltForSave(void)
{
    ACCEL_DATA_t accel;
    accel.x = readAxis(REG_DATAX0);
    accel.y = readAxis(REG_DATAY0);
    accel.z = readAxis(REG_DATAZ0);

    // Lower threshold => requires a more extreme tilt to return true.
    // (Example: 600.0 is stricter than 700.0.)
    const float tiltThreshold = 600.0f;

    // Convert raw values to a magnitude in 'mg' (if baseline is 1024 = 1G).
    float ax = accel.x * 4.0f;
    float ay = accel.y * 4.0f;
    float az = accel.z * 4.0f;
    float magnitude = sqrtf(ax * ax + ay * ay + az * az);

    // We say “tilt” = “magnitude is below tiltThreshold.”
    // e.g. if normal ~1024 mg is upright, then <600 mg is an extreme orientation.
    return (magnitude < tiltThreshold);
}

void handleSetTimePage(void)
{
    inTimeSetSubpage = true; // Enter Set Time page.

    // Initialize temporary time values from currentTime.
    setClock.hours = currentTime.hours;
    setClock.minutes = currentTime.minutes;
    timeSelection = 0; // Start with hours selected.

    drawSetTimeMenuBase();

    // Wait until both buttons are released.
    while ((PORTAbits.RA11 == 0) || (PORTAbits.RA12 == 0))
    {
        DELAY_milliseconds(10);
    }

    int tiltCounter = 0; // Debounce counter for the tilt
    // Poll for tilt while handling input.
    while (inTimeSetSubpage)
    {
        handleSetTimeInput();

        if (detectTiltForSave())
        {
            tiltCounter++;
            // Save if tilt is detected for one iteration (~50ms)
            if (tiltCounter >= 1)
            {
                currentTime.hours = setClock.hours;
                currentTime.minutes = setClock.minutes;
                currentTime.seconds = 0; // Reset seconds to 00

                inTimeSetSubpage = false;
                break;
            }
        }
        else
        {
            tiltCounter = 0; // Reset if no tilt is detected.
        }

        DELAY_milliseconds(50); // Reduced delay for quicker response
    }
}

// ---------------- SET DATE SYSTEM ---------------- //
void drawSetDateMenuBase(void)
{
    oledC_clearScreen();

    // Clear any mini clock area.
    oledC_DrawRectangle(30, 2, 115, 10, OLEDC_COLOR_BLACK);

    // Display header.
    oledC_DrawString(6, 10, 2, 2, (uint8_t *)"Set Date", OLEDC_COLOR_WHITE);

    if (dateSelection == 0)
    {
        // Day selected: draw day box with white border and black fill.
        oledC_DrawRectangle(8, 40, 44, 64, OLEDC_COLOR_WHITE);
        oledC_DrawRectangle(10, 42, 42, 62, OLEDC_COLOR_BLACK);

        // Draw month box completely black.
        oledC_DrawRectangle(50, 40, 86, 64, OLEDC_COLOR_BLACK);
        oledC_DrawRectangle(52, 42, 84, 62, OLEDC_COLOR_BLACK);
    }
    else
    {
        // Month selected: draw month box with white border and black fill.
        oledC_DrawRectangle(8, 40, 44, 64, OLEDC_COLOR_BLACK);
        oledC_DrawRectangle(10, 42, 42, 62, OLEDC_COLOR_BLACK);

        oledC_DrawRectangle(50, 40, 86, 64, OLEDC_COLOR_WHITE);
        oledC_DrawRectangle(52, 42, 84, 62, OLEDC_COLOR_BLACK);
    }

    drawSetDateStatus();
}

void drawSetDateStatus(void)
{
    char buf[3];

    // Clear and draw the day value.
    oledC_DrawRectangle(15, 46, 43, 62, OLEDC_COLOR_BLACK);
    sprintf(buf, "%02d", setDate.day);
    oledC_DrawString(15, 46, 2, 2, (uint8_t *)buf, OLEDC_COLOR_WHITE);

    // Clear and draw the month value.
    oledC_DrawRectangle(55, 46, 83, 62, OLEDC_COLOR_BLACK);
    sprintf(buf, "%02d", setDate.month);
    oledC_DrawString(55, 46, 2, 2, (uint8_t *)buf, OLEDC_COLOR_WHITE);
}

void handleSetDateInput(void)
{
    static bool s1WasPressed = false;
    static bool s2WasPressed = false;

    bool s1State = (PORTAbits.RA11 == 0);
    bool s2State = (PORTAbits.RA12 == 0);

    // If both buttons are pressed (new press), toggle the selected field.
    if (s1State && s2State && !s1WasPressed && !s2WasPressed)
    {
        dateSelection = !dateSelection;
        drawSetDateMenuBase();
    }
    // If S1 is pressed alone, increase the selected value.
    else if (s1State && !s1WasPressed && !(s1State && s2State))
    {
        if (dateSelection == 0) // adjust day
        {
            uint8_t maxDay = daysInMonth[setDate.month - 1];
            setDate.day = (setDate.day % maxDay) + 1;
        }
        else // adjust month
        {
            setDate.month = (setDate.month % 12) + 1;
            // Ensure day does not exceed the new month's max.
            uint8_t maxDay = daysInMonth[setDate.month - 1];
            if (setDate.day > maxDay)
                setDate.day = maxDay;
        }
        drawSetDateStatus();
    }
    // If S2 is pressed alone, decrease the selected value.
    else if (s2State && !s2WasPressed && !(s1State && s2State))
    {
        if (dateSelection == 0) // adjust day
        {
            if (setDate.day == 1)
                setDate.day = daysInMonth[setDate.month - 1];
            else
                setDate.day--;
        }
        else // adjust month
        {
            if (setDate.month == 1)
                setDate.month = 12;
            else
                setDate.month--;
            uint8_t maxDay = daysInMonth[setDate.month - 1];
            if (setDate.day > maxDay)
                setDate.day = maxDay;
        }
        drawSetDateStatus();
    }

    s1WasPressed = s1State;
    s2WasPressed = s2State;
}

void handleSetDatePage(void)
{
    inTimeSetSubpage = true; // Reuse the same flag for a subpage.

    // Initialize temporary date values from currentTime.
    setDate.day = currentTime.day;
    setDate.month = currentTime.month;
    dateSelection = 0; // Start with day selected.

    drawSetDateMenuBase();

    // Wait until both buttons are released.
    while ((PORTAbits.RA11 == 0) || (PORTAbits.RA12 == 0))
    {
        DELAY_milliseconds(10);
    }

    int tiltCounter = 0; // Debounce counter.
    while (inTimeSetSubpage)
    {
        handleSetDateInput();

        if (detectTiltForSave())
        {
            tiltCounter++;
            // Save if a tilt is detected (using your chosen sensitivity).
            if (tiltCounter >= 1)
            {
                currentTime.day = setDate.day;
                currentTime.month = setDate.month;
                // Exit the set date page.
                inTimeSetSubpage = false;
                break;
            }
        }
        else
        {
            tiltCounter = 0;
        }

        DELAY_milliseconds(50);
    }
}

// ---------------- STEPS GRAPH SYSTEM ---------------- //
void stepsGraph(void)
{
    graphActive = true;
    bool localGraphMode = true;

    oledC_clearScreen(); // Only clear the screen once to avoid flickering

    int x_left = 5;
    int x_right = GRAPH_WIDTH;
    int baseline = GRAPH_HEIGHT - 10;
    int top = 10;

    // Draw static gridlines
    int step_values[] = {30, 60, 100};
    for (int i = 0; i < 3; i++)
    {
        int val = step_values[i];
        int y_pos = baseline - ((val * (baseline - top)) / 100);

        for (int x = x_left; x <= x_right; x += 3)
        {
            oledC_DrawPoint(x, y_pos, OLEDC_COLOR_GRAY);
        }

        char label[4];
        sprintf(label, "%d", val);
        oledC_DrawString(0, y_pos - 10, 1, 1, (uint8_t *)label, OLEDC_COLOR_WHITE);
    }

    // Draw x-axis ticks (every 10 seconds)
    for (int i = 0; i <= 9; i++)
    {
        int x_tick = x_left + (i * (x_right - x_left) / 9);
        oledC_DrawRectangle(x_tick, baseline - 2, x_tick + 2, baseline, OLEDC_COLOR_WHITE);
    }

    // ----------------------------------------------------------------
    // 4) Plot the stored step rates as a continuous line
    // ----------------------------------------------------------------
    int prevX = x_left;
    int prevY = baseline - ((instantStepRate[0] * (baseline - top)) / 100);

    for (int i = 1; i < GRAPH_WIDTH; i++)
    {
        int curX = x_left + (i * (x_right - x_left) / (GRAPH_WIDTH - 1));
        int curY = baseline - ((instantStepRate[i] * (baseline - top)) / 100);

        if (instantStepRate[i] > 0 || instantStepRate[i - 1] > 0)
        {
            oledC_DrawLine(prevX, prevY, curX, curY, 1, OLEDC_COLOR_WHITE);
        }

        prevX = curX;
        prevY = curY;
    }

    static uint8_t s1HoldCounter = 0;

    while (localGraphMode)
    {
        bool s1State = (PORTAbits.RA11 == 0);
        bool s2State = (PORTAbits.RA12 == 0);

        // S2 => back to menu
        if (s2State)
        {
            localGraphMode = false;
            inMenu = true;
            drawMenu();
            break;
        }

        // Long S1 => main clock
        if (s1State)
        {
            s1HoldCounter++;
            // ~2 seconds with a 100ms loop
            if (s1HoldCounter >= 20)
            {
                oledC_clearScreen(); // *** Added: Clear the screen before leaving graph mode ***
                localGraphMode = false;
                inMenu = false; // Return to main clock screen
                forceClockRedraw = true;
                break;
            }
        }
        else
        {
            s1HoldCounter = 0;
        }

        DELAY_milliseconds(100);
    }

    graphActive = false; // We’re leaving the graph
}

// ---------------- MENU SYSTEM  ----------------
#define MENU_ITEMS_COUNT 5
const char *menuItems[MENU_ITEMS_COUNT] = {
    "Pedometer Graph",
    "12H/24H Interval",
    "Set Time",
    "Set Date",
    "Exit"};

bool inMenu = false;
uint8_t selectedMenuItem = 0;

void drawMenu(void)
{
    // Clear the screen to black.
    oledC_clearScreen();

    // Draw the mini clock at the top right.
    updateMenuClock();

    // Draw menu items (no "MENU" label)
    for (uint8_t i = 0; i < MENU_ITEMS_COUNT; i++)
    {
        uint8_t yPos = 20 + (i * 12);
        if (i == selectedMenuItem)
        {
            // Draw a white highlight rectangle for the selected item.
            oledC_DrawRectangle(3, yPos - 2, 115, yPos + 10, OLEDC_COLOR_WHITE);
            // Draw black text inside the blue rectangle.
            oledC_DrawString(4, yPos, 1, 1, (uint8_t *)menuItems[i], OLEDC_COLOR_BLACK);
        }
        else
        {
            // Draw unselected items as white text on black.
            oledC_DrawString(4, yPos, 1, 1, (uint8_t *)menuItems[i], OLEDC_COLOR_WHITE);
        }
    }
}

void updateMenuClock(void)
{
    char timeStr[9], buff[3];
    // If 12H, subtract 12 if hours >= 12, etc.
    // Then append AM/PM if is12HourFormat is true.

    uint8_t displayHrs = currentTime.hours;
    bool pm = false;
    if (is12HourFormat)
    {
        if (displayHrs >= 12)
        {
            pm = true;
            if (displayHrs > 12)
                displayHrs -= 12;
        }
        else if (displayHrs == 0)
        {
            displayHrs = 12; // midnight hour = 12 AM
        }
    }

    twoDigitString(displayHrs, buff);
    strcpy(timeStr, buff);
    strcat(timeStr, ":");
    twoDigitString(currentTime.minutes, buff);
    strcat(timeStr, buff);
    strcat(timeStr, ":");
    twoDigitString(currentTime.seconds, buff);
    strcat(timeStr, buff);

    // Clear old clock
    oledC_DrawRectangle(30, 2, 115, 10, OLEDC_COLOR_BLACK);
    // Draw time
    oledC_DrawString(30, 2, 1, 1, (uint8_t *)timeStr, OLEDC_COLOR_WHITE);

    // If 12H, append AM/PM
    if (is12HourFormat)
    {
        if (pm)
            oledC_DrawString(80, 2, 1, 1, (uint8_t *)"PM", OLEDC_COLOR_WHITE);
        else
            oledC_DrawString(80, 2, 1, 1, (uint8_t *)"AM", OLEDC_COLOR_WHITE);
    }
}

void executeMenuAction(void)
{
    switch (selectedMenuItem)
    {
    case 0:
        stepsGraph();
        // drawMenu();
        break;
    case 1: // "12H/24H Interval"
        handleTimeFormatSelection();
        drawMenu();
        break;
    case 2: // "Set Time"
        // inMenu = false;
        handleSetTimePage();
        drawMenu();
        break;
    case 3: // "Set Date"
        handleSetDatePage();
        drawMenu();
        break;
    case 4: // "Exit"
        inMenu = false;
        forceClockRedraw = true;
        oledC_DrawRectangle(40, 2, 115, 10, OLEDC_COLOR_BLACK);
        oledC_clearScreen();
        break;

    default:
        break;
    }
}

// ---------------- TIMER & USER INITIALIZATION ---------------- //

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

    // Set S1 and S2 as inputs
    S1_TRIS = 1;
    S2_TRIS = 1;
}

// ---------------- TIMER1 INTERRUPT  ----------------
// Global or file-scope variable to indicate we just entered the menu
static bool justEnteredMenu = false;

void __attribute__((__interrupt__, auto_psv)) _T1Interrupt(void)
{
    // 1) Existing clock increments
    incrementTime(&currentTime);
    globalSeconds++;

    // 2) Toggle foot icon (if you still use that)
    footToggle = !footToggle;

    // 3) Check if S1 is held to enter the menu (only if not in the graph)
    if (!graphActive)
    {
        static uint8_t s1HoldCounter = 0;
        bool s1State = (PORTAbits.RA11 == 0);
        if (s1State)
        {
            s1HoldCounter++;
            // If held for ~2 seconds, open menu
            if (s1HoldCounter >= 2 && !inMenu)
            {
                inMenu = true;
                selectedMenuItem = 0;
                drawMenu();
                justEnteredMenu = true;
                s1HoldCounter = 0;
            }
        }
        else
        {
            s1HoldCounter = 0;
        }
    }

    // 4) If not in the menu, do normal second-based housekeeping
    if (!inMenu)
    {
        // Advance your ring buffer index for stepsHistory (if you still use it)
        currentSecondIndex = (currentSecondIndex + 1) % HISTORY_SIZE;
        stepsHistory[currentSecondIndex] = 0;

        // ---- NEW SMOOTHING LOGIC ----
        static uint16_t oldSteps = 0;
        static uint8_t inactivity = 0;

        // How many steps happened since last second?
        uint16_t totalSteps = stepCount;
        uint16_t stepsInLastSecond = totalSteps - oldSteps;
        oldSteps = totalSteps;

        // Convert that to steps/min (steps * 60)
        float newPace = (float)stepsInLastSecond * 60.0f;

        // Exponential smoothing:
        float alpha = 0.15f; // lower = smoother, but slower
        displayedPace = (1.0f - alpha) * displayedPace + alpha * newPace;

        // If no steps for a while, gradually decay displayedPace to zero
        if (stepsInLastSecond == 0)
        {
            inactivity++;
            if (inactivity >= 3) // e.g. after 3 consecutive seconds of no steps
            {
                displayedPace *= 0.90f; // decay 10% per second
                if (displayedPace < 1.0f)
                    displayedPace = 0.0f;
            }
        }
        else
        {
            inactivity = 0;
        }

        // Optional: clamp to max 100 steps/min if you want
        if (displayedPace > 100.0f)
        {
            displayedPace = 100.0f;
        }

        // Store the smoothed pace into your graph buffer (for the line plot)
        instantStepRate[globalSeconds % GRAPH_WIDTH] = (uint8_t)displayedPace;
    }

    // 5) Clear the Timer1 interrupt flag
    IFS0bits.T1IF = 0;
}

// ---------------- MAIN ----------------

int main(void)
{
    int rc;
    uint8_t deviceId = 0;
    SYSTEM_Initialize();
    User_Initialize();
    oledC_setBackground(OLEDC_COLOR_BLACK);
    oledC_clearScreen();
    i2c1_open();

    // Detect the accelerometer
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
    static bool wasInMenu = false;

    while (1)
    {
        if (inMenu)
        {
            // In menu mode: Handle navigation and update the mini clock.
            static bool s1WasPressed = false;
            static bool s2WasPressed = false;

            if (justEnteredMenu)
            {
                // When first entering the menu, wait for the user to release the buttons.
                justEnteredMenu = false;
                bool s1State = (PORTAbits.RA11 == 0);
                bool s2State = (PORTAbits.RA12 == 0);
                s1WasPressed = s1State;
                s2WasPressed = s2State;
            }
            else
            {
                // Poll the button states.
                bool s1State = (PORTAbits.RA11 == 0);
                bool s2State = (PORTAbits.RA12 == 0);

                // If both buttons are pressed, execute the selected action and delay.
                if (s1State && s2State)
                {
                    executeMenuAction();
                    DELAY_milliseconds(200); // Wait 200ms to allow for debouncing.
                }
                else
                {
                    // If S1 is newly pressed, move selection UP.
                    if (s1State && !s1WasPressed)
                    {
                        if (selectedMenuItem > 0)
                            selectedMenuItem--;
                        drawMenu();
                    }
                    // If S2 is newly pressed, move selection DOWN.
                    if (s2State && !s2WasPressed)
                    {
                        if (selectedMenuItem < MENU_ITEMS_COUNT - 1)
                            selectedMenuItem++;
                        drawMenu();
                    }
                }
                s1WasPressed = s1State;
                s2WasPressed = s2State;

                // Update the mini clock so it stays current.
                updateMenuClock();
            }
            wasInMenu = true;
        }
        else
        {
            // When not in menu: If we were in menu in the previous loop, clear the mini clock area.
            if (wasInMenu)
            {
                oledC_DrawRectangle(30, 2, 115, 10, OLEDC_COLOR_BLACK);
                wasInMenu = false;
            }

            // Run the normal pedometer/clock functions.
            detectStep();
            drawSteps();
            drawClock(&currentTime);
            oledC_DrawRectangle(0, 0, 15, 15, OLEDC_COLOR_BLACK);
            if (displayedPace > 0)
                drawFootIcon(0, 0, footToggle ? foot1Bitmap : foot2Bitmap, 16, 16);
        }

        DELAY_milliseconds(100);
    }

    return 0;
}
