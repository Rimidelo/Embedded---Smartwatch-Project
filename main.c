/*
 * File:   Final-Project-Smartwatch.c
 * Author: Vladimir Lihatchov
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "System/system.h"
#include "System/delay.h"
#include "oledDriver/oledC.h"
#include "oledDriver/oledC_colors.h"
#include "oledDriver/oledC_shapes.h"

/* ------------------ Global Time Structure ------------------ */
typedef struct
{
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
} ClockTime;

ClockTime currentTime = {.hours = 12, .minutes = 0, .seconds = 0};

/* ------------------ Function Declarations ------------------ */
void User_Initialize(void);
void Timer_Initialize(void);
void Timer1_Interrupt_Initialize(void);

// Increments the clock by 1 second
void incrementTime(ClockTime *time);

// Draws the clock (HH:MM:SS) on the OLED
void drawClock(ClockTime *time);

/* ------------------ Interrupt Service Routine (ISR) ------------------ */
void __attribute__((__interrupt__, auto_psv)) _T1Interrupt(void)
{
  // 1) Increment clock
  incrementTime(&currentTime);

  // 2) Clear Timer1 interrupt flag
  IFS0bits.T1IF = 0;
}

/* ------------------ Main Application ------------------ */
int main(void)
{
  // 1) Initialize system and user configuration
  SYSTEM_Initialize();
  User_Initialize();

  // 2) Initialize Timer & Interrupt
  Timer_Initialize();
  Timer1_Interrupt_Initialize();

  // 3) Clear the display (black background)
  oledC_clearScreen();

  // 4) Main loop
  while (1)
  {
    // For demonstration, just redraw the clock each loop
    // (For better performance, you could track changes and only redraw when needed)
    drawClock(&currentTime);
  }

  return 1;
}

/* ------------------ Function Definitions ------------------ */

// Configure pins (switches, LEDs, optional ADC)
void User_Initialize(void)
{
  TRISA &= ~(1 << 8); // LED1 as output
  TRISA &= ~(1 << 9); // LED2 as output
  TRISA |= (1 << 11); // S1 as input
  TRISA |= (1 << 12); // S2 as input

  // If you need the ADC from your old code, keep it; else remove:
  TRISB |= (1 << 12);
  ANSB = 0;
  AD1CON1 = 0;
  AD1CON2 = 0;
  AD1CON3 = 0b001000011111111; // sample time
  AD1CHS = 0;
  AD1CHS |= (1 << 3);
  AD1CON1 |= (1 << 15);
}

void Timer_Initialize(void)
{
  TMR1 = 0;            // Clear Timer1 counter
  PR1 = 15625;         // Set Timer1 period for 1-second interval
  T1CONbits.TON = 0;   // Disable Timer1 during config
  T1CONbits.TCKPS = 3; // Prescaler 1:256
  T1CONbits.TCS = 0;   // Internal clock (Fosc/2)
  T1CONbits.TGATE = 0; // Disable gated
  T1CONbits.TON = 1;   // Enable Timer1
}

void Timer1_Interrupt_Initialize(void)
{
  IPC0bits.T1IP = 5; // Priority
  IFS0bits.T1IF = 0; // Clear flag
  IEC0bits.T1IE = 1; // Enable interrupt
}

// Add 1 second, handle rollover for seconds/minutes/hours
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
        time->hours = 0; // Next day
      }
    }
  }
}

// Simple helper to convert two digits to string: "00".."59"
static void twoDigitString(uint8_t val, char *buffer)
{
  buffer[0] = (val / 10) + '0';
  buffer[1] = (val % 10) + '0';
  buffer[2] = '\0';
}

// Draw "HH:MM:SS" at a fixed location on the OLED
void drawClock(ClockTime *time)
{
  // 1) Create a background rectangle to clear the old text
  oledC_DrawRectangle(0, 40, 95, 60, OLEDC_COLOR_BLACK);

  // 2) Format the time into a string "HH:MM:SS"
  char displayStr[9];
  char buff[4];

  // HH:
  twoDigitString(time->hours, buff);
  sprintf(displayStr, "%s:", buff);

  // MM:
  twoDigitString(time->minutes, buff);
  strcat(displayStr, buff);
  strcat(displayStr, ":");

  // SS
  twoDigitString(time->seconds, buff);
  strcat(displayStr, buff);

  // 3) Draw the string in the center or any position you like
  // (x=10, y=45 chosen arbitrarily)
  oledC_DrawString(10, 45, 2, 2, displayStr, OLEDC_COLOR_WHITE);
}
