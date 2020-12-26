#ifndef _DBG_H_
#define _DBG_H_

#include <avr/io.h>
#include <util.h>

#define DEBUG_UART_BAUD 38400

PUBLIC bool initDBG(void);

PUBLIC int printfDBG(const char *__fmt, ...);

#endif