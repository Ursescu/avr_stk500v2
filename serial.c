#include <avr/io.h>
#include <avr/interrupt.h>

#define BAUD 38400
#include <util/setbaud.h>

#include "util.h"
#include "serial.h"

#define UART_BAUD_CALC(UART_BAUD_RATE, F_OSC) \
    ((F_OSC) / ((UART_BAUD_RATE)*16UL) - 1)

void enableSerial(uint8_t xRxEnable, uint8_t xTxEnable)
{

    UCSR0B |= _BV(TXEN0);


    if (xRxEnable)
    {
        UCSR0B |= _BV(RXEN0) | _BV(RXCIE0);
    }
    else
    {
        UCSR0B &= ~(_BV(RXEN0) | _BV(RXCIE0));
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

uint8_t initSerial(uint8_t ucDataBits, uint8_t eParity)
{
    uint8_t ucUCSRC = 0;

    UBRR0H = UBRRH_VALUE;
    UBRR0L = UBRRL_VALUE;

    switch (eParity)
    {
    case 0:
        ucUCSRC |= _BV(UPM01);
        break;
    case 1:
        ucUCSRC |= _BV(UPM01) | _BV(UPM00);
        break;
    case 2:
        break;
    }

    switch (ucDataBits)
    {
    case 8:
        ucUCSRC |= _BV(UCSZ00) | _BV(UCSZ01);
        break;
    case 7:
        ucUCSRC |= _BV(UCSZ01);
        break;
    }

    UCSR0C |= ucUCSRC;

    return 1;
}

uint8_t serialPutByte(uint8_t ucByte)
{
    UDR0 = ucByte;
    return 1;
}

uint8_t serialGetByte(uint8_t *pucByte)
{
    *pucByte = UDR0;
    return 1;
}

ISR(USART0_UDRE_vect)
{
    serialTransmitCb();
}

ISR(USART0_RX_vect)
{
    serialReceiveCb();
}