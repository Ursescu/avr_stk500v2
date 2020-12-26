#ifndef _SERIAL_H_
#define _SERIAL_H_

#include <avr/io.h>
#include "util.h"

#define SERIAL_BUFFER_SIZE 100

typedef enum
{
    PAR_NONE,
    PAR_ODD,
    PAR_EVEN
} UART_parity;

typedef enum
{
    BITS_8,
    BITS_7
} UART_bits;

PUBLIC uint8_t initUart(uint64_t baudrate, UART_bits bits, UART_parity parity);

PUBLIC bool getUartByte(uint8_t *byte); 
PUBLIC bool sendUartByte(uint8_t byte);

#endif