#ifndef _MISC_H_
#define _MISC_H_

#include <avr/io.h>

#define WRITE_BUF8(buffer, data, size)              \
    do                                              \
    {                                               \
        *((uint8_t *)(buffer)++) = (uint8_t)(data); \
        (size) += 1;                                \
    } while (0)

#endif