#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>

#include <dbg.h>
#include "serialdbg.h"

volatile bool initiated = FALSE;

PRIVATE int dbg_putchar_printf(char var, FILE *stream) {
    sendUartByteDBG(var);

    /* Make it blocking */
    while (!isEmptyUartBufferDBG()) {
        ;
    }

    return 0;
}

static FILE uartStdout = FDEV_SETUP_STREAM(dbg_putchar_printf, NULL, _FDEV_SETUP_WRITE);

PUBLIC bool initDBG(void) {
    stdout = &uartStdout;

    initUartDBG(DEBUG_UART_BAUD, BITS_8, PAR_NONE);

    initiated = TRUE;

    return TRUE;
}

PUBLIC int printfDBG(const char *__fmt, ...) {
    int ret = -1;
    if (initiated) {
        va_list args;
        va_start(args, __fmt);
        ret = vprintf(__fmt, args);
        va_end(args);
    }

    return ret;
}
