#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <stdlib.h>
#include <util/delay.h>

#include <serial.h>
#include <buffer.h>
#include <util.h>
#include <spi.h>
#include <stk500.h>
#include <dbg.h>
#include <timer.h>

#define RESET_TARGET_DDR DDRD
#define RESET_TARGET_PORT PORTD
#define RESET_TARGET_PIN PD6

PRIVATE uartSendByteFunc_t sendUser = sendUartByte;
PRIVATE uartGetByteFunc_t getUser = getUartByte;
PRIVATE spiSendByteFunc_t sendTarget = sendSpiByte;
PRIVATE spiGetByteFunc_t getTarget = getSpiByte;
PRIVATE resetTargetFunc_t resetTarget = NULL;

#define UART_BAUDRATE 38400

int main(void)
{
    initMSTimer();

    // Init serial
    initUart(UART_BAUDRATE, BITS_8, PAR_NONE);

    // Enable debug print
    initDBG();

    // Init SPI, mode 0 with clock div 128
    initSPI(0, 3);

    initSTK500(sendUser, getUser, sendTarget, getTarget, resetTarget);

    // Debug led output
    DDRD |= _BV(PD7);

    // Set the reset pin to output 
    RESET_TARGET_DDR |= _BV(RESET_TARGET_PIN);

    // Enable interrupts
    sei();

    // Main loop
    while (1)
    {
        tickSTK500();
    }

    return 0;
}