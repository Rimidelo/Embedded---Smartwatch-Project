 /*
     (c) 2016 Microchip Technology Inc. and its subsidiaries. You may use this
    software and any derivatives exclusively with Microchip products.

    THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
    EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
    WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
    PARTICULAR PURPOSE, OR ITS INTERACTION WITH MICROCHIP PRODUCTS, COMBINATION
    WITH ANY OTHER PRODUCTS, OR USE IN ANY APPLICATION.

    IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
    WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
    BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
    FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN
    ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
    THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.

    MICROCHIP PROVIDES THIS SOFTWARE CONDITIONALLY UPON YOUR ACCEPTANCE OF THESE
    TERMS.
 */

#ifndef OLED_C_H
#define	OLED_C_H

#include <stdint.h>
#include <stdbool.h>

typedef struct oledc_color_t 
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} oledc_color_t;

typedef enum OLEDC_CMDS 
{
    OLEDC_CMD_SET_COLUMN_ADDRESS = 0x15,
    OLEDC_CMD_SET_ROW_ADDRESS = 0x75,
    OLEDC_CMD_WRITE_RAM = 0x5C,
    OLEDC_CMD_READ_RAM = 0x5D,
    OLEDC_CMD_SET_REMAP_DUAL_COM_LINE_MODE = 0xA0,
    OLEDC_CMD_SET_DISPLAY_START_LINE = 0xA1,
    OLEDC_CMD_SET_DISPLAY_OFFSET = 0xA2,
    OLEDC_CMD_SET_DISPLAY_MODE_OFF_BLACK = 0xA4,
    OLEDC_CMD_SET_DISPLAY_MODE_OFF_GS60 = 0xA5,
    OLEDC_CMD_SET_DISPLAY_MODE_ON = 0xA6,
    OLEDC_CMD_SET_DISPLAY_MODE_INVERSE = 0xA7,
    OLEDC_CMD_SET_FUNCTION_SELECTION = 0xAB,
    OLEDC_CMD_SET_SLEEP_MODE_ON = 0xAE,
    OLEDC_CMD_SET_SLEEP_MODE_OFF = 0xAF,
    OLEDC_CMD_SET_PHASE_LENGTH = 0xB1,
    OLEDC_CMD_DISPLAY_ENHANCEMENT = 0xB2,
    OLEDC_CMD_SET_FRONT_CLOCK_DIVIDER_OSC_FREQ = 0xB3,
    OLEDC_CMD_SET_GPIO = 0xB5,
    OLEDC_CMD_SET_SECOND_PRECHARGE_PERIOD = 0xB6,
    OLEDC_CMD_GRAY_SCALE_PULSE_WIDTH_LUT = 0xB8,
    OLEDC_CMD_USE_LINEAR_LUT = 0xB9,
    OLEDC_CMD_SET_PRECHARGE_VOLTAGE = 0xBB,
    OLEDC_CMD_SET_VCOMH_VOLTAGE = 0xBE,
    OLEDC_CMD_SET_CONTRAST_CURRENT = 0xC1,
    OLEDC_CMD_MASTER_CONTRAST_CURRENT_CONTROL = 0xC7,
    OLEDC_CMD_SET_MUX_RATIO = 0xCA,
    OLEDC_CMD_SET_COMMAND_LOCK = 0xFD
} OLEDC_COMMAND;

void oledC_sendCommand(OLEDC_COMMAND cmd, uint8_t *payload, uint8_t payload_size);

void oledC_setRowAddressBounds(uint8_t min, uint8_t max);
void oledC_setColumnAddressBounds(uint8_t min, uint8_t max);
void oledC_setSleepMode(bool on);
void oledC_setDisplayOrientation(void);
void oledC_setBackground(uint16_t color);

void oledC_clearScreen(void);
void oledC_startReadingDisplay(void);
void oledC_stopReadingDisplay(void);
uint16_t oledC_readColor(void);

bool oledC_open(void);
void oledC_setup(void);
void oledC_sendColor(uint8_t r, uint8_t g, uint8_t b);
void oledC_sendColorInt(uint16_t raw);
void oledC_startWritingDisplay(void);
void oledC_stopWritingDisplay(void);

#endif