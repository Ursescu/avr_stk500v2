#include "avr/io.h"
#include "avr/interrupt.h"

#include "util/delay.h"

#include "serial.h"
#include "buffer.h"
#include "util.h"
#include "spi.h"
#include "stk500/stk500.h"

#define SERIAL_BUFFER_SIZE 200

static uint8_t rxUartBuffer[SERIAL_BUFFER_SIZE];
static uint8_t txUartBuffer[SERIAL_BUFFER_SIZE];

CIRC_BUFFER(uartTxBuffer, txUartBuffer, SERIAL_BUFFER_SIZE);
CIRC_BUFFER(uartRxBuffer, rxUartBuffer, SERIAL_BUFFER_SIZE);

void serialTransmitCb(void)
{
    uint8_t byte;

    if (getByteBuffer(&uartTxBuffer, &byte))
    {  
        serialPutByte(byte);
    }
    else {
        enableSerial(1, 0);
    }
};

void serialReceiveCb(void)
{
    uint8_t byte = 0;
    serialGetByte(&byte);
    writeByteBuffer(&uartRxBuffer, byte);
};

bool pushToTxSerialQueue(uint8_t byte) {
    return writeByteBuffer(&uartTxBuffer, byte);
}

int main(void)
{
    cli();

    // Init serial
    initSerial(8, 2);

    // Enable receive and transmit
    enableSerial(1, 0);
    // // Init SPI, mode 0 with clock div 128
    initSPI(0, 3);

    initSTK500(pushToTxSerialQueue);

    // Debug led output
    DDRD |= _BV(PD7);

    // Enable interrupts
    sei();

    DDRD |= _BV(PD6);

    PORTD |= _BV(PD6);

    // Reset UC
    PORTD &= ~(_BV(PD6));

    uint8_t byte;

    // Main loop
    while (1)
    {
        // Get from UART
        if(getByteBuffer(&uartRxBuffer, &byte)) {
            processByteSTK500(byte);
        }

        // Call this every time
        if(handleCommandSTK500()) {
            enableSerial(1, 1);
        }

        // if (getByteSPI(&byte)) {
        //     writeByteBuffer(&uartTxBuffer, byte);
        //     enableSerial(0, 1);
        // }
    }

    return 0;
}