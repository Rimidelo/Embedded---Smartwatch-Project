/*
 * File:   Final-Project-Smartwatch.c
 * Author: Vladimir Lihatchov
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "System/system.h"
#include "System/delay.h"
#include "oledDriver/oledC.h"
#include "oledDriver/oledC_colors.h"
#include "oledDriver/oledC_shapes.h"

typedef struct
{
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    uint8_t day;
    uint8_t month;
} ClockTime;

static const uint8_t daysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
ClockTime currentTime = {.hours = 12, .minutes = 0, .seconds = 0, .day = 7, .month = 3}; // Example init

void User_Initialize(void);
void Timer_Initialize(void);
void Timer1_Interrupt_Initialize(void);

// Forward declarations
void incrementTime(ClockTime *time);
void drawClock(ClockTime *time);
static void twoDigitString(uint8_t val, char *buffer);

/* ------------------ Interrupt Service Routine (ISR) ------------------ */
void __attribute__((__interrupt__, auto_psv)) _T1Interrupt(void)
{
    // Increment time by 1 second
    incrementTime(&currentTime);

    // Clear interrupt flag
    IFS0bits.T1IF = 0;
}

/* ------------------ Main ------------------ */
int main(void)
{
    // System init
    SYSTEM_Initialize();
    User_Initialize();

    // Timer init
    Timer_Initialize();
    Timer1_Interrupt_Initialize();

    // Clear screen
    oledC_clearScreen();

    while (1)
    {
        // Continuously update the time & date display
        drawClock(&currentTime);
    }
    return 1;
}

/* ------------------ Function Definitions ------------------ */
void User_Initialize(void)
{
    TRISA &= ~(1 << 8); // LED1 as output
    TRISA &= ~(1 << 9); // LED2 as output
    TRISA |= (1 << 11); // S1 as input
    TRISA |= (1 << 12); // S2 as input

    TRISB |= (1 << 12);
    ANSB = 0;
    AD1CON1 = 0;
    AD1CON2 = 0;
    AD1CON3 = 0b001000011111111;
    AD1CHS = 0;
    AD1CHS |= (1 << 3);
    AD1CON1 |= (1 << 15);
}

void Timer_Initialize(void)
{
    TMR1 = 0;
    PR1 = 15625; // 1-second interval
    T1CONbits.TON = 0;
    T1CONbits.TCKPS = 3; // 1:256 prescaler
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

// Increments time by 1 second
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
                uint8_t mIndex = time->month - 1; // e.g. March => 2
                if (time->day > daysInMonth[mIndex])
                {
                    time->day = 1;
                    time->month++;
                    if (time->month > 12)
                    {
                        time->month = 1; // no year tracking
                    }
                }
            }
        }
    }
}

// Helper: Convert val -> "00".."59"
static void twoDigitString(uint8_t val, char *buffer)
{
    buffer[0] = (val / 10) + '0';
    buffer[1] = (val % 10) + '0';
    buffer[2] = '\0';
}

// Draw HH:MM:SS, plus DD/MM in bottom-right
void drawClock(ClockTime *time)
{
    static char oldTimeString[9] = "";
    static char oldDateString[6] = "";

    // Format new time string "HH:MM:SS"
    char newTimeString[9];
    char buff[3];

    twoDigitString(time->hours, buff);
    sprintf(newTimeString, "%s:", buff);

    twoDigitString(time->minutes, buff);
    strcat(newTimeString, buff);
    strcat(newTimeString, ":");

    twoDigitString(time->seconds, buff);
    strcat(newTimeString, buff);

    // If time changed, overwrite old time, draw new
    if (strcmp(oldTimeString, newTimeString) != 0)
    {
        // Erase old time
        oledC_DrawString(8, 45, 2, 2, oldTimeString, OLEDC_COLOR_BLACK);
        // Draw new time
        oledC_DrawString(8, 45, 2, 2, newTimeString, OLEDC_COLOR_WHITE);
        // Save new string
        strcpy(oldTimeString, newTimeString);
    }

    // Format new date string "DD/MM"
    char newDateString[6];
    twoDigitString(time->day, buff);
    sprintf(newDateString, "%s/", buff);
    twoDigitString(time->month, buff);
    strcat(newDateString, buff);

    // If date changed, overwrite old date, draw new
    if (strcmp(oldDateString, newDateString) != 0)
    {
        // Erase old date (small rectangle or old date text)
        oledC_DrawString(65, 85, 1, 1, oldDateString, OLEDC_COLOR_BLACK);
        // Draw new date
        oledC_DrawString(65, 85, 1, 1, newDateString, OLEDC_COLOR_WHITE);
        // Save new date
        strcpy(oldDateString, newDateString);
    }
}
