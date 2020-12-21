#ifndef _SERIAL_DBG_H_
#define _SERIAL_DBG_H_

#include <avr/io.h>
#include "util.h"

#define SERIAL_BUFFER_SIZE 100

PUBLIC uint8_t initUartDBG(uint8_t ucDataBits, uint8_t eParity);

PUBLIC bool getUartByteDBG(uint8_t *byte);
PUBLIC bool sendUartByteDBG(uint8_t byte);
PUBLIC bool isEmptyUartBufferDBG(void);

#endif