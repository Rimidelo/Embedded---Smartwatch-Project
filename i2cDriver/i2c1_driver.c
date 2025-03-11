/*
    (c) 2020 Microchip Technology Inc. and its subsidiaries. You may use this
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

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "i2c1_driver.h"

#ifndef FCY
#define FCY _XTAL_FREQ / 2
#endif

#include <xc.h>

void (*i2c1_driver_busCollisionISR)(void);
void (*i2c1_driver_Masteri2cISR)(void);
void (*i2c1_driver_Slavei2cISR)(void);

#include "i2c1_driver.h" // Make sure this header is available


void i2cReadSlaveRegister(uint8_t slaveAddr, uint8_t reg, unsigned char *dataPtr)
{
    // Generate a Start condition
    i2c1_driver_start();

    // Transmit the slave address in write mode (LSB = 0)
    i2c1_driver_TXData(slaveAddr << 1);
    while (I2C1STATbits.TRSTAT)
        ; // wait until transmission completes

    // Transmit the register address
    i2c1_driver_TXData(reg);
    while (I2C1STATbits.TRSTAT)
        ;

    // Generate a Repeated Start condition
    i2c1_driver_restart();

    // Transmit the slave address in read mode (LSB = 1)
    i2c1_driver_TXData((slaveAddr << 1) | 1);
    while (I2C1STATbits.TRSTAT)
        ;

    // Start reception of data
    i2c1_driver_startRX();
    i2c1_driver_waitRX();

    // Retrieve the received data and store it
    *dataPtr = i2c1_driver_getRXData();

    // Send a NACK to signal the end of the read and then a Stop condition
    i2c1_driver_sendNACK();
    i2c1_driver_stop();
}

void i2c1_driver_close(void)
{
    I2C1CONLbits.I2CEN = 0;
}

bool i2c1_driver_open(void)
{
    if (!I2C1CONLbits.I2CEN)
    {
        // initialize the hardware
        // STAT Setting
        I2C1STAT = 0x0;

        // CON Setting
        I2C1CONL = 0x8000;

        // Baud Rate Generator Value: I2CBRG 100000;
        I2C1BRG = 18;

        return true;
    }
    else
        return false;
}

void i2c1_driver_start(void)
{
    I2C1CONLbits.SEN = 1;
    while (I2C1CONLbits.SEN)
        ;
}

void i2c1_driver_restart(void)
{
    I2C1CONLbits.RSEN = 1;
    while (I2C1CONLbits.RSEN)
        ;
}

void i2c1_driver_stop(void)
{
    I2C1CONLbits.PEN = 1;
    while (I2C1CONLbits.PEN)
        ;
}

bool i2c1_driver_isNACK(void)
{
    return I2C1STATbits.ACKSTAT;
}

void i2c1_driver_startRX(void)
{
    I2C1CONLbits.RCEN = 1;
}

void i2c1_driver_waitRX(void)
{
    while (I2C1CONLbits.RCEN)
        ;
}

char i2c1_driver_getRXData(void)
{
    return I2C1RCV;
}

void i2c1_driver_TXData(uint8_t d)
{
    I2C1TRN = d;
    while (I2C1STATbits.TRSTAT)
        ;
}

void i2c1_driver_sendACK(void)
{
    I2C1CONLbits.ACKDT = 0;
    I2C1CONLbits.ACKEN = 1; // start the ACK/NACK
    while (I2C1CONLbits.ACKEN)
        ;
}

void i2c1_driver_sendNACK(void)
{
    I2C1CONLbits.ACKDT = 1;
    I2C1CONLbits.ACKEN = 1; // start the ACK/NACK
}

void i2c1_driver_clearBusCollision(void)
{
    I2C1STATbits.BCL = 0; // clear the bus collision.
}
