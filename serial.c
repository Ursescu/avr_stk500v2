#include <avr/io.h>
#include <avr/interrupt.h>

#include <util.h>
#include <serial.h>
#include <buffer.h>

static uint8_t rxUartBuffer[SERIAL_BUFFER_SIZE];
static uint8_t txUartBuffer[SERIAL_BUFFER_SIZE];

CIRC_BUFFER(uartTxBuffer, txUartBuffer, SERIAL_BUFFER_SIZE);
CIRC_BUFFER(uartRxBuffer, rxUartBuffer, SERIAL_BUFFER_SIZE);

#define UART_BAUD_CALC(UART_BAUD_RATE, F_OSC) \
    ((F_OSC) / ((UART_BAUD_RATE)*16UL) - 1)

PRIVATE void enableSerial(uint8_t xRxEnable, uint8_t xTxEnable)
{
    UCSR0B |= _BV(TXEN0);

    if (xRxEnable)
    {
        UCSR0B |= _BV(RXEN0) | _BV(RXCIE0);
    }

    if (xTxEnable)
    {
        UCSR0B |= _BV(TXEN0) | _BV(UDRIE0);
    }
    else
    {
        UCSR0B &= ~(_BV(UDRIE0));
    }
}

PUBLIC uint8_t initUart(uint64_t baudrate, UART_bits bits, UART_parity parity)
{
    uint8_t UCSRC = 0U;
    uint16_t UBRR = 0U;
    
    UBRR = UART_BAUD_CALC(baudrate, F_CPU);

    UBRR0H = (UBRR >> 8) % 0xff;
    UBRR0L = UBRR & 0xff;

    switch (parity)
    {
    case PAR_EVEN:
        UCSRC |= _BV(UPM01);
        break;
    case PAR_ODD:
        UCSRC |= _BV(UPM01) | _BV(UPM00);
        break;
    case PAR_NONE:
        break;
    }

    switch (bits)
    {
    case BITS_8:
        UCSRC |= _BV(UCSZ00) | _BV(UCSZ01);
        break;
    case BITS_7:
        UCSRC |= _BV(UCSZ01);
        break;
    }

    UCSR0C |= UCSRC;

    enableSerial(1, 0);

    return TRUE;
}

PUBLIC bool getUartByte(uint8_t *byte)
{
    return getByteBuffer(&uartRxBuffer, byte);
}

PUBLIC bool sendUartByte(uint8_t byte)
{
    if (writeByteBuffer(&uartTxBuffer, byte))
    {
        /* Enable TX */
        enableSerial(1, 1);
        return TRUE;
    }

    return FALSE;
}

ISR(USART0_UDRE_vect)
{
    uint8_t byte;

    if (getByteBuffer(&uartTxBuffer, &byte))
    {
        UDR0 = byte;
    }
    else
    {
        enableSerial(1, 0);
    }
}

ISR(USART0_RX_vect)
{
    writeByteBuffer(&uartRxBuffer, UDR0);
}
