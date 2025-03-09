/*
 * File:   LabC5-lighthouse.c
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

int potentio = 0;

void User_Initialize(void)
{
  // Initialize LED/Switch IO Direction (TRISx)
  TRISA &= ~(1 << 8); // LED1 as output
  TRISA &= ~(1 << 9); // LED2 as output
  TRISA |= (1 << 11); // S1 as input
  TRISA |= (1 << 12); // S2 as input

  // Set RB12 and ANSB as Analog Input
  TRISB |= (1 << 12);
  ANSB = 0;

  // Initialize A/D Circuit (AD1CON1)
  AD1CON1 = 0;
  AD1CON2 = 0;
  AD1CON3 = 0b001000011111111;

  // Select channel AN8 for A/D conversion
  AD1CHS = 0;
  AD1CHS |= (1 << 3);
  AD1CON1 |= (1 << 15);
}

void Timer_Initialize(void)
{
  TMR1 = 0;            // Clear Timer1 counter
  PR1 = 15625;         // Set Timer1 period for 1-second interval
  T1CONbits.TON = 0;   // Disable Timer1 during configuration
  T1CONbits.TCKPS = 3; // Prescaler 1:256
  T1CONbits.TCS = 0;   // Use internal clock (Fosc/2)
  T1CONbits.TGATE = 0; // Disable gated accumulation
  T1CONbits.TON = 1;   // Enable Timer1
}

void Timer1_Interrupt_Initialize(void)
{
  IPC0bits.T1IP = 5; // Set Timer1 interrupt priority to 5
  IFS0bits.T1IF = 0; // Clear Timer1 interrupt flag
  IEC0bits.T1IE = 1; // Enable Timer1 interrupt
}

void __attribute__((__interrupt__, auto_psv)) _T1Interrupt(void)
{
  static int toggle = 0;

  // Draw the lighthouse body
  oledC_DrawRectangle(40, 70, 60, 95, OLEDC_COLOR_BLUE); // Body
  oledC_DrawCircle(50, 65, 10, OLEDC_COLOR_BLUE);        // Top

  if (toggle)
  {
    // ON State: Add beams of light
    oledC_DrawRectangle(55, 62, 59, 68, OLEDC_COLOR_YELLOW); // Light in lighthouse top
    oledC_DrawLine(60, 65, 90, 50, 2, OLEDC_COLOR_YELLOW);   // Right beam
    oledC_DrawLine(60, 65, 90, 80, 2, OLEDC_COLOR_YELLOW);   // Lower-right beam
    oledC_DrawLine(60, 65, 90, 65, 2, OLEDC_COLOR_YELLOW);   // Center beam
  }
  else
  {
    // OFF State: Turn off the beams of light
    oledC_DrawRectangle(55, 62, 59, 68, OLEDC_COLOR_BLUE);
    oledC_DrawLine(60, 65, 90, 50, 2, OLEDC_COLOR_BLACK);
    oledC_DrawLine(60, 65, 90, 80, 2, OLEDC_COLOR_BLACK);
    oledC_DrawLine(60, 65, 90, 65, 2, OLEDC_COLOR_BLACK);
  }

  toggle = !toggle; // Toggle state

  IFS0bits.T1IF = 0; // Clear Timer1 interrupt flag
}

/* Main application */
int main(void)
{
  // Initialize system
  SYSTEM_Initialize();
  User_Initialize();
  Timer_Initialize();
  Timer1_Interrupt_Initialize();

  // Set OLED Background color and Clear the display
  oledC_clearScreen();

  // Main infinite loop
  while (1)
  {
  }
  return 1;
}
