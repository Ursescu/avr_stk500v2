#ifndef _MISC_H_
#define _MISC_H_

#include <stdint.h>

#define WRITE_BUF8(buffer, data, size)              \
    do {                                            \
        *((uint8_t *)(buffer)++) = (uint8_t)(data); \
        (size) += 1;                                \
    } while (0)

#define READ_BUF8(buffer, data)            \
    do {                                   \
        (data) = *((uint8_t *)(buffer)++); \
    } while (0)

#endif