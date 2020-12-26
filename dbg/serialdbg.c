#include <avr/io.h>
#include <avr/interrupt.h>

#define BAUD 250000
#include <util/setbaud.h>

#include "util.h"
#include "serial.h"
#include "buffer.h"

static uint8_t rxUartBuffer[SERIAL_BUFFER_SIZE];
static uint8_t txUartBuffer[SERIAL_BUFFER_SIZE];

CIRC_BUFFER(uartTxBuffer, txUartBuffer, SERIAL_BUFFER_SIZE);
CIRC_BUFFER(uartRxBuffer, rxUartBuffer, SERIAL_BUFFER_SIZE);

#define UART_BAUD_CALC(UART_BAUD_RATE, F_OSC) \
    ((F_OSC) / ((UART_BAUD_RATE)*16UL) - 1)

PRIVATE void enableSerial(uint8_t xRxEnable, uint8_t xTxEnable)
{
    UCSR1B |= _BV(TXEN1);

    if (xRxEnable)
    {
        UCSR1B |= _BV(RXEN1) | _BV(RXCIE1);
    }
    else
    {
        UCSR1B &= ~(_BV(RXEN1) | _BV(RXCIE1));
    }

    if (xTxEnable)
    {
        UCSR1B |= _BV(TXEN1) | _BV(UDRIE1);
    }
    else
    {
        UCSR1B &= ~(_BV(UDRIE1));
    }
}

PUBLIC uint8_t initUartDBG(uint64_t baudrate, UART_bits bits, UART_parity parity)
{
    uint8_t ucUCSRC = 0;
    uint16_t UBRR = 0U;

    UBRR = UART_BAUD_CALC(baudrate, F_CPU);

    UBRR1H = (UBRR >> 8) & 0xff;
    UBRR1L = UBRR & 0xff;

    switch (parity)
    {
    case PAR_EVEN:
        ucUCSRC |= _BV(UPM11);
        break;
    case PAR_ODD:
        ucUCSRC |= _BV(UPM11) | _BV(UPM10);
        break;
    case PAR_NONE:
        break;
    }

    switch (bits)
    {
    case BITS_8:
        ucUCSRC |= _BV(UCSZ10) | _BV(UCSZ11);
        break;
    case BITS_7:
        ucUCSRC |= _BV(UCSZ11);
        break;
    }

    UCSR1C |= ucUCSRC;

    enableSerial(1, 0);

    return TRUE;
}

PUBLIC bool getUartByteDBG(uint8_t *byte)
{
    return getByteBuffer(&uartRxBuffer, byte);
}

PUBLIC bool sendUartByteDBG(uint8_t byte)
{
    if (writeByteBuffer(&uartTxBuffer, byte))
    {
        /* Enable TX */
        enableSerial(1, 1);
        return TRUE;
    }

    return FALSE;
}

PUBLIC bool isEmptyUartBufferDBG(void)
{
    return isEmptyBuffer(&uartTxBuffer);
}

ISR(USART1_UDRE_vect)
{
    uint8_t byte;

    if (getByteBuffer(&uartTxBuffer, &byte))
    {
        UDR1 = byte;
    }
    else
    {
        enableSerial(1, 0);
    }
}

ISR(USART1_RX_vect)
{
    writeByteBuffer(&uartRxBuffer, UDR1);
}
